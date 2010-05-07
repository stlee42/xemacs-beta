/* X-specific Lisp objects.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.
   Copyright (C) 1995 Board of Trustees, University of Illinois.
   Copyright (C) 1995 Tinker Systems.
   Copyright (C) 1995, 1996, 2002 Ben Wing.
   Copyright (C) 1995 Sun Microsystems, Inc.

This file is part of XEmacs.

XEmacs is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

XEmacs is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with XEmacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Synched up with: Not in FSF. */

/* Authors: Jamie Zawinski, Chuck Thompson, Ben Wing */
/* Gtk version by William Perry */

#include <config.h>
#include "lisp.h"

#include "buffer.h"
#include "charset.h"
#include "device-impl.h"
#include "insdel.h"

#include "console-gtk-impl.h"
#include "fontcolor-gtk-impl.h"

/* sigh */
#include "sysgdkx.h"

/* XListFonts doesn't allocate memory unconditionally based on this. (For
   XFree86 in 2005, at least. */
#define MAX_FONT_COUNT INT_MAX

#ifdef DEBUG_XEMACS 
Fixnum debug_x_objects;
#endif /* DEBUG_XEMACS */


/************************************************************************/
/*                          color instances                             */
/************************************************************************/

/* Replacement for XAllocColor() that tries to return the nearest
   available color if the colormap is full.  Original was from FSFmacs,
   but rewritten by Jareth Hein <jareth@camelot-soft.com> 97/11/25
   Modified by Lee Kindness <lkindness@csl.co.uk> 31/08/99 to handle previous
   total failure which was due to a read/write colorcell being the nearest
   match - tries the next nearest...

   Gdk takes care of all this behind the scenes, so we don't need to
   worry about it.

   Return value is 1 for normal success, 2 for nearest color success,
   3 for Non-deallocable sucess. */
int
allocate_nearest_color (GdkColormap *colormap, GdkVisual *UNUSED (visual),
		        GdkColor *color_def)
{
  int rc;

  rc = gdk_colormap_alloc_color (colormap, color_def, FALSE, TRUE);

  if (rc == TRUE)
      return (1);

  return (0);
}

int
gtk_parse_nearest_color (struct device *d, GdkColor *color, Ibyte *name,
			 Bytecount len, Error_Behavior errb)
{
  GdkColormap *cmap;
  GdkVisual *visual;
  int result;

  cmap = DEVICE_GTK_COLORMAP(d);
  visual = DEVICE_GTK_VISUAL (d);

  xzero (*color);
  {
    const Extbyte *extname;
    Bytecount extnamelen;

    TO_EXTERNAL_FORMAT (DATA, (name, len), ALLOCA, (extname, extnamelen), Qbinary);

    result = gdk_color_parse (extname, color);
  }
  
  if (result == FALSE)
    {
      maybe_invalid_argument ("unrecognized color", make_string (name, len),
			  Qcolor, errb);
      return 0;
    }
  result = allocate_nearest_color (cmap, visual, color);
  if (!result)
    {
      maybe_signal_error (Qgui_error, "couldn't allocate color",
			  make_string (name, len), Qcolor, errb);
      return 0;
    }

  return result;
}

static int
gtk_initialize_color_instance (struct Lisp_Color_Instance *c, Lisp_Object name,
			       Lisp_Object device, Error_Behavior errb)
{
  GdkColor color;
  int result;

  result = gtk_parse_nearest_color (XDEVICE (device), &color,
				    XSTRING_DATA   (name),
				    XSTRING_LENGTH (name),
				    errb);

  if (!result)
    return 0;

  /* Don't allocate the data until we're sure that we will succeed,
     or the finalize method may get fucked. */
  c->data = xnew (struct gtk_color_instance_data);
  if (result == 3)
    COLOR_INSTANCE_GTK_DEALLOC (c) = 0;
  else
    COLOR_INSTANCE_GTK_DEALLOC (c) = 1;
  COLOR_INSTANCE_GTK_COLOR (c) = gdk_color_copy (&color);
  return 1;
}

static void
gtk_print_color_instance (struct Lisp_Color_Instance *c,
			  Lisp_Object printcharfun,
			  int UNUSED (escapeflag))
{
  GdkColor *color = COLOR_INSTANCE_GTK_COLOR (c);
  write_fmt_string (printcharfun, " %ld=(%X,%X,%X)",
		    color->pixel, color->red, color->green, color->blue);
}

static void
gtk_finalize_color_instance (struct Lisp_Color_Instance *c)
{
  if (c->data)
    {
      if (DEVICE_LIVE_P (XDEVICE (c->device)))
	{
	  if (COLOR_INSTANCE_GTK_DEALLOC (c))
	    {
		gdk_colormap_free_colors (DEVICE_GTK_COLORMAP (XDEVICE (c->device)),
					  COLOR_INSTANCE_GTK_COLOR (c), 1);
	    }
	    gdk_color_free (COLOR_INSTANCE_GTK_COLOR (c));
	}
      xfree (c->data);
      c->data = 0;
    }
}

