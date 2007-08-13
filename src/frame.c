/* Generic frame functions.
   Copyright (C) 1989, 1992, 1993, 1994, 1995 Free Software Foundation, Inc.
   Copyright (C) 1995, 1996 Ben Wing.
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

/* Synched up with: FSF 19.30. */

/* This file has been Mule-ized. */

#include <config.h>
#include "lisp.h"

#include "buffer.h"             /* for Vbuffer_alist */
#include "console.h"
#include "events.h"
#include "extents.h"
#include "faces.h"
#include "frame.h"
#include "glyphs.h"
#include "menubar.h"
#ifdef MSDOS
#include "msdos.h"
#endif
#include "redisplay.h"
#include "scrollbar.h"
#include "window.h"

#include <errno.h>
#include "sysdep.h"

Lisp_Object Vselect_frame_hook, Qselect_frame_hook;
Lisp_Object Vdeselect_frame_hook, Qdeselect_frame_hook;
Lisp_Object Vcreate_frame_hook, Qcreate_frame_hook;
Lisp_Object Vdelete_frame_hook, Qdelete_frame_hook;
Lisp_Object Vmouse_enter_frame_hook, Qmouse_enter_frame_hook;
Lisp_Object Vmouse_leave_frame_hook, Qmouse_leave_frame_hook;
Lisp_Object Vmap_frame_hook, Qmap_frame_hook;
Lisp_Object Vunmap_frame_hook, Qunmap_frame_hook;
int  allow_deletion_of_last_visible_frame;
#if defined (HAVE_CDE) || defined (HAVE_OFFIX_DND)
Lisp_Object Vdrag_and_drop_functions, Qdrag_and_drop_functions;
#endif
Lisp_Object Vmouse_motion_handler;
Lisp_Object Vsynchronize_minibuffers;
Lisp_Object Qsynchronize_minibuffers;
Lisp_Object Qbuffer_predicate;
Lisp_Object Qmake_initial_minibuffer_frame;
Lisp_Object Qcustom_initialize_frame;

/* We declare all these frame properties here even though many of them
   are currently only used in frame-x.c, because we should generalize
   them. */

Lisp_Object Qminibuffer;
Lisp_Object Qunsplittable;
Lisp_Object Qinternal_border_width;
Lisp_Object Qtop_toolbar_shadow_color;
Lisp_Object Qbottom_toolbar_shadow_color;
Lisp_Object Qbackground_toolbar_color;
Lisp_Object Qtop_toolbar_shadow_pixmap;
Lisp_Object Qbottom_toolbar_shadow_pixmap;
Lisp_Object Qtoolbar_shadow_thickness;
Lisp_Object Qscrollbar_placement;
Lisp_Object Qinter_line_space;
Lisp_Object Qvisual_bell;
Lisp_Object Qbell_volume;
Lisp_Object Qpointer_background;
Lisp_Object Qpointer_color;
Lisp_Object Qtext_pointer;
Lisp_Object Qspace_pointer;
Lisp_Object Qmodeline_pointer;
Lisp_Object Qgc_pointer;
Lisp_Object Qinitially_unmapped;
Lisp_Object Quse_backing_store;
Lisp_Object Qborder_color;
Lisp_Object Qborder_width;

Lisp_Object Qframep, Qframe_live_p;
Lisp_Object Qframe_x_p, Qframe_tty_p;
Lisp_Object Qdelete_frame;

Lisp_Object Qframe_title_format, Vframe_title_format;
Lisp_Object Qframe_icon_title_format, Vframe_icon_title_format;

Lisp_Object Vdefault_frame_name;
Lisp_Object Vdefault_frame_plist;

Lisp_Object Vframe_icon_glyph;

Lisp_Object Qhidden;

Lisp_Object Qvisible, Qiconic, Qinvisible, Qvisible_iconic, Qinvisible_iconic;
Lisp_Object Qnomini, Qvisible_nomini, Qiconic_nomini, Qinvisible_nomini;
Lisp_Object Qvisible_iconic_nomini, Qinvisible_iconic_nomini;

Lisp_Object Qset_specifier, Qset_glyph_image, Qset_face_property;
Lisp_Object Qface_property_instance;

Lisp_Object Qframe_property_alias;

/* If this is non-nil, it is the frame that make-frame is currently
   creating.  We can't set the current frame to this in case the
   debugger goes off because it would try and display to it.  However,
   there are some places which need to reference it which have no
   other way of getting it if it isn't the selected frame. */
Lisp_Object Vframe_being_created;
Lisp_Object Qframe_being_created;

static void store_minibuf_frame_prop (struct frame *f, Lisp_Object val);

MAC_DEFINE (struct frame *, MTframe_data)


static Lisp_Object mark_frame (Lisp_Object, void (*) (Lisp_Object));
static void print_frame (Lisp_Object, Lisp_Object, int);
DEFINE_LRECORD_IMPLEMENTATION ("frame", frame,
                               mark_frame, print_frame, 0, 0, 0,
			       struct frame);

static Lisp_Object
mark_frame (Lisp_Object obj, void (*markobj) (Lisp_Object))
{
  struct frame *f = XFRAME (obj);

#define MARKED_SLOT(x) ((markobj) (f->x));
#include "frameslots.h"
#undef MARKED_SLOT

#ifdef HAVE_TOOLBARS
  ((markobj) (f->toolbar_data[0]));
  ((markobj) (f->toolbar_data[1]));
  ((markobj) (f->toolbar_data[2]));
  ((markobj) (f->toolbar_data[3]));

  ((markobj) (f->toolbar_size[0]));
  ((markobj) (f->toolbar_size[1]));
  ((markobj) (f->toolbar_size[2]));
  ((markobj) (f->toolbar_size[3]));

  ((markobj) (f->toolbar_visible_p[0]));
  ((markobj) (f->toolbar_visible_p[1]));
  ((markobj) (f->toolbar_visible_p[2]));
  ((markobj) (f->toolbar_visible_p[3]));
#endif /* HAVE_TOOLBARS */

  if (FRAME_LIVE_P (f)) /* device is nil for a dead frame */
    MAYBE_FRAMEMETH (f, mark_frame, (f, markobj));

  return Qnil;
}

static void
print_frame (Lisp_Object obj, Lisp_Object printcharfun, int escapeflag)
{
  struct frame *frm = XFRAME (obj);
  char buf[200];

  if (print_readably)
    error ("printing unreadable object #<frame %s 0x%x>",
           XSTRING_DATA (frm->name), frm->header.uid);

  sprintf (buf, "#<%s-frame ", !FRAME_LIVE_P (frm) ? "dead" :
	   FRAME_TYPE_NAME (frm));
  write_c_string (buf, printcharfun);
  print_internal (frm->name, printcharfun, 1);
  sprintf (buf, " 0x%x>", frm->header.uid);
  write_c_string (buf, printcharfun);
}


static void
nuke_all_frame_slots (struct frame *f)
{
#define MARKED_SLOT(x)	f->x = Qnil;
#include "frameslots.h"
#undef MARKED_SLOT

#ifdef HAVE_TOOLBARS
  f->toolbar_data[0] = Qnil;
  f->toolbar_data[1] = Qnil;
  f->toolbar_data[2] = Qnil;
  f->toolbar_data[3] = Qnil;

  f->toolbar_size[0] = Qnil;
  f->toolbar_size[1] = Qnil;
  f->toolbar_size[2] = Qnil;
  f->toolbar_size[3] = Qnil;

  f->toolbar_visible_p[0] = Qnil;
  f->toolbar_visible_p[1] = Qnil;
  f->toolbar_visible_p[2] = Qnil;
  f->toolbar_visible_p[3] = Qnil;
#endif /* HAVE_TOOLBARS */
}

/* Allocate a new frame object and set all its fields to reasonable
   values.  The root window is created but the minibuffer will be done
   later. */

static struct frame *
allocate_frame_core (Lisp_Object device)
{
  /* This function can GC */
  Lisp_Object frame = Qnil;
  Lisp_Object root_window;
  struct frame *f = alloc_lcrecord_type (struct frame, lrecord_frame);

  zero_lcrecord (f);
  nuke_all_frame_slots (f);
  XSETFRAME (frame, f);

  f->device = device;
  f->framemeths = XDEVICE (device)->devmeths;
  f->buffer_alist = Fcopy_sequence (Vbuffer_alist);

  root_window = allocate_window ();
  XWINDOW (root_window)->frame = frame;

  /* 10 is arbitrary,
     just so that there is "something there."
     Correct size will be set up later with change_frame_size.  */

  f->width = 10;
  f->height = 10;

  XWINDOW (root_window)->pixel_width = 10;
  XWINDOW (root_window)->pixel_height = 9;

  /* The size of the minibuffer window is now set in x_create_frame
     in xfns.c. */

  f->root_window = root_window;
  f->selected_window = root_window;
  f->last_nonminibuf_window = root_window;

  /* Choose a buffer for the frame's root window.  */
  XWINDOW (root_window)->buffer = Qt;
  {
    Lisp_Object buf;

    buf = Fcurrent_buffer ();
    /* If buf is a 'hidden' buffer (i.e. one whose name starts with
       a space), try to find another one.  */
    if (string_char (XSTRING (Fbuffer_name (buf)), 0) == ' ')
      buf = Fother_buffer (buf, Qnil, Qnil);
    Fset_window_buffer (root_window, buf);
  }

  return f;
}

static void
setup_normal_frame (struct frame *f)
{
  Lisp_Object mini_window;
  Lisp_Object frame;

  XSETFRAME (frame, f);

  mini_window = allocate_window ();
  XWINDOW (f->root_window)->next = mini_window;
  XWINDOW (mini_window)->prev = f->root_window;
  XWINDOW (mini_window)->mini_p = Qt;
  XWINDOW (mini_window)->frame = frame;
  f->minibuffer_window = mini_window;
  f->has_minibuffer = 1;

  XWINDOW (mini_window)->buffer = Qt;
  Fset_window_buffer (mini_window, Vminibuffer_zero);
}

/* Make a frame using a separate minibuffer window on another frame.
   MINI_WINDOW is the minibuffer window to use.  nil means use the
   default-minibuffer-frame.  */

static void
setup_frame_without_minibuffer (struct frame *f, Lisp_Object mini_window)
{
  /* This function can GC */
  Lisp_Object device = f->device;

  if (!NILP (mini_window))
    CHECK_LIVE_WINDOW (mini_window);

  if (!NILP (mini_window)
      && !EQ (DEVICE_CONSOLE (XDEVICE (device)),
	      FRAME_CONSOLE (XFRAME (XWINDOW (mini_window)->frame))))
    error ("frame and minibuffer must be on the same console");

  if (NILP (mini_window))
    {
      struct console *con = XCONSOLE (FRAME_CONSOLE (f));
      /* Use default-minibuffer-frame if possible.  */
      if (!FRAMEP (con->default_minibuffer_frame)
	  || ! FRAME_LIVE_P (XFRAME (con->default_minibuffer_frame)))
	{
	  /* If there's no minibuffer frame to use, create one.  */
	  con->default_minibuffer_frame
	    = call1 (Qmake_initial_minibuffer_frame, device);
	}
      mini_window = XFRAME (con->default_minibuffer_frame)->minibuffer_window;
    }

  /* Install the chosen minibuffer window, with proper buffer.  */
  store_minibuf_frame_prop (f, mini_window);
  Fset_window_buffer (mini_window, Vminibuffer_zero);
}

/* Make a frame containing only a minibuffer window.  */

static void
setup_minibuffer_frame (struct frame *f)
{
  /* This function can GC */
  /* First make a frame containing just a root window, no minibuffer.  */
  Lisp_Object mini_window;
  Lisp_Object frame;

  XSETFRAME (frame, f);

  f->no_split = 1;
  f->has_minibuffer = 1;

  /* Now label the root window as also being the minibuffer.
     Avoid infinite looping on the window chain by marking next pointer
     as nil. */

  mini_window = f->minibuffer_window = f->root_window;
  XWINDOW (mini_window)->mini_p = Qt;
  XWINDOW (mini_window)->next   = Qnil;
  XWINDOW (mini_window)->prev   = Qnil;
  XWINDOW (mini_window)->frame  = frame;

  /* Put the proper buffer in that window.  */

  Fset_window_buffer (mini_window, Vminibuffer_zero);
}

static Lisp_Object
make_sure_its_a_fresh_plist (Lisp_Object foolist)
{
  if (CONSP (Fcar (foolist)))
    {
      /* looks like an alist to me. */
      foolist = Fcopy_alist (foolist);
      foolist = Fdestructive_alist_to_plist (foolist);
    }
  else
    foolist = Fcopy_sequence (foolist);

  return foolist;
}