/* Color instances are equal if they resolve to the same color on the
   screen (have the same RGB values).  I imagine that
   "same RGB values" == "same cell in the colormap."  Arguably we should
   be comparing their names or pixel values instead. */

static int
gtk_color_instance_equal (struct Lisp_Color_Instance *c1,
			  struct Lisp_Color_Instance *c2,
			  int UNUSED (depth))
{
    return (gdk_color_equal (COLOR_INSTANCE_GTK_COLOR (c1),
			     COLOR_INSTANCE_GTK_COLOR (c2)));
}

static Hashcode
gtk_color_instance_hash (struct Lisp_Color_Instance *c, int UNUSED (depth))
{
    return (gdk_color_hash (COLOR_INSTANCE_GTK_COLOR (c)));
}

static Lisp_Object
gtk_color_instance_rgb_components (struct Lisp_Color_Instance *c)
{
  GdkColor *color = COLOR_INSTANCE_GTK_COLOR (c);
  return (list3 (make_int (color->red),
		 make_int (color->green),
		 make_int (color->blue)));
}

static int
gtk_valid_color_name_p (struct device *UNUSED (d), Lisp_Object color)
{
  GdkColor c;
  const char *extname;

  extname = LISP_STRING_TO_EXTERNAL (color, Qctext);

  if (gdk_color_parse (extname, &c) != TRUE)
      return(0);
  return (1);
}

static Lisp_Object
gtk_color_list (void)
{
  /* #### BILL!!!
     Is this correct? */
  return call0 (intern ("x-color-list-internal"));
}


/************************************************************************/
/*                           font instances                             */
/************************************************************************/

static int
gtk_initialize_font_instance (struct Lisp_Font_Instance *f,
			      Lisp_Object UNUSED (name),
			      Lisp_Object UNUSED (device), Error_Behavior errb)
{
  GdkFont *gf;
  const char *extname;
  PangoFontDescription *pango_desc;
  
  extname = LISP_STRING_TO_EXTERNAL (f->name, Qctext);

  /* Load font or fontset? */
  /* gf = gdk_font_load_for_display (extname); --jsparkes */
  pango_desc = pango_font_description_new ();
  pango_font_description_set_family (pango_desc, extname);
  char *nm = pango_font_description_to_string (pango_desc);

  gf = gdk_font_load (extname);

  if (!gf)
    {
      maybe_signal_error (Qgui_error, "couldn't load font", f->name,
			  Qfont, errb);
      return 0;
    }

  /* Don't allocate the data until we're sure that we will succeed,
     or the finalize method may get fucked. */
  f->data = xnew (struct gtk_font_instance_data);
  FONT_INSTANCE_GTK_FONT (f) = gf;
  /* Kludge until we support Pango. --jsparkes */
  f->ascent = gf->ascent;
  f->descent = gf->descent;
  f->height = gf->ascent + gf->descent;

  /* Now lets figure out the width of the font.
     We could use a longer string and get the averaage length */
  /* f->width = gdk_text_width (gf, "abcdefghijklmnopqrstuvwxyz", 26)/26; */
  f->width = gdk_text_width (gf, "n", 1); /* em or en? */
  f->proportional_p = (gdk_text_width (gf, "|", 1) !=
                       gdk_text_width (gf, "W", 1));
  return 1;
}

static void
gtk_print_font_instance (struct Lisp_Font_Instance *f,
			 Lisp_Object printcharfun,
			 int UNUSED (escapeflag))
{
  write_fmt_string (printcharfun, " 0x%lx",
		    (unsigned long) gdk_font_id (FONT_INSTANCE_GTK_FONT (f)));
}

static void
gtk_finalize_font_instance (struct Lisp_Font_Instance *f)
{
  if (f->data)
    {
      if (DEVICE_LIVE_P (XDEVICE (f->device)))
	{
	    gdk_font_unref (FONT_INSTANCE_GTK_FONT (f));
	}
      xfree (f->data);
      f->data = 0;
    }
}

/* Forward declarations for X specific functions at the end of the file */
Lisp_Object __get_gtk_font_truename (GdkFont *gdk_font, int expandp);
static Lisp_Object __gtk_font_list_internal (const char *pattern);

static Lisp_Object
gtk_font_instance_truename (struct Lisp_Font_Instance *f,
			    Error_Behavior UNUSED (errb))
{
  if (NILP (FONT_INSTANCE_TRUENAME (f)))
    {
      FONT_INSTANCE_TRUENAME (f) = __get_gtk_font_truename (FONT_INSTANCE_GTK_FONT (f), 1);

      if (NILP (FONT_INSTANCE_TRUENAME (f)))
	{
	  /* Ok, just this once, return the font name as the truename.
	     (This is only used by Fequal() right now.) */
	  return f->name;
	}
    }
  return (FONT_INSTANCE_TRUENAME (f));
}

static Lisp_Object
gtk_font_instance_properties (struct Lisp_Font_Instance *UNUSED (f))
{
  Lisp_Object result = Qnil;

  /* #### BILL!!! */
  /* There seems to be no way to get this information under Gtk */
  return result;
}

static Lisp_Object
gtk_font_list (Lisp_Object pattern, Lisp_Object UNUSED (device),
		Lisp_Object UNUSED (maxnumber))
{
  const char *patternext;

  patternext = LISP_STRING_TO_EXTERNAL (pattern, Qbinary);

  return (__gtk_font_list_internal (patternext));
}

/* Include the charset support, shared, for the moment, with X11.  */
#define THIS_IS_GTK
#include "fontcolor-xlike-inc.c"


/************************************************************************/
/*                            initialization                            */
/************************************************************************/

void
syms_of_fontcolor_gtk (void)
{
}

void
console_type_create_fontcolor_gtk (void)
{
  /* object methods */

  CONSOLE_HAS_METHOD (gtk, initialize_color_instance);
  CONSOLE_HAS_METHOD (gtk, print_color_instance);
  CONSOLE_HAS_METHOD (gtk, finalize_color_instance);
  CONSOLE_HAS_METHOD (gtk, color_instance_equal);
  CONSOLE_HAS_METHOD (gtk, color_instance_hash);
  CONSOLE_HAS_METHOD (gtk, color_instance_rgb_components);
  CONSOLE_HAS_METHOD (gtk, valid_color_name_p);
  CONSOLE_HAS_METHOD (gtk, color_list);

  CONSOLE_HAS_METHOD (gtk, initialize_font_instance);
  CONSOLE_HAS_METHOD (gtk, print_font_instance);
  CONSOLE_HAS_METHOD (gtk, finalize_font_instance);
  CONSOLE_HAS_METHOD (gtk, font_instance_truename);
  CONSOLE_HAS_METHOD (gtk, font_instance_properties);
  CONSOLE_HAS_METHOD (gtk, font_list);
#ifdef MULE
  CONSOLE_HAS_METHOD (gtk, find_charset_font);
  CONSOLE_HAS_METHOD (gtk, font_spec_matches_charset);
#endif
}

void
vars_of_fontcolor_gtk (void)
{
#ifdef DEBUG_XEMACS
  DEFVAR_INT ("debug-x-objects", &debug_x_objects /*
If non-zero, display debug information about X objects
*/ );
  debug_x_objects = 0;
#endif
}

static int
valid_font_name_p (Display *dpy, char *name)
{
  /* Maybe this should be implemented by callign XLoadFont and trapping
     the error.  That would be a lot of work, and wasteful as hell, but
     might be more correct.
   */
  int nnames = 0;
  char **names = 0;
  if (! name)
    return 0;
  names = XListFonts (dpy, name, 1, &nnames);
  if (names)
    XFreeFontNames (names);
  return (nnames != 0);
}

Lisp_Object
__get_gtk_font_truename (GdkFont *gdk_font, int expandp)
{
  //Display *dpy = GDK_FONT_XDISPLAY (gdk_font);
  //GSList *names = ((GdkFontPrivate *) gdk_font)->names;
  Lisp_Object font_name = Qnil;
  char *name = (char *) gdk_x11_font_get_name (gdk_font);

  //name = GDK_FONT_X_FONT (gdk_font);
  if (name != NULL)
    font_name = build_cistring (name);

#if 0

  while (names)
    {
      if (names->data)
	{
	  if (valid_font_name_p (dpy, (char*) names->data))
	    {
	      if (!expandp)
		{
		  /* They want the wildcarded version */
		  font_name = build_cistring ((char*) names->data);
		}
	      else
		{
		  /* Need to expand out */
		  int nnames = 0;
		  char **x_font_names = 0;

		  x_font_names = XListFonts (dpy, (char*) names->data, 1, &nnames);
		  if (x_font_names)
		    {
		      font_name = build_cistring (x_font_names[0]);
		      XFreeFontNames (x_font_names);
		    }
		}
	      break;
	    }
	}
      names = names->next;
    }
#endif
  return (font_name);
}

static Lisp_Object __gtk_font_list_internal (const char *pattern)
{
  char **names;
  int count = 0;
  Lisp_Object result = Qnil;

  names = XListFonts (GDK_DISPLAY (), pattern, MAX_FONT_COUNT, &count);
  while (count--)
    result = Fcons (build_extstring (names [count], Qbinary), result);
  if (names)
    XFreeFontNames (names);

  return result;
}