DEFUN ("make-frame", Fmake_frame, 0, 2, "", /*
Create a new frame, displaying the current buffer.
Runs the functions listed in `create-frame-hook' after frame creation.

Optional argument PROPS is a property list (a list of alternating
keyword-value specifications) of properties for the new frame.
\(An alist is accepted for backward compatibility but should not
be passed in.)

See `set-frame-properties', `default-x-frame-plist', and
`default-tty-frame-plist' for the specially-recognized properties.
*/
       (props, device))
{
  struct frame *f;
  struct device *d;
  Lisp_Object frame = Qnil, name = Qnil, minibuf;
  struct gcpro gcpro1, gcpro2, gcpro3;
  int speccount = specpdl_depth ();
  int first_frame_on_device = 0;
  int first_frame_on_console = 0;

  d = decode_device (device);
  XSETDEVICE (device, d);

  /* PROPS and NAME may be freshly-created, so make sure to GCPRO. */
  GCPRO3 (frame, props, name);

  props = make_sure_its_a_fresh_plist (props);
  if (DEVICE_SPECIFIC_FRAME_PROPS (d))
    /* Put the device-specific props before the more general ones so
       that they override them. */
    props = nconc2 (props,
		    make_sure_its_a_fresh_plist
		    (*DEVICE_SPECIFIC_FRAME_PROPS (d)));
  props = nconc2 (props, make_sure_its_a_fresh_plist (Vdefault_frame_plist));
  Fcanonicalize_lax_plist (props, Qnil);

  name = Flax_plist_get (props, Qname, Qnil);
  if (!NILP (name))
    CHECK_STRING (name);
  else if (STRINGP (Vdefault_frame_name))
    name = Vdefault_frame_name;
  else
    name = build_string ("emacs");

  if (!NILP (Fstring_match (make_string ((CONST Bufbyte *) "\\.", 2), name,
			    Qnil, Qnil)))
    signal_simple_error (". not allowed in frame names", name);

  f = allocate_frame_core (device);

  specbind (Qframe_being_created, name);
  f->name = name;

  FRAMEMETH (f, init_frame_1, (f, props));

  minibuf = Flax_plist_get (props, Qminibuffer, Qunbound);
  if (UNBOUNDP (minibuf))
    {
      /* If minibuf is unspecified, then look for a minibuffer X resource. */
      /* #### Not implemented any more.  We need to fix things up so
	 that we search out all X resources and append them to the end of
	 props, above.  This is the only way in general to assure
	 coherent behavior for all frame properties/resources/etc. */
    }
  else
    props = Flax_plist_remprop (props, Qminibuffer);

  if (EQ (minibuf, Qnone) || NILP (minibuf))
    setup_frame_without_minibuffer (f, Qnil);
  else if (EQ (minibuf, Qonly))
    setup_minibuffer_frame (f);
  else if (WINDOWP (minibuf))
    setup_frame_without_minibuffer (f, minibuf);
  else if (EQ (minibuf, Qt) || UNBOUNDP (minibuf))
    setup_normal_frame (f);
  else
    signal_simple_error ("Invalid value for `minibuffer'", minibuf);

  XSETFRAME (frame, f);
  update_frame_window_mirror (f);

  if (initialized)
    {
      if (!NILP (f->minibuffer_window))
        reset_face_cachels (XWINDOW (f->minibuffer_window));
      reset_face_cachels (XWINDOW (f->root_window));
    }

  /* If no frames on this device formerly existed, say this is the
     first frame.  It kind of assumes that frameless devices don't
     exist, but it shouldn't be too harmful.  */
  if (NILP (DEVICE_FRAME_LIST (d)))
    first_frame_on_device = 1;

  /* This *must* go before the init_*() methods.  Those functions
     call Lisp code, and if any of them causes a warning to be displayed
     and the *Warnings* buffer to be created, it won't get added to
     the frame-specific version of the buffer-alist unless the frame
     is accessible from the device. */

#if 0
  DEVICE_FRAME_LIST (d) = nconc2 (DEVICE_FRAME_LIST (d), Fcons (frame, Qnil));
#endif
  DEVICE_FRAME_LIST (d) = Fcons (frame, DEVICE_FRAME_LIST (d));
  RESET_CHANGED_SET_FLAGS;

  /* Now make sure that the initial cached values are set correctly.
     Do this after the init_frame method is called because that may
     do things (e.g. create widgets) that are necessary for the
     specifier value-changed methods to work OK. */
  recompute_all_cached_specifiers_in_frame (f);

  if (!DEVICE_STREAM_P (d))
    {
      init_frame_faces (f);

#ifdef HAVE_SCROLLBARS
      /* Finish up resourcing the scrollbars. */
      init_frame_scrollbars (f);
#endif

#ifdef HAVE_TOOLBARS
      /* Create the initial toolbars.  We have to do this after the frame
	 methods are called because it may potentially call some things itself
	 which depend on the normal frame methods having initialized
	 things. */
      init_frame_toolbars (f);
#endif

      reset_face_cachels (XWINDOW (FRAME_SELECTED_WINDOW (f)));
      reset_glyph_cachels (XWINDOW (FRAME_SELECTED_WINDOW (f)));
      change_frame_size (f, f->height, f->width, 0);
    }

  MAYBE_FRAMEMETH (f, init_frame_2, (f, props));
  Fset_frame_properties (frame, props);
  MAYBE_FRAMEMETH (f, init_frame_3, (f));

  /* Hallelujah, praise the lord. */
  f->init_finished = 1;

  /* If this is the first frame on the device, make it the selected one. */
  if (first_frame_on_device && NILP (DEVICE_SELECTED_FRAME (d)))
    set_device_selected_frame (d, frame);

  /* If at startup or if the current console is a stream console
     (usually also at startup), make this console the selected one
     so that messages show up on it. */
  if (NILP (Fselected_console ()) ||
      CONSOLE_STREAM_P (XCONSOLE (Fselected_console ())))
    Fselect_console (DEVICE_CONSOLE (d));

  first_frame_on_console =
    (first_frame_on_device &&
     XINT (Flength (CONSOLE_DEVICE_LIST (XCONSOLE (DEVICE_CONSOLE (d)))))
     == 1);

  /* #### all this calling of frame methods at various odd times
     is somewhat of a mess.  It's necessary to do it this way due
     to strange console-type-specific things that need to be done. */
  MAYBE_FRAMEMETH (f, after_init_frame, (f, first_frame_on_device,
					 first_frame_on_console));

  if (first_frame_on_device)
    {
      if (first_frame_on_console)
	va_run_hook_with_args (Qcreate_console_hook, 1, DEVICE_CONSOLE (d));
      va_run_hook_with_args (Qcreate_device_hook, 1, device);
    }
  va_run_hook_with_args (Qcreate_frame_hook, 1, frame);

  /* Initialize custom-specific stuff. */
  if (!UNBOUNDP (symbol_function (XSYMBOL (Qcustom_initialize_frame))))
    call1 (Qcustom_initialize_frame, frame);

  unbind_to (speccount, Qnil);

  UNGCPRO;
  return frame;
}


/* this function should be used in most cases when a Lisp function is passed
   a FRAME argument.  Use this unless you don't accept nil == current frame
   (in which case, do a CHECK_LIVE_FRAME() and then an XFRAME()) or you
   allow dead frames.  Note that very few functions should accept dead
   frames.  It could be argued that functions should just do nothing when
   given a dead frame, but the presence of a dead frame usually indicates
   an oversight in the Lisp code that could potentially lead to strange
   results and so it is better to catch the error early.

   If you only accept X frames, use decode_x_frame(), which does what this
   function does but also makes sure the frame is an X frame. */

struct frame *
decode_frame (Lisp_Object frame)
{
  if (NILP (frame))
    return selected_frame ();

  CHECK_LIVE_FRAME (frame);
  return XFRAME (frame);
}

struct frame *
decode_frame_or_selected (Lisp_Object cdf)
{
  if (CONSOLEP (cdf))
    cdf = CONSOLE_SELECTED_DEVICE (decode_console (cdf));
  if (DEVICEP (cdf))
    cdf = DEVICE_SELECTED_FRAME (decode_device (cdf));
  return decode_frame (cdf);
}

Lisp_Object
make_frame (struct frame *f)
{
  Lisp_Object frame = Qnil;
  XSETFRAME (frame, f);
  return frame;
}


/*
 * window size changes are held up during critical regions.  Afterwards,
 * we want to deal with any delayed changes.
 */
void
hold_frame_size_changes (void)
{
  in_display = 1;
}

void
unhold_one_frame_size_changes (struct frame *f)
{
  in_display = 0;

  if (f->size_change_pending)
    change_frame_size (f, f->new_height, f->new_width, 0);
}

void
unhold_frame_size_changes (void)
{
  Lisp_Object frmcons, devcons, concons;

  FRAME_LOOP_NO_BREAK (frmcons, devcons, concons)
    unhold_one_frame_size_changes (XFRAME (XCAR (frmcons)));
}



DEFUN ("framep", Fframep, 1, 1, 0, /*
Return non-nil if OBJECT is a frame.
Also see `frame-live-p'.
Note that FSF Emacs kludgily returns a value indicating what type of
frame this is.  Use the cleaner function `frame-type' for that.
*/
       (object))
{
  return FRAMEP (object) ? Qt : Qnil;
}

DEFUN ("frame-live-p", Fframe_live_p, 1, 1, 0, /*
Return non-nil if OBJECT is a frame which has not been deleted.
*/
       (object))
{
  return FRAMEP (object) && FRAME_LIVE_P (XFRAME (object)) ? Qt : Qnil;
}


/* Called from Fselect_window() */
void
select_frame_1 (Lisp_Object frame)
{
  struct frame *f = XFRAME (frame);
  Lisp_Object old_selected_frame = Fselected_frame (Qnil);

  if (EQ (frame, old_selected_frame))
    return;

  /* now select the frame's device */
  set_device_selected_frame (XDEVICE (FRAME_DEVICE (f)), frame);
  select_device_1 (FRAME_DEVICE (f));

  update_frame_window_mirror (f);
}

DEFUN ("select-frame", Fselect_frame, 1, 1, 0, /*
Select the frame FRAME.
Subsequent editing commands apply to its selected window.
The selection of FRAME lasts until the next time the user does
something to select a different frame, or until the next time this
function is called.

Note that this does not actually cause the window-system focus to
be set to this frame, or the select-frame-hook or deselect-frame-hook
to be run, until the next time that XEmacs is waiting for an event.
*/
       (frame))
{
  CHECK_LIVE_FRAME (frame);

  /* select the frame's selected window.  This will call
     selected_frame_1(). */
  Fselect_window (FRAME_SELECTED_WINDOW (XFRAME (frame)));

  /* Nothing should be depending on the return value of this function.
     But, of course, there is stuff out there which is. */
  return frame;
}

/* use this to retrieve the currently selected frame.  You should use
   this in preference to Fselected_frame (Qnil) unless you are prepared
   to handle the possibility of there being no selected frame (this
   happens at some points during startup). */

struct frame *
selected_frame (void)
{
  Lisp_Object device = Fselected_device (Qnil);
  Lisp_Object frame = DEVICE_SELECTED_FRAME (XDEVICE (device));
  if (NILP (frame))
    signal_simple_error ("No frames exist on device", device);
  return XFRAME (frame);
}

/* use this instead of XFRAME (DEVICE_SELECTED_FRAME (d)) to catch
   the possibility of there being no frames on the device (just created).
   There is no point doing this inside of redisplay because errors
   cause an abort(), indicating a flaw in the logic, and error_check_frame()
   will catch this just as well. */

struct frame *
device_selected_frame (struct device *d)
{
  Lisp_Object frame = DEVICE_SELECTED_FRAME (d);
  if (NILP (frame))
    {
      Lisp_Object device;
      XSETDEVICE (device, d);
      signal_simple_error ("No frames exist on device", device);
    }
  return XFRAME (frame);
}

#if 0 /* FSFmacs */

xxDEFUN ("handle-switch-frame", Fhandle_switch_frame, 1, 2, "e", /*
Handle a switch-frame event EVENT.
Switch-frame events are usually bound to this function.
A switch-frame event tells Emacs that the window manager has requested
that the user's events be directed to the frame mentioned in the event.
This function selects the selected window of the frame of EVENT.

If EVENT is frame object, handle it as if it were a switch-frame event
to that frame.
*/
	 (frame, no_enter))
{
  /* Preserve prefix arg that the command loop just cleared.  */
  XCONSOLE (Vselected_console)->prefix_arg = Vcurrent_prefix_arg;
#if 0 /* unclean! */
  run_hook (Qmouse_leave_buffer_hook);
#endif
  return do_switch_frame (frame, no_enter, 0);
}

/* A load of garbage. */
xxDEFUN ("ignore-event", Fignore_event, 0, 0, "", /*
Do nothing, but preserve any prefix argument already specified.
This is a suitable binding for iconify-frame and make-frame-visible.
*/
	 ())
{
  struct console *c = XCONSOLE (Vselected_console);

  c->prefix_arg = Vcurrent_prefix_arg;
  return Qnil;
}

#endif /* 0 */

DEFUN ("selected-frame", Fselected_frame, 0, 1, 0, /*
Return the frame that is now selected on device DEVICE.
If DEVICE is not specified, the selected device will be used.
If no frames exist on the device, nil is returned.
*/
       (device))
{
  if (NILP (device) && NILP (Fselected_device (Qnil)))
    return Qnil; /* happens early in temacs */
  return DEVICE_SELECTED_FRAME (decode_device (device));
}

Lisp_Object
frame_first_window (struct frame *f)
{
  Lisp_Object w = f->root_window;

  while (1)
    {
      if (! NILP (XWINDOW (w)->hchild))
	w = XWINDOW (w)->hchild;
      else if (! NILP (XWINDOW (w)->vchild))
	w = XWINDOW (w)->vchild;
      else
	break;
    }

  return w;
}

DEFUN ("active-minibuffer-window", Factive_minibuffer_window, 0, 0, 0, /*
Return the currently active minibuffer window, or nil if none.
*/
       ())
{
  return minibuf_level ? minibuf_window : Qnil;
}

DEFUN ("last-nonminibuf-frame", Flast_nonminibuf_frame, 0, 1, 0, /*
Return the most-recently-selected non-minibuffer-only frame on CONSOLE.
This will always be the same as (selected-frame device) unless the
selected frame is a minibuffer-only frame.
CONSOLE defaults to the selected console if omitted.
*/
       (console))
{
  Lisp_Object result;

  XSETCONSOLE (console, decode_console (console));
  /* Just in case the machinations in delete_frame_internal() resulted
     in the last-nonminibuf-frame getting out of sync, make sure and
     return the selected frame if it's acceptable. */
  result = Fselected_frame (CONSOLE_SELECTED_DEVICE (XCONSOLE (console)));
  if (!NILP (result) && !FRAME_MINIBUF_ONLY_P (XFRAME (result)))
    return result;
  return CONSOLE_LAST_NONMINIBUF_FRAME (XCONSOLE (console));
}

DEFUN ("frame-root-window", Fframe_root_window, 0, 1, 0, /*
Return the root-window of FRAME.
If omitted, FRAME defaults to the currently selected frame.
*/
       (frame))
{
  return FRAME_ROOT_WINDOW (decode_frame (frame));
}

DEFUN ("frame-selected-window", Fframe_selected_window, 0, 1, 0, /*
Return the selected window of frame object FRAME.
If omitted, FRAME defaults to the currently selected frame.
*/
       (frame))
{
  return FRAME_SELECTED_WINDOW (decode_frame (frame));
}

void
set_frame_selected_window (struct frame *f, Lisp_Object window)
{
  assert (XFRAME (WINDOW_FRAME (XWINDOW (window))) == f);
  f->selected_window = window;
  if (!MINI_WINDOW_P (XWINDOW (window)) || FRAME_MINIBUF_ONLY_P (f))
    f->last_nonminibuf_window = window;
}

DEFUN ("set-frame-selected-window", Fset_frame_selected_window, 2, 2, 0, /*
Set the selected window of frame object FRAME to WINDOW.
If FRAME is nil, the selected frame is used.
If FRAME is the selected frame, this makes WINDOW the selected window.
*/
       (frame, window))
{
  XSETFRAME (frame, decode_frame (frame));
  CHECK_LIVE_WINDOW (window);

  if (! EQ (frame, WINDOW_FRAME (XWINDOW (window))))
    error ("In `set-frame-selected-window', WINDOW is not on FRAME");

  if (XFRAME (frame) == selected_frame ())
    return Fselect_window (window);

  set_frame_selected_window (XFRAME (frame), window);
  return window;
}


DEFUN ("frame-device", Fframe_device, 0, 1, 0, /*
Return the device that FRAME is on.
If omitted, FRAME defaults to the currently selected frame.
*/
       (frame))
{
  return FRAME_DEVICE (decode_frame (frame));
}

int
is_surrogate_for_selected_frame (struct frame *f)
{
  struct device *d = XDEVICE (f->device);
  struct frame *dsf = device_selected_frame (d);

  /* Can't be a surrogate for ourselves. */
  if (f == dsf)
    return 0;

  if (!FRAME_HAS_MINIBUF_P (dsf) &&
      f == XFRAME (WINDOW_FRAME (XWINDOW (FRAME_MINIBUF_WINDOW (dsf)))))
    return 1;
  else
    return 0;
}

static int
frame_matches_frametype (Lisp_Object frame, Lisp_Object type)
{
  struct frame *f = XFRAME (frame);

  if (WINDOWP (type))
    {
      CHECK_LIVE_WINDOW (type);

      if (EQ (FRAME_MINIBUF_WINDOW (f), type)
	  /* Check that F either is, or has forwarded
	     its focus to, TYPE's frame.  */
	  && (EQ (WINDOW_FRAME (XWINDOW (type)), frame)
	      || EQ (WINDOW_FRAME (XWINDOW (type)),
		     FRAME_FOCUS_FRAME (f))))
	return 1;
      else
	return 0;
    }

#if 0 /* FSFmacs */
  if (EQ (type, Qvisible) || EQ (type, Qiconic) || EQ (type, Qvisible_iconic)
      || EQ (type, Qvisible_nomini) || EQ (type, Qiconic_nomini)
      || EQ (type, Qvisible_iconic_nomini))
    FRAME_SAMPLE_VISIBILITY (f);
#endif

  if (NILP (type))
    type = Qnomini;
  if (ZEROP (type))
    type = Qvisible_iconic;

  if (EQ (type, Qvisible))
    return FRAME_VISIBLE_P (f);
  if (EQ (type, Qiconic))
    return FRAME_ICONIFIED_P (f);
  if (EQ (type, Qinvisible))
    return !FRAME_VISIBLE_P (f) && !FRAME_ICONIFIED_P (f);
  if (EQ (type, Qvisible_iconic))
    return FRAME_VISIBLE_P (f) || FRAME_ICONIFIED_P (f);
  if (EQ (type, Qinvisible_iconic))
    return !FRAME_VISIBLE_P (f);

  if (EQ (type, Qnomini))
    return !FRAME_MINIBUF_ONLY_P (f);
  if (EQ (type, Qvisible_nomini))
    return FRAME_VISIBLE_P (f) && !FRAME_MINIBUF_ONLY_P (f);
  if (EQ (type, Qiconic_nomini))
    return FRAME_ICONIFIED_P (f) && !FRAME_MINIBUF_ONLY_P (f);
  if (EQ (type, Qinvisible_nomini))
    return !FRAME_VISIBLE_P (f) && !FRAME_ICONIFIED_P (f) &&
      !FRAME_MINIBUF_ONLY_P (f);
  if (EQ (type, Qvisible_iconic_nomini))
    return ((FRAME_VISIBLE_P (f) || FRAME_ICONIFIED_P (f))
	    && !FRAME_MINIBUF_ONLY_P (f));
  if (EQ (type, Qinvisible_iconic_nomini))
    return !FRAME_VISIBLE_P (f) && !FRAME_MINIBUF_ONLY_P (f);

  return 1;
}

int
device_matches_console_spec (Lisp_Object frame, Lisp_Object device,
			     Lisp_Object console)
{
  if (EQ (console, Qwindow_system))
    return DEVICE_WIN_P (XDEVICE (device));
  if (NILP (console))
    console = (DEVICE_CONSOLE (XDEVICE (FRAME_DEVICE (XFRAME (frame)))));
  if (DEVICEP (console))
    return EQ (device, console);
  if (CONSOLEP (console))
    return EQ (DEVICE_CONSOLE (XDEVICE (device)), console);
  if (valid_console_type_p (console))
    return EQ (DEVICE_TYPE (XDEVICE (device)), console);
  return 1;
}

/* Return the next frame in the frame list after FRAME.
   FRAMETYPE and CONSOLE control which frames and devices
   are considered; see `next-frame'. */

static Lisp_Object
next_frame_internal (Lisp_Object frame, Lisp_Object frametype,
		     Lisp_Object console, int called_from_delete_device)
{
  int passed = 0;
  int started_over = 0;

  /* If this frame is dead, it won't be in frame_list, and we'll loop
     forever.  Forestall that.  */
  CHECK_LIVE_FRAME (frame);

  while (1)
    {
      Lisp_Object devcons, concons;

      DEVICE_LOOP_NO_BREAK (devcons, concons)
	{
	  Lisp_Object device = XCAR (devcons);
	  Lisp_Object frmcons;

	  if (!device_matches_console_spec (frame, device, console))
	    continue;

	  DEVICE_FRAME_LOOP (frmcons, XDEVICE (device))
	    {
	      Lisp_Object f = XCAR (frmcons);
	      if (passed)
		{
		  /* #### Doing this here is bad and is now
                     unnecessary.  The real bug was that f->iconified
                     was never, ever updated unless a user explicitly
                     called frame-iconified-p.  That has now been
                     fixed.  With this change removed all of the other
                     changes made to support this routine having the
                     called_from_delete_device arg could be removed.
                     But it is too close to release to do that now. */
#if 0
		  /* Make sure the visibility and iconified flags are
                     up-to-date unless we're being deleted. */
		  if (!called_from_delete_device)
		    {
		      Fframe_iconified_p (f);
		      Fframe_visible_p (f);
		    }
#endif

		  /* Decide whether this frame is eligible to be returned.  */

		  /* If we've looped all the way around without finding any
		     eligible frames, return the original frame.  */
		  if (EQ (f, frame))
		    return f;

		  if (frame_matches_frametype (f, frametype))
		    return f;
		}

	      if (EQ (frame, f))
		passed++;
	    }
	}
      /* We hit the end of the list, and need to start over again. */
      if (started_over)
	return Qnil;
      started_over++;
    }
}

Lisp_Object
next_frame (Lisp_Object frame, Lisp_Object frametype, Lisp_Object console)
{
  return next_frame_internal (frame, frametype, console, 0);
}

/* Return the previous frame in the frame list before FRAME.
   FRAMETYPE and CONSOLE control which frames and devices
   are considered; see `next-frame'. */

Lisp_Object
prev_frame (Lisp_Object frame, Lisp_Object frametype, Lisp_Object console)
{
  Lisp_Object devcons, concons;
  Lisp_Object prev;

  /* If this frame is dead, it won't be in frame_list, and we'll loop
     forever.  Forestall that.  */
  CHECK_LIVE_FRAME (frame);

  prev = Qnil;
  DEVICE_LOOP_NO_BREAK (devcons, concons)
    {
      Lisp_Object device = XCAR (devcons);
      Lisp_Object frmcons;

      if (!device_matches_console_spec (frame, device, console))
	continue;

      DEVICE_FRAME_LOOP (frmcons, XDEVICE (device))
	{
	  Lisp_Object f = XCAR (frmcons);

	  if (EQ (frame, f) && !NILP (prev))
	    return prev;

	  /* Decide whether this frame is eligible to be returned,
	     according to frametype.  */

	  if (frame_matches_frametype (f, frametype))
	    prev = f;

	}
    }

  /* We've scanned the entire list.  */
  if (NILP (prev))
    /* We went through the whole frame list without finding a single
       acceptable frame.  Return the original frame.  */
    return frame;
  else
    /* There were no acceptable frames in the list before FRAME; otherwise,
       we would have returned directly from the loop.  Since PREV is the last
       acceptable frame in the list, return it.  */
    return prev;
}

DEFUN ("next-frame", Fnext_frame, 0, 3, 0, /*
Return the next frame of the right type in the frame list after FRAME.
FRAMETYPE controls which frames are eligible to be returned; all
others will be skipped.  Note that if there is only one eligible
frame, then `next-frame' called repeatedly will always return
the same frame, and if there is no eligible frame, then FRAME is
returned.

Possible values for FRAMETYPE are

'visible		 Consider only frames that are visible.
'iconic			 Consider only frames that are iconic.
'invisible		 Consider only frames that are invisible
			 (this is different from iconic).
'visible-iconic		 Consider frames that are visible or iconic.
'invisible-iconic	 Consider frames that are invisible or iconic.
'nomini			 Consider all frames except minibuffer-only ones.
'visible-nomini		 Like `visible' but omits minibuffer-only frames.
'iconic-nomini		 Like `iconic' but omits minibuffer-only frames.
'invisible-nomini	 Like `invisible' but omits minibuffer-only frames.
'visible-iconic-nomini	 Like `visible-iconic' but omits minibuffer-only
			 frames.
'invisible-iconic-nomini Like `invisible-iconic' but omits minibuffer-only
			 frames.
any other value		 Consider all frames.

If FRAMETYPE is omitted, 'nomini is used.  A FRAMETYPE of 0 (a number)
is treated like 'iconic, for backwards compatibility.

If FRAMETYPE is a window, include only its own frame and any frame now
using that window as the minibuffer.

Optional third argument CONSOLE controls which consoles or devices the
returned frame may be on.  If CONSOLE is a console, return frames only
on that console.  If CONSOLE is a device, return frames only on that
device.  If CONSOLE is a console type, return frames only on consoles
of that type.  If CONSOLE is 'window-system, return any frames on any
window-system consoles.  If CONSOLE is nil or omitted, return frames only
on the FRAME's console.  Otherwise, all frames are considered.
*/
       (frame, frametype, console))
{
  XSETFRAME (frame, decode_frame (frame));

  return next_frame (frame, frametype, console);
}

DEFUN ("previous-frame", Fprevious_frame, 0, 3, 0, /*
Return the next frame of the right type in the frame list after FRAME.
FRAMETYPE controls which frames are eligible to be returned; all
others will be skipped.  Note that if there is only one eligible
frame, then `previous-frame' called repeatedly will always return
the same frame, and if there is no eligible frame, then FRAME is
returned.

See `next-frame' for an explanation of the FRAMETYPE and CONSOLE
arguments.
*/
       (frame, frametype, console))
{
  XSETFRAME (frame, decode_frame (frame));

  return prev_frame (frame, frametype, console);
}

/* Return any frame for which PREDICATE is non-zero, or return Qnil
   if there aren't any. */

Lisp_Object
find_some_frame (int (*predicate) (Lisp_Object, void *),
		 void *closure)
{
  Lisp_Object framecons, devcons, concons;

  FRAME_LOOP_NO_BREAK (framecons, devcons, concons)
    {
      Lisp_Object frame = XCAR (framecons);

      if ((predicate) (frame, closure))
	return frame;
    }

  return Qnil;
}



extern void free_window_mirror (struct window_mirror *mir);
extern void free_line_insertion_deletion_costs (struct frame *f);

/* Return 1 if it is ok to delete frame F;
   0 if all frames aside from F are invisible.
   (Exception: if F is a stream frame, it's OK to delete if
   any other frames exist.) */

static int
other_visible_frames_internal (struct frame *f, int called_from_delete_device)
{
  Lisp_Object frame;

  XSETFRAME (frame, f);
  if (FRAME_STREAM_P (f))
    return !EQ (frame, next_frame_internal (frame, Qt, Qt,
					    called_from_delete_device));
  return !EQ (frame, next_frame_internal (frame, Qvisible_iconic_nomini, Qt,
					  called_from_delete_device));
}

int
other_visible_frames (struct frame *f)
{
  return other_visible_frames_internal (f, 0);
}

/* Delete frame F.

   If FORCE is non-zero, allow deletion of the only frame.

   If CALLED_FROM_DELETE_DEVICE is non-zero, then, if
   deleting the last frame on a device, just delete it,
   instead of calling `delete-device'.

   If FROM_IO_ERROR is non-zero, then the frame is gone due
   to an I/O error.  This affects what happens if we exit
   (we do an emergency exit instead of `save-buffers-kill-emacs'.)
*/

void
delete_frame_internal (struct frame *f, int force,
		       int called_from_delete_device,
		       int from_io_error)
{
  /* This function can GC */
  int minibuffer_selected;
  struct device *d;
  struct console *con;
  Lisp_Object frame;
  Lisp_Object device;
  Lisp_Object console;
  struct gcpro gcpro1;

  /* OK to delete an already deleted frame. */
  if (! FRAME_LIVE_P (f))
    return;

  XSETFRAME (frame, f);
  GCPRO1 (frame);

  device = FRAME_DEVICE (f);
  d = XDEVICE (device);
  console = DEVICE_CONSOLE (d);
  con = XCONSOLE (console);

  if (!called_from_delete_device)
    {
      /* If we're deleting the only non-minibuffer frame on the
	 device, delete the device. */
      if (EQ (frame, next_frame (frame, Qnomini, FRAME_DEVICE (f))))
	{
	  delete_device_internal (d, force, 0, from_io_error);
	  UNGCPRO;
	  return;
	}
    }

  /* In FSF, delete-frame will not normally allow you to delete the
     last visible frame.  This was too annoying, so we changed it to the
     only frame.  However, this would let people shoot themselves by
     deleting all frames which were either visible or iconified and thus
     losing any way of communicating with the still running XEmacs process.
     So we put it back.  */
  if (!force && !allow_deletion_of_last_visible_frame &&
      !other_visible_frames_internal (f, called_from_delete_device))
    error ("Attempt to delete the sole visible or iconified frame");

  /* Does this frame have a minibuffer, and is it the surrogate
     minibuffer for any other frame?  */
  if (FRAME_HAS_MINIBUF_P (f))
    {
      Lisp_Object frmcons, devcons, concons;

      FRAME_LOOP_NO_BREAK (frmcons, devcons, concons)
	{
	  Lisp_Object this = XCAR (frmcons);

	  if (! EQ (this, frame)
	      && EQ (frame, (WINDOW_FRAME
			     (XWINDOW
			      (FRAME_MINIBUF_WINDOW (XFRAME (this)))))))
	    {
	      /* We've found another frame whose minibuffer is on
		 this frame. */
	      signal_simple_error
		("Attempt to delete a surrogate minibuffer frame", frame);
	    }
	}
    }

  /* Test for popup frames hanging around. */
  /* Deletion of a parent frame with popups is deadly. */
  {
    Lisp_Object frmcons, devcons, concons;

    FRAME_LOOP_NO_BREAK (frmcons, devcons, concons)
      {
	Lisp_Object this = XCAR (frmcons);


	if (! EQ (this, frame)
	    && EQ (frame, DEVMETH_OR_GIVEN(XDEVICE(XCAR(devcons)),
					   get_frame_parent,
					   (XFRAME(this)),
					   Qnil)))
	  {
	    /* We've found a popup frame whose parent is this frame. */
	    signal_simple_error
	      ("Attempt to delete a frame with live popups", frame);
	  }
      }
  }

  /* Before here, we haven't made any dangerous changes (just checked for
     error conditions).  Now run the delete-frame-hook.  Remember that
     user code there could do any number of dangerous things, including
     signalling an error. */

  va_run_hook_with_args (Qdelete_frame_hook, 1, frame);

  if (!FRAME_LIVE_P (f)) /* Make sure the delete-frame-hook didn't */
    {		         /* go ahead and delete anything. */
      UNGCPRO;
      return;
    }

  /* Call the delete-device-hook and delete-console-hook now if
     appropriate, before we do any dangerous things -- they too could
     signal an error. */
  if (XINT (Flength (DEVICE_FRAME_LIST (d))) == 1)
    {
      va_run_hook_with_args (Qdelete_device_hook, 1, device);
      if (!FRAME_LIVE_P (f)) /* Make sure the delete-device-hook didn't */
	{		     /*	go ahead and delete anything. */
	  UNGCPRO;
	  return;
	}

      if (XINT (Flength (CONSOLE_DEVICE_LIST (con))) == 1)
	{
	  va_run_hook_with_args (Qdelete_console_hook, 1, console);
	  if (!FRAME_LIVE_P (f)) /* Make sure the delete-console-hook didn't */
	    {			 /* go ahead and delete anything. */
	      UNGCPRO;
	      return;
	    }
	}
    }

  minibuffer_selected = EQ (minibuf_window, Fselected_window (Qnil));

  /* If we were focused on this frame, then we're not any more.
     Assume that we lost the focus; that way, the call to
     Fselect_frame() below won't end up making us explicitly
     focus on another frame, which is generally undesirable in
     a point-to-type world.  If our mouse ends up sitting over
     another frame, we will receive a FocusIn event and end up
     making that frame the selected frame.

     #### This may not be an ideal solution in a click-to-type
     world (in that case, we might want to explicitly choose
     another frame to have the focus, rather than relying on
     the WM, which might focus on a frame in a different app
     or focus on nothing at all).  But there's no easy way
     to detect which focus model we're running on, and the
     alternative is more heinous. */

  if (EQ (frame, DEVICE_FRAME_WITH_FOCUS_REAL (d)))
    DEVICE_FRAME_WITH_FOCUS_REAL (d) = Qnil;
  if (EQ (frame, DEVICE_FRAME_WITH_FOCUS_FOR_HOOKS (d)))
    DEVICE_FRAME_WITH_FOCUS_FOR_HOOKS (d) = Qnil;
  if (EQ (frame, DEVICE_FRAME_THAT_OUGHT_TO_HAVE_FOCUS (d)))
    DEVICE_FRAME_THAT_OUGHT_TO_HAVE_FOCUS (d) = Qnil;

  /* Don't allow the deleted frame to remain selected.
     Note that in the former scheme of things, this would
     have caused us to regain the focus.  This no longer
     applies (see above); I think the new behavior is more
     logical.  If someone disagrees, it can always be
     changed (or a new user variable can be introduced, ugh.) */
  if (EQ (frame, DEVICE_SELECTED_FRAME (d)))
    {
      Lisp_Object next;

      /* If this is a popup frame, select its parent if possible.
	 Otherwise, find another visible frame; if none, just take any frame.
         First try the same device, then the same console. */

      next = DEVMETH_OR_GIVEN (d, get_frame_parent, (f), Qnil);
      if (NILP (next) || EQ (next, frame) || ! FRAME_LIVE_P (XFRAME (next)))
	next = next_frame_internal (frame, Qvisible, device,
				    called_from_delete_device);
      if (NILP (next) || EQ (next, frame))
	next = next_frame_internal (frame, Qvisible, console,
				    called_from_delete_device);
      if (NILP (next) || EQ (next, frame))
	next = next_frame_internal (frame, Qvisible, Qt,
				    called_from_delete_device);
      if (NILP (next) || EQ (next, frame))
	next = next_frame_internal (frame, Qt, device,
				    called_from_delete_device);
      if (NILP (next) || EQ (next, frame))
	next = next_frame_internal (frame, Qt, console,
				    called_from_delete_device);
      if (NILP (next) || EQ (next, frame))
	next = next_frame_internal (frame, Qt, Qt, called_from_delete_device);

      /* if we haven't found another frame at this point
	 then there aren't any. */
      if (NILP (next) || EQ (next, frame))
	;
      else
	{
	  int did_select = 0;
	  /* if this is the global selected frame, select another one. */
	  if (EQ (frame, Fselected_frame (Qnil)))
	    {
		Fselect_frame (next);
		did_select = 1;
	    }
	  /*
	   * If the new frame we just selected is on a different
	   * device then we still need to change DEVICE_SELECTED_FRAME(d)
	   * to a live frame, if there are any left on this device.
	   */
	  if (!EQ (device, FRAME_DEVICE(XFRAME(next))))
	    {
		Lisp_Object next_f =
		    next_frame_internal (frame, Qt, device,
					 called_from_delete_device);
		if (NILP (next_f) || EQ (next_f, frame))
		  ;
		else
		  set_device_selected_frame (d, next_f);
	    }
	  else if (! did_select)
	    set_device_selected_frame (d, next);

	}
    }

  /* Don't allow minibuf_window to remain on a deleted frame.  */
  if (EQ (f->minibuffer_window, minibuf_window))
    {
      struct frame *sel_frame = selected_frame ();
      Fset_window_buffer (sel_frame->minibuffer_window,
			  XWINDOW (minibuf_window)->buffer);
      minibuf_window = sel_frame->minibuffer_window;

      /* If the dying minibuffer window was selected,
	 select the new one.  */
      if (minibuffer_selected)
	Fselect_window (minibuf_window);
    }

  /* After this point, no errors must be allowed to occur. */

#ifdef HAVE_MENUBARS
  free_frame_menubars (f);
#endif
#ifdef HAVE_SCROLLBARS
  free_frame_scrollbars (f);
#endif
#ifdef HAVE_TOOLBARS
  free_frame_toolbars (f);
#endif

  /* This must be done before the window and window_mirror structures
     are freed.  The scrollbar information is attached to them. */
  MAYBE_FRAMEMETH (f, delete_frame, (f));

  /* Mark all the windows that used to be on FRAME as deleted, and then
     remove the reference to them.  */
  delete_all_subwindows (XWINDOW (f->root_window));
  f->root_window = Qnil;

  /* Remove the frame now from the list.  This way, any events generated
     on this frame by the maneuvers below will disperse themselves. */

  /* This used to be Fdelq(), but that will cause a seg fault if the
     QUIT checker happens to get invoked, because the frame list is in
     an inconsistent state. */
  d->frame_list = delq_no_quit (frame, d->frame_list);
  RESET_CHANGED_SET_FLAGS;

  f->dead = 1;
  f->visible = 0;

  free_window_mirror (f->root_mirror);
/*  free_line_insertion_deletion_costs (f); */

  /* If we've deleted the last non-minibuf frame, then try to find
     another one.  */
  if (EQ (frame, CONSOLE_LAST_NONMINIBUF_FRAME (con)))
    {
      Lisp_Object frmcons, devcons;

      set_console_last_nonminibuf_frame (con, Qnil);

      CONSOLE_FRAME_LOOP_NO_BREAK (frmcons, devcons, con)
	{
	  Lisp_Object ecran = XCAR (frmcons);
	  if (!FRAME_MINIBUF_ONLY_P (XFRAME (ecran)))
	    {
	      set_console_last_nonminibuf_frame (con, ecran);
	      goto double_break_1;
	    }
	}
    }
 double_break_1:

  if (called_from_delete_device < 0)
    /* then we're being called from delete-console, and we shouldn't
       try to find another default-minibuffer frame for the console.
       */
    con->default_minibuffer_frame = Qnil;

  /* If we've deleted this console's default_minibuffer_frame, try to
     find another one.  Prefer minibuffer-only frames, but also notice
     frames with other windows.  */
  if (EQ (frame, con->default_minibuffer_frame))
    {
      Lisp_Object frmcons, devcons;
      /* The last frame we saw with a minibuffer, minibuffer-only or not.  */
      Lisp_Object frame_with_minibuf;
      /* Some frame we found on the same console, or nil if there are none. */
      Lisp_Object frame_on_same_console;

      frame_on_same_console = Qnil;
      frame_with_minibuf = Qnil;

      set_console_last_nonminibuf_frame (con, Qnil);

      CONSOLE_FRAME_LOOP_NO_BREAK (frmcons, devcons, con)
	{
	  Lisp_Object this;
	  struct frame *f1;

	  this = XCAR (frmcons);
	  f1 = XFRAME (this);

	  /* Consider only frames on the same console
	     and only those with minibuffers.  */
	  if (FRAME_HAS_MINIBUF_P (f1))
	    {
	      frame_with_minibuf = this;
	      if (FRAME_MINIBUF_ONLY_P (f1))
		goto double_break_2;
	    }

	  frame_on_same_console = this;
	}
    double_break_2:

      if (!NILP (frame_on_same_console))
	{
	  /* We know that there must be some frame with a minibuffer out
	     there.  If this were not true, all of the frames present
	     would have to be minibuffer-less, which implies that at some
	     point their minibuffer frames must have been deleted, but
	     that is prohibited at the top; you can't delete surrogate
	     minibuffer frames.  */
	  if (NILP (frame_with_minibuf))
	    abort ();

	  con->default_minibuffer_frame = frame_with_minibuf;
	}
      else
	/* No frames left on this console--say no minibuffer either.  */
	con->default_minibuffer_frame = Qnil;
    }

  nuke_all_frame_slots (f); /* nobody should be accessing the device
			       or anything else any more, and making
			       them Qnil allows for better GC'ing
			       in case a pointer to the dead frame
			       continues to hang around. */
  f->framemeths = dead_console_methods;
  UNGCPRO;
}

void
io_error_delete_frame (Lisp_Object frame)
{
  delete_frame_internal (XFRAME (frame), 1, 0, 1);
}

DEFUN ("delete-frame", Fdelete_frame, 0, 2, "", /*
Delete FRAME, permanently eliminating it from use.
If omitted, FRAME defaults to the selected frame.
A frame may not be deleted if its minibuffer is used by other frames.
Normally, you cannot delete the last non-minibuffer-only frame (you must
use `save-buffers-kill-emacs' or `kill-emacs').  However, if optional
second argument FORCE is non-nil, you can delete the last frame. (This
will automatically call `save-buffers-kill-emacs'.)
*/
       (frame, force))
{
  /* This function can GC */
  struct frame *f;

  if (NILP (frame))
    {
      f = selected_frame ();
      XSETFRAME (frame, f);
    }
  else
    {
      CHECK_FRAME (frame);
      f = XFRAME (frame);
    }

  delete_frame_internal (f, !NILP (force), 0, 0);
  return Qnil;
}


/* Return mouse position in character cell units.  */

DEFUN ("mouse-position", Fmouse_position, 0, 1, 0, /*
Return a list (WINDOW X . Y) giving the current mouse window and position.
The position is given in character cells, where (0, 0) is the
upper-left corner of the window.

DEVICE specifies the device on which to read the mouse position, and
defaults to the selected device.  If the device is a mouseless terminal
or Emacs hasn't been programmed to read its mouse position, it returns
the device's selected window for WINDOW and nil for X and Y.
*/
       (device))
{
  Lisp_Object val = Fmouse_pixel_position (device);
  int x, y, obj_x, obj_y;
  struct window *w;
  struct frame *f;
  Bufpos bufpos, closest;
  Charcount modeline_closest;
  Lisp_Object obj1, obj2;

  if (NILP (XCAR (val)) || NILP (XCAR (XCDR (val))))
    return val;
  w = XWINDOW (XCAR (val));
  x = XINT (XCAR (XCDR (val)));
  y = XINT (XCDR (XCDR (val)));
  f = XFRAME (w->frame);

  if (x >= 0 && y >= 0)
    {
      if (pixel_to_glyph_translation (f, x, y, &x, &y, &obj_x, &obj_y, &w,
				      &bufpos, &closest, &modeline_closest,
				      &obj1, &obj2)
	  != OVER_NOTHING)
	{
	  XCAR (XCDR (val)) = make_int (x);
	  XCDR (XCDR (val)) = make_int (y);
	}
    }
  else
    {
      XCAR (XCDR (val)) = Qnil;
      XCDR (XCDR (val)) = Qnil;
    }

  return val;
}

static int
mouse_pixel_position_1 (struct device *d, Lisp_Object *frame,
			int *x, int *y)
{
  switch (DEVMETH_OR_GIVEN (d, get_mouse_position, (d, frame, x, y), -1))
    {
    case 1:
      return 1;

    case 0:
      *frame = Qnil;
      break;

    case -1:
      *frame = DEVICE_SELECTED_FRAME (d);
      break;

    default:
      abort (); /* method is incorrectly written */
    }

  return 0;
}

DEFUN ("mouse-pixel-position", Fmouse_pixel_position, 0, 1, 0, /*
Return a list (WINDOW X . Y) giving the current mouse window and position.
The position is given in pixel units, where (0, 0) is the
upper-left corner.

DEVICE specifies the device on which to read the mouse position, and
defaults to the selected device.  If the device is a mouseless terminal
or Emacs hasn't been programmed to read its mouse position, it returns
the device's selected window for WINDOW and nil for X and Y.
*/
       (device))
{
  struct device *d = decode_device (device);
  Lisp_Object frame;
  Lisp_Object window;
  Lisp_Object x, y;
  int intx, inty;

  x = y = Qnil;

  if (mouse_pixel_position_1 (d, &frame, &intx, &inty))
    {
      struct window *w =
	find_window_by_pixel_pos (intx, inty, XFRAME (frame)->root_window);
      if (!w)
	window = Qnil;
      else
	{
	  XSETWINDOW (window, w);

	  /* Adjust the position to be relative to the window. */
	  intx -= w->pixel_left;
	  inty -= w->pixel_top;
	  XSETINT (x, intx);
	  XSETINT (y, inty);
	}
    }
  else
    {
      if (FRAMEP (frame))
	window = FRAME_SELECTED_WINDOW (XFRAME (frame));
      else
	window = Qnil;
    }

  return Fcons (window, Fcons (x, y));
}

DEFUN ("mouse-position-as-motion-event", Fmouse_position_as_motion_event, 0, 1, 0, /*
Return the current mouse position as a motion event.
This allows you to call the standard event functions such as
`event-over-toolbar-p' to determine where the mouse is.

DEVICE specifies the device on which to read the mouse position, and
defaults to the selected device.  If the mouse position can't be determined
(e.g. DEVICE is a TTY device), nil is returned instead of an event.
*/
       (device))
{
  struct device *d = decode_device (device);
  Lisp_Object frame;
  int intx, inty;

  if (mouse_pixel_position_1 (d, &frame, &intx, &inty))
    {
      Lisp_Object event = Fmake_event (Qnil, Qnil);
      XEVENT (event)->event_type = pointer_motion_event;
      XEVENT (event)->channel = frame;
      XEVENT (event)->event.motion.x = intx;
      XEVENT (event)->event.motion.y = inty;
      return event;
    }
  else
    return Qnil;
}

DEFUN ("set-mouse-position", Fset_mouse_position, 3, 3, 0, /*
Move the mouse pointer to the center of character cell (X,Y) in WINDOW.
Note, this is a no-op for an X frame that is not visible.
If you have just created a frame, you must wait for it to become visible
before calling this function on it, like this.
  (while (not (frame-visible-p frame)) (sleep-for .5))
Note also: Warping the mouse is contrary to the ICCCM, so be very sure
 that the behavior won't end up being obnoxious!
*/
       (window, x, y))
{
  struct window *w;
  int pix_x, pix_y;

  CHECK_WINDOW (window);
  CHECK_INT (x);
  CHECK_INT (y);

  /* Warping the mouse will cause EnterNotify and Focus events under X. */
  w = XWINDOW (window);
  glyph_to_pixel_translation (w, XINT (x), XINT (y), &pix_x, &pix_y);

  MAYBE_FRAMEMETH (XFRAME (w->frame), set_mouse_position, (w, pix_x, pix_y));

  return Qnil;
}

DEFUN ("set-mouse-pixel-position", Fset_mouse_pixel_position, 3, 3, 0, /*
Move the mouse pointer to pixel position (X,Y) in WINDOW.
Note, this is a no-op for an X frame that is not visible.
If you have just created a frame, you must wait for it to become visible
before calling this function on it, like this.
  (while (not (frame-visible-p frame)) (sleep-for .5))
*/
       (window, x, y))
{
  struct window *w;

  CHECK_WINDOW (window);
  CHECK_INT (x);
  CHECK_INT (y);

  /* Warping the mouse will cause EnterNotify and Focus events under X. */
  w = XWINDOW (window);
  FRAMEMETH (XFRAME (w->frame), set_mouse_position, (w, XINT (x), XINT (y)));

  return Qnil;
}

DEFUN ("make-frame-visible", Fmake_frame_visible, 0, 1, 0, /*
Make the frame FRAME visible (assuming it is an X-window).
If omitted, FRAME defaults to the currently selected frame.
Also raises the frame so that nothing obscures it.
*/
       (frame))
{
  struct frame *f = decode_frame (frame);

  MAYBE_FRAMEMETH (f, make_frame_visible, (f));
  return frame;
}

DEFUN ("make-frame-invisible", Fmake_frame_invisible, 0, 2, 0, /*
Unconditionally removes frame from the display (assuming it is an X-window).
If omitted, FRAME defaults to the currently selected frame.
If what you want to do is iconify the frame (if the window manager uses
icons) then you should call `iconify-frame' instead.
Normally you may not make FRAME invisible if all other frames are invisible
and uniconified, but if the second optional argument FORCE is non-nil,
you may do so.
*/
       (frame, force))
{
  struct frame *f, *sel_frame;
  struct device *d;

  f = decode_frame (frame);
  d = XDEVICE (FRAME_DEVICE (f));
  sel_frame = XFRAME (DEVICE_SELECTED_FRAME (d));

  if (NILP (force) && !other_visible_frames (f))
    error ("Attempt to make invisible the sole visible or iconified frame");

  /* Don't allow minibuf_window to remain on a deleted frame.  */
  if (EQ (f->minibuffer_window, minibuf_window))
    {
      Fset_window_buffer (sel_frame->minibuffer_window,
			  XWINDOW (minibuf_window)->buffer);
      minibuf_window = sel_frame->minibuffer_window;
    }

  MAYBE_FRAMEMETH (f, make_frame_invisible, (f));

  return Qnil;
}

DEFUN ("iconify-frame", Ficonify_frame, 0, 1, "", /*
Make the frame FRAME into an icon, if the window manager supports icons.
If omitted, FRAME defaults to the currently selected frame.
*/
       (frame))
{
  struct frame *f, *sel_frame;
  struct device *d;

  f = decode_frame (frame);
  d = XDEVICE (FRAME_DEVICE (f));
  sel_frame = XFRAME (DEVICE_SELECTED_FRAME (d));

  /* Don't allow minibuf_window to remain on a deleted frame.  */
  if (EQ (f->minibuffer_window, minibuf_window))
    {
      Fset_window_buffer (sel_frame->minibuffer_window,
			  XWINDOW (minibuf_window)->buffer);
      minibuf_window = sel_frame->minibuffer_window;
    }

  MAYBE_FRAMEMETH (f, iconify_frame, (f));

  return Qnil;
}

DEFUN ("deiconify-frame", Fdeiconify_frame, 0, 1, 0, /*
Open (de-iconify) the iconified frame FRAME.
Under X, this is currently the same as `make-frame-visible'.
If omitted, FRAME defaults to the currently selected frame.
Also raises the frame so that nothing obscures it.
*/
       (frame))
{
  return Fmake_frame_visible (frame);
}

/* FSF returns 'icon for iconized frames.  What a crock! */

DEFUN ("frame-visible-p", Fframe_visible_p, 0, 1, 0, /*
Return non NIL if FRAME is now "visible" (actually in use for display).
A frame that is not visible is not updated, and, if it works through a
window system, may not show at all.
N.B. Under X "visible" means Mapped. It the window is mapped but not
actually visible on screen then frame_visible returns 'hidden.
*/
       (frame))
{
  struct frame *f = decode_frame (frame);
  int visible = FRAMEMETH_OR_GIVEN (f, frame_visible_p, (f), f->visible);
  return visible ? ( visible > 0 ? Qt : Qhidden ) : Qnil;
}

DEFUN ("frame-totally-visible-p", Fframe_totally_visible_p, 0, 1, 0, /*
Return T if frame is not obscured by any other X windows, NIL otherwise.
Always returns t for tty frames.
*/
       (frame))
{
  struct frame *f = decode_frame (frame);
  return (FRAMEMETH_OR_GIVEN (f, frame_totally_visible_p, (f), f->visible)
	  ? Qt : Qnil);
}

DEFUN ("frame-iconified-p", Fframe_iconified_p, 0, 1, 0, /*
Return t if FRAME is iconified.
Not all window managers use icons; some merely unmap the window, so this
function is not the inverse of `frame-visible-p'.  It is possible for a
frame to not be visible and not be iconified either.  However, if the
frame is iconified, it will not be visible.
*/
       (frame))
{
  struct frame *f = decode_frame (frame);
  if (f->visible)
    return Qnil;
  f->iconified = FRAMEMETH_OR_GIVEN (f, frame_iconified_p, (f), 0);
  return f->iconified ? Qt : Qnil;
}

DEFUN ("visible-frame-list", Fvisible_frame_list, 0, 1, 0, /*
Return a list of all frames now "visible" (being updated).
If DEVICE is specified only frames on that device will be returned.
Note that under virtual window managers not all these frame are necessarily
really updated.
*/
       (device))
{
  Lisp_Object devcons, concons;
  struct frame *f;
  Lisp_Object value;

  value = Qnil;

  DEVICE_LOOP_NO_BREAK (devcons, concons)
    {
      assert (DEVICEP (XCAR (devcons)));

      if (NILP (device) || EQ (device, XCAR (devcons)))
	{
	  Lisp_Object frmcons;

	  DEVICE_FRAME_LOOP (frmcons, XDEVICE (XCAR (devcons)))
	    {
	      Lisp_Object frame = XCAR (frmcons);
	      f = XFRAME (frame);
	      if (FRAME_VISIBLE_P(f))
		value = Fcons (frame, value);
	    }
	}
    }

  return value;
}


DEFUN ("raise-frame", Fraise_frame, 0, 1, "", /*
Bring FRAME to the front, so it occludes any frames it overlaps.
If omitted, FRAME defaults to the currently selected frame.
If FRAME is invisible, make it visible.
If Emacs is displaying on an ordinary terminal or some other device which
doesn't support multiple overlapping frames, this function does nothing.
*/
       (frame))
{
  struct frame *f = decode_frame (frame);

  /* Do like the documentation says. */
  Fmake_frame_visible (frame);
  MAYBE_FRAMEMETH (f, raise_frame, (f));
  return Qnil;
}

DEFUN ("lower-frame", Flower_frame, 0, 1, "", /*
Send FRAME to the back, so it is occluded by any frames that overlap it.
If omitted, FRAME defaults to the currently selected frame.
If Emacs is displaying on an ordinary terminal or some other device which
doesn't support multiple overlapping frames, this function does nothing.
*/
       (frame))
{
  struct frame *f = decode_frame (frame);

  MAYBE_FRAMEMETH (f, lower_frame, (f));
  return Qnil;
}

/* Ben thinks there is no need for `redirect-frame-focus' or `frame-focus',
   crockish FSFmacs functions.  See summary on focus in event-stream.c. */


/***************************************************************************/
/*                           frame properties                              */
/***************************************************************************/

static void internal_set_frame_size (struct frame *f, int cols, int rows,
				     int pretend);

static void
store_minibuf_frame_prop (struct frame *f, Lisp_Object val)
{
  Lisp_Object frame;
  XSETFRAME (frame, f);

  if (WINDOWP (val))
    {
      if (! MINI_WINDOW_P (XWINDOW (val)))
	signal_simple_error
	  ("Surrogate minibuffer windows must be minibuffer windows",
	   val);

      if (FRAME_HAS_MINIBUF_P (f) || FRAME_MINIBUF_ONLY_P (f))
	signal_simple_error
	  ("Can't change the surrogate minibuffer of a frame with its own minibuffer", frame);

      /* Install the chosen minibuffer window, with proper buffer.  */
      f->minibuffer_window = val;
    }
  else if (EQ (val, Qt))
    {
      if (FRAME_HAS_MINIBUF_P (f) || FRAME_MINIBUF_ONLY_P (f))
	signal_simple_error
	  ("Frame already has its own minibuffer", frame);
      else
	{
	  setup_normal_frame (f);
	  f->mirror_dirty = 1;

	  update_frame_window_mirror (f);
	  internal_set_frame_size (f, f->width, f->height, 1);
	}
    }
}

#if 0

/* possible code if you want to have symbols such as `default-background'
   map to setting the background of `default', etc. */

static int
dissect_as_face_setting (Lisp_Object sym, Lisp_Object *face_out,
			 Lisp_Object *face_prop_out)
{
  Lisp_Object list = Vbuilt_in_face_specifiers;
  struct Lisp_String *s;

  if (!SYMBOLP (sym))
    return 0;

  s = symbol_name (XSYMBOL (sym));

  while (!NILP (list))
    {
      Lisp_Object prop = Fcar (list);
      struct Lisp_String *prop_name;

      if (!SYMBOLP (prop))
	continue;
      prop_name = symbol_name (XSYMBOL (prop));
      if (string_length (s) > string_length (prop_name) + 1
	  && !memcmp (string_data (prop_name),
		      string_data (s) + string_length (s)
		      - string_length (prop_name),
		      string_length (prop_name))
	  && string_data (s)[string_length (s) - string_length (prop_name)
			     - 1] == '-')
	{
	  Lisp_Object face =
	    Ffind_face (make_string (string_data (s),
				     string_length (s)
				     - string_length (prop_name)
				     - 1));
	  if (!NILP (face))
	    {
	      *face_out = face;
	      *face_prop_out = prop;
	      return 1;
	    }
	}

      list = Fcdr (list);
    }

  return 0;
}

#endif /* 0 */

static Lisp_Object
get_property_alias (Lisp_Object prop)
{
  while (1)
    {
      Lisp_Object alias = Qnil;

      if (SYMBOLP (prop))
	alias = Fget (prop, Qframe_property_alias, Qnil);
      if (NILP (alias))
	break;
      prop = alias;
      QUIT;
    }

  return prop;
}

/* #### Using this to modify the internal border width has no effect
   because the change isn't propagated to the windows.  Are there
   other properties which this claims to handle, but doesn't?

   But of course.  This stuff needs more work, but it's a lot closer
   to sanity now than before with the horrible frame-params stuff. */

DEFUN ("set-frame-properties", Fset_frame_properties, 2, 2, 0, /*
Change some properties of a frame.
PLIST is a property list.
You can also change frame properties individually using `set-frame-property',
but it may be more efficient to change many properties at once.

Frame properties can be retrieved using `frame-property' or `frame-properties'.

The following symbols etc. have predefined meanings:

 name		Name of the frame, used with X resources.
		Unchangeable after creation.

 height		Height of the frame, in lines.

 width		Width of the frame, in characters.

 minibuffer	Gives the minibuffer behavior for this frame.  Either
		t (frame has its own minibuffer), `only' (frame is
		a minibuffer-only frame), or a window (frame uses that
		window, which is on another frame, as the minibuffer).

 unsplittable	If non-nil, frame cannot be split by `display-buffer'.

 current-display-table, menubar-visible-p, left-margin-width,
 right-margin-width, minimum-line-ascent, minimum-line-descent,
 use-left-overflow, use-right-overflow, scrollbar-width, scrollbar-height,
 default-toolbar, top-toolbar, bottom-toolbar, left-toolbar, right-toolbar,
 default-toolbar-height, default-toolbar-width, top-toolbar-height,
 bottom-toolbar-height, left-toolbar-width, right-toolbar-width,
 default-toolbar-visible-p, top-toolbar-visible-p, bottom-toolbar-visible-p,
 left-toolbar-visible-p, right-toolbar-visible-p, toolbar-buttons-captioned-p,
 top-toolbar-border-width, bottom-toolbar-border-width,
 left-toolbar-border-width, right-toolbar-border-width,
 modeline-shadow-thickness, has-modeline-p
		[Giving the name of any built-in specifier variable is
		equivalent to calling `set-specifier' on the specifier,
		with a locale of FRAME.  Giving the name to `frame-property'
		calls `specifier-instance' on the specifier.]

 text-pointer-glyph, nontext-pointer-glyph, modeline-pointer-glyph,
 selection-pointer-glyph, busy-pointer-glyph, toolbar-pointer-glyph,
 menubar-pointer-glyph, scrollbar-pointer-glyph, gc-pointer-glyph,
 octal-escape-glyph, control-arrow-glyph, invisible-text-glyph,
 hscroll-glyph, truncation-glyph, continuation-glyph
		[Giving the name of any glyph variable is equivalent to
		calling `set-glyph-image' on the glyph, with a locale
		of FRAME.  Giving the name to `frame-property' calls
		`glyph-image-instance' on the glyph.]

 [default foreground], [default background], [default font],
 [modeline foreground], [modeline background], [modeline font],
 etc.
		[Giving a vector of a face and a property is equivalent
		to calling `set-face-property' on the face and property,
		with a locale of FRAME.  Giving the vector to
		`frame-property' calls `face-property-instance' on the
		face and property.]

Finally, if a frame property symbol has the property `frame-property-alias'
on it, then the value will be used in place of that symbol when looking
up and setting frame property values.  This allows you to alias one
frame property name to another.

See the variables `default-x-frame-plist', `default-tty-frame-plist'
and `default-mswindows-frame-plist' for a description of the properties
recognized for particular types of frames.
*/
       (frame, plist))
{
  struct frame *f = decode_frame (frame);
  Lisp_Object tail;
  Lisp_Object *tailp;
  struct gcpro gcpro1, gcpro2;

  XSETFRAME (frame, f);
  GCPRO2 (frame, plist);
  Fcheck_valid_plist (plist);
  plist = Fcopy_sequence (plist);
  Fcanonicalize_lax_plist (plist, Qnil);
  for (tail = plist; !NILP (tail); tail = Fcdr (Fcdr (tail)))
    {
      Lisp_Object prop = Fcar (tail);
      Lisp_Object val = Fcar (Fcdr (tail));

      prop = get_property_alias (prop);

#if 0
      /* mly wants this, but it's not reasonable to change the name of a
	 frame after it has been created, because the old name was used
	 for resource lookup. */
      if (EQ (prop, Qname))
        {
          CHECK_STRING (val);
          f->name = val;
        }
#endif /* 0 */
      if (EQ (prop, Qminibuffer))
	store_minibuf_frame_prop (f, val);
      if (EQ (prop, Qunsplittable))
	f->no_split = !NILP (val);
      if (EQ (prop, Qbuffer_predicate))
	f->buffer_predicate = val;
      if (SYMBOLP (prop) && EQ (Fbuilt_in_variable_type (prop),
				Qconst_specifier))
	call3 (Qset_specifier, Fsymbol_value (prop), val, frame);
      if (SYMBOLP (prop) && !NILP (Fget (prop, Qconst_glyph_variable, Qnil)))
	call3 (Qset_glyph_image, Fsymbol_value (prop), val, frame);
      if (VECTORP (prop) && XVECTOR_LENGTH (prop) == 2)
	{
	  Lisp_Object face_prop = XVECTOR_DATA (prop)[1];
	  CHECK_SYMBOL (face_prop);
	  call4 (Qset_face_property,
		 Fget_face (XVECTOR_DATA (prop)[0]),
		 face_prop, val, frame);
	}
    }

  MAYBE_FRAMEMETH (f, set_frame_properties, (f, plist));
  for (tailp = &plist; !NILP (*tailp);)
    {
      Lisp_Object *next_tailp;
      Lisp_Object next;
      Lisp_Object prop;

      next = Fcdr (*tailp);
      CHECK_CONS (next);
      next_tailp = &XCDR (next);
      prop = Fcar (*tailp);

      prop = get_property_alias (prop);

      if (EQ (prop, Qminibuffer)
	  || EQ (prop, Qunsplittable)
	  || EQ (prop, Qbuffer_predicate)
	  || EQ (prop, Qheight)
	  || EQ (prop, Qwidth)
	  || (SYMBOLP (prop) && EQ (Fbuilt_in_variable_type (prop),
				    Qconst_specifier))
	  || (SYMBOLP (prop) && !NILP (Fget (prop, Qconst_glyph_variable,
					     Qnil)))
	  || (VECTORP (prop) && XVECTOR_LENGTH (prop) == 2)
	  || FRAMEMETH_OR_GIVEN (f, internal_frame_property_p, (f, prop), 0))
	*tailp = *next_tailp;
      tailp = next_tailp;
    }

  f->plist = nconc2 (plist, f->plist);
  Fcanonicalize_lax_plist (f->plist, Qnil);
  UNGCPRO;
  return Qnil;
}

DEFUN ("frame-property", Fframe_property, 2, 3, 0, /*
Return FRAME's value for property PROPERTY.
See `set-frame-properties' for the built-in property names.
*/
       (frame, property, default_))
{
  struct frame *f = decode_frame (frame);

  XSETFRAME (frame, f);

  property = get_property_alias (property);

#define FROB(propprop, value) 	\
do {				\
  if (EQ (property, propprop))	\
      return value;		\
} while (0)

  FROB (Qname, f->name);
  FROB (Qheight, make_int (FRAME_HEIGHT (f)));
  FROB (Qwidth,  make_int (FRAME_WIDTH  (f)));
  /* NOTE: FSF returns Qnil instead of Qt for FRAME_HAS_MINIBUF_P.
     This is over-the-top bogosity, because it's inconsistent with
     the semantics of `minibuffer' when passed to `make-frame'.
     Returning Qt makes things consistent. */
  FROB (Qminibuffer, (FRAME_MINIBUF_ONLY_P (f) ? Qonly :
		      FRAME_HAS_MINIBUF_P  (f) ? Qt    :
		      FRAME_MINIBUF_WINDOW (f)));
  FROB (Qunsplittable, FRAME_NO_SPLIT_P (f) ? Qt : Qnil);
  FROB (Qbuffer_predicate, f->buffer_predicate);

#undef FROB

  if (SYMBOLP (property) && EQ (Fbuilt_in_variable_type (property),
				Qconst_specifier))
    return Fspecifier_instance (Fsymbol_value (property), frame, default_, Qnil);
  if (SYMBOLP (property) && !NILP (Fget (property, Qconst_glyph_variable,
					 Qnil)))
    {
      Lisp_Object glyph = Fsymbol_value (property);
      CHECK_GLYPH (glyph);
      return Fspecifier_instance (XGLYPH_IMAGE (glyph), frame, default_, Qnil);
    }
  if (VECTORP (property) && XVECTOR_LENGTH (property) == 2)
    {
      Lisp_Object face_prop = XVECTOR_DATA (property)[1];
      CHECK_SYMBOL (face_prop);
      return call3 (Qface_property_instance,
		    Fget_face (XVECTOR_DATA (property)[0]),
		    face_prop, frame);
    }

  {
    Lisp_Object value;

    value = FRAMEMETH_OR_GIVEN (f, frame_property, (f, property), Qunbound);
    if (!UNBOUNDP (value))
      return value;

    value = external_plist_get (&f->plist, property, 1, ERROR_ME);
    if (!UNBOUNDP (value))
      return value;
    return default_;
  }
}

DEFUN ("frame-properties", Fframe_properties, 0, 1, 0, /*
Return a property list of the properties of FRAME.
Do not modify this list; use `set-frame-property' instead.
*/
       (frame))
{
  struct frame *f = decode_frame (frame);
  Lisp_Object result = Qnil;
  struct gcpro gcpro1;

  GCPRO1 (result);

#define FROB(propprop, value)				\
do {							\
  Lisp_Object temtem = (value);				\
  if (!NILP (temtem))					\
    /* backwards order; we reverse it below */		\
    result = Fcons (temtem, Fcons (propprop, result));	\
} while (0)

  FROB (Qname, f->name);
  FROB (Qheight, make_int (FRAME_HEIGHT (f)));
  FROB (Qwidth,  make_int (FRAME_WIDTH  (f)));
 /* NOTE: FSF returns Qnil instead of Qt for FRAME_HAS_MINIBUF_P.
     This is over-the-top bogosity, because it's inconsistent with
     the semantics of `minibuffer' when passed to `make-frame'.
     Returning Qt makes things consistent. */
  FROB (Qminibuffer, (FRAME_MINIBUF_ONLY_P (f) ? Qonly :
		      FRAME_HAS_MINIBUF_P  (f) ? Qt    :
		      FRAME_MINIBUF_WINDOW (f)));
  FROB (Qunsplittable, FRAME_NO_SPLIT_P (f) ? Qt : Qnil);
  FROB (Qbuffer_predicate, f->buffer_predicate);

#undef FROB

  /* #### should we be adding all the specifiers and glyphs?
     That would entail having a list of them all. */
  {
    Lisp_Object value;

    value = FRAMEMETH_OR_GIVEN (f, frame_properties, (f), Qnil);
    result = nconc2 (value, result);
    /* #### for the moment (since old code uses `frame-parameters'),
       we call `copy-sequence' on f->plist.  That allows frame-parameters
       to destructively convert the plist into an alist, which is more
       efficient than doing it non-destructively.  At some point we
       should remove the call to copy-sequence. */
    result = nconc2 (Fnreverse (result), Fcopy_sequence (f->plist));
    RETURN_UNGCPRO (result);
  }
}


DEFUN ("frame-pixel-height", Fframe_pixel_height, 0, 1, 0, /*
Return the height in pixels of FRAME.
*/
       (frame))
{
  struct frame *f = decode_frame (frame);
  return make_int (f->pixheight);
}

DEFUN ("frame-pixel-width", Fframe_pixel_width, 0, 1, 0, /*
Return the width in pixels of FRAME.
*/
       (frame))
{
  struct frame *f = decode_frame (frame);
  return make_int (f->pixwidth);
}

DEFUN ("frame-name", Fframe_name, 0, 1, 0, /*
Return the name of FRAME (defaulting to the selected frame).
This is not the same as the `title' of the frame.
*/
       (frame))
{
  return decode_frame (frame)->name;
}

DEFUN ("frame-modified-tick", Fframe_modified_tick, 0, 1, 0, /*
Return FRAME's tick counter, incremented for each change to the frame.
Each frame has a tick counter which is incremented each time the frame
is resized, a window is resized, added, or deleted, a face is changed,
`set-window-buffer' or `select-window' is called on a window in the
frame, the window-start of a window in the frame has changed, or
anything else interesting has happened.  It wraps around occasionally.
No argument or nil as argument means use selected frame as FRAME.
*/
       (frame))
{
  return make_int (decode_frame (frame)->modiff);
}

static void
internal_set_frame_size (struct frame *f, int cols, int rows, int pretend)
{
  if (pretend || !HAS_FRAMEMETH_P (f, set_frame_size))
    change_frame_size (f, rows, cols, 0);
  else
    FRAMEMETH (f, set_frame_size, (f, cols, rows));
}

DEFUN ("set-frame-height", Fset_frame_height, 2, 3, 0, /*
Specify that the frame FRAME has LINES lines.
Optional third arg non-nil means that redisplay should use LINES lines
but that the idea of the actual height of the frame should not be changed.
*/
       (frame, rows, pretend))
{
  struct frame *f = decode_frame (frame);
  XSETFRAME (frame, f);
  CHECK_INT (rows);

  internal_set_frame_size (f, FRAME_WIDTH (f), XINT (rows),
			   !NILP (pretend));
  return frame;
}

DEFUN ("set-frame-width", Fset_frame_width, 2, 3, 0, /*
Specify that the frame FRAME has COLS columns.
Optional third arg non-nil means that redisplay should use COLS columns
but that the idea of the actual width of the frame should not be changed.
*/
       (frame, cols, pretend))
{
  struct frame *f = decode_frame (frame);
  XSETFRAME (frame, f);
  CHECK_INT (cols);

  internal_set_frame_size (f, XINT (cols), FRAME_HEIGHT (f),
			    !NILP (pretend));
  return frame;
}

DEFUN ("set-frame-size", Fset_frame_size, 3, 4, 0, /*
Sets size of FRAME to COLS by ROWS.
Optional fourth arg non-nil means that redisplay should use COLS by ROWS
but that the idea of the actual size of the frame should not be changed.
*/
       (frame, cols, rows, pretend))
{
  struct frame *f = decode_frame (frame);
  XSETFRAME (frame, f);
  CHECK_INT (cols);
  CHECK_INT (rows);

  internal_set_frame_size (f, XINT (cols), XINT (rows), !NILP (pretend));
  return frame;
}

DEFUN ("set-frame-position", Fset_frame_position, 3, 3, 0, /*
Sets position of FRAME in pixels to XOFFSET by YOFFSET.
This is actually the position of the upper left corner of the frame.
Negative values for XOFFSET or YOFFSET are interpreted relative to
the rightmost or bottommost possible position (that stays within the screen).
*/
       (frame, xoffset, yoffset))
{
  struct frame *f = decode_frame (frame);
  CHECK_INT (xoffset);
  CHECK_INT (yoffset);

  MAYBE_FRAMEMETH (f, set_frame_position, (f, XINT (xoffset), XINT (yoffset)));

  return Qt;
}



/* Frame size conversion functions moved here from EmacsFrame.c
   because they're generic and really don't belong in that file.
   Function get_default_char_pixel_size() removed because it's
   exactly the same as default_face_height_and_width(). */
static void
frame_conversion_internal (struct frame *f, int pixel_to_char,
			   int *pixel_width, int *pixel_height,
			   int *char_width, int *char_height)
{
  int cpw;
  int cph;
  int egw;
  int obw, obh, bdr;
  Lisp_Object frame, window;

  XSETFRAME (frame, f);
  default_face_height_and_width (frame, &cph, &cpw);
  window = FRAME_SELECTED_WINDOW (f);

  egw = max (glyph_width (Vcontinuation_glyph, Vdefault_face, 0, window),
	     glyph_width (Vtruncation_glyph, Vdefault_face, 0, window));
  egw = max (egw, cpw);
  bdr = 2 * f->internal_border_width;
  obw = FRAME_SCROLLBAR_WIDTH (f) + FRAME_THEORETICAL_LEFT_TOOLBAR_WIDTH (f) +
    FRAME_THEORETICAL_RIGHT_TOOLBAR_WIDTH (f) +
    2 * FRAME_THEORETICAL_LEFT_TOOLBAR_BORDER_WIDTH (f) +
    2 * FRAME_THEORETICAL_RIGHT_TOOLBAR_BORDER_WIDTH (f);
  obh = FRAME_SCROLLBAR_HEIGHT (f) + FRAME_THEORETICAL_TOP_TOOLBAR_HEIGHT (f) +
    FRAME_THEORETICAL_BOTTOM_TOOLBAR_HEIGHT (f) +
    2 * FRAME_THEORETICAL_TOP_TOOLBAR_BORDER_WIDTH (f) +
    2 * FRAME_THEORETICAL_BOTTOM_TOOLBAR_BORDER_WIDTH (f);

  if (pixel_to_char)
    {
      *char_width = 1 + ((*pixel_width - egw) - bdr - obw) / cpw;
      *char_height = (*pixel_height - bdr - obh) / cph;
    }
  else
    {
      *pixel_width = (*char_width - 1) * cpw + egw + bdr + obw;
      *pixel_height = *char_height * cph + bdr + obh;
    }
}

/* This takes the size in pixels of the text area, and returns the number
   of characters that will fit there, taking into account the internal
   border width, and the pixel width of the line terminator glyphs (which
   always count as one "character" wide, even if they are not the same size
   as the default character size of the default font).  The frame scrollbar
   width and left and right toolbar widths are also subtracted out of the
   available width.  The frame scrollbar height and top and bottom toolbar
   heights are subtracted out of the available height.

   Therefore the result is not necessarily a multiple of anything in
   particular.  */
void
pixel_to_char_size (struct frame *f, int pixel_width, int pixel_height,
		    int *char_width, int *char_height)
{
  frame_conversion_internal (f, 1, &pixel_width, &pixel_height, char_width,
			     char_height);
}

/* Given a character size, this returns the minimum number of pixels
   necessary to display that many characters, taking into account the
   internal border width, scrollbar height and width, toolbar heights and
   widths and the size of the line terminator glyphs (assuming the line
   terminators take up exactly one character position).

   Therefore the result is not necessarily a multiple of anything in
   particular.  */
void
char_to_pixel_size (struct frame *f, int char_width, int char_height,
		    int *pixel_width, int *pixel_height)
{
  frame_conversion_internal (f, 0, pixel_width, pixel_height, &char_width,
			     &char_height);
}

/* Given a pixel size, rounds DOWN to the smallest size in pixels necessary
   to display the same number of characters as are displayable now.
 */
void
round_size_to_char (struct frame *f, int in_width, int in_height,
		    int *out_width, int *out_height)
{
  int char_width;
  int char_height;
  pixel_to_char_size (f, in_width, in_height, &char_width, &char_height);
  char_to_pixel_size (f, char_width, char_height, out_width, out_height);
}

/* Change the frame height and/or width.  Values may be given as zero to
   indicate no change is to take place. */
static void
change_frame_size_1 (struct frame *f, int newheight, int newwidth)
{
  Lisp_Object frame;
  int new_pixheight, new_pixwidth;
  int font_height, font_width;

  /* #### Chuck -- shouldn't we be checking to see if the frame
     is being "changed" to its existing size, and do nothing if so? */
  /* No, because it would hose toolbar updates.  The toolbar
     update code relies on this function to cause window `top' and
     `left' coordinates to be recomputed even though no frame size
     change occurs. --kyle */
  if (in_display)
    abort ();

  XSETFRAME (frame, f);

  default_face_height_and_width (frame, &font_height, &font_width);

  /* This size-change overrides any pending one for this frame.  */
  FRAME_NEW_HEIGHT (f) = 0;
  FRAME_NEW_WIDTH (f) = 0;

  new_pixheight = newheight * font_height;
  new_pixwidth = (newwidth - 1) * font_width;

  /* #### dependency on FRAME_WIN_P should be removed. */
  if (FRAME_WIN_P (f))
    {
      new_pixheight += FRAME_SCROLLBAR_HEIGHT (f);
      new_pixwidth += FRAME_SCROLLBAR_WIDTH (f);
    }

  /* when frame_conversion_internal() calculated the number of rows/cols
     in the frame, the toolbar sizes were subtracted out.  However,
     if the corresponding toolbar is not actually visible in the
     selected window, then the extra space needs to be filled in
     with rows/cols. */
  if (!FRAME_REAL_TOP_TOOLBAR_VISIBLE (f))
    new_pixheight += FRAME_THEORETICAL_TOP_TOOLBAR_HEIGHT (f) +
      2 * FRAME_THEORETICAL_TOP_TOOLBAR_BORDER_WIDTH (f);
  if (!FRAME_REAL_BOTTOM_TOOLBAR_VISIBLE (f))
    new_pixheight += FRAME_THEORETICAL_BOTTOM_TOOLBAR_HEIGHT (f) +
      2 * FRAME_THEORETICAL_BOTTOM_TOOLBAR_BORDER_WIDTH (f);
  if (!FRAME_REAL_LEFT_TOOLBAR_VISIBLE (f))
    new_pixwidth += FRAME_THEORETICAL_LEFT_TOOLBAR_WIDTH (f) +
      2 * FRAME_THEORETICAL_LEFT_TOOLBAR_BORDER_WIDTH (f);
  if (!FRAME_REAL_RIGHT_TOOLBAR_VISIBLE (f))
    new_pixwidth += FRAME_THEORETICAL_RIGHT_TOOLBAR_WIDTH (f) +
      2 * FRAME_THEORETICAL_RIGHT_TOOLBAR_BORDER_WIDTH (f);

  /* Adjust the width for the end glyph which may be a different width
     than the default character width. */
  {
    int adjustment, trunc_width, cont_width;

    trunc_width = glyph_width (Vtruncation_glyph, Vdefault_face, 0,
			       FRAME_SELECTED_WINDOW (f));
    cont_width = glyph_width (Vcontinuation_glyph, Vdefault_face, 0,
			      FRAME_SELECTED_WINDOW (f));
    adjustment = max (trunc_width, cont_width);
    adjustment = max (adjustment, font_width);

    new_pixwidth += adjustment;
  }

  /* If we don't have valid values, exit. */
  if (!new_pixheight && !new_pixwidth)
    return;

  if (new_pixheight)
    {
      XWINDOW (FRAME_ROOT_WINDOW (f))->pixel_top = FRAME_TOP_BORDER_END (f);

      if (FRAME_HAS_MINIBUF_P (f)
	  && ! FRAME_MINIBUF_ONLY_P (f))
	/* Frame has both root and minibuffer.  */
	{
	  /*
	   * Leave the minibuffer height the same if the frame has
	   * been initialized, and the minibuffer height is tall
	   * enough to display at least one line of text in the default
	   * font, and the old minibuffer height is a multiple of the
	   * default font height.  This should cause the minibuffer
	   * height to be recomputed on font changes but not for
	   * other frame size changes, which seems reasonable.
	   */
	  int old_minibuf_height =
	    XWINDOW(FRAME_MINIBUF_WINDOW(f))->pixel_height;
	  int minibuf_height =
	    f->init_finished && (old_minibuf_height % font_height) == 0 ?
	    max(old_minibuf_height, font_height) :
	    font_height;
	  set_window_pixheight (FRAME_ROOT_WINDOW (f),
				/* - font_height for minibuffer */
				new_pixheight - minibuf_height, 0);

	  XWINDOW (FRAME_MINIBUF_WINDOW (f))->pixel_top =
	    new_pixheight - minibuf_height + FRAME_TOP_BORDER_END (f);

	  set_window_pixheight (FRAME_MINIBUF_WINDOW (f), minibuf_height, 0);
	}
      else
	/* Frame has just one top-level window.  */
	set_window_pixheight (FRAME_ROOT_WINDOW (f), new_pixheight, 0);

      FRAME_HEIGHT (f) = newheight;
      if (FRAME_TTY_P (f))
	f->pixheight = newheight;
    }

  if (new_pixwidth)
    {
      XWINDOW (FRAME_ROOT_WINDOW (f))->pixel_left = FRAME_LEFT_BORDER_END (f);
      set_window_pixwidth (FRAME_ROOT_WINDOW (f), new_pixwidth, 0);

      if (FRAME_HAS_MINIBUF_P (f))
	{
	  XWINDOW (FRAME_MINIBUF_WINDOW (f))->pixel_left =
	    FRAME_LEFT_BORDER_END (f);
	  set_window_pixwidth (FRAME_MINIBUF_WINDOW (f), new_pixwidth, 0);
	}

      FRAME_WIDTH (f) = newwidth;
      if (FRAME_TTY_P (f))
	f->pixwidth = newwidth;
    }

  MARK_FRAME_TOOLBARS_CHANGED (f);
  MARK_FRAME_CHANGED (f);
}

void
change_frame_size (struct frame *f, int newheight, int newwidth, int delay)
{
  /* sometimes we get passed a size that's too small (esp. when a
     client widget gets resized, since we have no control over this).
     So deal. */
  check_frame_size (f, &newheight, &newwidth);

  if (delay || in_display || gc_in_progress)
    {
      MARK_FRAME_SIZE_CHANGED (f);
      f->new_width = newwidth;
      f->new_height = newheight;
      return;
    }

  f->size_change_pending = 0;
  /* For TTY frames, it's like one, like all ...
     Can't have two TTY frames of different sizes on the same device. */
  if (FRAME_TTY_P (f))
    {
      Lisp_Object frmcons;

      DEVICE_FRAME_LOOP (frmcons, XDEVICE (FRAME_DEVICE (f)))
	change_frame_size_1 (XFRAME (XCAR (frmcons)), newheight, newwidth);
    }
  else
    change_frame_size_1 (f, newheight, newwidth);
}


void
update_frame_title (struct frame *f)
{
  struct window *w = XWINDOW (FRAME_SELECTED_WINDOW (f));
  Lisp_Object title_format;
  Lisp_Object icon_format;
  Bufbyte *title;

  /* We don't change the title for the minibuffer unless the frame
     only has a minibuffer. */
  if (MINI_WINDOW_P (w) && !FRAME_MINIBUF_ONLY_P (f))
    return;

  /* And we don't want dead buffers to blow up on us. */
  if (!BUFFER_LIVE_P (XBUFFER (w->buffer)))
    return;

  title = NULL;
  title_format = symbol_value_in_buffer (Qframe_title_format,      w->buffer);
  icon_format  = symbol_value_in_buffer (Qframe_icon_title_format, w->buffer);

  if (HAS_FRAMEMETH_P (f, set_title_from_bufbyte))
    {
      title = generate_formatted_string (w, title_format, Qnil,
                                         DEFAULT_INDEX, CURRENT_DISP);
      FRAMEMETH (f, set_title_from_bufbyte, (f, title));
    }

  if (HAS_FRAMEMETH_P (f, set_icon_name_from_bufbyte))
    {
      if (!EQ (icon_format, title_format) || !title)
	{
	  if (title)
	    xfree (title);

	  title = generate_formatted_string (w, icon_format, Qnil,
                                             DEFAULT_INDEX, CURRENT_DISP);
	}
      FRAMEMETH (f, set_icon_name_from_bufbyte, (f, title));
    }

  if (title)
    xfree (title);
}


DEFUN ("set-frame-pointer", Fset_frame_pointer, 2, 2, 0, /*
Set the mouse pointer of FRAME to the given pointer image instance.
You should not call this function directly.  Instead, set one of
the variables `text-pointer-glyph', `nontext-pointer-glyph',
`modeline-pointer-glyph', `selection-pointer-glyph',
`busy-pointer-glyph', or `toolbar-pointer-glyph'.
*/
       (frame, image_instance))
{
  struct frame *f = decode_frame (frame);
  CHECK_POINTER_IMAGE_INSTANCE (image_instance);
  if (!EQ (f->pointer, image_instance))
    {
      f->pointer = image_instance;
      MAYBE_FRAMEMETH (f, set_frame_pointer, (f));
    }
  return Qnil;
}


void
update_frame_icon (struct frame *f)
{
  if (f->icon_changed || f->windows_changed)
    {
      Lisp_Object frame = Qnil;
      Lisp_Object new_icon;

      XSETFRAME (frame, f);
      new_icon = glyph_image_instance (Vframe_icon_glyph, frame,
				       ERROR_ME_WARN, 0);
      if (!EQ (new_icon, f->icon))
	{
	  f->icon = new_icon;
	  MAYBE_FRAMEMETH (f, set_frame_icon, (f));
	}
    }

  f->icon_changed = 0;
}

static void
icon_glyph_changed (Lisp_Object glyph, Lisp_Object property,
		    Lisp_Object locale)
{
  MARK_ICON_CHANGED;
}


void
syms_of_frame (void)
{
  defsymbol (&Qdelete_frame_hook, "delete-frame-hook");
  defsymbol (&Qselect_frame_hook, "select-frame-hook");
  defsymbol (&Qdeselect_frame_hook, "deselect-frame-hook");
  defsymbol (&Qcreate_frame_hook, "create-frame-hook");
  defsymbol (&Qcustom_initialize_frame, "custom-initialize-frame");
  defsymbol (&Qmouse_enter_frame_hook, "mouse-enter-frame-hook");
  defsymbol (&Qmouse_leave_frame_hook, "mouse-leave-frame-hook");
  defsymbol (&Qmap_frame_hook, "map-frame-hook");
  defsymbol (&Qunmap_frame_hook, "unmap-frame-hook");
#if defined (HAVE_CDE) || defined (HAVE_OFFIX_DND)
  defsymbol (&Qdrag_and_drop_functions, "drag-and-drop-functions");
#endif

  defsymbol (&Qframep, "framep");
  defsymbol (&Qframe_live_p, "frame-live-p");
  defsymbol (&Qframe_x_p, "frame-x-p");
  defsymbol (&Qframe_tty_p, "frame-tty-p");
  defsymbol (&Qdelete_frame, "delete-frame");
  defsymbol (&Qsynchronize_minibuffers, "synchronize-minibuffers");
  defsymbol (&Qbuffer_predicate, "buffer-predicate");
  defsymbol (&Qframe_being_created, "frame-being-created");
  defsymbol (&Qmake_initial_minibuffer_frame, "make-initial-minibuffer-frame");

  defsymbol (&Qframe_title_format, "frame-title-format");
  defsymbol (&Qframe_icon_title_format, "frame-icon-title-format");

  defsymbol (&Qhidden, "hidden");
  defsymbol (&Qvisible, "visible");
  defsymbol (&Qiconic, "iconic");
  defsymbol (&Qinvisible, "invisible");
  defsymbol (&Qvisible_iconic, "visible-iconic");
  defsymbol (&Qinvisible_iconic, "invisible-iconic");
  defsymbol (&Qnomini, "nomini");
  defsymbol (&Qvisible_nomini, "visible-nomini");
  defsymbol (&Qiconic_nomini, "iconic-nomini");
  defsymbol (&Qinvisible_nomini, "invisible-nomini");
  defsymbol (&Qvisible_iconic_nomini, "visible-iconic-nomini");
  defsymbol (&Qinvisible_iconic_nomini, "invisible-iconic-nomini");

  defsymbol (&Qminibuffer, "minibuffer");
  defsymbol (&Qunsplittable, "unsplittable");
  defsymbol (&Qinternal_border_width, "internal-border-width");
  defsymbol (&Qtop_toolbar_shadow_color, "top-toolbar-shadow-color");
  defsymbol (&Qbottom_toolbar_shadow_color, "bottom-toolbar-shadow-color");
  defsymbol (&Qbackground_toolbar_color, "background-toolbar-color");
  defsymbol (&Qtop_toolbar_shadow_pixmap, "top-toolbar-shadow-pixmap");
  defsymbol (&Qbottom_toolbar_shadow_pixmap, "bottom-toolbar-shadow-pixmap");
  defsymbol (&Qtoolbar_shadow_thickness, "toolbar-shadow-thickness");
  defsymbol (&Qscrollbar_placement, "scrollbar-placement");
  defsymbol (&Qinter_line_space, "inter-line-space");
  /* Qiconic already in this function. */
  defsymbol (&Qvisual_bell, "visual-bell");
  defsymbol (&Qbell_volume, "bell-volume");
  defsymbol (&Qpointer_background, "pointer-background");
  defsymbol (&Qpointer_color, "pointer-color");
  defsymbol (&Qtext_pointer, "text-pointer");
  defsymbol (&Qspace_pointer, "space-pointer");
  defsymbol (&Qmodeline_pointer, "modeline-pointer");
  defsymbol (&Qgc_pointer, "gc-pointer");
  defsymbol (&Qinitially_unmapped, "initially-unmapped");
  defsymbol (&Quse_backing_store, "use-backing-store");
  defsymbol (&Qborder_color, "border-color");
  defsymbol (&Qborder_width, "border-width");
  /* Qwidth, Qheight, Qleft, Qtop in general.c */
  defsymbol (&Qset_specifier, "set-specifier");
  defsymbol (&Qset_glyph_image, "set-glyph-image");
  defsymbol (&Qset_face_property, "set-face-property");
  defsymbol (&Qface_property_instance, "face-property-instance");
  defsymbol (&Qframe_property_alias, "frame-property-alias");

  DEFSUBR (Fmake_frame);
  DEFSUBR (Fframep);
  DEFSUBR (Fframe_live_p);
#if 0 /* FSFmacs */
  DEFSUBR (Fignore_event);
#endif
  DEFSUBR (Fselect_frame);
  DEFSUBR (Fselected_frame);
  DEFSUBR (Factive_minibuffer_window);
  DEFSUBR (Flast_nonminibuf_frame);
  DEFSUBR (Fframe_root_window);
  DEFSUBR (Fframe_selected_window);
  DEFSUBR (Fset_frame_selected_window);
  DEFSUBR (Fframe_device);
  DEFSUBR (Fnext_frame);
  DEFSUBR (Fprevious_frame);
  DEFSUBR (Fdelete_frame);
  DEFSUBR (Fmouse_position);
  DEFSUBR (Fmouse_pixel_position);
  DEFSUBR (Fmouse_position_as_motion_event);
  DEFSUBR (Fset_mouse_position);
  DEFSUBR (Fset_mouse_pixel_position);
  DEFSUBR (Fmake_frame_visible);
  DEFSUBR (Fmake_frame_invisible);
  DEFSUBR (Ficonify_frame);
  DEFSUBR (Fdeiconify_frame);
  DEFSUBR (Fframe_visible_p);
  DEFSUBR (Fframe_totally_visible_p);
  DEFSUBR (Fframe_iconified_p);
  DEFSUBR (Fvisible_frame_list);
  DEFSUBR (Fraise_frame);
  DEFSUBR (Flower_frame);
  DEFSUBR (Fframe_property);
  DEFSUBR (Fframe_properties);
  DEFSUBR (Fset_frame_properties);
  DEFSUBR (Fframe_pixel_height);
  DEFSUBR (Fframe_pixel_width);
  DEFSUBR (Fframe_name);
  DEFSUBR (Fframe_modified_tick);
  DEFSUBR (Fset_frame_height);
  DEFSUBR (Fset_frame_width);
  DEFSUBR (Fset_frame_size);
  DEFSUBR (Fset_frame_position);
  DEFSUBR (Fset_frame_pointer);
}

void
vars_of_frame (void)
{
  /* */
  Vframe_being_created = Qnil;
  staticpro (&Vframe_being_created);

#ifdef HAVE_CDE
  Fprovide (intern ("cde"));
#endif

#ifdef HAVE_OFFIX_DND
  Fprovide (intern ("offix"));
#endif

#if 0 /* FSFmacs stupidity */
  xxDEFVAR_LISP ("emacs-iconified", &Vemacs_iconified /*
Non-nil if all of emacs is iconified and frame updates are not needed.
*/ );
  Vemacs_iconified = Qnil;
#endif

  DEFVAR_LISP ("select-frame-hook", &Vselect_frame_hook /*
Function or functions to run just after a new frame is given the focus.
Note that calling `select-frame' does not necessarily set the focus:
The actual window-system focus will not be changed until the next time
that XEmacs is waiting for an event, and even then, the window manager
may refuse the focus-change request.
*/ );
  Vselect_frame_hook = Qnil;

  DEFVAR_LISP ("deselect-frame-hook", &Vdeselect_frame_hook /*
Function or functions to run just before a frame loses the focus.
See `select-frame-hook'.
*/ );
  Vdeselect_frame_hook = Qnil;

  DEFVAR_LISP ("delete-frame-hook", &Vdelete_frame_hook /*
Function or functions to call when a frame is deleted.
One argument, the about-to-be-deleted frame.
*/ );
  Vdelete_frame_hook = Qnil;

  DEFVAR_LISP ("create-frame-hook", &Vcreate_frame_hook /*
Function or functions to call when a frame is created.
One argument, the newly-created frame.
*/ );
  Vcreate_frame_hook = Qnil;

  DEFVAR_LISP ("mouse-enter-frame-hook", &Vmouse_enter_frame_hook /*
Function or functions to call when the mouse enters a frame.
One argument, the frame.
Be careful not to make assumptions about the window manager's focus model.
In most cases, the `deselect-frame-hook' is more appropriate.
*/ );
  Vmouse_enter_frame_hook = Qnil;

  DEFVAR_LISP ("mouse-leave-frame-hook", &Vmouse_leave_frame_hook /*
Function or functions to call when the mouse leaves a frame.
One argument, the frame.
Be careful not to make assumptions about the window manager's focus model.
In most cases, the `select-frame-hook' is more appropriate.
*/ );
  Vmouse_leave_frame_hook = Qnil;

  DEFVAR_LISP ("map-frame-hook", &Vmap_frame_hook /*
Function or functions to call when a frame is mapped.
One argument, the frame.
*/ );
  Vmap_frame_hook = Qnil;

  DEFVAR_LISP ("unmap-frame-hook", &Vunmap_frame_hook /*
Function or functions to call when a frame is unmapped.
One argument, the frame.
*/ );
  Vunmap_frame_hook = Qnil;

  DEFVAR_BOOL ("allow-deletion-of-last-visible-frame",
	       &allow_deletion_of_last_visible_frame /*
*Non-nil means to assume the force option to delete-frame.
*/ );
  allow_deletion_of_last_visible_frame = 0;

#if defined (HAVE_CDE) || defined (HAVE_OFFIX_DND)
  DEFVAR_LISP ("drag-and-drop-functions", &Vdrag_and_drop_functions /*
Function or functions to run when an object is dropped on a frame.
Each function is called with either two or three args.  If called with
two args, the args are a frame and a pathname.  If with three, the
args are a frame, a pathname (which will be either a string or nil)
and the textual representation of the dragged object.
*/ );
  Vdrag_and_drop_functions = Qnil;
#endif /* HAVE_CDE */

  DEFVAR_LISP ("mouse-motion-handler", &Vmouse_motion_handler /*
Handler for motion events.  One arg, the event.
For most applications, you should use `mode-motion-hook' instead of this.
*/ );
  Vmouse_motion_handler = Qnil;

  DEFVAR_LISP ("synchronize-minibuffers",&Vsynchronize_minibuffers /*
Set to t if all minibuffer windows are to be synchronized.
This will cause echo area messages to appear in the minibuffers of all
visible frames.
*/ );
  Vsynchronize_minibuffers = Qnil;

  DEFVAR_LISP ("frame-title-format", &Vframe_title_format /*
Controls the title of the X window corresponding to the selected frame.
This is the same format as `modeline-format' with the exception that
%- is ignored.
*/ );
  Vframe_title_format = Fpurecopy (build_string ("%S: %b"));

  DEFVAR_LISP ("frame-icon-title-format", &Vframe_icon_title_format /*
Controls the title of the icon corresponding to the selected frame.
See also the variable `frame-title-format'.
*/ );
  Vframe_icon_title_format = Fpurecopy (build_string ("%b"));

  DEFVAR_LISP ("default-frame-name", &Vdefault_frame_name /*
The default name to assign to newly-created frames.
This can be overridden by arguments to `make-frame'.
This must be a string.
*/ );
  Vdefault_frame_name = Fpurecopy (build_string ("emacs"));

  DEFVAR_LISP ("default-frame-plist", &Vdefault_frame_plist /*
Plist of default values for frame creation, other than the first one.
These may be set in your init file, like this:

  \(setq default-frame-plist '(width 80 height 55))

The properties may be in alist format for backward compatibility
but you should not rely on this behavior.

These override values given in window system configuration data,
 including X Windows' defaults database.

Since the first X frame is created before loading your .emacs file,
you must use the X resource database for that.

For values specific to the first Emacs frame, see `initial-frame-plist'.
For values specific to the separate minibuffer frame, see
 `minibuffer-frame-plist'.

See also the variables `default-x-frame-plist' and
`default-tty-frame-plist', which are like `default-frame-plist'
except that they apply only to X or tty frames, respectively
\(whereas `default-frame-plist' applies to all types of frames).
*/ );
  Vdefault_frame_plist = Qnil;

  DEFVAR_LISP ("frame-icon-glyph", &Vframe_icon_glyph /*
Icon glyph used to iconify a frame.
*/ );
}

void
complex_vars_of_frame (void)
{
  Vframe_icon_glyph = allocate_glyph (GLYPH_ICON, icon_glyph_changed);
}
