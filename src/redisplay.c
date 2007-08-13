/* Display generation from window structure and buffer text.
   Copyright (C) 1994, 1995, 1996 Board of Trustees, University of Illinois.
   Copyright (C) 1995 Free Software Foundation, Inc.
   Copyright (C) 1995, 1996 Ben Wing.
   Copyright (C) 1995 Sun Microsystems, Inc.
   Copyright (C) 1996 Chuck Thompson.

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

/* Synched up with:  Not in FSF. */

/* Author: Chuck Thompson */

/* Fixed up by Ben Wing for Mule */

/* This file has been Mule-ized. */

/*****************************************************************************
 The Golden Rules of Redisplay

 First:		It Is Better To Be Correct Than Fast
 Second:	Thou Shalt Not Run Elisp From Within Redisplay
 Third:		It Is Better To Be Fast Than Not To Be
 ****************************************************************************/

#include <config.h>
#include "lisp.h"
#include <limits.h>

#include "buffer.h"
#include "commands.h"
#include "debug.h"
#include "device.h"
#include "elhash.h"
#include "extents.h"
#include "faces.h"
#include "frame.h"
#include "glyphs.h"
#include "insdel.h"
#include "menubar.h"
#include "objects.h"
#include "process.h"
#include "redisplay.h"
#include "toolbar.h"
#include "window.h"
#include "line-number.h"
#ifdef FILE_CODING
#include "file-coding.h"
#endif

#ifdef HAVE_TTY
#include "console-tty.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for isatty() */
#endif
#endif /* HAVE_TTY */

/* Note: We have to be careful throughout this code to properly handle
   and differentiate between Bufbytes and Emchars.

   Since strings are generally composed of Bufbytes, I've taken the tack
   that any contiguous set of Bufbytes is called a "string", while
   any contiguous set of Emchars is called an "array". */

/* Return value to indicate a failure by an add_*_rune routine to add
   a rune, but no propagation information needs to be returned. */
#define ADD_FAILED (prop_block_dynarr *) 1

#define BEGIN_GLYPHS	0
#define END_GLYPHS	1
#define LEFT_GLYPHS	2
#define RIGHT_GLYPHS	3

/* Set the vertical clip to 0 if we are currently updating the line
   start cache.  Otherwise for buffers of line height 1 it may fail to
   be able to work properly because regenerate_window will not layout
   a single line.  */
#define VERTICAL_CLIP(w, display)					\
  (updating_line_start_cache						\
   ? 0									\
   : ((WINDOW_TTY_P (w) | (!display && scroll_on_clipped_lines))	\
      ? INT_MAX								\
      : vertical_clip))

/* The following structures are completely private to redisplay.c so
   we put them here instead of in a header file, for modularity. */

/* NOTE: Bytinds not Bufpos's in this structure. */

typedef struct position_redisplay_data_type
{
  /* This information is normally filled in by the create_*_block
     routines and is used by the add_*_rune routines. */
  Lisp_Object window;
  struct device *d;
  struct display_block *db;
  struct display_line *dl;
  Emchar ch;		/* Character that is to be added.  This is
			   used to communicate this information to
			   add_emchar_rune(). */
  Lisp_Object last_charset; /* The charset of the previous character.
			       Used to optimize some lookups -- we
			       only have to do some things when
			       the charset changes. */
  face_index last_findex;   /* The face index of the previous character.
			       Needed to ensure the validity of the
			       last_charset optimization. */

  int last_char_width;	/* The width of the previous character. */
  int font_is_bogus;	/* If true, it means we couldn't instantiate
			   the font for this charset, so we substitute
			   ~'s from the ASCII charset. */
  Bytind bi_bufpos;
  Bytind bi_endpos;
  int pixpos;
  int max_pixpos;
  int blank_width;	/* Width of the blank that is to be added.
			   This is used to communicate this information
			   to add_blank_rune().

			   This is also used rather cheesily to
			   communicate the width of the eol-cursor-size
			   blank that exists at the end of the line.
			   add_emchar_rune() is called cheesily with
			   the non-printing char '\n', which is stuck
			   in the output routines with its width being
			   BLANK_WIDTH. */
  Bytind bi_cursor_bufpos;/* This stores the buffer position of the cursor. */
  unsigned int cursor_type :3;
  int cursor_x;		/* rune block cursor is at */
  int start_col;	/* Number of character columns (each column has
			   a width of the default char width) that still
			   need to be skipped.  This is used for horizontal
			   scrolling, where a certain number of columns
			   (those off the left side of the screen) need
			   to be skipped before anything is displayed. */
  Bytind bi_start_col_enabled;

  int hscroll_glyph_width_adjust;  /* how much the width of the hscroll
				      glyph differs from space_width (w).
				      0 if no hscroll glyph was used,
				      i.e. the window is not scrolled
				      horizontally.  Used in tab
				      calculations. */

  /* Information about the face the text should be displayed in and
     any begin-glyphs and end-glyphs. */
  struct extent_fragment *ef;
  face_index findex;

  /* The height of a pixmap may either be predetermined if the user
     has set a baseline value, or it may be dependent on whatever the
     line ascent and descent values end up being, based just on font
     information.  In the first case we can immediately update the
     values, thus their inclusion here.  In the last case we cannot
     determine the actual contribution to the line height until we
     have finished laying out all text on the line.  Thus we propagate
     the max height of such pixmaps and do a final calculation after
     all text has been added to the line. */
  int new_ascent;
  int new_descent;
  int max_pixmap_height;

  Lisp_Object result_str; /* String where we put the result of
			     generating a formatted string in the modeline. */
  int is_modeline; /* Non-zero if we're generating the modeline. */
  Charcount modeline_charpos; /* Number of chars used in result_str so far;
				 corresponds to bytepos. */
  Bytecount bytepos; /* Number of bytes used in result_str so far.
		        We don't actually copy the bytes into result_str
			until the end because we don't know how big the
			string is going to be until then. */
} pos_data;

enum prop_type
{
  PROP_STRING,
  PROP_CHAR,
  PROP_MINIBUF_PROMPT,
  PROP_BLANK
};

/* Data that should be propagated to the next line.  Either a single
   Emchar or a string of Bufbyte's.

   The actual data that is propagated ends up as a Dynarr of these
   blocks.

   #### It's unclean that both Emchars and Bufbytes are here.
   */

typedef struct prop_block prop_block;
struct prop_block
{
  enum prop_type type;

  union data
  {
    struct
    {
      Bufbyte *str;
      Bytecount len; /* length of the string. */
    } p_string;

    struct
    {
      Emchar ch;
      Bytind bi_cursor_bufpos; /* NOTE: is in Bytinds */
      unsigned int cursor_type :3;
    } p_char;

    struct
    {
      int width;
      face_index findex;
    } p_blank;
  } data;
};

typedef struct
{
  Dynarr_declare (prop_block);
} prop_block_dynarr;


static void generate_formatted_string_db (Lisp_Object format_str,
					  Lisp_Object result_str,
					  struct window *w,
					  struct display_line *dl,
					  struct display_block *db,
					  face_index findex, int min_pixpos,
					  int max_pixpos, int type);
static Charcount generate_fstring_runes (struct window *w, pos_data *data,
					 Charcount pos, Charcount min_pos,
					 Charcount max_pos, Lisp_Object elt,
					 int depth, int max_pixsize,
					 face_index findex, int type);
static prop_block_dynarr *add_glyph_rune (pos_data *data,
					  struct glyph_block *gb,
					  int pos_type, int allow_cursor,
					  struct glyph_cachel *cachel);
static Bytind create_text_block (struct window *w, struct display_line *dl,
				 Bytind bi_start_pos, int start_col,
				 prop_block_dynarr **prop,
				 int type);
static int create_overlay_glyph_block (struct window *w,
				       struct display_line *dl);
static void create_left_glyph_block (struct window *w,
				     struct display_line *dl,
				     int overlay_width);
static void create_right_glyph_block (struct window *w,
				      struct display_line *dl);
static void redisplay_windows (Lisp_Object window, int skip_selected);
static void decode_mode_spec (struct window *w, Emchar spec, int type);
static void free_display_line (struct display_line *dl);
static void update_line_start_cache (struct window *w, Bufpos from, Bufpos to,
				     Bufpos point, int no_regen);
static int point_visible (struct window *w, Bufpos point, int type);

/* This used to be 10 but 30 seems to give much better performance. */
#define INIT_MAX_PREEMPTS	30
static int max_preempts;

#define REDISPLAY_PREEMPTION_CHECK					\
((void)									\
 (preempted =								\
  (!disable_preemption &&						\
   ((preemption_count < max_preempts) || !NILP (Vexecuting_macro)) &&	\
   (!INTERACTIVE || detect_input_pending ()))))

/*
 * Redisplay global variables.
 */

/* We need a third set of display structures for the cursor motion
   routines.  We used to just give each window a third set.  However,
   we always fully regenerate the structures when needed so there
   isn't any reason we need more than a single set. */
display_line_dynarr *cmotion_display_lines;

/* Used by generate_formatted_string.  Global because they get used so
   much that the dynamic allocation time adds up. */
Emchar_dynarr *formatted_string_emchar_dynarr;
struct display_line formatted_string_display_line;
/* We store the extents that we need to generate in a Dynarr and then
   frob them all on at the end of generating the string.  We do it
   this way rather than adding them as we generate the string because
   we don't store the text into the resulting string until we're done
   (to avoid having to resize the string multiple times), and we don't
   want to go around adding extents to a string when the extents might
   stretch off the end of the string. */
EXTENT_dynarr *formatted_string_extent_dynarr;
Bytecount_dynarr *formatted_string_extent_start_dynarr;
Bytecount_dynarr *formatted_string_extent_end_dynarr;


/* #### probably temporary */
int cache_adjustment;

/* This holds a string representing the text corresponding to a single
   modeline % spec. */
static Bufbyte_dynarr *mode_spec_bufbyte_string;

int in_display;		/* 1 if in redisplay.  */

int disable_preemption;	/* Used for debugging redisplay and for
			   force-redisplay. */

/* We only allow max_preempts preemptions before we force a redisplay. */
static int preemption_count;

/* Minimum pixel height of clipped bottom display line. */
int vertical_clip;

/* Minimum visible pixel width of clipped glyphs at right margin. */
int horizontal_clip;

/* Set if currently inside update_line_start_cache. */
int updating_line_start_cache;

/* Nonzero means reading single-character input with prompt
   so put cursor on minibuffer after the prompt.  */
int cursor_in_echo_area;
Lisp_Object Qcursor_in_echo_area;

/* Nonzero means truncate lines in all windows less wide than the frame */
int truncate_partial_width_windows;

/* non-nil if a buffer has changed since the last time redisplay completed */
int buffers_changed;
int buffers_changed_set;

/* non-nil if hscroll has changed somewhere or a buffer has been
   narrowed or widened */
int clip_changed;
int clip_changed_set;

/* non-nil if any extent has changed since the last time redisplay completed */
int extents_changed;
int extents_changed_set;

/* non-nil if any face has changed since the last time redisplay completed */
int faces_changed;

/* Nonzero means some frames have been marked as garbaged */
int frame_changed;

/* non-zero if any of the builtin display glyphs (continuation,
   hscroll, control-arrow, etc) is in need of updating
   somewhere. */
int glyphs_changed;
int glyphs_changed_set;

/* non-zero if any displayed subwindow is in need of updating
   somewhere. */
int subwindows_changed;
int subwindows_changed_set;

/* This variable is 1 if the icon has to be updated.
 It is set to 1 when `frame-icon-glyph' changes. */
int icon_changed;
int icon_changed_set;

/* This variable is 1 if the menubar widget has to be updated.
 It is set to 1 by set-menubar-dirty-flag and cleared when the widget
 has been updated. */
int menubar_changed;
int menubar_changed_set;

/* true iff we should redraw the modelines on the next redisplay */
int modeline_changed;
int modeline_changed_set;

/* non-nil if point has changed in some buffer since the last time
   redisplay completed */
int point_changed;
int point_changed_set;

/* non-nil if some frame has changed its size */
int size_changed;

/* non-nil if some device has signaled that it wants to change size */
int asynch_device_change_pending;

/* non-nil if any toolbar has changed */
int toolbar_changed;
int toolbar_changed_set;

/* non-nil if any window has changed since the last time redisplay completed */
int windows_changed;

/* non-nil if any frame's window structure has changed since the last
   time redisplay completed */
int windows_structure_changed;

/* If non-nil, use vertical bar cursor. */
Lisp_Object Vbar_cursor;
Lisp_Object Qbar_cursor;


int visible_bell;	/* If true and the terminal will support it
			   then the frame will flash instead of
			   beeping when an error occurs */

/* Nonzero means no need to redraw the entire frame on resuming
   a suspended Emacs.  This is useful on terminals with multiple pages,
   where one page is used for Emacs and another for all else. */
int no_redraw_on_reenter;

Lisp_Object Vwindow_system;	/* nil or a symbol naming the window system
				   under which emacs is running
				   ('x is the only current possibility) */
Lisp_Object Vinitial_window_system;

Lisp_Object Vglobal_mode_string;

/* The number of lines scroll a window by when point leaves the window; if
  it is <=0 then point is centered in the window */
int scroll_step;

/* Scroll up to this many lines, to bring point back on screen. */
int scroll_conservatively;

/* Marker for where to display an arrow on top of the buffer text.  */
Lisp_Object Voverlay_arrow_position;
/* String to display for the arrow.  */
Lisp_Object Voverlay_arrow_string;

Lisp_Object Vwindow_size_change_functions;
Lisp_Object Qwindow_scroll_functions, Vwindow_scroll_functions;
Lisp_Object Qredisplay_end_trigger_functions, Vredisplay_end_trigger_functions;

#define INHIBIT_REDISPLAY_HOOKS  /* #### Until we've thought about
				    this more. */
#ifndef INHIBIT_REDISPLAY_HOOKS
/* #### Chuck says: I think this needs more thought.
   Think about this for 19.14. */
Lisp_Object Vpre_redisplay_hook, Vpost_redisplay_hook;
Lisp_Object Qpre_redisplay_hook, Qpost_redisplay_hook;
#endif /* INHIBIT_REDISPLAY_HOOKS */

int last_display_warning_tick, display_warning_tick;
Lisp_Object Qdisplay_warning_buffer;
int inhibit_warning_display;

Lisp_Object Vleft_margin_width, Vright_margin_width;
Lisp_Object Vminimum_line_ascent, Vminimum_line_descent;
Lisp_Object Vuse_left_overflow, Vuse_right_overflow;
Lisp_Object Vtext_cursor_visible_p;

int column_number_start_at_one;

/***************************************************************************/
/*									   */
/*              low-level interfaces onto device routines                  */
/*									   */
/***************************************************************************/

static int
redisplay_text_width_emchar_string (struct window *w, int findex,
				    Emchar *str, Charcount len)
{
  unsigned char charsets[NUM_LEADING_BYTES];
  Lisp_Object window;

  find_charsets_in_emchar_string (charsets, str, len);
  XSETWINDOW (window, w);
  ensure_face_cachel_complete (WINDOW_FACE_CACHEL (w, findex), window,
			       charsets);
  return DEVMETH (XDEVICE (FRAME_DEVICE (XFRAME (WINDOW_FRAME (w)))),
		  text_width, (XFRAME (WINDOW_FRAME (w)),
			       WINDOW_FACE_CACHEL (w, findex), str, len));
}

static Emchar_dynarr *rtw_emchar_dynarr;

int
redisplay_text_width_string (struct window *w, int findex,
			     Bufbyte *nonreloc, Lisp_Object reloc,
			     Bytecount offset, Bytecount len)
{
  if (!rtw_emchar_dynarr)
    rtw_emchar_dynarr = Dynarr_new (Emchar);
  Dynarr_reset (rtw_emchar_dynarr);

  fixup_internal_substring (nonreloc, reloc, offset, &len);
  if (STRINGP (reloc))
    nonreloc = XSTRING_DATA (reloc);
  convert_bufbyte_string_into_emchar_dynarr (nonreloc, len, rtw_emchar_dynarr);
  return redisplay_text_width_emchar_string
    (w, findex, Dynarr_atp (rtw_emchar_dynarr, 0),
     Dynarr_length (rtw_emchar_dynarr));
}

int
redisplay_frame_text_width_string (struct frame *f, Lisp_Object face,
				   Bufbyte *nonreloc, Lisp_Object reloc,
				   Bytecount offset, Bytecount len)
{
  unsigned char charsets[NUM_LEADING_BYTES];
  Lisp_Object frame;
  struct face_cachel cachel;

  if (!rtw_emchar_dynarr)
    rtw_emchar_dynarr = Dynarr_new (Emchar);
  Dynarr_reset (rtw_emchar_dynarr);

  fixup_internal_substring (nonreloc, reloc, offset, &len);
  if (STRINGP (reloc))
    nonreloc = XSTRING_DATA (reloc);
  convert_bufbyte_string_into_emchar_dynarr (nonreloc, len, rtw_emchar_dynarr);
  find_charsets_in_bufbyte_string (charsets, nonreloc, len);
  reset_face_cachel (&cachel);
  cachel.face = face;
  XSETFRAME (frame, f);
  ensure_face_cachel_complete (&cachel, frame, charsets);
  return DEVMETH (XDEVICE (FRAME_DEVICE (f)),
		  text_width, (f, &cachel, Dynarr_atp (rtw_emchar_dynarr, 0),
			       Dynarr_length (rtw_emchar_dynarr)));
}

/* Return the display block from DL of the given TYPE.  A display line
   can have only one display block of each possible type.  If DL does
   not have a block of type TYPE, one will be created and added to DL. */

struct display_block *
get_display_block_from_line (struct display_line *dl, enum display_type type)
{
  int elt;
  struct display_block db;

  /* Check if this display line already has a block of the desired type and
     if so, return it. */
  if (dl->display_blocks)
    {
      for (elt = 0; elt < Dynarr_length (dl->display_blocks); elt++)
	{
	  if (Dynarr_at (dl->display_blocks, elt).type == type)
	    return Dynarr_atp (dl->display_blocks, elt);
	}

      /* There isn't an active block of the desired type, but there
         might still be allocated blocks we need to reuse. */
      if (elt < Dynarr_largest (dl->display_blocks))
	{
	  struct display_block *dbp = Dynarr_atp (dl->display_blocks, elt);

	  /* 'add' the block to the list */
	  Dynarr_increment (dl->display_blocks);

	  /* initialize and return */
	  dbp->type = type;
	  return dbp;
	}
    }
  else
    {
      /* This line doesn't have any display blocks, so initialize the display
         bock array. */
      dl->display_blocks = Dynarr_new (display_block);
    }

  /* The line doesn't have a block of the desired type so go ahead and create
     one and add it to the line. */
  xzero (db);
  db.type = type;
  db.runes = Dynarr_new (rune);
  Dynarr_add (dl->display_blocks, db);

  /* Return the newly added display block. */
  elt = Dynarr_length (dl->display_blocks) - 1;

  return Dynarr_atp (dl->display_blocks, elt);
}

static int
tab_char_width (struct window *w)
{
  struct buffer *b = XBUFFER (w->buffer);
  int char_tab_width = XINT (b->tab_width);

  if (char_tab_width <= 0 || char_tab_width > 1000) char_tab_width = 8;

  return char_tab_width;
}

static int
space_width (struct window *w)
{
  /* While tabs are traditional composed of spaces, for variable-width
     fonts the space character tends to give too narrow a value.  So
     we use 'n' instead.  Except that we don't.  We use the default
     character width for the default face.  If this is actually
     defined by the font then it is probably the best thing to
     actually use.  If it isn't, we have assumed it is 'n' and have
     already calculated its width.  Thus we can avoid a call to
     XTextWidth on X frames by just querying the default width. */
  return XFONT_INSTANCE
    (WINDOW_FACE_CACHEL_FONT (w, DEFAULT_INDEX, Vcharset_ascii))->width;
}

static int
tab_pix_width (struct window *w)
{
  return space_width (w) * tab_char_width (w);
}

/* Given a pixel position in a window, return the pixel location of
   the next tabstop.  Tabs are calculated from the left window edge in
   terms of spaces displayed in the default face.  Formerly the space
   width was determined using the currently active face.  That method
   leads to tabstops which do not line up. */

static int
next_tab_position (struct window *w, int start_pixpos, int left_pixpos)
{
  int n_pos = left_pixpos;
  int pix_tab_width = tab_pix_width (w);

  /* Adjust n_pos for any hscrolling which has happened. */
  if (w->hscroll > 1)
    n_pos -= space_width (w) * (w->hscroll - 1);

  while (n_pos <= start_pixpos)
    n_pos += pix_tab_width;

  return n_pos;
}

/* For the given window, calculate the outside and margin boundaries for a
   display line.  The whitespace boundaries must be calculated by the text
   layout routines. */

layout_bounds
calculate_display_line_boundaries (struct window *w, int modeline)
{
  layout_bounds bounds;

  /* Set the outermost boundaries which are the boundaries of the
     window itself minus the gutters (and minus the scrollbars if this
     is for the modeline). */
  if (!modeline)
    {
      bounds.left_out = WINDOW_TEXT_LEFT (w);
      bounds.right_out = WINDOW_TEXT_RIGHT (w);
    }
  else
    {
      bounds.left_out = WINDOW_MODELINE_LEFT (w);
      bounds.right_out = WINDOW_MODELINE_RIGHT (w);
    }

  /* The inner boundaries mark where the glyph margins are located. */
  bounds.left_in = bounds.left_out + window_left_margin_width (w);
  bounds.right_in = bounds.right_out - window_right_margin_width (w);

  /* We cannot fully calculate the whitespace boundaries as they
     depend on the contents of the line being displayed. */
  bounds.left_white = bounds.left_in;
  bounds.right_white = bounds.right_in;

  return bounds;
}

/* Given a display line and a starting position, ensure that the
   contents of the display line accurately represent the visual
   representation of the buffer contents starting from the given
   position when displayed in the given window.  The display line ends
   when the contents of the line reach the right boundary of the given
   window. */

static Bufpos
generate_display_line (struct window *w, struct display_line *dl, int bounds,
		       Bufpos start_pos, int start_col,
		       prop_block_dynarr **prop,
		       int type)
{
  Bufpos ret_bufpos;
  int overlay_width;
  struct buffer *b = XBUFFER (WINDOW_BUFFER (w));

  /* If our caller hasn't already set the boundaries, then do so now. */
  if (!bounds)
    dl->bounds = calculate_display_line_boundaries (w, 0);

  /* Reset what this line is using. */
  if (dl->display_blocks)
    Dynarr_reset (dl->display_blocks);
  if (dl->left_glyphs)
    {
      Dynarr_free (dl->left_glyphs);
      dl->left_glyphs = 0;
    }
  if (dl->right_glyphs)
    {
      Dynarr_free (dl->right_glyphs);
      dl->right_glyphs = 0;
    }

  /* We aren't generating a modeline at the moment. */
  dl->modeline = 0;

  /* Create a display block for the text region of the line. */
  {
    /* #### urk urk urk!!! Chuck fix this shit! */
    Bytind hacked_up_bytind =
      create_text_block (w, dl, bufpos_to_bytind (b, start_pos),
			 start_col, prop, type);
    if (hacked_up_bytind > BI_BUF_ZV (b))
      ret_bufpos = BUF_ZV (b) + 1;
    else
      ret_bufpos = bytind_to_bufpos (b, hacked_up_bytind);
  }
  dl->bufpos = start_pos;
  if (dl->end_bufpos < dl->bufpos)
    dl->end_bufpos = dl->bufpos;

  if (MARKERP (Voverlay_arrow_position)
      && EQ (w->buffer, Fmarker_buffer (Voverlay_arrow_position))
      && start_pos == marker_position (Voverlay_arrow_position)
      && (STRINGP (Voverlay_arrow_string)
	  || GLYPHP (Voverlay_arrow_string)))
    {
      overlay_width = create_overlay_glyph_block (w, dl);
    }
  else
    overlay_width = 0;

  /* If there are left glyphs associated with any character in the
     text block, then create a display block to handle them. */
  if (dl->left_glyphs != NULL && Dynarr_length (dl->left_glyphs))
    create_left_glyph_block (w, dl, overlay_width);

  /* If there are right glyphs associated with any character in the
     text block, then create a display block to handle them. */
  if (dl->right_glyphs != NULL && Dynarr_length (dl->right_glyphs))
    create_right_glyph_block (w, dl);

  /* In the future additional types of display blocks may be generated
     here. */

  w->last_redisplay_pos = ret_bufpos;

  return ret_bufpos;
}

/* Adds an hscroll glyph to a display block.  If this is called, then
   the block had better be empty.

   Yes, there are multiple places where this function is called but
   that is the way it has to be.  Each calling function has to deal
   with bi_start_col_enabled a little differently depending on the
   object being worked with. */

static prop_block_dynarr *
add_hscroll_rune (pos_data *data)
{
  struct glyph_block gb;
  prop_block_dynarr *retval;
  Bytind bi_old_cursor_bufpos = data->bi_cursor_bufpos;
  unsigned int old_cursor_type = data->cursor_type;
  Bytind bi_old_bufpos = data->bi_bufpos;

  if (data->cursor_type == CURSOR_ON
      && data->bi_cursor_bufpos >= data->bi_start_col_enabled
      && data->bi_cursor_bufpos <= data->bi_bufpos)
    {
      data->bi_cursor_bufpos = data->bi_start_col_enabled;
    }
  else
    {
      data->cursor_type = NO_CURSOR;
    }

  data->bi_endpos = data->bi_bufpos;
  data->bi_bufpos = data->bi_start_col_enabled;

  gb.extent = Qnil;
  gb.glyph = Vhscroll_glyph;
  {
    int oldpixpos = data->pixpos;
    retval = add_glyph_rune (data, &gb, BEGIN_GLYPHS, 1,
			     GLYPH_CACHEL (XWINDOW (data->window),
					   HSCROLL_GLYPH_INDEX));
    data->hscroll_glyph_width_adjust =
      data->pixpos - oldpixpos - space_width (XWINDOW (data->window));
  }
  data->bi_endpos = 0;
  data->bi_cursor_bufpos = bi_old_cursor_bufpos;
  data->cursor_type = old_cursor_type;
  data->bi_bufpos = bi_old_bufpos;

  data->bi_start_col_enabled = 0;
  return retval;
}

/* Adds a character rune to a display block.  If there is not enough
   room to fit the rune on the display block (as determined by the
   MAX_PIXPOS) then it adds nothing and returns ADD_FAILED. */

static prop_block_dynarr *
add_emchar_rune (pos_data *data)
{
  struct rune rb, *crb;
  int width, local;

  if (data->start_col)
    {
      data->start_col--;

      if (data->start_col)
	return NULL;
    }

  if (data->bi_start_col_enabled)
    {
      return add_hscroll_rune (data);
    }

  if (data->ch == '\n')
    {
      data->font_is_bogus = 0;
      /* Cheesy end-of-line pseudo-character. */
      width = data->blank_width;
    }
  else
    {
      Lisp_Object charset = CHAR_CHARSET (data->ch);
      if (!EQ (charset, data->last_charset) ||
	  data->findex != data->last_findex)
	{
	  /* OK, we need to do things the hard way. */
	  struct window *w = XWINDOW (data->window);
	  struct face_cachel *cachel = WINDOW_FACE_CACHEL (w, data->findex);
	  Lisp_Object font_instance =
	    ensure_face_cachel_contains_charset (cachel, data->window,
						 charset);
	  struct Lisp_Font_Instance *fi;

	  if (EQ (font_instance, Vthe_null_font_instance))
	    {
	      font_instance = FACE_CACHEL_FONT (cachel, Vcharset_ascii);
	      data->font_is_bogus = 1;
	    }
	  else
	    data->font_is_bogus = 0;

	  fi = XFONT_INSTANCE (font_instance);
	  if (!fi->proportional_p)
	    /* sweetness and light. */
	    data->last_char_width = fi->width;
	  else
	    data->last_char_width = -1;
	  data->new_ascent  = max (data->new_ascent,  (int) fi->ascent);
	  data->new_descent = max (data->new_descent, (int) fi->descent);
	  data->last_charset = charset;
	  data->last_findex = data->findex;
	}

      width = data->last_char_width;
      if (width < 0)
	{
	  /* bummer.  Proportional fonts. */
	  width = redisplay_text_width_emchar_string (XWINDOW (data->window),
						      data->findex,
						      &data->ch, 1);
	}
    }

  if (data->max_pixpos != -1 && (data->pixpos + width > data->max_pixpos))
    {
      return ADD_FAILED;
    }

  if (Dynarr_length (data->db->runes) < Dynarr_largest (data->db->runes))
    {
      crb = Dynarr_atp (data->db->runes, Dynarr_length (data->db->runes));
      local = 0;
    }
  else
    {
      crb = &rb;
      local = 1;
    }

  crb->findex = data->findex;
  crb->xpos = data->pixpos;
  crb->width = width;
  if (data->bi_bufpos)
    crb->bufpos =
      bytind_to_bufpos (XBUFFER (WINDOW_BUFFER (XWINDOW (data->window))),
			data->bi_bufpos);
  else if (data->is_modeline)
    crb->bufpos = data->modeline_charpos;
  else
    /* fuckme if this shouldn't be an abort. */
    /* abort (); fuckme harder, this abort gets tripped quite often,
		 in propagation and whatnot.  #### fixme */
    crb->bufpos = 0;
  crb->type = RUNE_CHAR;
  crb->object.chr.ch = data->font_is_bogus ? '~' : data->ch;
  crb->endpos = 0;

  if (data->cursor_type == CURSOR_ON)
    {
      if (data->bi_bufpos == data->bi_cursor_bufpos)
	{
	  crb->cursor_type = CURSOR_ON;
	  data->cursor_x = Dynarr_length (data->db->runes);
	}
      else
	crb->cursor_type = CURSOR_OFF;
    }
  else if (data->cursor_type == NEXT_CURSOR)
    {
      crb->cursor_type = CURSOR_ON;
      data->cursor_x = Dynarr_length (data->db->runes);
      data->cursor_type = NO_CURSOR;
    }
  else if (data->cursor_type == IGNORE_CURSOR)
    crb->cursor_type = IGNORE_CURSOR;
  else
    crb->cursor_type = CURSOR_OFF;

  if (local)
    Dynarr_add (data->db->runes, *crb);
  else
    Dynarr_increment (data->db->runes);

  data->pixpos += width;

  return NULL;
}

/* Given a string C_STRING of length C_LENGTH, call add_emchar_rune
   for each character in the string.  Propagate any left-over data
   unless NO_PROP is non-zero. */

static prop_block_dynarr *
add_bufbyte_string_runes (pos_data *data, Bufbyte *c_string,
			  Bytecount c_length, int no_prop)
{
  Bufbyte *pos, *end = c_string + c_length;
  prop_block_dynarr *prop;

  /* #### This function is too simplistic.  It needs to do the same
     sort of character interpretation (display-table lookup,
     ctl-arrow checking), etc. that create_text_block() does.
     The functionality to do this in that routine needs to be
     modularized. */

  for (pos = c_string; pos < end;)
    {
      data->ch = charptr_emchar (pos);

      prop = add_emchar_rune (data);

      if (prop)
	{
	  if (no_prop)
	    return ADD_FAILED;
	  else
	    {
	      struct prop_block pb;
	      Bytecount len = end - pos;
	      prop = Dynarr_new (prop_block);

	      pb.type = PROP_STRING;
	      pb.data.p_string.str = xnew_array (Bufbyte, len);
	      strncpy ((char *) pb.data.p_string.str, (char *) pos, len);
	      pb.data.p_string.len = len;

	      Dynarr_add (prop, pb);
	      return prop;
	    }
	}
      INC_CHARPTR (pos);
      assert (pos <= end);
    }

  return NULL;
}

/* Add a single rune of the specified width.  The area covered by this
   rune will be displayed in the foreground color of the associated
   face. */

static prop_block_dynarr *
add_blank_rune (pos_data *data, struct window *w, int char_tab_width)
{
  struct rune rb;

  /* If data->start_col is not 0 then this call to add_blank_rune must have
     been to add it as a tab. */
  if (data->start_col)
    {
      /* assert (w != NULL) */
      prop_block_dynarr *retval;

      /* If we have still not fully scrolled horizontally, subtract
         the width of this tab and return. */
      if (char_tab_width < data->start_col)
	{
	  data->start_col -= char_tab_width;
	  return NULL;
	}
      else if (char_tab_width == data->start_col)
	data->blank_width = 0;
      else
	{
	  int spcwid = space_width (w);

	  if (spcwid >= data->blank_width)
	    data->blank_width = 0;
	  else
	    data->blank_width -= spcwid;
	}

      data->start_col = 0;
      retval = add_hscroll_rune (data);

      /* Could be caused by the handling of the hscroll rune. */
      if (retval != NULL || !data->blank_width)
	return retval;
    }

  /* Blank runes are always calculated to fit. */
  assert (data->pixpos + data->blank_width <= data->max_pixpos);

  rb.findex = data->findex;
  rb.xpos = data->pixpos;
  rb.width = data->blank_width;
  if (data->bi_bufpos)
    rb.bufpos =
      bytind_to_bufpos (XBUFFER (WINDOW_BUFFER (XWINDOW (data->window))),
			data->bi_bufpos);
  else
    /* #### and this is really correct too? */
    rb.bufpos = 0;
  rb.endpos = 0;
  rb.type = RUNE_BLANK;

  if (data->cursor_type == CURSOR_ON)
    {
      if (data->bi_bufpos == data->bi_cursor_bufpos)
	{
	  rb.cursor_type = CURSOR_ON;
	  data->cursor_x = Dynarr_length (data->db->runes);
	}
      else
	rb.cursor_type = CURSOR_OFF;
    }
  else if (data->cursor_type == NEXT_CURSOR)
    {
      rb.cursor_type = CURSOR_ON;
      data->cursor_x = Dynarr_length (data->db->runes);
      data->cursor_type = NO_CURSOR;
    }
  else
    rb.cursor_type = CURSOR_OFF;

  Dynarr_add (data->db->runes, rb);
  data->pixpos += data->blank_width;

  return NULL;
}

/* Add runes representing a character in octal. */

#define ADD_NEXT_OCTAL_RUNE_CHAR do				\
{								\
  if (add_failed || (add_failed = add_emchar_rune (data)))	\
    {								\
      struct prop_block pb;					\
      if (!prop)						\
	prop = Dynarr_new (prop_block);				\
								\
      pb.type = PROP_CHAR;					\
      pb.data.p_char.ch = data->ch;				\
      pb.data.p_char.cursor_type = data->cursor_type;		\
      Dynarr_add (prop, pb);					\
    }								\
} while (0)

static prop_block_dynarr *
add_octal_runes (pos_data *data)
{
  prop_block_dynarr *prop, *add_failed;
  Emchar orig_char = data->ch;
  unsigned int orig_cursor_type = data->cursor_type;

  /* Initialize */
  prop = NULL;
  add_failed = NULL;

  if (data->start_col)
    data->start_col--;

  if (!data->start_col)
    {
    if (data->bi_start_col_enabled)
      {
	add_failed = add_hscroll_rune (data);
      }
    else
      {
	struct glyph_block gb;
	struct window *w = XWINDOW (data->window);

	gb.extent = Qnil;
	gb.glyph = Voctal_escape_glyph;
	add_failed =
	  add_glyph_rune (data, &gb, BEGIN_GLYPHS, 1,
			  GLYPH_CACHEL (w, OCT_ESC_GLYPH_INDEX));
      }
    }

  /* We only propagate information if the glyph was partially
     added. */
  if (add_failed)
    return add_failed;

  data->cursor_type = IGNORE_CURSOR;

  if (data->ch >= 0x100)
    {
      /* If the character is an extended Mule character, it could have
	 up to 19 bits.  For the moment, we treat it as a seven-digit
	 octal number.  This is not that pretty, but whatever. */
      data->ch = (7 & (orig_char >> 18)) + '0';
      ADD_NEXT_OCTAL_RUNE_CHAR;

      data->ch = (7 & (orig_char >> 15)) + '0';
      ADD_NEXT_OCTAL_RUNE_CHAR;

      data->ch = (7 & (orig_char >> 12)) + '0';
      ADD_NEXT_OCTAL_RUNE_CHAR;

      data->ch = (7 & (orig_char >> 9)) + '0';
      ADD_NEXT_OCTAL_RUNE_CHAR;
    }

  data->ch = (7 & (orig_char >> 6)) + '0';
  ADD_NEXT_OCTAL_RUNE_CHAR;

  data->ch = (7 & (orig_char >> 3)) + '0';
  ADD_NEXT_OCTAL_RUNE_CHAR;

  data->ch = (7 & orig_char) + '0';
  ADD_NEXT_OCTAL_RUNE_CHAR;

  data->cursor_type = orig_cursor_type;
  return prop;
}

#undef ADD_NEXT_OCTAL_RUNE_CHAR

/* Add runes representing a control character to a display block. */

static prop_block_dynarr *
add_control_char_runes (pos_data *data, struct buffer *b)
{
  if (!NILP (b->ctl_arrow))
    {
      prop_block_dynarr *prop;
      Emchar orig_char = data->ch;
      unsigned int old_cursor_type = data->cursor_type;

      /* Initialize */
      prop = NULL;

      if (data->start_col)
	data->start_col--;

      if (!data->start_col)
	{
	  if (data->bi_start_col_enabled)
	    {
	      prop_block_dynarr *retval;

	      retval = add_hscroll_rune (data);
	      if (retval)
		return retval;
	    }
	  else
	    {
	      struct glyph_block gb;
	      struct window *w = XWINDOW (data->window);

	      gb.extent = Qnil;
	      gb.glyph = Vcontrol_arrow_glyph;

	      /* We only propagate information if the glyph was partially
		 added. */
	      if (add_glyph_rune (data, &gb, BEGIN_GLYPHS, 1,
				  GLYPH_CACHEL (w, CONTROL_GLYPH_INDEX)))
		return ADD_FAILED;
	    }
	}

      if (orig_char == 0177)
	data->ch = '?';
      else
	data->ch = orig_char ^ 0100;
      data->cursor_type = IGNORE_CURSOR;

      if (add_emchar_rune (data))
	{
	  struct prop_block pb;
	  if (!prop)
	    prop = Dynarr_new (prop_block);

	  pb.type = PROP_CHAR;
	  pb.data.p_char.ch = data->ch;
	  pb.data.p_char.cursor_type = data->cursor_type;
	  Dynarr_add (prop, pb);
	}

      data->cursor_type = old_cursor_type;
      return prop;
    }
  else
    {
      return add_octal_runes (data);
    }
}

static prop_block_dynarr *
add_disp_table_entry_runes_1 (pos_data *data, Lisp_Object entry)
{
  prop_block_dynarr *prop = NULL;

  if (STRINGP (entry))
    {
      prop = add_bufbyte_string_runes (data,
				       XSTRING_DATA   (entry),
				       XSTRING_LENGTH (entry),
				       0);
    }
  else if (GLYPHP (entry))
    {
      if (data->start_col)
	data->start_col--;

      if (!data->start_col && data->bi_start_col_enabled)
	{
	  prop = add_hscroll_rune (data);
	}
      else
	{
	  struct glyph_block gb;

	  gb.glyph = entry;
	  gb.extent = Qnil;
	  prop = add_glyph_rune (data, &gb, BEGIN_GLYPHS, 0, 0);
	}
    }
  else if (CHAR_OR_CHAR_INTP (entry))
    {
      data->ch = XCHAR_OR_CHAR_INT (entry);
      prop = add_emchar_rune (data);
    }
  else if (CONSP (entry))
    {
      if (EQ (XCAR (entry), Qformat)
	  && CONSP (XCDR (entry))
	  && STRINGP (XCAR (XCDR (entry))))
	{
	  Lisp_Object format = XCAR (XCDR (entry));
	  Bytind len = XSTRING_LENGTH (format);
	  Bufbyte *src = XSTRING_DATA (format), *end = src + len;
	  Bufbyte *result = alloca_array (Bufbyte, len);
	  Bufbyte *dst = result;

	  while (src < end)
	    {
	      Emchar c = charptr_emchar (src);
	      INC_CHARPTR (src);
	      if (c != '%' || src == end)
		dst += set_charptr_emchar (dst, c);
	      else
		{
		  c = charptr_emchar (src);
		  INC_CHARPTR (src);
		  switch (c)
		    {
		      /*case 'x':
		      dst += long_to_string_base ((char *)dst, data->ch, 16);
		      break;*/
		    case '%':
		      dst += set_charptr_emchar (dst, '%');
		      break;
		    }
		}
	    }
	  prop = add_bufbyte_string_runes (data, result, dst - result, 0);
	}
    }

  /* Else blow it off because someone added a bad entry and we don't
     have any safe way of signaling an error. */
  return prop;
}

/* Given a display table entry, call the appropriate functions to
   display each element of the entry. */

static prop_block_dynarr *
add_disp_table_entry_runes (pos_data *data, Lisp_Object entry)
{
  prop_block_dynarr *prop = NULL;
  if (VECTORP (entry))
    {
      struct Lisp_Vector *de = XVECTOR (entry);
      EMACS_INT len = vector_length (de);
      int elt;

      for (elt = 0; elt < len; elt++)
	{
	  if (NILP (vector_data (de)[elt]))
	    continue;
	  else
	    prop = add_disp_table_entry_runes_1 (data, vector_data (de)[elt]);
	  /* Else blow it off because someone added a bad entry and we
	     don't have any safe way of signaling an error.  Hey, this
	     comment sounds familiar. */

	  /* #### Still need to add any remaining elements to the
             propagation information. */
	  if (prop)
	    return prop;
	}
    }
  else
    prop = add_disp_table_entry_runes_1 (data, entry);
  return prop;
}

/* Add runes which were propagated from the previous line. */

static prop_block_dynarr *
add_propagation_runes (prop_block_dynarr **prop, pos_data *data)
{
  /* #### Remember to handle start_col parameter of data when the rest of
     this is finished. */
  /* #### Chuck -- I've redone this function a bit.  It looked like the
     case of not all the propagation blocks being added was not handled
     well. */
  /* #### Chuck -- I also think the double indirection of PROP is kind
     of bogus.  A cleaner solution is just to check for
     Dynarr_length (prop) > 0. */
  /* #### This function also doesn't even pay attention to ADD_FAILED!
     This is seriously fucked!  Seven ####'s in 130 lines -- is that a
     record? */
  int elt;
  prop_block_dynarr *add_failed;
  Bytind bi_old_cursor_bufpos = data->bi_cursor_bufpos;
  unsigned int old_cursor_type = data->cursor_type;

  for (elt = 0; elt < Dynarr_length (*prop); elt++)
    {
      struct prop_block *pb = Dynarr_atp (*prop, elt);

      switch (pb->type)
	{
	case PROP_CHAR:
	  data->ch = pb->data.p_char.ch;
	  data->bi_cursor_bufpos = pb->data.p_char.bi_cursor_bufpos;
	  data->cursor_type = pb->data.p_char.cursor_type;
	  add_failed = add_emchar_rune (data);

	  if (add_failed)
	    goto oops_no_more_space;
	  break;
	case PROP_STRING:
	  if (pb->data.p_string.str)
	    xfree (pb->data.p_string.str);
	  /* #### bogus bogus -- this doesn't do anything!
	     Should probably call add_bufbyte_string_runes(),
	     once that function is fixed. */
	  break;
	case PROP_MINIBUF_PROMPT:
	  {
	    face_index old_findex = data->findex;
	    Bytind bi_old_bufpos = data->bi_bufpos;

	    data->findex = DEFAULT_INDEX;
	    data->bi_bufpos = 0;
	    data->cursor_type = NO_CURSOR;

	    while (pb->data.p_string.len > 0)
	      {
		data->ch = charptr_emchar (pb->data.p_string.str);
		add_failed = add_emchar_rune (data);

		if (add_failed)
		  {
		    data->findex = old_findex;
		    data->bi_bufpos = bi_old_bufpos;
		    goto oops_no_more_space;
		  }
		else
		  {
		    /* Complicated equivalent of ptr++, len-- */
		    Bufbyte *oldpos = pb->data.p_string.str;
		    INC_CHARPTR (pb->data.p_string.str);
		    pb->data.p_string.len -= pb->data.p_string.str - oldpos;
		  }
	      }

	    data->findex = old_findex;
	    /* ##### FIXME FIXME FIXME -- Upon successful return from
	       this function, data->bi_bufpos is automatically incremented.
	       However, we don't want that to happen if we were adding
	       the minibuffer prompt. */
	    {
	      struct buffer *buf =
		XBUFFER (WINDOW_BUFFER (XWINDOW (data->window)));
	      /* #### Chuck fix this shit or I'm gonna scream! */
	      if (bi_old_bufpos > BI_BUF_BEGV (buf))
	        data->bi_bufpos = prev_bytind (buf, bi_old_bufpos);
              else
		/* #### is this correct?  Does anyone know?
		   Does anyone care? Is this a cheesy hack or what? */
	        data->bi_bufpos = BI_BUF_BEGV (buf) - 1;
	    }
	  }
	  break;
	case PROP_BLANK:
	  {
	    /* #### I think it's unnecessary and misleading to preserve
	       the blank_width, as it implies that the value carries
	       over from one rune to the next, which is wrong. */
	    int old_width = data->blank_width;
	    face_index old_findex = data->findex;

	    data->findex = pb->data.p_blank.findex;
	    data->blank_width = pb->data.p_blank.width;
	    data->bi_cursor_bufpos = 0;
	    data->cursor_type = IGNORE_CURSOR;

	    if (data->pixpos + data->blank_width > data->max_pixpos)
	      data->blank_width = data->max_pixpos - data->pixpos;

	    /* We pass a bogus value of char_tab_width.  It shouldn't
               matter because unless something is really screwed up
               this call won't cause that arg to be used. */
	    add_failed = add_blank_rune (data, XWINDOW (data->window), 0);

	    /* This can happen in the case where we have a tab which
               is wider than the window. */
	    if (data->blank_width != pb->data.p_blank.width)
	      {
		pb->data.p_blank.width -= data->blank_width;
		add_failed = ADD_FAILED;
	      }

	    data->findex = old_findex;
	    data->blank_width = old_width;

	    if (add_failed)
	      goto oops_no_more_space;
	  }
	  break;
	default:
	  abort ();
	}
    }

 oops_no_more_space:

  data->bi_cursor_bufpos = bi_old_cursor_bufpos;
  data->cursor_type = old_cursor_type;
  if (elt < Dynarr_length (*prop))
    {
      Dynarr_delete_many (*prop, 0, elt);
      return *prop;
    }
  else
    {
      Dynarr_free (*prop);
      return NULL;
    }
}

/* Add 'text layout glyphs at position POS_TYPE that are contained to
   the display block, but add all other types to the appropriate list
   of the display line.  They will be added later by different
   routines. */

static prop_block_dynarr *
add_glyph_rune (pos_data *data, struct glyph_block *gb, int pos_type,
		int allow_cursor, struct glyph_cachel *cachel)
{
  struct window *w = XWINDOW (data->window);

  /* A nil extent indicates a special glyph (ex. truncator). */
  if (NILP (gb->extent)
      || (pos_type == BEGIN_GLYPHS &&
	  extent_begin_glyph_layout (XEXTENT (gb->extent)) == GL_TEXT)
      || (pos_type == END_GLYPHS &&
	  extent_end_glyph_layout (XEXTENT (gb->extent)) == GL_TEXT))
    {
      struct rune rb;
      int width;
      int xoffset = 0;
      int ascent, descent;
      Lisp_Object baseline;
      Lisp_Object face;

      if (cachel)
	width = cachel->width;
      else
	width = glyph_width (gb->glyph, Qnil, data->findex, data->window);

      if (!width)
	return NULL;

      if (data->start_col)
	{
	  prop_block_dynarr *retval;
	  int glyph_char_width = width / space_width (w);

	  /* If we still have not fully scrolled horizontally after
             taking into account the width of the glyph, subtract its
             width and return. */
	  if (glyph_char_width < data->start_col)
	    {
	      data->start_col -= glyph_char_width;
	      return NULL;
	    }
	  else if (glyph_char_width == data->start_col)
	    width = 0;
	  else
	    {
	      xoffset = space_width (w) * data->start_col;
	      width -= xoffset;

	      /* #### Can this happen? */
	      if (width < 0)
		width = 0;
	    }

	  data->start_col = 0;
	  retval = add_hscroll_rune (data);

	  /* Could be caused by the handling of the hscroll rune. */
	  if (retval != NULL || !width)
	    return retval;
	}
      else
	xoffset = 0;

      if (data->pixpos + width > data->max_pixpos)
	{
	  /* If this is the first object we are attempting to add to
             the line then we ignore the horizontal_clip threshold.
             Otherwise we will loop until the bottom of the window
             continually failing to add this glyph because it is wider
             than the window.  We could alternatively just completely
             ignore the glyph and proceed from there but I think that
             this is a better solution. */
	  if (Dynarr_length (data->db->runes)
	      && data->max_pixpos - data->pixpos < horizontal_clip)
	    return ADD_FAILED;
	  else
	    width = data->max_pixpos - data->pixpos;
	}

      if (cachel)
	{
	  ascent = cachel->ascent;
	  descent = cachel->descent;
	}
      else
	{
	  ascent = glyph_ascent (gb->glyph, Qnil, data->findex, data->window);
	  descent = glyph_descent (gb->glyph, Qnil, data->findex,
				   data->window);
	}

      baseline = glyph_baseline (gb->glyph, data->window);

      if (glyph_contrib_p (gb->glyph, data->window))
	{
	  /* A pixmap that has not had a baseline explicitly set.  Its
	     contribution will be determined later. */
	  if (NILP (baseline))
	    {
	      int height = ascent + descent;
	      data->max_pixmap_height = max (data->max_pixmap_height, height);
	    }

	  /* A string so determine contribution normally. */
	  else if (EQ (baseline, Qt))
	    {
	      data->new_ascent = max (data->new_ascent, ascent);
	      data->new_descent = max (data->new_descent, descent);
	    }

	  /* A pixmap with an explicitly set baseline.  We determine the
	     contribution here. */
	  else if (INTP (baseline))
	    {
	      int height = ascent + descent;
	      int pix_ascent, pix_descent;

	      pix_ascent = height * XINT (baseline) / 100;
	      pix_descent = height - pix_ascent;

	      data->new_ascent = max (data->new_ascent, pix_ascent);
	      data->new_descent = max (data->new_descent, pix_descent);
	    }

	  /* Otherwise something is screwed up. */
	  else
	    abort ();
	}

      face = glyph_face (gb->glyph, data->window);
      if (NILP (face))
	rb.findex = data->findex;
      else
	rb.findex = get_builtin_face_cache_index (w, face);

      rb.xpos = data->pixpos;
      rb.width = width;
      rb.bufpos = 0;			/* glyphs are never "at" anywhere */
      if (data->bi_endpos)
	/* #### is this necessary at all? */
	rb.endpos = bytind_to_bufpos (XBUFFER (WINDOW_BUFFER (w)),
				      data->bi_endpos);
      else
        rb.endpos = 0;
      rb.type = RUNE_DGLYPH;
      /* #### Ben sez: this is way bogus if the glyph is a string.
	 You should not make the output routines have to cope with
	 this.  The string could contain Mule characters, or non-
	 printable characters, or characters to be passed through
	 the display table, or non-character objects (when this gets
	 implemented), etc.  Instead, this routine here should parse
	 the string into a series of runes. */
      rb.object.dglyph.glyph = gb->glyph;
      rb.object.dglyph.extent = gb->extent;
      rb.object.dglyph.xoffset = xoffset;

      if (allow_cursor)
	{
	  rb.bufpos = bytind_to_bufpos (XBUFFER (WINDOW_BUFFER (w)),
					data->bi_bufpos);

	  if (data->cursor_type == CURSOR_ON)
	    {
	      if (data->bi_bufpos == data->bi_cursor_bufpos)
		{
		  rb.cursor_type = CURSOR_ON;
		  data->cursor_x = Dynarr_length (data->db->runes);
		}
	      else
		rb.cursor_type = CURSOR_OFF;
	    }
	  else if (data->cursor_type == NEXT_CURSOR)
	    {
	      rb.cursor_type = CURSOR_ON;
	      data->cursor_x = Dynarr_length (data->db->runes);
	      data->cursor_type = NO_CURSOR;
	    }
	  else if (data->cursor_type == IGNORE_CURSOR)
	    rb.cursor_type = IGNORE_CURSOR;
	  else if (data->cursor_type == NO_CURSOR)
	    rb.cursor_type = NO_CURSOR;
	  else
	    rb.cursor_type = CURSOR_OFF;
	}
      else
	rb.cursor_type = CURSOR_OFF;

      Dynarr_add (data->db->runes, rb);
      data->pixpos += width;

      return NULL;
    }
  else
    {
      if (!NILP (glyph_face (gb->glyph, data->window)))
	gb->findex =
	  get_builtin_face_cache_index (w, glyph_face (gb->glyph,
						       data->window));
      else
	gb->findex = data->findex;

      if (pos_type == BEGIN_GLYPHS)
	{
	  if (!data->dl->left_glyphs)
	    data->dl->left_glyphs = Dynarr_new (glyph_block);
	  Dynarr_add (data->dl->left_glyphs, *gb);
	  return NULL;
	}
      else if (pos_type == END_GLYPHS)
	{
	  if (!data->dl->right_glyphs)
	    data->dl->right_glyphs = Dynarr_new (glyph_block);
	  Dynarr_add (data->dl->right_glyphs, *gb);
	  return NULL;
	}
      else
	abort ();	/* there are no unknown types */
    }

  return NULL;	/* shut up compiler */
}

/* Add all glyphs at position POS_TYPE that are contained in the given
   data. */

static prop_block_dynarr *
add_glyph_runes (pos_data *data, int pos_type)
{
  /* #### This still needs to handle the start_col parameter.  Duh, Chuck,
     why didn't you just modify add_glyph_rune in the first place? */
  int elt;
  glyph_block_dynarr *glyph_arr = (pos_type == BEGIN_GLYPHS
				   ? data->ef->begin_glyphs
				   : data->ef->end_glyphs);
  prop_block_dynarr *prop;

  for (elt = 0; elt < Dynarr_length (glyph_arr); elt++)
    {
      prop = add_glyph_rune (data, Dynarr_atp (glyph_arr, elt), pos_type, 0,
			     0);

      if (prop)
	{
	  /* #### Add some propagation information. */
	  return prop;
	}
    }

  Dynarr_reset (glyph_arr);

  return NULL;
}

/* Given a position for a buffer in a window, ensure that the given
   display line DL accurately represents the text on a line starting
   at the given position.

   NOTE NOTE NOTE NOTE: This function works with and returns Bytinds.
   You must do appropriate conversion. */

static Bytind
create_text_block (struct window *w, struct display_line *dl,
		   Bytind bi_start_pos, int start_col,
		   prop_block_dynarr **prop,
		   int type)
{
  struct frame *f = XFRAME (w->frame);
  struct buffer *b = XBUFFER (w->buffer);
  struct device *d = XDEVICE (f->device);

  pos_data data;

  /* Don't display anything in the minibuffer if this window is not on
     a selected frame.  We consider all other windows to be active
     minibuffers as it simplifies the coding. */
  int active_minibuffer = (!MINI_WINDOW_P (w) ||
			   (f == device_selected_frame (d)) ||
			   is_surrogate_for_selected_frame (f));

  int truncate_win = window_truncation_on (w);
  int end_glyph_width;

  /* If the buffer's value of selective_display is an integer then
     only lines that start with less than selective_display columns of
     space will be displayed.  If selective_display is t then all text
     after a ^M is invisible. */
  int selective = (INTP (b->selective_display)
		   ? XINT (b->selective_display)
		   : ((!NILP (b->selective_display) ? -1 : 0)));

  /* The variable ctl-arrow allows the user to specify what characters
     can actually be displayed and which octal should be used for.
     #### This variable should probably have some rethought done to
     it.

     #### It would also be really nice if you could specify that
     the characters come out in hex instead of in octal.  Mule
     does that by adding a ctl-hexa variable similar to ctl-arrow,
     but that's bogus -- we need a more general solution.  I
     think you need to extend the concept of display tables
     into a more general conversion mechanism.  Ideally you
     could specify a Lisp function that converts characters,
     but this violates the Second Golden Rule and besides would
     make things way way way way slow.

     So instead, we extend the display-table concept, which was
     historically limited to 256-byte vectors, to one of the
     following:

     a) A 256-entry vector, for backward compatibility;
     b) char-table, mapping characters to values;
     c) range-table, mapping ranges of characters to values;
     d) a list of the above.

     The (d) option allows you to specify multiple display tables
     instead of just one.  Each display table can specify conversions
     for some characters and leave others unchanged.  The way the
     character gets displayed is determined by the first display table
     with a binding for that character.  This way, you could call a
     function `enable-hex-display' that adds a hex display-table to
     the list of display tables for the current buffer.

     #### ...not yet implemented...  Also, we extend the concept of
     "mapping" to include a printf-like spec.  Thus you can make all
     extended characters show up as hex with a display table like
     this:

         #s(range-table data ((256 524288) (format "%x")))

     Since more than one display table is possible, you have
     great flexibility in mapping ranges of characters.  */
  Emchar printable_min = (CHAR_OR_CHAR_INTP (b->ctl_arrow)
			  ? XCHAR_OR_CHAR_INT (b->ctl_arrow)
			  : ((EQ (b->ctl_arrow, Qt) || EQ (b->ctl_arrow, Qnil))
			     ? 255 : 160));

  Lisp_Object face_dt, window_dt;

  /* The text display block for this display line. */
  struct display_block *db = get_display_block_from_line (dl, TEXT);

  /* The first time through the main loop we need to force the glyph
     data to be updated. */
  int initial = 1;

  /* Apparently the new extent_fragment_update returns an end position
     equal to the position passed in if there are no more runs to be
     displayed. */
  int no_more_frags = 0;

  Lisp_Object synch_minibuffers_value =
    symbol_value_in_buffer (Qsynchronize_minibuffers, w->buffer);

  dl->used_prop_data = 0;
  dl->num_chars = 0;

  xzero (data);
  data.ef = extent_fragment_new (w->buffer, f);

  /* These values are used by all of the rune addition routines.  We add
     them to this structure for ease of passing. */
  data.d = d;
  XSETWINDOW (data.window, w);
  data.db = db;
  data.dl = dl;

  data.bi_bufpos = bi_start_pos;
  data.pixpos = dl->bounds.left_in;
  data.last_charset = Qunbound;
  data.last_findex = DEFAULT_INDEX;
  data.result_str = Qnil;

  /* Set the right boundary adjusting it to take into account any end
     glyph.  Save the width of the end glyph for later use. */
  data.max_pixpos = dl->bounds.right_in;
  if (truncate_win)
    end_glyph_width = GLYPH_CACHEL_WIDTH (w, TRUN_GLYPH_INDEX);
  else
    end_glyph_width = GLYPH_CACHEL_WIDTH (w, CONT_GLYPH_INDEX);
  data.max_pixpos -= end_glyph_width;

  if (cursor_in_echo_area && MINI_WINDOW_P (w) && echo_area_active (f))
    {
      data.bi_cursor_bufpos = BI_BUF_ZV (b);
      data.cursor_type = CURSOR_ON;
    }
  else if (MINI_WINDOW_P (w) && !active_minibuffer)
    data.cursor_type = NO_CURSOR;
  else if (w == XWINDOW (FRAME_SELECTED_WINDOW (f)) &&
	   EQ(DEVICE_CONSOLE(d), Vselected_console) &&
	   d == XDEVICE(CONSOLE_SELECTED_DEVICE(XCONSOLE(DEVICE_CONSOLE(d))))&&
	   f == XFRAME(DEVICE_SELECTED_FRAME(d)))
    {
      data.bi_cursor_bufpos = BI_BUF_PT (b);
      data.cursor_type = CURSOR_ON;
    }
  else if (w == XWINDOW (FRAME_SELECTED_WINDOW (f)))
    {
      data.bi_cursor_bufpos = bi_marker_position (w->pointm[type]);
      data.cursor_type = CURSOR_ON;
    }
  else
    data.cursor_type = NO_CURSOR;
  data.cursor_x = -1;

  data.start_col = w->hscroll;
  data.bi_start_col_enabled = (w->hscroll ? bi_start_pos : 0);
  data.hscroll_glyph_width_adjust = 0;

  /* We regenerate the line from the very beginning. */
  Dynarr_reset (db->runes);

  /* Why is this less than or equal and not just less than?  If the
     starting position is already equal to the maximum we can't add
     anything else, right?  Wrong.  We might still have a newline to
     add.  A newline can use the room allocated for an end glyph since
     if we add it we know we aren't going to be adding any end
     glyph. */

  /* #### Chuck -- I think this condition should be while (1).
     Otherwise if (e.g.) there is one begin-glyph and one end-glyph
     and the begin-glyph ends exactly at the end of the window, the
     end-glyph and text might not be displayed.  while (1) ensures
     that the loop terminates only when either (a) there is
     propagation data or (b) the end-of-line or end-of-buffer is hit.

     #### Also I think you need to ensure that the operation
     "add begin glyphs; add end glyphs; add text" is atomic and
     can't get interrupted in the middle.  If you run off the end
     of the line during that operation, then you keep accumulating
     propagation data until you're done.  Otherwise, if the (e.g.)
     there's a begin glyph at a particular position and attempting
     to display that glyph results in window-end being hit and
     propagation data being generated, then the character at that
     position won't be displayed.

     #### See also the comment after the end of this loop, below.
     */
  while (data.pixpos <= data.max_pixpos
	 && (active_minibuffer || !NILP (synch_minibuffers_value)))
    {
      /* #### This check probably should not be necessary. */
      if (data.bi_bufpos > BI_BUF_ZV (b))
	{
	  /* #### urk!  More of this lossage! */
	  data.bi_bufpos--;
	  goto done;
	}

      /* If selective display was an integer and we aren't working on
         a continuation line then find the next line we are actually
         supposed to display. */
      if (selective > 0
	  && (data.bi_bufpos == BI_BUF_BEGV (b)
	      || BUF_FETCH_CHAR (b, prev_bytind (b, data.bi_bufpos)) == '\n'))
	{
	  while (bi_spaces_at_point (b, data.bi_bufpos) >= selective)
	    {
	      data.bi_bufpos =
		bi_find_next_newline_no_quit (b, data.bi_bufpos, 1);
	      if (data.bi_bufpos >= BI_BUF_ZV (b))
		{
		  data.bi_bufpos = BI_BUF_ZV (b);
		  goto done;
		}
	    }
	}

      /* Check for face changes. */
      if (initial || (!no_more_frags && data.bi_bufpos == data.ef->end))
	{
	  /* Now compute the face and begin/end-glyph information. */
	  data.findex =
	    /* Remember that the extent-fragment routines deal in Bytind's. */
	    extent_fragment_update (w, data.ef, data.bi_bufpos);

	  get_display_tables (w, data.findex, &face_dt, &window_dt);

	  if (data.bi_bufpos == data.ef->end)
	    no_more_frags = 1;
	}
      initial = 0;

      /* Determine what is next to be displayed.  We first handle any
         glyphs returned by glyphs_at_bufpos.  If there are no glyphs to
         display then we determine what to do based on the character at the
         current buffer position. */

      /* If the current position is covered by an invisible extent, do
         nothing (except maybe add some ellipses).

	 #### The behavior of begin and end-glyphs at the edge of an
	 invisible extent should be investigated further.  This is
	 fairly low priority though. */
      if (data.ef->invisible)
	{
	  /* #### Chuck, perhaps you could look at this code?  I don't
	     really know what I'm doing. */
	  if (*prop)
	    {
	      Dynarr_free (*prop);
	      *prop = 0;
	    }

	  /* The extent fragment code only sets this when we should
	     really display the ellipses.  It makes sure the ellipses
	     don't get displayed more than once in a row. */
	  if (data.ef->invisible_ellipses)
	    {
	      struct glyph_block gb;

	      data.ef->invisible_ellipses_already_displayed = 1;
	      data.ef->invisible_ellipses = 0;
	      gb.extent = Qnil;
	      gb.glyph = Vinvisible_text_glyph;
	      *prop = add_glyph_rune (&data, &gb, BEGIN_GLYPHS, 0,
				      GLYPH_CACHEL (w, INVIS_GLYPH_INDEX));
	      /* Perhaps they shouldn't propagate if the very next thing
		 is to display a newline (for compatibility with
		 selective-display-ellipses)?  Maybe that's too
		 abstruse. */
	      if (*prop)
		goto done;
	    }

	  /* If point is in an invisible region we place it on the
             next visible character. */
	  if (data.cursor_type == CURSOR_ON
	      && data.bi_bufpos == data.bi_cursor_bufpos)
	    {
	      data.cursor_type = NEXT_CURSOR;
	    }

	  /* #### What if we we're dealing with a display table? */
	  if (data.start_col)
	    data.start_col--;

	  if (data.bi_bufpos == BI_BUF_ZV (b))
	    goto done;
	  else
	    INC_BYTIND (b, data.bi_bufpos);
	}

      /* If there is propagation data, then it represents the current
         buffer position being displayed.  Add them and advance the
         position counter.  This might also add the minibuffer
         prompt. */
      else if (*prop)
	{
	  dl->used_prop_data = 1;
	  *prop = add_propagation_runes (prop, &data);

	  if (*prop)
	    goto done;	/* gee, a really narrow window */
	  else if (data.bi_bufpos == BI_BUF_ZV (b))
	    goto done;
	  else if (data.bi_bufpos < BI_BUF_BEGV (b))
	    /* #### urk urk urk! Aborts are not very fun! Fix this please! */
	    data.bi_bufpos = BI_BUF_BEGV (b);
	  else
	    INC_BYTIND (b, data.bi_bufpos);
	}

      /* If there are end glyphs, add them to the line.  These are
	 the end glyphs for the previous run of text.  We add them
	 here rather than doing them at the end of handling the
	 previous run so that glyphs at the beginning and end of
	 a line are handled correctly. */
      else if (Dynarr_length (data.ef->end_glyphs) > 0)
	{
	  *prop = add_glyph_runes (&data, END_GLYPHS);
	  if (*prop)
	    goto done;
	}

      /* If there are begin glyphs, add them to the line. */
      else if (Dynarr_length (data.ef->begin_glyphs) > 0)
	{
	  *prop = add_glyph_runes (&data, BEGIN_GLYPHS);
	  if (*prop)
	    goto done;
	}

      /* If at end-of-buffer, we've already processed begin and
	 end-glyphs at this point and there's no text to process,
	 so we're done. */
      else if (data.bi_bufpos == BI_BUF_ZV (b))
	goto done;

      else
	{
	  Lisp_Object entry = Qnil;
	  /* Get the character at the current buffer position. */
	  data.ch = BI_BUF_FETCH_CHAR (b, data.bi_bufpos);
	  if (!NILP (face_dt) || !NILP (window_dt))
	    entry = display_table_entry (data.ch, face_dt, window_dt);

	  /* If there is a display table entry for it, hand it off to
             add_disp_table_entry_runes and let it worry about it. */
	  if (!NILP (entry) && !EQ (entry, make_char (data.ch)))
	    {
	      *prop = add_disp_table_entry_runes (&data, entry);

	      if (*prop)
		goto done;
	    }

	  /* Check if we have hit a newline character.  If so, add a marker
             to the line and end this loop. */
	  else if (data.ch == '\n')
	    {
	      /* We aren't going to be adding an end glyph so give its
                 space back in order to make sure that the cursor can
                 fit. */
	      data.max_pixpos += end_glyph_width;

	      if (selective > 0
		  && (bi_spaces_at_point
		      (b, next_bytind (b, data.bi_bufpos))
		      >= selective))
		{
		  if (!NILP (b->selective_display_ellipses))
		    {
		      struct glyph_block gb;

		      gb.extent = Qnil;
		      gb.glyph = Vinvisible_text_glyph;
		      add_glyph_rune (&data, &gb, BEGIN_GLYPHS, 0,
				      GLYPH_CACHEL (w, INVIS_GLYPH_INDEX));
		    }
		  else
		    {
		      /* Cheesy, cheesy, cheesy.  We mark the end of the
			 line with a special "character rune" whose width
			 is the EOL cursor width and whose character is
			 the non-printing character '\n'. */
		      data.blank_width = DEVMETH (d, eol_cursor_width, ());
		      *prop = add_emchar_rune (&data);
		    }

		  /* We need to set data.bi_bufpos to the start of the
                     next visible region in order to make this line
                     appear to contain all of the invisible area.
                     Otherwise, the line cache won't work
                     correctly. */
		  INC_BYTIND (b, data.bi_bufpos);
		  while (bi_spaces_at_point (b, data.bi_bufpos) >= selective)
		    {
		      data.bi_bufpos =
			bi_find_next_newline_no_quit (b, data.bi_bufpos, 1);
		      if (data.bi_bufpos >= BI_BUF_ZV (b))
			{
			  data.bi_bufpos = BI_BUF_ZV (b);
			  break;
			}
		    }
		  if (BI_BUF_FETCH_CHAR
		      (b, prev_bytind (b, data.bi_bufpos)) == '\n')
		    DEC_BYTIND (b, data.bi_bufpos);
		}
	      else
		{
		  data.blank_width = DEVMETH (d, eol_cursor_width, ());
		  *prop = add_emchar_rune (&data);
		}

	      goto done;
	    }

	  /* If the current character is ^M, and selective display is
             enabled, then add the invisible-text-glyph if
             selective-display-ellipses is set.  In any case, this
             line is done. */
	  else if (data.ch == (('M' & 037)) && selective == -1)
	    {
	      Bytind bi_next_bufpos;

	      /* Find the buffer position at the end of the line. */
	      bi_next_bufpos =
		bi_find_next_newline_no_quit (b, data.bi_bufpos, 1);
	      if (BI_BUF_FETCH_CHAR (b, prev_bytind (b, bi_next_bufpos))
		  == '\n')
		DEC_BYTIND (b, bi_next_bufpos);

	      /* If the cursor is somewhere in the elided text make
                 sure that the cursor gets drawn appropriately. */
	      if (data.cursor_type == CURSOR_ON
		  && (data.bi_cursor_bufpos >= data.bi_bufpos &&
		      data.bi_cursor_bufpos < bi_next_bufpos))
		{
		    data.cursor_type = NEXT_CURSOR;
		}

	      /* We won't be adding a truncation or continuation glyph
                 so give up the room allocated for them. */
	      data.max_pixpos += end_glyph_width;

	      if (!NILP (b->selective_display_ellipses))
		{
		  /* We don't propagate anything from the invisible
                     text glyph if it fails to fit.  This is
                     intentional. */
		  struct glyph_block gb;

		  gb.extent = Qnil;
		  gb.glyph = Vinvisible_text_glyph;
		  add_glyph_rune (&data, &gb, BEGIN_GLYPHS, 1,
				  GLYPH_CACHEL (w, INVIS_GLYPH_INDEX));
		}

	      /* Set the buffer position to the end of the line.  We
                 need to do this before potentially adding a newline
                 so that the cursor flag will get set correctly (if
                 needed). */
	      data.bi_bufpos = bi_next_bufpos;

	      if (NILP (b->selective_display_ellipses)
		  || data.bi_cursor_bufpos == bi_next_bufpos)
		{
		  /* We have to at least add a newline character so
                     that the cursor shows up properly. */
		  data.ch = '\n';
		  data.blank_width = DEVMETH (d, eol_cursor_width, ());
		  data.findex = DEFAULT_INDEX;
		  data.start_col = 0;
		  data.bi_start_col_enabled = 0;

		  add_emchar_rune (&data);
		}

	      /* This had better be a newline but doing it this way
                 we'll see obvious incorrect results if it isn't.  No
                 need to abort here. */
	      data.ch = BI_BUF_FETCH_CHAR (b, data.bi_bufpos);

	      goto done;
	    }

	  /* If the current character is considered to be printable, then
             just add it. */
	  else if (data.ch >= printable_min)
	    {
	      *prop = add_emchar_rune (&data);
	      if (*prop)
		goto done;
	    }

	  /* If the current character is a tab, determine the next tab
             starting position and add a blank rune which extends from the
             current pixel position to that starting position. */
	  else if (data.ch == '\t')
	    {
	      int tab_start_pixpos = data.pixpos;
	      int next_tab_start;
	      int char_tab_width;
	      int prop_width = 0;

	      if (data.start_col > 1)
		tab_start_pixpos -= (space_width (w) * (data.start_col - 1));

	      next_tab_start =
		next_tab_position (w, tab_start_pixpos,
				   dl->bounds.left_in +
				   data.hscroll_glyph_width_adjust);
	      if (next_tab_start > data.max_pixpos)
		{
		  prop_width = next_tab_start - data.max_pixpos;
		  next_tab_start = data.max_pixpos;
		}
	      data.blank_width = next_tab_start - data.pixpos;
	      char_tab_width =
		(next_tab_start - tab_start_pixpos) / space_width (w);

	      *prop = add_blank_rune (&data, w, char_tab_width);

	      /* add_blank_rune is only supposed to be called with
                 sizes guaranteed to fit in the available space. */
	      assert (!(*prop));

	      if (prop_width)
		{
		  struct prop_block pb;
		  *prop = Dynarr_new (prop_block);

		  pb.type = PROP_BLANK;
		  pb.data.p_blank.width = prop_width;
		  pb.data.p_blank.findex = data.findex;
		  Dynarr_add (*prop, pb);

		  goto done;
		}
	    }

	  /* If character is a control character, pass it off to
             add_control_char_runes.

	     The is_*() routines have undefined results on
	     arguments outside of the range [-1, 255].  (This
	     often bites people who carelessly use `char' instead
	     of `unsigned char'.)
	     */
	  else if (data.ch < 0x100 && iscntrl ((Bufbyte) data.ch))
	    {
	      *prop = add_control_char_runes (&data, b);

	      if (*prop)
		goto done;
	    }

	  /* If the character is above the ASCII range and we have not
             already handled it, then print it as an octal number. */
	  else if (data.ch >= 0200)
	    {
	      *prop = add_octal_runes (&data);

	      if (*prop)
		goto done;
	    }

	  /* Assume the current character is considered to be printable,
             then just add it. */
	  else
	    {
	      *prop = add_emchar_rune (&data);
	      if (*prop)
		goto done;
	    }

	  INC_BYTIND (b, data.bi_bufpos);
	}
    }

done:

  /* Determine the starting point of the next line if we did not hit the
     end of the buffer. */
  if (data.bi_bufpos < BI_BUF_ZV (b)
      && (active_minibuffer || !NILP (synch_minibuffers_value)))
    {
      /* #### This check is not correct.  If the line terminated
	 due to a begin-glyph or end-glyph hitting window-end, then
	 data.ch will not point to the character at data.bi_bufpos.  If
	 you make the two changes mentioned at the top of this loop,
	 you should be able to say '(if (*prop))'.  That should also
	 make it possible to eliminate the data.bi_bufpos < BI_BUF_ZV (b)
	 check. */

      /* The common case is that the line ended because we hit a newline.
         In that case, the next character is just the next buffer
         position. */
      if (data.ch == '\n')
	{
	  /* If data.start_col_enabled is still true, then the window is
             scrolled far enough so that nothing on this line is visible.
             We need to stick a truncation glyph at the beginning of the
             line in that case unless the line is completely blank. */
	  if (data.bi_start_col_enabled)
	    {
	      if (data.cursor_type == CURSOR_ON)
		{
		  if (data.bi_cursor_bufpos >= bi_start_pos
		      && data.bi_cursor_bufpos <= data.bi_bufpos)
		    data.bi_cursor_bufpos = data.bi_bufpos;
		}
	      data.findex = DEFAULT_INDEX;
	      data.start_col = 0;
	      data.bi_start_col_enabled = 0;

	      if (data.bi_bufpos != bi_start_pos)
		{
		  struct glyph_block gb;

		  gb.extent = Qnil;
		  gb.glyph = Vhscroll_glyph;
		  add_glyph_rune (&data, &gb, BEGIN_GLYPHS, 0,
				  GLYPH_CACHEL (w, HSCROLL_GLYPH_INDEX));
		}
	      else
		{
		  /* This duplicates code down below to add a newline to
                     the end of an otherwise empty line.*/
		  data.ch = '\n';
		  data.blank_width = DEVMETH (d, eol_cursor_width, ());

		  add_emchar_rune (&data);
		}
	    }

	  INC_BYTIND (b, data.bi_bufpos);
	}

      /* Otherwise we have a buffer line which cannot fit on one display
         line. */
      else
	{
	  struct glyph_block gb;
	  struct glyph_cachel *cachel;

	  /* If the line is to be truncated then we actually have to look
             for the next newline.  We also add the end-of-line glyph which
             we know will fit because we adjusted the right border before
             we starting laying out the line. */
	  data.max_pixpos += end_glyph_width;
	  data.findex = DEFAULT_INDEX;
	  gb.extent = Qnil;

	  if (truncate_win)
	    {
	      Bytind bi_pos;

	      /* Now find the start of the next line. */
	      bi_pos = bi_find_next_newline_no_quit (b, data.bi_bufpos, 1);

	      /* If the cursor is past the truncation line then we
                 make it appear on the truncation glyph.  If we've hit
                 the end of the buffer then we also make the cursor
                 appear unless eob is immediately preceded by a
                 newline.  In that case the cursor should actually
                 appear on the next line. */
	      if (data.cursor_type == CURSOR_ON
		  && data.bi_cursor_bufpos >= data.bi_bufpos
		  && (data.bi_cursor_bufpos < bi_pos ||
		      (bi_pos == BI_BUF_ZV (b)
		       && (bi_pos == BI_BUF_BEGV (b)
			   || (BI_BUF_FETCH_CHAR (b, prev_bytind (b, bi_pos))
			       != '\n')))))
		data.bi_cursor_bufpos = bi_pos;
	      else
		data.cursor_type = NO_CURSOR;

	      data.bi_bufpos = bi_pos;
	      gb.glyph = Vtruncation_glyph;
	      cachel = GLYPH_CACHEL (w, TRUN_GLYPH_INDEX);
	    }
	  else
	    {
	      /* The cursor can never be on the continuation glyph. */
	      data.cursor_type = NO_CURSOR;

	      /* data.bi_bufpos is already at the start of the next line. */

	      gb.glyph = Vcontinuation_glyph;
	      cachel = GLYPH_CACHEL (w, CONT_GLYPH_INDEX);
	    }

	  add_glyph_rune (&data, &gb, BEGIN_GLYPHS, 1, cachel);

	  if (truncate_win && data.bi_bufpos == BI_BUF_ZV (b)
	      && BI_BUF_FETCH_CHAR (b, prev_bytind (b, BI_BUF_ZV (b))) != '\n')
	    /* #### Damn this losing shit. */
	    data.bi_bufpos++;
	}
    }
  else if ((active_minibuffer || !NILP (synch_minibuffers_value))
	   && (!echo_area_active (f) || data.bi_bufpos == BI_BUF_ZV (b)))
    {
      /* We need to add a marker to the end of the line since there is no
         newline character in order for the cursor to get drawn.  We label
         it as a newline so that it gets handled correctly by the
         whitespace routines below. */

      data.ch = '\n';
      data.blank_width = DEVMETH (d, eol_cursor_width, ());
      data.findex = DEFAULT_INDEX;
      data.start_col = 0;
      data.bi_start_col_enabled = 0;

      data.max_pixpos += data.blank_width;
      add_emchar_rune (&data);
      data.max_pixpos -= data.blank_width;

      /* #### urk!  Chuck, this shit is bad news.  Going around
	 manipulating invalid positions is guaranteed to result in
	 trouble sooner or later. */
      data.bi_bufpos = BI_BUF_ZV (b) + 1;
    }

  /* Calculate left whitespace boundary. */
  {
    int elt = 0;

    /* Whitespace past a newline is considered right whitespace. */
    while (elt < Dynarr_length (db->runes))
      {
	struct rune *rb = Dynarr_atp (db->runes, elt);

	if ((rb->type == RUNE_CHAR && rb->object.chr.ch == ' ')
	    || rb->type == RUNE_BLANK)
	  {
	    dl->bounds.left_white += rb->width;
	    elt++;
	  }
	else
	  elt = Dynarr_length (db->runes);
      }
  }

  /* Calculate right whitespace boundary. */
  {
    int elt = Dynarr_length (db->runes) - 1;
    int done = 0;

    while (!done && elt >= 0)
      {
	struct rune *rb = Dynarr_atp (db->runes, elt);

	if (!(rb->type == RUNE_CHAR && rb->object.chr.ch < 0x100
	    && isspace (rb->object.chr.ch))
	    && !rb->type == RUNE_BLANK)
	  {
	    dl->bounds.right_white = rb->xpos + rb->width;
	    done = 1;
	  }

	elt--;

      }

    /* The line is blank so everything is considered to be right
       whitespace. */
    if (!done)
      dl->bounds.right_white = dl->bounds.left_in;
  }

  /* Set the display blocks bounds. */
  db->start_pos = dl->bounds.left_in;
  if (Dynarr_length (db->runes))
    {
      struct rune *rb = Dynarr_atp (db->runes, Dynarr_length (db->runes) - 1);

      db->end_pos = rb->xpos + rb->width;
    }
  else
    db->end_pos = dl->bounds.right_white;

  /* update line height parameters */
  if (!data.new_ascent && !data.new_descent)
    {
      /* We've got a blank line so initialize these values from the default
         face. */
      default_face_font_info (data.window, &data.new_ascent,
			      &data.new_descent, 0, 0, 0);
    }

  if (data.max_pixmap_height)
    {
      int height = data.new_ascent + data.new_descent;
      int pix_ascent, pix_descent;

      pix_descent = data.max_pixmap_height * data.new_descent / height;
      pix_ascent = data.max_pixmap_height - pix_descent;

      data.new_ascent = max (data.new_ascent, pix_ascent);
      data.new_descent = max (data.new_descent, pix_descent);
    }

  dl->ascent = data.new_ascent;
  dl->descent = data.new_descent;

  {
    unsigned short ascent = (unsigned short) XINT (w->minimum_line_ascent);

    if (dl->ascent < ascent)
      dl->ascent = ascent;
  }
  {
    unsigned short descent = (unsigned short) XINT (w->minimum_line_descent);

    if (dl->descent < descent)
      dl->descent = descent;
  }

  dl->cursor_elt = data.cursor_x;
  /* #### lossage lossage lossage! Fix this shit! */
  if (data.bi_bufpos > BI_BUF_ZV (b))
    dl->end_bufpos = BUF_ZV (b);
  else
    dl->end_bufpos = bytind_to_bufpos (b, data.bi_bufpos) - 1;
  if (truncate_win)
    data.dl->num_chars = column_at_point (b, dl->end_bufpos, 0);
  else
    /* This doesn't correctly take into account tabs and control
       characters but if the window isn't being truncated then this
       value isn't going to end up being used anyhow. */
    data.dl->num_chars = dl->end_bufpos - dl->bufpos;

  /* #### handle horizontally scrolled line with text none of which
     was actually laid out. */

  /* #### handle any remainder of overlay arrow */

  if (*prop == ADD_FAILED)
    *prop = NULL;

  if (truncate_win && *prop)
    {
      Dynarr_free (*prop);
      *prop = NULL;
    }

  extent_fragment_delete (data.ef);

  /* #### If we started at EOB, then make sure we return a value past
     it so that regenerate_window will exit properly.  This is bogus.
     The main loop should get fixed so that it isn't necessary to call
     this function if we are already at EOB. */

  if (data.bi_bufpos == BI_BUF_ZV (b) && bi_start_pos == BI_BUF_ZV (b))
    return data.bi_bufpos + 1; /* Yuck! */
  else
    return data.bi_bufpos;
}

/* Display the overlay arrow at the beginning of the given line. */

static int
create_overlay_glyph_block (struct window *w, struct display_line *dl)
{
  struct frame *f = XFRAME (w->frame);
  struct device *d = XDEVICE (f->device);
  pos_data data;

  /* If Voverlay_arrow_string isn't valid then just fail silently. */
  if (!STRINGP (Voverlay_arrow_string) && !GLYPHP (Voverlay_arrow_string))
    return 0;

  xzero (data);
  data.ef = NULL;
  data.d = d;
  XSETWINDOW (data.window, w);
  data.db = get_display_block_from_line (dl, OVERWRITE);
  data.dl = dl;
  data.pixpos = dl->bounds.left_in;
  data.max_pixpos = dl->bounds.right_in;
  data.cursor_type = NO_CURSOR;
  data.cursor_x = -1;
  data.findex = DEFAULT_INDEX;
  data.last_charset = Qunbound;
  data.last_findex = DEFAULT_INDEX;
  data.result_str = Qnil;

  Dynarr_reset (data.db->runes);

  if (STRINGP (Voverlay_arrow_string))
    {
      add_bufbyte_string_runes
	(&data,
	 XSTRING_DATA   (Voverlay_arrow_string),
	 XSTRING_LENGTH (Voverlay_arrow_string),
	 1);
    }
  else if (GLYPHP (Voverlay_arrow_string))
    {
      struct glyph_block gb;

      gb.glyph = Voverlay_arrow_string;
      gb.extent = Qnil;
      add_glyph_rune (&data, &gb, BEGIN_GLYPHS, 0, 0);
    }

  if (data.max_pixmap_height)
    {
      int height = data.new_ascent + data.new_descent;
      int pix_ascent, pix_descent;

      pix_descent = data.max_pixmap_height * data.new_descent / height;
      pix_ascent = data.max_pixmap_height - pix_descent;

      data.new_ascent = max (data.new_ascent, pix_ascent);
      data.new_descent = max (data.new_descent, pix_descent);
    }

  dl->ascent = data.new_ascent;
  dl->descent = data.new_descent;

  data.db->start_pos = dl->bounds.left_in;
  data.db->end_pos = data.pixpos;

  return data.pixpos - dl->bounds.left_in;
}

/* Add a type of glyph to a margin display block. */

static int
add_margin_runes (struct display_line *dl, struct display_block *db, int start,
		  int count, enum glyph_layout layout, int side, Lisp_Object window)
{
  glyph_block_dynarr *gbd = (side == LEFT_GLYPHS
			     ? dl->left_glyphs
			     : dl->right_glyphs);
  int elt, end;
  int xpos = start;
  int reverse;

  if ((layout == GL_WHITESPACE && side == LEFT_GLYPHS)
      || (layout == GL_INSIDE_MARGIN && side == RIGHT_GLYPHS))
    {
      reverse = 1;
      elt = Dynarr_length (gbd) - 1;
      end = 0;
    }
  else
    {
      reverse = 0;
      elt = 0;
      end = Dynarr_length (gbd);
    }

  while (count && ((!reverse && elt < end) || (reverse && elt >= end)))
    {
      struct glyph_block *gb = Dynarr_atp (gbd, elt);

      if (NILP (gb->extent))
	abort ();	/* these should have been handled in add_glyph_rune */

      if (gb->active &&
	  ((side == LEFT_GLYPHS &&
	    extent_begin_glyph_layout (XEXTENT (gb->extent)) == layout)
	   || (side == RIGHT_GLYPHS &&
	       extent_end_glyph_layout (XEXTENT (gb->extent)) == layout)))
	{
	  struct rune rb;

	  rb.width = gb->width;
	  rb.findex = gb->findex;
	  rb.xpos = xpos;
	  rb.bufpos = -1;
	  rb.endpos = 0;
	  rb.type = RUNE_DGLYPH;
	  rb.object.dglyph.glyph = gb->glyph;
	  rb.object.dglyph.extent = gb->extent;
	  rb.object.dglyph.xoffset = 0;
	  rb.cursor_type = CURSOR_OFF;

	  Dynarr_add (db->runes, rb);
	  xpos += rb.width;
	  count--;
	  gb->active = 0;

	  if (glyph_contrib_p (gb->glyph, window))
	    {
	      unsigned short ascent, descent;
	      Lisp_Object baseline = glyph_baseline (gb->glyph, window);

	      ascent = glyph_ascent (gb->glyph, Qnil, gb->findex, window);
	      descent = glyph_descent (gb->glyph, Qnil, gb->findex, window);

	      /* A pixmap that has not had a baseline explicitly set.
                 We use the existing ascent / descent ratio of the
                 line. */
	      if (NILP (baseline))
		{
		  int gheight = ascent + descent;
		  int line_height = dl->ascent + dl->descent;
		  int pix_ascent, pix_descent;

		  pix_descent = (int) (gheight * dl->descent) / line_height;
		  pix_ascent = gheight - pix_descent;

		  dl->ascent = max ((int) dl->ascent, pix_ascent);
		  dl->descent = max ((int) dl->descent, pix_descent);
		}

	      /* A string so determine contribution normally. */
	      else if (EQ (baseline, Qt))
		{
		  dl->ascent = max (dl->ascent, ascent);
		  dl->descent = max (dl->descent, descent);
		}

	      /* A pixmap with an explicitly set baseline.  We determine the
		 contribution here. */
	      else if (INTP (baseline))
		{
		  int height = ascent + descent;
		  int pix_ascent, pix_descent;

		  pix_ascent = height * XINT (baseline) / 100;
		  pix_descent = height - pix_ascent;

		  dl->ascent = max ((int) dl->ascent, pix_ascent);
		  dl->descent = max ((int) dl->descent, pix_descent);
		}

	      /* Otherwise something is screwed up. */
	      else
		abort ();
	    }
	}

      (reverse ? elt-- : elt++);
    }

  return xpos;
}

/* Add a blank to a margin display block. */

static void
add_margin_blank (struct display_line *dl, struct display_block *db,
		  struct window *w, int xpos, int width, int side)
{
  struct rune rb;

  rb.findex = (side == LEFT_GLYPHS
	       ? get_builtin_face_cache_index (w, Vleft_margin_face)
	       : get_builtin_face_cache_index (w, Vright_margin_face));
  rb.xpos = xpos;
  rb.width = width;
  rb.bufpos = -1;
  rb.endpos = 0;
  rb.type = RUNE_BLANK;
  rb.cursor_type = CURSOR_OFF;

  Dynarr_add (db->runes, rb);
}

/* Display glyphs in the left outside margin, left inside margin and
   left whitespace area. */

static void
create_left_glyph_block (struct window *w, struct display_line *dl,
			 int overlay_width)
{
  Lisp_Object window;

  int use_overflow = (NILP (w->use_left_overflow) ? 0 : 1);
  int elt, end_xpos;
  int out_end, in_out_start, in_in_end, white_out_start, white_in_start;
  int out_cnt, in_out_cnt, in_in_cnt, white_out_cnt, white_in_cnt;
  int left_in_start = dl->bounds.left_in;
  int left_in_end = dl->bounds.left_in + overlay_width;

  struct display_block *odb, *idb;

  XSETWINDOW (window, w);

  /* We have to add the glyphs to the line in the order outside,
     inside, whitespace.  However the precedence dictates that we
     determine how many will fit in the reverse order. */

  /* Determine how many whitespace glyphs we can display and where
     they should start. */
  white_in_start = dl->bounds.left_white;
  white_out_start = left_in_start;
  white_out_cnt = white_in_cnt = 0;
  elt = 0;

  while (elt < Dynarr_length (dl->left_glyphs))
    {
      struct glyph_block *gb = Dynarr_atp (dl->left_glyphs, elt);

      if (NILP (gb->extent))
	abort ();	/* these should have been handled in add_glyph_rune */

      if (extent_begin_glyph_layout (XEXTENT (gb->extent)) == GL_WHITESPACE)
	{
	  int width;

	  width = glyph_width (gb->glyph, Qnil, gb->findex, window);

	  if (white_in_start - width >= left_in_end)
	    {
	      white_in_cnt++;
	      white_in_start -= width;
	      gb->width = width;
	      gb->active = 1;
	    }
	  else if (use_overflow
		   && (white_out_start - width > dl->bounds.left_out))
	    {
	      white_out_cnt++;
	      white_out_start -= width;
	      gb->width = width;
	      gb->active = 1;
	    }
	  else
	    gb->active = 0;
	}

      elt++;
    }

  /* Determine how many inside margin glyphs we can display and where
     they should start.  The inside margin glyphs get whatever space
     is left after the whitespace glyphs have been displayed.  These
     are tricky to calculate since if we decide to use the overflow
     area we basically have to start over.  So for these we build up a
     list of just the inside margin glyphs and manipulate it to
     determine the needed info. */
  {
    glyph_block_dynarr *ib;
    int avail_in, avail_out;
    int done = 0;
    int marker = 0;
    int used_in, used_out;

    elt = 0;
    used_in = used_out = 0;
    ib = Dynarr_new (glyph_block);
    while (elt < Dynarr_length (dl->left_glyphs))
      {
	struct glyph_block *gb = Dynarr_atp (dl->left_glyphs, elt);

	if (NILP (gb->extent))
	  abort ();	/* these should have been handled in add_glyph_rune */

	if (extent_begin_glyph_layout (XEXTENT (gb->extent)) ==
	    GL_INSIDE_MARGIN)
	  {
	    gb->width = glyph_width (gb->glyph, Qnil, gb->findex, window);
	    used_in += gb->width;
	    Dynarr_add (ib, *gb);
	  }

	elt++;
      }

    if (white_out_cnt)
      avail_in = 0;
    else
      {
	avail_in = white_in_start - left_in_end;
	if (avail_in < 0)
	  avail_in = 0;
      }

    if (!use_overflow)
      avail_out = 0;
    else
      avail_out = white_out_start - dl->bounds.left_out;

    marker = 0;
    while (!done && marker < Dynarr_length (ib))
      {
	int width = Dynarr_atp (ib, marker)->width;

	/* If everything now fits in the available inside margin
           space, we're done. */
	if (used_in <= avail_in)
	  done = 1;
	else
	  {
	    /* Otherwise see if we have room to move a glyph to the
               outside. */
	    if (used_out + width <= avail_out)
	      {
		used_out += width;
		used_in -= width;
	      }
	    else
	      done = 1;
	  }

	if (!done)
	  marker++;
      }

    /* At this point we now know that everything from marker on goes in
       the inside margin and everything before it goes in the outside
       margin.  The stuff going into the outside margin is guaranteed
       to fit, but we may have to trim some stuff from the inside. */

    in_in_end = left_in_end;
    in_out_start = white_out_start;
    in_out_cnt = in_in_cnt = 0;

    Dynarr_free (ib);
    elt = 0;
    while (elt < Dynarr_length (dl->left_glyphs))
      {
	struct glyph_block *gb = Dynarr_atp (dl->left_glyphs, elt);

	if (NILP (gb->extent))
	  abort ();	/* these should have been handled in add_glyph_rune */

	if (extent_begin_glyph_layout (XEXTENT (gb->extent)) ==
	    GL_INSIDE_MARGIN)
	  {
	    int width = glyph_width (gb->glyph, Qnil, gb->findex, window);

	    if (used_out)
	      {
		in_out_cnt++;
		in_out_start -= width;
		gb->width = width;
		gb->active = 1;
		used_out -= width;
	      }
	    else if (in_in_end + width < white_in_start)
	      {
		in_in_cnt++;
		in_in_end += width;
		gb->width = width;
		gb->active = 1;
	      }
	    else
	      gb->active = 0;
	  }

	elt++;
      }
  }

  /* Determine how many outside margin glyphs we can display.  They
     always start at the left outside margin and can only use the
     outside margin space. */
  out_end = dl->bounds.left_out;
  out_cnt = 0;
  elt = 0;

  while (elt < Dynarr_length (dl->left_glyphs))
    {
      struct glyph_block *gb = Dynarr_atp (dl->left_glyphs, elt);

      if (NILP (gb->extent))
	abort ();	/* these should have been handled in add_glyph_rune */

      if (extent_begin_glyph_layout (XEXTENT (gb->extent)) ==
	  GL_OUTSIDE_MARGIN)
	{
	  int width = glyph_width (gb->glyph, Qnil, gb->findex, window);

	  if (out_end + width <= in_out_start)
	    {
	      out_cnt++;
	      out_end += width;
	      gb->width = width;
	      gb->active = 1;
	    }
	  else
	    gb->active = 0;
	}

      elt++;
    }

  /* Now that we know where everything goes, we add the glyphs as
     runes to the appropriate display blocks. */
  if (out_cnt || in_out_cnt || white_out_cnt)
    {
      odb = get_display_block_from_line (dl, LEFT_OUTSIDE_MARGIN);
      odb->start_pos = dl->bounds.left_out;
      /* #### We should stop adding a blank to account for the space
         between the end of the glyphs and the margin and instead set
         this accordingly. */
      odb->end_pos = dl->bounds.left_in;
      Dynarr_reset (odb->runes);
    }
  else
    odb = 0;

  if (in_in_cnt || white_in_cnt)
    {
      idb = get_display_block_from_line (dl, LEFT_INSIDE_MARGIN);
      idb->start_pos = dl->bounds.left_in;
      /* #### See above comment for odb->end_pos */
      idb->end_pos = dl->bounds.left_white;
      Dynarr_reset (idb->runes);
    }
  else
    idb = 0;

  /* First add the outside margin glyphs. */
  if (out_cnt)
    end_xpos = add_margin_runes (dl, odb, dl->bounds.left_out, out_cnt,
				 GL_OUTSIDE_MARGIN, LEFT_GLYPHS, window);
  else
    end_xpos = dl->bounds.left_out;

  /* There may be blank space between the outside margin glyphs and
     the inside margin glyphs.  If so, add a blank. */
  if (in_out_cnt && (in_out_start - end_xpos))
    {
      add_margin_blank (dl, odb, w, end_xpos, in_out_start - end_xpos,
			LEFT_GLYPHS);
    }

  /* Next add the inside margin glyphs which are actually in the
     outside margin. */
  if (in_out_cnt)
    {
      end_xpos = add_margin_runes (dl, odb, in_out_start, in_out_cnt,
				   GL_INSIDE_MARGIN, LEFT_GLYPHS, window);
    }

  /* If we didn't add any inside margin glyphs to the outside margin,
     but are adding whitespace glyphs, then we need to add a blank
     here. */
  if (!in_out_cnt && white_out_cnt && (white_out_start - end_xpos))
    {
      add_margin_blank (dl, odb, w, end_xpos, white_out_start - end_xpos,
			LEFT_GLYPHS);
    }

  /* Next add the whitespace margin glyphs which are actually in the
     outside margin. */
  if (white_out_cnt)
    {
      end_xpos = add_margin_runes (dl, odb, white_out_start, white_out_cnt,
				   GL_WHITESPACE, LEFT_GLYPHS, window);
    }

  /* We take care of clearing between the end of the glyphs and the
     start of the inside margin for lines which have glyphs.  */
  if (odb && (left_in_start - end_xpos))
    {
      add_margin_blank (dl, odb, w, end_xpos, left_in_start - end_xpos,
			LEFT_GLYPHS);
    }

  /* Next add the inside margin glyphs which are actually in the
     inside margin. */
  if (in_in_cnt)
    {
      end_xpos = add_margin_runes (dl, idb, left_in_end, in_in_cnt,
				   GL_INSIDE_MARGIN, LEFT_GLYPHS, window);
    }
  else
    end_xpos = left_in_end;

  /* Make sure that the area between the end of the inside margin
     glyphs and the whitespace glyphs is cleared. */
  if (idb && (white_in_start - end_xpos > 0))
    {
      add_margin_blank (dl, idb, w, end_xpos, white_in_start - end_xpos,
			LEFT_GLYPHS);
    }

  /* Next add the whitespace margin glyphs which are actually in the
     inside margin. */
  if (white_in_cnt)
    {
      add_margin_runes (dl, idb, white_in_start, white_in_cnt, GL_WHITESPACE,
			LEFT_GLYPHS, window);
    }

  /* Whitespace glyphs always end right next to the text block so
     there is nothing we have to make sure is cleared after them. */
}

/* Display glyphs in the right outside margin, right inside margin and
   right whitespace area. */

static void
create_right_glyph_block (struct window *w, struct display_line *dl)
{
  Lisp_Object window;

  int use_overflow = (NILP (w->use_right_overflow) ? 0 : 1);
  int elt, end_xpos;
  int out_start, in_out_end, in_in_start, white_out_end, white_in_end;
  int out_cnt, in_out_cnt, in_in_cnt, white_out_cnt, white_in_cnt;

  struct display_block *odb, *idb;

  XSETWINDOW (window, w);

  /* We have to add the glyphs to the line in the order outside,
     inside, whitespace.  However the precedence dictates that we
     determine how many will fit in the reverse order. */

  /* Determine how many whitespace glyphs we can display and where
     they should start. */
  white_in_end = dl->bounds.right_white;
  white_out_end = dl->bounds.right_in;
  white_out_cnt = white_in_cnt = 0;
  elt = 0;

  while (elt < Dynarr_length (dl->right_glyphs))
    {
      struct glyph_block *gb = Dynarr_atp (dl->right_glyphs, elt);

      if (NILP (gb->extent))
	abort ();	/* these should have been handled in add_glyph_rune */

      if (extent_end_glyph_layout (XEXTENT (gb->extent)) == GL_WHITESPACE)
	{
	  int width = glyph_width (gb->glyph, Qnil, gb->findex, window);

	  if (white_in_end + width <= dl->bounds.right_in)
	    {
	      white_in_cnt++;
	      white_in_end += width;
	      gb->width = width;
	      gb->active = 1;
	    }
	  else if (use_overflow
		   && (white_out_end + width <= dl->bounds.right_out))
	    {
	      white_out_cnt++;
	      white_out_end += width;
	      gb->width = width;
	      gb->active = 1;
	    }
	  else
	    gb->active = 0;
	}

      elt++;
    }

  /* Determine how many inside margin glyphs we can display and where
     they should start.  The inside margin glyphs get whatever space
     is left after the whitespace glyphs have been displayed.  These
     are tricky to calculate since if we decide to use the overflow
     area we basically have to start over.  So for these we build up a
     list of just the inside margin glyphs and manipulate it to
     determine the needed info. */
  {
    glyph_block_dynarr *ib;
    int avail_in, avail_out;
    int done = 0;
    int marker = 0;
    int used_in, used_out;

    elt = 0;
    used_in = used_out = 0;
    ib = Dynarr_new (glyph_block);
    while (elt < Dynarr_length (dl->right_glyphs))
      {
	struct glyph_block *gb = Dynarr_atp (dl->right_glyphs, elt);

	if (NILP (gb->extent))
	  abort ();	/* these should have been handled in add_glyph_rune */

	if (extent_end_glyph_layout (XEXTENT (gb->extent)) == GL_INSIDE_MARGIN)
	  {
	    gb->width = glyph_width (gb->glyph, Qnil, gb->findex, window);
	    used_in += gb->width;
	    Dynarr_add (ib, *gb);
	  }

	elt++;
      }

    if (white_out_cnt)
      avail_in = 0;
    else
      avail_in = dl->bounds.right_in - white_in_end;

    if (!use_overflow)
      avail_out = 0;
    else
      avail_out = dl->bounds.right_out - white_out_end;

    marker = 0;
    while (!done && marker < Dynarr_length (ib))
      {
	int width = Dynarr_atp (ib, marker)->width;

	/* If everything now fits in the available inside margin
           space, we're done. */
	if (used_in <= avail_in)
	  done = 1;
	else
	  {
	    /* Otherwise see if we have room to move a glyph to the
               outside. */
	    if (used_out + width <= avail_out)
	      {
		used_out += width;
		used_in -= width;
	      }
	    else
	      done = 1;
	  }

	if (!done)
	  marker++;
      }

    /* At this point we now know that everything from marker on goes in
       the inside margin and everything before it goes in the outside
       margin.  The stuff going into the outside margin is guaranteed
       to fit, but we may have to trim some stuff from the inside. */

    in_in_start = dl->bounds.right_in;
    in_out_end = dl->bounds.right_in;
    in_out_cnt = in_in_cnt = 0;

    Dynarr_free (ib);
    elt = 0;
    while (elt < Dynarr_length (dl->right_glyphs))
      {
	struct glyph_block *gb = Dynarr_atp (dl->right_glyphs, elt);

	if (NILP (gb->extent))
	  abort ();	/* these should have been handled in add_glyph_rune */

	if (extent_end_glyph_layout (XEXTENT (gb->extent)) == GL_INSIDE_MARGIN)
	  {
	    int width = glyph_width (gb->glyph, Qnil, gb->findex, window);

	    if (used_out)
	      {
		in_out_cnt++;
		in_out_end += width;
		gb->width = width;
		gb->active = 1;
		used_out -= width;
	      }
	    else if (in_in_start - width >= white_in_end)
	      {
		in_in_cnt++;
		in_in_start -= width;
		gb->width = width;
		gb->active = 1;
	      }
	    else
	      gb->active = 0;
	  }

	elt++;
      }
  }

  /* Determine how many outside margin glyphs we can display.  They
     always start at the right outside margin and can only use the
     outside margin space. */
  out_start = dl->bounds.right_out;
  out_cnt = 0;
  elt = 0;

  while (elt < Dynarr_length (dl->right_glyphs))
    {
      struct glyph_block *gb = Dynarr_atp (dl->right_glyphs, elt);

      if (NILP (gb->extent))
	abort ();	/* these should have been handled in add_glyph_rune */

      if (extent_end_glyph_layout (XEXTENT (gb->extent)) == GL_OUTSIDE_MARGIN)
	{
	  int width = glyph_width (gb->glyph, Qnil, gb->findex, window);

	  if (out_start - width >= in_out_end)
	    {
	      out_cnt++;
	      out_start -= width;
	      gb->width = width;
	      gb->active = 1;
	    }
	  else
	    gb->active = 0;
	}

      elt++;
    }

  /* Now that we now where everything goes, we add the glyphs as runes
     to the appropriate display blocks. */
  if (out_cnt || in_out_cnt || white_out_cnt)
    {
      odb = get_display_block_from_line (dl, RIGHT_OUTSIDE_MARGIN);
      /* #### See comments before odb->start_pos init in
         create_left_glyph_block */
      odb->start_pos = dl->bounds.right_in;
      odb->end_pos = dl->bounds.right_out;
      Dynarr_reset (odb->runes);
    }
  else
    odb = 0;

  if (in_in_cnt || white_in_cnt)
    {
      idb = get_display_block_from_line (dl, RIGHT_INSIDE_MARGIN);
      idb->start_pos = dl->bounds.right_white;
      /* #### See comments before odb->start_pos init in
         create_left_glyph_block */
      idb->end_pos = dl->bounds.right_in;
      Dynarr_reset (idb->runes);
    }
  else
    idb = 0;

  /* First add the whitespace margin glyphs which are actually in the
     inside margin. */
  if (white_in_cnt)
    {
      end_xpos = add_margin_runes (dl, idb, dl->bounds.right_white,
				   white_in_cnt, GL_WHITESPACE, RIGHT_GLYPHS,
				   window);
    }
  else
    end_xpos = dl->bounds.right_white;

  /* Make sure that the area between the end of the whitespace glyphs
     and the inside margin glyphs is cleared. */
  if (in_in_cnt && (in_in_start - end_xpos))
    {
      add_margin_blank (dl, idb, w, end_xpos, in_in_start - end_xpos,
			RIGHT_GLYPHS);
    }

  /* Next add the inside margin glyphs which are actually in the
     inside margin. */
  if (in_in_cnt)
    {
      end_xpos = add_margin_runes (dl, idb, in_in_start, in_in_cnt,
				   GL_INSIDE_MARGIN, RIGHT_GLYPHS, window);
    }

  /* If we didn't add any inside margin glyphs then make sure the rest
     of the inside margin area gets cleared. */
  if (idb && (dl->bounds.right_in - end_xpos))
    {
      add_margin_blank (dl, idb, w, end_xpos, dl->bounds.right_in - end_xpos,
			RIGHT_GLYPHS);
    }

  /* Next add any whitespace glyphs in the outside margin. */
  if (white_out_cnt)
    {
      end_xpos = add_margin_runes (dl, odb, dl->bounds.right_in, white_out_cnt,
				   GL_WHITESPACE, RIGHT_GLYPHS, window);
    }
  else
    end_xpos = dl->bounds.right_in;

  /* Next add any inside margin glyphs in the outside margin. */
  if (in_out_cnt)
    {
      end_xpos = add_margin_runes (dl, odb, end_xpos, in_out_cnt,
				   GL_INSIDE_MARGIN, RIGHT_GLYPHS, window);
    }

  /* There may be space between any whitespace or inside margin glyphs
     in the outside margin and the actual outside margin glyphs. */
  if (odb && (out_start - end_xpos))
    {
      add_margin_blank (dl, odb, w, end_xpos, out_start - end_xpos,
			RIGHT_GLYPHS);
    }

  /* Finally, add the outside margin glyphs. */
  if (out_cnt)
    {
      add_margin_runes (dl, odb, out_start, out_cnt, GL_OUTSIDE_MARGIN,
			RIGHT_GLYPHS, window);
    }
}


/***************************************************************************/
/*									   */
/*                            modeline routines                            */
/*									   */
/***************************************************************************/

/* Ensure that the given display line DL accurately represents the
   modeline for the given window. */

static void
generate_modeline (struct window *w, struct display_line *dl, int type)
{
  struct buffer *b = XBUFFER (w->buffer);
  struct frame *f = XFRAME (w->frame);
  struct device *d = XDEVICE (f->device);

  /* Unlike display line and rune pointers, this one can't change underneath
     our feet. */
  struct display_block *db = get_display_block_from_line (dl, TEXT);
  int max_pixpos, min_pixpos, ypos_adj;
  Lisp_Object font_inst;

  /* This will actually determine incorrect inside boundaries for the
     modeline since it ignores the margins.  However being aware of this fact
     we never use those values anywhere so it doesn't matter. */
  dl->bounds = calculate_display_line_boundaries (w, 1);

  /* We are generating a modeline. */
  dl->modeline = 1;
  dl->cursor_elt = -1;

  /* Reset the runes on the modeline. */
  Dynarr_reset (db->runes);

  if (!WINDOW_HAS_MODELINE_P (w))
    {
      struct rune rb;

      /* If there is a horizontal scrollbar, don't add anything. */
      if (window_scrollbar_height (w))
	return;

      dl->ascent = DEVMETH (d, divider_height, ());
      dl->descent = 0;
      /* The modeline is at the bottom of the gutters. */
      dl->ypos = WINDOW_BOTTOM (w);

      rb.findex = MODELINE_INDEX;
      rb.xpos = dl->bounds.left_out;
      rb.width = dl->bounds.right_out - dl->bounds.left_out;
      rb.bufpos = 0;
      rb.endpos = 0;
      rb.type = RUNE_HLINE;
      rb.object.hline.thickness = 1;
      rb.object.hline.yoffset = 0;
      rb.cursor_type = NO_CURSOR;

      if (!EQ (Qzero, w->modeline_shadow_thickness)
	  && FRAME_WIN_P (f))
	{
	  int shadow_thickness = MODELINE_SHADOW_THICKNESS (w);

	  dl->ypos -= shadow_thickness;
	  rb.xpos += shadow_thickness;
	  rb.width -= 2 * shadow_thickness;
	}

      Dynarr_add (db->runes, rb);
      return;
    }

  /* !!#### not right; needs to compute the max height of
     all the charsets */
  font_inst = WINDOW_FACE_CACHEL_FONT (w, MODELINE_INDEX, Vcharset_ascii);

  dl->ascent = XFONT_INSTANCE (font_inst)->ascent;
  dl->descent = XFONT_INSTANCE (font_inst)->descent;

  min_pixpos = dl->bounds.left_out;
  max_pixpos = dl->bounds.right_out;

  if (!EQ (Qzero, w->modeline_shadow_thickness) && FRAME_WIN_P (f))
    {
      int shadow_thickness = MODELINE_SHADOW_THICKNESS (w);

      ypos_adj = shadow_thickness;
      min_pixpos += shadow_thickness;
      max_pixpos -= shadow_thickness;
    }
  else
    ypos_adj = 0;

  generate_formatted_string_db (b->modeline_format,
				b->generated_modeline_string, w, dl, db,
				MODELINE_INDEX, min_pixpos, max_pixpos, type);

  /* The modeline is at the bottom of the gutters.  We have to wait to
     set this until we've generated the modeline in order to account
     for any embedded faces. */
  dl->ypos = WINDOW_BOTTOM (w) - dl->descent - ypos_adj;
}

static void
generate_formatted_string_db (Lisp_Object format_str, Lisp_Object result_str,
                              struct window *w, struct display_line *dl,
                              struct display_block *db, face_index findex,
                              int min_pixpos, int max_pixpos, int type)
{
  struct frame *f = XFRAME (w->frame);
  struct device *d = XDEVICE (f->device);

  pos_data data;
  int c_pixpos;

  xzero (data);
  data.d = d;
  data.db = db;
  data.dl = dl;
  data.findex = findex;
  data.pixpos = min_pixpos;
  data.max_pixpos = max_pixpos;
  data.cursor_type = NO_CURSOR;
  data.last_charset = Qunbound;
  data.last_findex = DEFAULT_INDEX;
  data.result_str = result_str;
  data.is_modeline = 1;
  XSETWINDOW (data.window, w);

  Dynarr_reset (formatted_string_extent_dynarr);
  Dynarr_reset (formatted_string_extent_start_dynarr);
  Dynarr_reset (formatted_string_extent_end_dynarr);

  /* This recursively builds up the modeline. */
  generate_fstring_runes (w, &data, 0, 0, -1, format_str, 0,
                          max_pixpos - min_pixpos, findex, type);

  if (Dynarr_length (db->runes))
    {
      struct rune *rb =
        Dynarr_atp (db->runes, Dynarr_length (db->runes) - 1);
      c_pixpos = rb->xpos + rb->width;
    }
  else
    c_pixpos = min_pixpos;

  /* If we don't reach the right side of the window, add a blank rune
     to make up the difference.  This usually only occurs if the
     modeline face is using a proportional width font or a fixed width
     font of a different size from the default face font. */

  if (c_pixpos < max_pixpos)
    {
      data.pixpos = c_pixpos;
      data.blank_width = max_pixpos - data.pixpos;

      add_blank_rune (&data, NULL, 0);
    }

  /* Now create the result string and frob the extents into it. */
  if (!NILP (result_str))
    {
      int elt;
      Bytecount len;
      Bufbyte *strdata;
      struct buffer *buf = XBUFFER (WINDOW_BUFFER (w));

      detach_all_extents (result_str);
      resize_string (XSTRING (result_str), -1,
                     data.bytepos - XSTRING_LENGTH (result_str));

      strdata = XSTRING_DATA (result_str);

      for (elt = 0, len = 0; elt < Dynarr_length (db->runes); elt++)
        {
          if (Dynarr_atp (db->runes, elt)->type == RUNE_CHAR)
            {
              len += (set_charptr_emchar
                      (strdata + len, Dynarr_atp (db->runes,
                                                  elt)->object.chr.ch));
            }
        }

      for (elt = 0; elt < Dynarr_length (formatted_string_extent_dynarr);
           elt++)
        {
          Lisp_Object extent = Qnil;
          Lisp_Object child;

          XSETEXTENT (extent, Dynarr_at (formatted_string_extent_dynarr, elt));
          child = Fgethash (extent, buf->modeline_extent_table, Qnil);
          if (NILP (child))
            {
              child = Fmake_extent (Qnil, Qnil, result_str);
              Fputhash (extent, child, buf->modeline_extent_table);
            }
          Fset_extent_parent (child, extent);
          set_extent_endpoints
            (XEXTENT (child),
             Dynarr_at (formatted_string_extent_start_dynarr, elt),
             Dynarr_at (formatted_string_extent_end_dynarr, elt),
             result_str);
        }
    }
}

static Charcount
add_string_to_fstring_db_runes (pos_data *data, CONST Bufbyte *str,
                                Charcount pos, Charcount min_pos, Charcount max_pos)
{
  /* This function has been Mule-ized. */
  Charcount end;
  CONST Bufbyte *cur_pos = str;
  struct display_block *db = data->db;

  data->blank_width = space_width (XWINDOW (data->window));
  while (Dynarr_length (db->runes) < pos)
    add_blank_rune (data, NULL, 0);

  end = (Dynarr_length (db->runes) +
         bytecount_to_charcount (str, strlen ((CONST char *) str)));
  if (max_pos != -1)
    end = min (max_pos, end);

  while (pos < end && *cur_pos)
    {
      CONST Bufbyte *old_cur_pos = cur_pos;
      int succeeded;

      data->ch = charptr_emchar (cur_pos);
      succeeded = (add_emchar_rune (data) != ADD_FAILED);
      INC_CHARPTR (cur_pos);
      if (succeeded)
        {
          pos++;
          data->modeline_charpos++;
          data->bytepos += cur_pos - old_cur_pos;
        }
    }

  while (Dynarr_length (db->runes) < min_pos &&
         (data->pixpos + data->blank_width <= data->max_pixpos))
    add_blank_rune (data, NULL, 0);

  return Dynarr_length (db->runes);
}

/* #### Urk!  Should also handle begin-glyphs and end-glyphs in
   modeline extents. */
static Charcount
add_glyph_to_fstring_db_runes (pos_data *data, Lisp_Object glyph,
                               Charcount pos, Charcount min_pos, Charcount max_pos)
{
  /* This function has been Mule-ized. */
  Charcount end;
  struct display_block *db = data->db;
  struct glyph_block gb;

  data->blank_width = space_width (XWINDOW (data->window));
  while (Dynarr_length (db->runes) < pos)
    add_blank_rune (data, NULL, 0);

  end = Dynarr_length (db->runes) + 1;
  if (max_pos != -1)
    end = min (max_pos, end);

  gb.glyph = glyph;
  gb.extent = Qnil;
  add_glyph_rune (data, &gb, BEGIN_GLYPHS, 0, 0);
  pos++;

  while (Dynarr_length (db->runes) < pos &&
         (data->pixpos + data->blank_width <= data->max_pixpos))
    add_blank_rune (data, NULL, 0);

  return Dynarr_length (db->runes);
}

/* If max_pos is == -1, it is considered to be infinite.  The same is
   true of max_pixsize. */
#define SET_CURRENT_MODE_CHARS_PIXSIZE                                  \
  if (Dynarr_length (data->db->runes))                                  \
    cur_pixsize = data->pixpos - Dynarr_atp (data->db->runes, 0)->xpos; \
  else                                                                  \
    cur_pixsize = 0;

/* Note that this function does "positions" in terms of characters and
   not in terms of columns.  This is necessary to make the formatting
   work correctly when proportional width fonts are used in the
   modeline. */
static Charcount
generate_fstring_runes (struct window *w, pos_data *data, Charcount pos,
                        Charcount min_pos, Charcount max_pos,
                        Lisp_Object elt, int depth, int max_pixsize,
                        face_index findex, int type)
{
  /* This function has been Mule-ized. */
  /* #### The other losing things in this function are:

     -- C zero-terminated-string lossage.
     -- Non-printable characters should be converted into something
        appropriate (e.g. ^F) instead of blindly being printed anyway.
   */

tail_recurse:
  if (depth > 10)
    goto invalid;

  depth++;

  if (STRINGP (elt))
    {
      /* A string.  Add to the display line and check for %-constructs
         within it. */

      Bufbyte *this = XSTRING_DATA (elt);

      while ((pos < max_pos || max_pos == -1) && *this)
        {
          Bufbyte *last = this;

          while (*this && *this != '%')
            this++;

          if (this != last)
            {
              /* The string is just a string. */
              Charcount size =
                bytecount_to_charcount (last, this - last) + pos;
              Charcount tmp_max = (max_pos == -1 ? size : min (size, max_pos));

              pos = add_string_to_fstring_db_runes (data, last, pos, pos,
                                                    tmp_max);
            }
          else /* *this == '%' */
            {
              Charcount spec_width = 0;

              this++; /* skip over '%' */

              /* We can't allow -ve args due to the "%-" construct.
               * Argument specifies minwidth but not maxwidth
               * (maxwidth can be specified by
               * (<negative-number> . <stuff>) modeline elements)
               */
              while (isdigit (*this))
                {
                  spec_width = spec_width * 10 + (*this - '0');
                  this++;
                }
              spec_width += pos;

              if (*this == 'M')
                {
                  pos = generate_fstring_runes (w, data, pos, spec_width,
                                                max_pos, Vglobal_mode_string,
                                                depth, max_pixsize, findex,
                                                type);
                }
              else if (*this == '-')
                {
                  Charcount num_to_add;

                  if (max_pixsize < 0)
                    num_to_add = 0;
                  else if (max_pos != -1)
                    num_to_add = max_pos - pos;
                  else
                    {
                      int cur_pixsize;
                      int dash_pixsize;
                      Bufbyte ch = '-';
                      SET_CURRENT_MODE_CHARS_PIXSIZE;

                      dash_pixsize =
                        redisplay_text_width_string (w, findex, &ch, Qnil, 0,
                                                     1);

                      num_to_add = (max_pixsize - cur_pixsize) / dash_pixsize;
                      num_to_add++;
                    }

                  while (num_to_add--)
                    pos = add_string_to_fstring_db_runes
                      (data, (CONST Bufbyte *) "-", pos, pos, max_pos);
                }
              else if (*this != 0)
                {
                  Bufbyte *str;
                  Emchar ch = charptr_emchar (this);
                  decode_mode_spec (w, ch, type);

                  str = Dynarr_atp (mode_spec_bufbyte_string, 0);
                  pos = add_string_to_fstring_db_runes (data,str, pos, pos,
                                                        max_pos);
                }

              /* NOT this++.  There could be any sort of character at
                 the current position. */
              INC_CHARPTR (this);
            }

          if (max_pixsize > 0)
            {
              int cur_pixsize;
              SET_CURRENT_MODE_CHARS_PIXSIZE;

              if (cur_pixsize >= max_pixsize)
                break;
            }
        }
    }
  else if (SYMBOLP (elt))
    {
      /* A symbol: process the value of the symbol recursively
         as if it appeared here directly. */
      Lisp_Object tem = symbol_value_in_buffer (elt, w->buffer);

      if (!UNBOUNDP (tem))
        {
          /* If value is a string, output that string literally:
             don't check for % within it.  */
          if (STRINGP (tem))
            {
              pos =
                add_string_to_fstring_db_runes
                (data, XSTRING_DATA (tem), pos, min_pos, max_pos);
            }
          /* Give up right away for nil or t.  */
          else if (!EQ (tem, elt))
            {
              elt = tem;
              goto tail_recurse;
            }
        }
    }
  else if (GENERIC_SPECIFIERP (elt))
    {
      Lisp_Object window, tem;
      XSETWINDOW (window, w);
      tem = specifier_instance_no_quit (elt, Qunbound, window,
					ERROR_ME_NOT, 0, Qzero);
      if (!UNBOUNDP (tem))
	{
	  elt = tem;
	  goto tail_recurse;
	}
    }
  else if (CONSP (elt))
    {
      /* A cons cell: four distinct cases.
       * If first element is a string or a cons, process all the elements
       * and effectively concatenate them.
       * If first element is a negative number, truncate displaying cdr to
       * at most that many characters.  If positive, pad (with spaces)
       * to at least that many characters.
       * If first element is a symbol, process the cadr or caddr recursively
       * according to whether the symbol's value is non-nil or nil.
       * If first element is a face, process the cdr recursively
       * without altering the depth.
       */
      Lisp_Object car, tem;

      car = XCAR (elt);
      if (SYMBOLP (car))
        {
          elt = XCDR (elt);
          if (!CONSP (elt))
            goto invalid;
          tem = symbol_value_in_buffer (car, w->buffer);
          /* elt is now the cdr, and we know it is a cons cell.
             Use its car if CAR has a non-nil value.  */
          if (!UNBOUNDP (tem))
            {
              if (!NILP (tem))
                {
                  elt = XCAR (elt);
                  goto tail_recurse;
                }
            }
          /* Symbol's value is nil (or symbol is unbound)
           * Get the cddr of the original list
           * and if possible find the caddr and use that.
           */
          elt = XCDR (elt);
          if (NILP (elt))
            ;
          else if (!CONSP (elt))
            goto invalid;
          else
            {
              elt = XCAR (elt);
              goto tail_recurse;
            }
        }
      else if (INTP (car))
        {
          Charcount lim = XINT (car);

          elt = XCDR (elt);

          if (lim < 0)
            {
              /* Negative int means reduce maximum width.
               * DO NOT change MIN_PIXPOS here!
               * (20 -10 . foo) should truncate foo to 10 col
               * and then pad to 20.
               */
              if (max_pos == -1)
                max_pos = pos - lim;
              else
                max_pos = min (max_pos, pos - lim);
            }
          else if (lim > 0)
            {
              /* Padding specified.  Don't let it be more than
               * current maximum.
               */
              lim += pos;
              if (max_pos != -1 && lim > max_pos)
                lim = max_pos;
              /* If that's more padding than already wanted, queue it.
               * But don't reduce padding already specified even if
               * that is beyond the current truncation point.
               */
              if (lim > min_pos)
                min_pos = lim;
            }
          goto tail_recurse;
        }
      else if (STRINGP (car) || CONSP (car))
        {
          int limit = 50;
          /* LIMIT is to protect against circular lists.  */
          while (CONSP (elt) && --limit > 0
                 && (pos < max_pos || max_pos == -1))
            {
              pos = generate_fstring_runes (w, data, pos, pos, max_pos,
                                            XCAR (elt), depth,
                                            max_pixsize, findex, type);
              elt = XCDR (elt);
            }
        }
      else if (EXTENTP (car))
        {
          struct extent *ext = XEXTENT (car);

          if (EXTENT_LIVE_P (ext))
            {
              face_index old_findex = data->findex;
              Lisp_Object face;
              Lisp_Object font_inst;
              face_index new_findex;
              Bytecount start = data->bytepos;

              face = extent_face (ext);
              if (FACEP (face))
                {
                  /* #### needs to merge faces, sigh */
                  /* #### needs to handle list of faces */
                  new_findex = get_builtin_face_cache_index (w, face);
                  /* !!#### not right; needs to compute the max height of
                     all the charsets */
                  font_inst = WINDOW_FACE_CACHEL_FONT (w, new_findex,
                                                       Vcharset_ascii);

                  data->dl->ascent = max (data->dl->ascent,
                                          XFONT_INSTANCE (font_inst)->ascent);
                  data->dl->descent = max (data->dl->descent,
                                           XFONT_INSTANCE (font_inst)->
                                           descent);
                }
              else
                new_findex = old_findex;

              data->findex = new_findex;
              pos = generate_fstring_runes (w, data, pos, pos, max_pos,
                                            XCDR (elt), depth - 1,
                                            max_pixsize, new_findex, type);
              data->findex = old_findex;
              Dynarr_add (formatted_string_extent_dynarr, ext);
              Dynarr_add (formatted_string_extent_start_dynarr, start);
              Dynarr_add (formatted_string_extent_end_dynarr, data->bytepos);
            }
        }
    }
  else if (GLYPHP (elt))
    {
      pos = add_glyph_to_fstring_db_runes (data, elt, pos, pos, max_pos);
    }
  else
    {
    invalid:
      pos =
        add_string_to_fstring_db_runes
          (data, (CONST Bufbyte *) GETTEXT ("*invalid*"), pos, min_pos,
           max_pos);
    }

  if (min_pos > pos)
    {
      add_string_to_fstring_db_runes (data, (CONST Bufbyte *) "", pos, min_pos,
                                      -1);
    }

  return pos;
}

/* The caller is responsible for freeing the returned string. */
Bufbyte *
generate_formatted_string (struct window *w, Lisp_Object format_str,
			   Lisp_Object result_str, face_index findex, int type)
{
  struct display_line *dl;
  struct display_block *db;
  int elt = 0;

  dl = &formatted_string_display_line;
  db = get_display_block_from_line (dl, TEXT);
  Dynarr_reset (db->runes);

  generate_formatted_string_db (format_str, result_str, w, dl, db, findex, 0,
                                -1, type);

  Dynarr_reset (formatted_string_emchar_dynarr);
  while (elt < Dynarr_length (db->runes))
    {
      if (Dynarr_atp (db->runes, elt)->type == RUNE_CHAR)
	Dynarr_add (formatted_string_emchar_dynarr,
		    Dynarr_atp (db->runes, elt)->object.chr.ch);
      elt++;
    }

  return
    convert_emchar_string_into_malloced_string
    ( Dynarr_atp (formatted_string_emchar_dynarr, 0),
      Dynarr_length (formatted_string_emchar_dynarr), 0);
}

/* Update just the modeline.  Assumes the desired display structs.  If
   they do not have a modeline block, it does nothing. */
static void
regenerate_modeline (struct window *w)
{
  display_line_dynarr *dla = window_display_lines (w, DESIRED_DISP);

  if (!Dynarr_length (dla) || !Dynarr_atp (dla, 0)->modeline)
    return;
  else
    {
      generate_modeline (w, Dynarr_atp (dla, 0), DESIRED_DISP);
      redisplay_update_line (w, 0, 0, 0);
    }
}

/* Make sure that modeline display line is present in the given
   display structs if the window has a modeline and update that
   line.  Returns true if a modeline was needed. */
static int
ensure_modeline_generated (struct window *w, int type)
{
  int need_modeline;

  /* minibuffer windows don't have modelines */
  if (MINI_WINDOW_P (w))
    need_modeline = 0;
  /* windows which haven't had it turned off do */
  else if (WINDOW_HAS_MODELINE_P (w))
    need_modeline = 1;
  /* windows which have it turned off don't have a divider if there is
     a horizontal scrollbar */
  else if (window_scrollbar_height (w))
    need_modeline = 0;
  /* and in this case there is none */
  else
    need_modeline = 1;

  if (need_modeline)
    {
      display_line_dynarr *dla;

      dla = window_display_lines (w, type);

      /* We don't care if there is a display line which is not
         currently a modeline because it is definitely going to become
         one if we have gotten to this point. */
      if (Dynarr_length (dla) == 0)
	{
	  if (Dynarr_largest (dla) > 0)
	    {
	      struct display_line *mlp = Dynarr_atp (dla, 0);
	      Dynarr_add (dla, *mlp);
	    }
	  else
	    {
	      struct display_line modeline;
	      xzero (modeline);
	      Dynarr_add (dla, modeline);
	    }
	}

      /* If we're adding a new place marker go ahead and generate the
         modeline so that it is available for use by
         window_modeline_height. */
      generate_modeline (w, Dynarr_atp (dla, 0), type);
    }

  return need_modeline;
}

/* #### Kludge or not a kludge.  I tend towards the former. */
int
real_current_modeline_height (struct window *w)
{
  Fset_marker (w->start[CMOTION_DISP],  w->start[CURRENT_DISP],  w->buffer);
  Fset_marker (w->pointm[CMOTION_DISP], w->pointm[CURRENT_DISP], w->buffer);

  if (ensure_modeline_generated (w, CMOTION_DISP))
    {
      display_line_dynarr *dla = window_display_lines (w, CMOTION_DISP);

      if (Dynarr_length (dla))
	{
	  if (Dynarr_atp (dla, 0)->modeline)
	    return (Dynarr_atp (dla, 0)->ascent +
		    Dynarr_atp (dla, 0)->descent);
	}
    }
  return 0;
}


/***************************************************************************/
/*									   */
/*                        window-regeneration routines                     */
/*									   */
/***************************************************************************/

/* For a given window and starting position in the buffer it contains,
   ensure that the TYPE display lines accurately represent the
   presentation of the window.  We pass the buffer instead of getting
   it from the window since redisplay_window may have temporarily
   changed it to the echo area buffer. */

static void
regenerate_window (struct window *w, Bufpos start_pos, Bufpos point, int type)
{
  struct frame *f = XFRAME (w->frame);
  struct buffer *b = XBUFFER (w->buffer);
  int ypos = WINDOW_TEXT_TOP (w);
  int yend;	/* set farther down */

  prop_block_dynarr *prop;
  layout_bounds bounds;
  display_line_dynarr *dla;
  int need_modeline;

  /* The lines had better exist by this point. */
  if (!(dla = window_display_lines (w, type)))
    abort ();
  Dynarr_reset (dla);
  w->max_line_len = 0;

  /* Normally these get updated in redisplay_window but it is possible
     for this function to get called from some other points where that
     update may not have occurred.  This acts as a safety check. */
  if (!Dynarr_length (w->face_cachels))
    reset_face_cachels (w);
  if (!Dynarr_length (w->glyph_cachels))
    reset_glyph_cachels (w);

  Fset_marker (w->start[type], make_int (start_pos), w->buffer);
  Fset_marker (w->pointm[type], make_int (point), w->buffer);
  w->last_point_x[type] = -1;
  w->last_point_y[type] = -1;

  /* Make sure a modeline is in the structs if needed. */
  need_modeline = ensure_modeline_generated (w, type);

  /* Wait until here to set this so that the structs have a modeline
     generated in the case where one didn't exist. */
  yend = WINDOW_TEXT_BOTTOM (w);

  bounds = calculate_display_line_boundaries (w, 0);

  /* 97/3/14 jhod: stuff added here to support pre-prompts (used for input systems) */
  if (MINI_WINDOW_P (w)
      && (!NILP (Vminibuf_prompt) || !NILP (Vminibuf_preprompt))
      && !echo_area_active (f)
      && start_pos == BUF_BEGV (b))
    {
      struct prop_block pb;
      Lisp_Object string;
      prop = Dynarr_new (prop_block);

      string = concat2(Vminibuf_preprompt, Vminibuf_prompt);
      pb.type = PROP_MINIBUF_PROMPT;
      pb.data.p_string.str = XSTRING_DATA(string);
      pb.data.p_string.len = XSTRING_LENGTH(string);
      Dynarr_add (prop, pb);
    }
  else
    prop = 0;

  while (ypos < yend)
    {
      struct display_line dl;
      struct display_line *dlp;
      int local;

      if (Dynarr_length (dla) < Dynarr_largest (dla))
	{
	  dlp = Dynarr_atp (dla, Dynarr_length (dla));
	  local = 0;
	}
      else
	{
	  xzero (dl);
	  dlp = &dl;
	  local = 1;
	}

      dlp->bounds = bounds;
      dlp->offset = 0;
      start_pos = generate_display_line (w, dlp, 1, start_pos,
					 w->hscroll, &prop, type);
      dlp->ypos = ypos + dlp->ascent;
      ypos = dlp->ypos + dlp->descent;

      if (ypos > yend)
	{
	  int visible_height = dlp->ascent + dlp->descent;

	  dlp->clip = (ypos - yend);
	  visible_height -= dlp->clip;

	  if (visible_height < VERTICAL_CLIP (w, 1))
	    {
	      if (local)
		free_display_line (dlp);
	      break;
	    }
	}
      else
	dlp->clip = 0;

      if (dlp->cursor_elt != -1)
	{
	  /* #### This check is steaming crap.  Have to get things
             fixed so when create_text_block hits EOB, we're done,
             period. */
	  if (w->last_point_x[type] == -1)
	    {
	      w->last_point_x[type] = dlp->cursor_elt;
	      w->last_point_y[type] = Dynarr_length (dla);
	    }
	  else
	    {
	      /* #### This means that we've added a cursor at EOB
                 twice.  Yuck oh yuck. */
	      struct display_block *db =
		get_display_block_from_line (dlp, TEXT);

	      Dynarr_atp (db->runes, dlp->cursor_elt)->cursor_type = NO_CURSOR;
	      dlp->cursor_elt = -1;
	    }
	}

      if (dlp->num_chars > w->max_line_len)
	w->max_line_len = dlp->num_chars;

      Dynarr_add (dla, *dlp);

      /* #### This isn't right, but it is close enough for now. */
      w->window_end_pos[type] = start_pos;

      /* #### This type of check needs to be done down in the
	 generate_display_line call. */
      if (start_pos > BUF_ZV (b))
	break;
    }

  if (prop)
    Dynarr_free (prop);

  /* #### More not quite right, but close enough. */
  /* #### Ben sez: apparently window_end_pos[] is measured
     as the number of characters between the window end and the
     end of the buffer?  This seems rather weirdo.  What's
     the justification for this? */
  w->window_end_pos[type] = BUF_Z (b) - w->window_end_pos[type];

  if (need_modeline)
    {
      /* We know that this is the right thing to use because we put it
         there when we first started working in this function. */
      generate_modeline (w, Dynarr_atp (dla, 0), type);
    }
}

#define REGEN_INC_FIND_START_END		\
  do {						\
    /* Determine start and end of lines. */	\
    if (!Dynarr_length (cdla))			\
      return 0;					\
    else					\
      {						\
	if (Dynarr_atp (cdla, 0)->modeline && Dynarr_atp (ddla, 0)->modeline) \
	  {					\
	    dla_start = 1;			\
	  }					\
	else if (!Dynarr_atp (cdla, 0)->modeline \
		 && !Dynarr_atp (ddla, 0)->modeline) \
	  {					\
	    dla_start = 0;			\
	  }					\
	else					\
	  abort ();	/* structs differ */	\
						\
	dla_end = Dynarr_length (cdla) - 1;	\
      }						\
						\
    start_pos = (Dynarr_atp (cdla, dla_start)->bufpos \
		 + Dynarr_atp (cdla, dla_start)->offset); \
    /* If this isn't true, then startp has changed and we need to do a \
       full regen. */				\
    if (startp != start_pos)			\
      return 0;					\
						\
    /* Point is outside the visible region so give up. */ \
    if (pointm < start_pos)			\
      return 0;					\
						\
  } while (0)

/* This attempts to incrementally update the display structures.  It
   returns a boolean indicating success or failure.  This function is
   very similar to regenerate_window_incrementally and is in fact only
   called from that function.  However, because of the nature of the
   changes it deals with it sometimes makes different assumptions
   which can lead to success which are much more difficult to make
   when dealing with buffer changes. */

static int
regenerate_window_extents_only_changed (struct window *w, Bufpos startp,
					Bufpos pointm,
					Charcount beg_unchanged,
					Charcount end_unchanged)
{
  struct buffer *b = XBUFFER (w->buffer);
  display_line_dynarr *cdla = window_display_lines (w, CURRENT_DISP);
  display_line_dynarr *ddla = window_display_lines (w, DESIRED_DISP);

  int dla_start = 0;
  int dla_end, line;
  int first_line, last_line;
  Bufpos start_pos;
  /* Don't define this in the loop where it is used because we
     definitely want its value to survive between passes. */
  prop_block_dynarr *prop = NULL;

  /* If we don't have any buffer change recorded but the modiff flag has
     been incremented, then fail.  I'm not sure of the exact circumstances
     under which this can happen, but I believe that it is probably a
     reasonable happening. */
  if (!point_visible (w, pointm, CURRENT_DISP)
      || XINT (w->last_modified[CURRENT_DISP]) < BUF_MODIFF (b))
    return 0;

  /* If the cursor is moved we attempt to update it.  If we succeed we
     go ahead and proceed with the optimization attempt. */
  if (!EQ (Fmarker_buffer (w->last_point[CURRENT_DISP]), w->buffer)
      || pointm != marker_position (w->last_point[CURRENT_DISP]))
    {
      struct frame *f = XFRAME (w->frame);
      struct device *d = XDEVICE (f->device);
      struct frame *sel_f = device_selected_frame (d);
      int success = 0;

      if (w->last_point_x[CURRENT_DISP] != -1
	  && w->last_point_y[CURRENT_DISP] != -1)
	{

	  if (redisplay_move_cursor (w, pointm, WINDOW_TTY_P (w)))
	    {
	      /* Always regenerate the modeline in case it is
                 displaying the current line or column. */
	      regenerate_modeline (w);
	      success = 1;
	    }
	}
      else if (w != XWINDOW (FRAME_SELECTED_WINDOW (sel_f)))
	{
	  if (f->modeline_changed)
	    regenerate_modeline (w);
	  success = 1;
	}

      if (!success)
	return 0;
    }

  if (beg_unchanged == -1 && end_unchanged == -1)
    return 1;

  /* assert: There are no buffer modifications or they are all below the
     visible region.  We assume that regenerate_window_incrementally has
     not called us unless this is true.  */

  REGEN_INC_FIND_START_END;

  /* If the changed are starts before the visible area, give up. */
  if (beg_unchanged < startp)
    return 0;

  /* Find what display line the extent changes first affect. */
  line = dla_start;
  while (line <= dla_end)
    {
      struct display_line *dl = Dynarr_atp (cdla, line);
      Bufpos lstart = dl->bufpos + dl->offset;
      Bufpos lend = dl->end_bufpos + dl->offset;

      if (beg_unchanged >= lstart && beg_unchanged <= lend)
	break;

      line++;
    }

  /* If the changes are below the visible area then if point hasn't
     moved return success otherwise fail in order to be safe. */
  if (line > dla_end)
    {
      if (EQ (Fmarker_buffer (w->last_point[CURRENT_DISP]), w->buffer)
	  && pointm == marker_position (w->last_point[CURRENT_DISP]))
	return 1;
      else
	return 0;
    }

  /* At this point we know what line the changes first affect.  We now
     begin redrawing lines as long as we are still in the affected
     region and the line's size and positioning don't change.
     Otherwise we fail.  If we fail we will have altered the desired
     structs which could lead to an assertion failure.  However, if we
     fail the next thing that is going to happen is a full regen so we
     will actually end up being safe. */
  w->last_modified[DESIRED_DISP] = make_int (BUF_MODIFF (b));
  w->last_facechange[DESIRED_DISP] = make_int (BUF_FACECHANGE (b));
  Fset_marker (w->last_start[DESIRED_DISP], make_int (startp), w->buffer);
  Fset_marker (w->last_point[DESIRED_DISP], make_int (pointm), w->buffer);

  first_line = last_line = line;
  while (line <= dla_end)
    {
      Bufpos old_start, old_end, new_start;
      struct display_line *cdl = Dynarr_atp (cdla, line);
      struct display_line *ddl = Dynarr_atp (ddla, line);
      struct display_block *db;
      int initial_size;

      assert (cdl->bufpos == ddl->bufpos);
      assert (cdl->end_bufpos == ddl->end_bufpos);
      assert (cdl->offset == ddl->offset);

      db = get_display_block_from_line (ddl, TEXT);
      initial_size = Dynarr_length (db->runes);
      old_start = ddl->bufpos + ddl->offset;
      old_end = ddl->end_bufpos + ddl->offset;

      /* If this is the first line being updated and it used
         propagation data, fail.  Otherwise we'll be okay because
         we'll have the necessary propagation data. */
      if (line == first_line && ddl->used_prop_data)
	return 0;

      new_start = generate_display_line (w, ddl, 0, ddl->bufpos + ddl->offset,
					 w->hscroll, &prop, DESIRED_DISP);
      ddl->offset = 0;

      /* #### If there is propagated stuff the fail.  We could
         probably actually deal with this if the line had propagated
         information when originally created by a full
         regeneration. */
      if (prop)
	{
	  Dynarr_free (prop);
	  return 0;
	}

      /* If any line position parameters have changed or a
         cursor has disappeared or disappeared, fail.  */
      db = get_display_block_from_line (ddl, TEXT);
      if (cdl->ypos != ddl->ypos
	  || cdl->ascent != ddl->ascent
	  || cdl->descent != ddl->descent
	  || (cdl->cursor_elt != -1 && ddl->cursor_elt == -1)
	  || (cdl->cursor_elt == -1 && ddl->cursor_elt != -1)
	  || old_start != ddl->bufpos
	  || old_end != ddl->end_bufpos
	  || initial_size != Dynarr_length (db->runes))
	{
	  return 0;
	}

      if (ddl->cursor_elt != -1)
	{
	  w->last_point_x[DESIRED_DISP] = ddl->cursor_elt;
	  w->last_point_y[DESIRED_DISP] = line;
	}

      last_line = line;

      /* If the extent changes end on the line we just updated then
         we're done.  Otherwise go on to the next line. */
      if (end_unchanged <= ddl->end_bufpos)
	break;
      else
	line++;
    }

  redisplay_update_line (w, first_line, last_line, 1);
  return 1;
}

/* Attempt to update the display data structures based on knowledge of
   the changed region in the buffer.  Returns a boolean indicating
   success or failure.  If this function returns a failure then a
   regenerate_window _must_ be performed next in order to maintain
   invariants located here. */

static int
regenerate_window_incrementally (struct window *w, Bufpos startp,
				 Bufpos pointm)
{
  struct buffer *b = XBUFFER (w->buffer);
  display_line_dynarr *cdla = window_display_lines (w, CURRENT_DISP);
  display_line_dynarr *ddla = window_display_lines (w, DESIRED_DISP);
  Charcount beg_unchanged, end_unchanged;
  Charcount extent_beg_unchanged, extent_end_unchanged;

  int dla_start = 0;
  int dla_end, line;
  Bufpos start_pos;

  /* If this function is called, the current and desired structures
     had better be identical.  If they are not, then that is a bug. */
  assert (Dynarr_length (cdla) == Dynarr_length (ddla));

  /* We don't handle minibuffer windows yet.  The minibuffer prompt
     screws us up. */
  if (MINI_WINDOW_P (w))
    return 0;

  extent_beg_unchanged = BUF_EXTENT_BEGIN_UNCHANGED (b);
  extent_end_unchanged = (BUF_EXTENT_END_UNCHANGED (b) == -1
			  ? -1
			  : BUF_Z (b) - BUF_EXTENT_END_UNCHANGED (b));

  /* If nothing has changed in the buffer, then make sure point is ok
     and succeed. */
  if (BUF_BEGIN_UNCHANGED (b) == -1 && BUF_END_UNCHANGED (b) == -1)
    return regenerate_window_extents_only_changed (w, startp, pointm,
						   extent_beg_unchanged,
						   extent_end_unchanged);

  /* We can't deal with deleted newlines. */
  if (BUF_NEWLINE_WAS_DELETED (b))
    return 0;

  beg_unchanged = BUF_BEGIN_UNCHANGED (b);
  end_unchanged = (BUF_END_UNCHANGED (b) == -1
		   ? -1
		   : BUF_Z (b) - BUF_END_UNCHANGED (b));

  REGEN_INC_FIND_START_END;

  /* If the changed area starts before the visible area, give up. */
  if (beg_unchanged < startp)
    return 0;

  /* Find what display line the buffer changes first affect. */
  line = dla_start;
  while (line <= dla_end)
    {
      struct display_line *dl = Dynarr_atp (cdla, line);
      Bufpos lstart = dl->bufpos + dl->offset;
      Bufpos lend = dl->end_bufpos + dl->offset;

      if (beg_unchanged >= lstart && beg_unchanged <= lend)
	break;

      line++;
    }

  /* If the changes are below the visible area then if point hasn't
     moved return success otherwise fail in order to be safe. */
  if (line > dla_end)
    return regenerate_window_extents_only_changed (w, startp, pointm,
						   extent_beg_unchanged,
						   extent_end_unchanged);
  else
    /* At this point we know what line the changes first affect.  We
       now redraw that line.  If the changes are contained within it
       we are going to succeed and can update just that one line.
       Otherwise we fail.  If we fail we will have altered the desired
       structs which could lead to an assertion failure.  However, if
       we fail the next thing that is going to happen is a full regen
       so we will actually end up being safe. */
    {
      Bufpos new_start;
      prop_block_dynarr *prop = NULL;
      struct display_line *cdl = Dynarr_atp (cdla, line);
      struct display_line *ddl = Dynarr_atp (ddla, line);

      assert (cdl->bufpos == ddl->bufpos);
      assert (cdl->end_bufpos == ddl->end_bufpos);
      assert (cdl->offset == ddl->offset);

      /* If the last rune is already a continuation glyph, fail.
         #### We should be able to handle this better. */
      {
	struct display_block *db = get_display_block_from_line (ddl, TEXT);
	if (Dynarr_length (db->runes))
	  {
	    struct rune *rb =
	      Dynarr_atp (db->runes, Dynarr_length (db->runes) - 1);

	    if (rb->type == RUNE_DGLYPH
		&& EQ (rb->object.dglyph.glyph, Vcontinuation_glyph))
	      return 0;
	  }
      }

      /* If the line was generated using propagation data, fail. */
      if (ddl->used_prop_data)
	return 0;

      new_start = generate_display_line (w, ddl, 0, ddl->bufpos + ddl->offset,
					 w->hscroll, &prop, DESIRED_DISP);
      ddl->offset = 0;

      /* If there is propagated stuff then it is pretty much a
         guarantee that more than just the one line is affected. */
      if (prop)
	{
	  Dynarr_free (prop);
	  return 0;
	}

      /* If the last rune is now a continuation glyph, fail. */
      {
	struct display_block *db = get_display_block_from_line (ddl, TEXT);
	if (Dynarr_length (db->runes))
	  {
	    struct rune *rb =
	      Dynarr_atp (db->runes, Dynarr_length (db->runes) - 1);

	    if (rb->type == RUNE_DGLYPH
		&& EQ (rb->object.dglyph.glyph, Vcontinuation_glyph))
	      return 0;
	  }
      }

      /* If any line position parameters have changed or a
         cursor has disappeared or disappeared, fail. */
      if (cdl->ypos != ddl->ypos
	  || cdl->ascent != ddl->ascent
	  || cdl->descent != ddl->descent
	  || (cdl->cursor_elt != -1 && ddl->cursor_elt == -1)
	  || (cdl->cursor_elt == -1 && ddl->cursor_elt != -1))
	{
	  return 0;
	}

      /* If the changed area also ends on this line, then we may be in
         business.  Update everything and return success. */
      if (end_unchanged >= ddl->bufpos && end_unchanged <= ddl->end_bufpos)
	{
	  w->last_modified[DESIRED_DISP] = make_int (BUF_MODIFF (b));
	  w->last_facechange[DESIRED_DISP] = make_int (BUF_FACECHANGE (b));
	  Fset_marker (w->last_start[DESIRED_DISP], make_int (startp),
		       w->buffer);
	  Fset_marker (w->last_point[DESIRED_DISP], make_int (pointm),
		       w->buffer);

	  if (ddl->cursor_elt != -1)
	    {
	      w->last_point_x[DESIRED_DISP] = ddl->cursor_elt;
	      w->last_point_y[DESIRED_DISP] = line;
	    }

	  redisplay_update_line (w, line, line, 1);
	  regenerate_modeline (w);

	  /* #### For now we just flush the cache until this has been
             tested.  After that is done, this should correct the
             cache directly. */
	  Dynarr_reset (w->line_start_cache);

	  /* Adjust the extent changed boundaries to remove any
             overlap with the buffer changes since we've just
             successfully updated that area. */
	  if (extent_beg_unchanged != -1
	      && extent_beg_unchanged >= beg_unchanged
	      && extent_beg_unchanged < end_unchanged)
	    extent_beg_unchanged = end_unchanged;

	  if (extent_end_unchanged != -1
	      && extent_end_unchanged >= beg_unchanged
	      && extent_end_unchanged < end_unchanged)
	    extent_end_unchanged = beg_unchanged - 1;

	  if (extent_end_unchanged <= extent_beg_unchanged)
	    extent_beg_unchanged = extent_end_unchanged = -1;

	  /* This could lead to odd results if it fails, but since the
             buffer changes update succeeded this probably will to.
             We already know that the extent changes start at or after
             the line because we checked before entering the loop. */
	  if (extent_beg_unchanged != -1
	      && extent_end_unchanged != -1
	      && ((extent_beg_unchanged < ddl->bufpos)
		  || (extent_end_unchanged > ddl->end_bufpos)))
	    return regenerate_window_extents_only_changed (w, startp, pointm,
							   extent_beg_unchanged,
							   extent_end_unchanged);
	  else
	    return 1;
	}
    }

  /* Oh, well. */
  return 0;
}

/* Given a window and a point, update the given display lines such
   that point is displayed in the middle of the window.
   Return the window's new start position. */

static Bufpos
regenerate_window_point_center (struct window *w, Bufpos point, int type)
{
  Bufpos startp;

  /* We need to make sure that the modeline is generated so that the
     window height can be calculated correctly. */
  ensure_modeline_generated (w, type);

  startp = start_with_line_at_pixpos (w, point, window_half_pixpos (w));
  regenerate_window (w, startp, point, type);
  Fset_marker (w->start[type], make_int (startp), w->buffer);

  return startp;
}

/* Given a window and a set of display lines, return a boolean
   indicating whether the given point is contained within. */

static int
point_visible (struct window *w, Bufpos point, int type)
{
  struct buffer *b = XBUFFER (w->buffer);
  display_line_dynarr *dla = window_display_lines (w, type);
  int first_line;

  if (Dynarr_length (dla) && Dynarr_atp (dla, 0)->modeline)
    first_line = 1;
  else
    first_line = 0;

  if (Dynarr_length (dla) > first_line)
    {
      Bufpos start, end;
      struct display_line *dl = Dynarr_atp (dla, first_line);

      start = dl->bufpos;
      end = BUF_Z (b) - w->window_end_pos[type] - 1;

      if (point >= start && point <= end)
	{
	  if (!MINI_WINDOW_P (w) && scroll_on_clipped_lines)
	    {
	      dl = Dynarr_atp (dla, Dynarr_length (dla) - 1);

	      if (point >= (dl->bufpos + dl->offset)
		  && point <= (dl->end_bufpos + dl->offset))
		return !dl->clip;
	      else
		return 1;
	    }
	  else
	    return 1;
	}
      else
	return 0;
    }
  else
    return 0;
}

/* Return pixel position the middle of the window, not including the
   modeline and any potential horizontal scrollbar. */

int
window_half_pixpos (struct window *w)
{
  return WINDOW_TEXT_TOP (w) + (WINDOW_TEXT_HEIGHT (w) >> 1);
}

/* Return the display line which is currently in the middle of the
   window W for display lines TYPE. */

int
line_at_center (struct window *w, int type, Bufpos start, Bufpos point)
{
  display_line_dynarr *dla;
  int half;
  int elt;
  int first_elt = (MINI_WINDOW_P (w) ? 0 : 1);

  if (type == CMOTION_DISP)
    regenerate_window (w, start, point, type);

  dla = window_display_lines (w, type);
  half = window_half_pixpos (w);

  for (elt = first_elt; elt < Dynarr_length (dla); elt++)
    {
      struct display_line *dl = Dynarr_atp (dla, elt);
      int line_bot = dl->ypos + dl->descent;

      if (line_bot > half)
	return elt;
    }

  /* We may not have a line at the middle if the end of the buffer is
     being displayed. */
  return -1;
}

/* Return a value for point that would place it at the beginning of
   the line which is in the middle of the window. */

Bufpos
point_at_center (struct window *w, int type, Bufpos start, Bufpos point)
{
  /* line_at_center will regenerate the display structures, if necessary. */
  int line = line_at_center (w, type, start, point);

  if (line == -1)
    return BUF_ZV (XBUFFER (w->buffer));
  else
    {
      display_line_dynarr *dla = window_display_lines (w, type);
      struct display_line *dl = Dynarr_atp (dla, line);

      return dl->bufpos;
    }
}

/* For a given window, ensure that the current visual representation
   is accurate. */

static void
redisplay_window (Lisp_Object window, int skip_selected)
{
  struct window *w = XWINDOW (window);
  struct frame *f = XFRAME (w->frame);
  struct device *d = XDEVICE (f->device);
  Lisp_Object old_buffer = w->buffer;
  Lisp_Object the_buffer = w->buffer;
  struct buffer *b;
  int echo_active = 0;
  int startp = 1;
  int pointm;
  int old_startp = 1;
  int old_pointm = 1;
  int selected_in_its_frame;
  int selected_globally;
  int skip_output = 0;
  int truncation_changed;
  int inactive_minibuffer =
    (MINI_WINDOW_P (w) &&
     (f != device_selected_frame (d)) &&
     !is_surrogate_for_selected_frame (f));

  /* #### In the new world this function actually does a bunch of
     optimizations such as buffer-based scrolling, but none of that is
     implemented yet. */

  /* If this is a combination window, do its children; that's all.
     The selected window is always a leaf so we don't check for
     skip_selected here. */
  if (!NILP (w->vchild))
    {
      redisplay_windows (w->vchild, skip_selected);
      return;
    }
  if (!NILP (w->hchild))
    {
      redisplay_windows (w->hchild, skip_selected);
      return;
    }

  /* Is this window the selected window on its frame? */
  selected_in_its_frame = (w == XWINDOW (FRAME_SELECTED_WINDOW (f)));
  selected_globally =
      selected_in_its_frame &&
      EQ(DEVICE_CONSOLE(d), Vselected_console) &&
      XDEVICE(CONSOLE_SELECTED_DEVICE(XCONSOLE(DEVICE_CONSOLE(d)))) == d &&
      XFRAME(DEVICE_SELECTED_FRAME(d)) == f;
  if (skip_selected && selected_in_its_frame)
    return;

  /* It is possible that the window is not fully initialized yet. */
  if (NILP (w->buffer))
    return;

  if (MINI_WINDOW_P (w) && echo_area_active (f))
    {
      w->buffer = the_buffer = Vecho_area_buffer;
      echo_active = 1;
    }

  b = XBUFFER (w->buffer);

  if (echo_active)
    {
      old_pointm = selected_globally
                   ? BUF_PT (b)
                   : marker_position (w->pointm[CURRENT_DISP]);
      pointm = 1;
    }
  else
    {
      if (selected_globally)
	{
	  pointm = BUF_PT (b);
	}
      else
	{
	  pointm = marker_position (w->pointm[CURRENT_DISP]);

	  if (pointm < BUF_BEGV (b))
	    pointm = BUF_BEGV (b);
	  else if (pointm > BUF_ZV (b))
	    pointm = BUF_ZV (b);
	}
    }
  Fset_marker (w->pointm[DESIRED_DISP], make_int (pointm), the_buffer);

  /* If the buffer has changed we have to invalid all of our face
     cache elements. */
  if ((!echo_active && b != window_display_buffer (w))
      || !Dynarr_length (w->face_cachels)
      || f->faces_changed)
    reset_face_cachels (w);
  else
    mark_face_cachels_as_not_updated (w);

  /* Ditto the glyph cache elements. */
  if ((!echo_active && b != window_display_buffer (w))
      || !Dynarr_length (w->glyph_cachels)
      || f->glyphs_changed)
    reset_glyph_cachels (w);
  else
    mark_glyph_cachels_as_not_updated (w);

  /* If the marker's buffer is not the window's buffer, then we need
     to find a new starting position. */
  if (!MINI_WINDOW_P (w)
      && !EQ (Fmarker_buffer (w->start[CURRENT_DISP]), w->buffer))
    {
      startp = regenerate_window_point_center (w, pointm, DESIRED_DISP);

      goto regeneration_done;
    }

  if (echo_active)
    {
      old_startp = marker_position (w->start[CURRENT_DISP]);
      startp = 1;
    }
  else
    {
      startp = marker_position (w->start[CURRENT_DISP]);
      if (startp < BUF_BEGV (b))
	startp = BUF_BEGV (b);
      else if (startp > BUF_ZV (b))
	startp = BUF_ZV (b);
    }
  Fset_marker (w->start[DESIRED_DISP], make_int (startp), the_buffer);

  truncation_changed = (find_window_mirror (w)->truncate_win !=
			window_truncation_on (w));

  /* If w->force_start is set, then some function set w->start and we
     should display from there and change point, if necessary, to
     ensure that it is visible. */
  if (w->force_start || inactive_minibuffer)
    {
      w->force_start = 0;
      w->last_modified[DESIRED_DISP] = Qzero;
      w->last_facechange[DESIRED_DISP] = Qzero;

      regenerate_window (w, startp, pointm, DESIRED_DISP);

      if (!point_visible (w, pointm, DESIRED_DISP) && !inactive_minibuffer)
	{
	  pointm = point_at_center (w, DESIRED_DISP, 0, 0);

	  if (selected_globally)
	    BUF_SET_PT (b, pointm);

	  Fset_marker (w->pointm[DESIRED_DISP], make_int (pointm),
		       the_buffer);

	  /* #### BUFU amounts of overkill just to get the cursor
             location marked properly.  FIX ME FIX ME FIX ME */
	  regenerate_window (w, startp, pointm, DESIRED_DISP);
	}

      goto regeneration_done;
    }

  /* If nothing has changed since the last redisplay, then we just
     need to make sure that point is still visible. */
  if (XINT (w->last_modified[CURRENT_DISP]) >= BUF_MODIFF (b)
      && XINT (w->last_facechange[CURRENT_DISP]) >= BUF_FACECHANGE (b)
      && pointm >= startp
      /* This check is to make sure we restore the minibuffer after a
         temporary change to the echo area. */
      && !(MINI_WINDOW_P (w) && f->buffers_changed)
      && !f->frame_changed
      && !truncation_changed
      /* check whether start is really at the begining of a line  GE */
      && (!w->start_at_line_beg || beginning_of_line_p (b, startp))
      )
    {
      /* Check if the cursor has actually moved. */
      if (EQ (Fmarker_buffer (w->last_point[CURRENT_DISP]), w->buffer)
	  && pointm == marker_position (w->last_point[CURRENT_DISP])
	  && selected_globally
	  && !w->windows_changed
	  && !f->clip_changed
	  && !f->extents_changed
	  && !f->faces_changed
	  && !f->glyphs_changed
	  && !f->subwindows_changed
	  && !f->point_changed
	  && !f->windows_structure_changed)
	{
	  /* If not, we're done. */
	  if (f->modeline_changed)
	    regenerate_modeline (w);

	  skip_output = 1;
	  goto regeneration_done;
	}
      else
	{
	  /* If the new point is visible in the redisplay structures,
             then let the output update routines handle it, otherwise
             do things the hard way. */
	  if (!w->windows_changed
	      && !f->clip_changed
	      && !f->extents_changed
	      && !f->faces_changed
	      && !f->glyphs_changed
	      && !f->subwindows_changed
	      && !f->windows_structure_changed)
	    {
	      if (point_visible (w, pointm, CURRENT_DISP)
		  && w->last_point_x[CURRENT_DISP] != -1
		  && w->last_point_y[CURRENT_DISP] != -1)
		{
		  if (redisplay_move_cursor (w, pointm, FRAME_TTY_P (f)))
		    {
		      /* Always regenerate in case it is displaying
                         the current line or column. */
		      regenerate_modeline (w);

		      skip_output = 1;
		      goto regeneration_done;
		    }
		}
	      else if (!selected_in_its_frame && !f->point_changed)
		{
		  if (f->modeline_changed)
		    regenerate_modeline (w);

		  skip_output = 1;
		  goto regeneration_done;
		}
	    }

	  /* If we weren't able to take the shortcut method, then use
             the brute force method. */
	  regenerate_window (w, startp, pointm, DESIRED_DISP);

	  if (point_visible (w, pointm, DESIRED_DISP))
	    goto regeneration_done;
	}
    }

  /* Check if the starting point is no longer at the beginning of a
     line, in which case find a new starting point.  We also recenter
     if our start position is equal to point-max.  Otherwise we'll end
     up with a blank window. */
  else if (((w->start_at_line_beg || MINI_WINDOW_P (w))
	    && !(startp == BUF_BEGV (b)
		 || BUF_FETCH_CHAR (b, startp - 1) == '\n'))
	   || (pointm == startp &&
	       EQ (Fmarker_buffer (w->last_start[CURRENT_DISP]), w->buffer) &&
	       startp < marker_position (w->last_start[CURRENT_DISP]))
	   || (startp == BUF_ZV (b)))
    {
      startp = regenerate_window_point_center (w, pointm, DESIRED_DISP);

      goto regeneration_done;
    }
  /* See if we can update the data structures locally based on
     knowledge of what changed in the buffer. */
  else if (!w->windows_changed
	   && !f->clip_changed
	   && !f->faces_changed
	   && !f->glyphs_changed
	   && !f->subwindows_changed
	   && !f->windows_structure_changed
	   && !f->frame_changed
	   && !truncation_changed
	   && pointm >= startp
	   && regenerate_window_incrementally (w, startp, pointm))
    {
      if (f->modeline_changed
	  || XINT (w->last_modified[CURRENT_DISP]) < BUF_MODIFF (b)
	  || XINT (w->last_facechange[CURRENT_DISP]) < BUF_FACECHANGE (b))
	regenerate_modeline (w);

      skip_output = 1;
      goto regeneration_done;
    }
  /* #### This is where a check for structure based scrolling would go. */
  /* If all else fails, try just regenerating and see what happens. */
  else
    {
      regenerate_window (w, startp, pointm, DESIRED_DISP);

      if (point_visible (w, pointm, DESIRED_DISP))
	goto regeneration_done;
    }

  /* We still haven't gotten the window regenerated with point
     visible.  Next we try scrolling a little and see if point comes
     back onto the screen. */
  if (scroll_step > 0)
    {
      int scrolled = scroll_conservatively;
      for (; scrolled >= 0; scrolled -= scroll_step)
	{
	  startp = vmotion (w, startp,
			    (pointm < startp) ? -scroll_step : scroll_step, 0);
	  regenerate_window (w, startp, pointm, DESIRED_DISP);

	  if (point_visible (w, pointm, DESIRED_DISP))
	    goto regeneration_done;
	}
    }

  /* We still haven't managed to get the screen drawn with point on
     the screen, so just center it and be done with it. */
  startp = regenerate_window_point_center (w, pointm, DESIRED_DISP);


regeneration_done:

  /* If the window's frame is changed then reset the current display
     lines in order to force a full repaint. */
  if (f->frame_changed)
    {
      display_line_dynarr *cla = window_display_lines (w, CURRENT_DISP);

      Dynarr_reset (cla);
    }

  /* Must do this before calling redisplay_output_window because it
     sets some markers on the window. */
  if (echo_active)
    {
      w->buffer = old_buffer;
      Fset_marker (w->pointm[DESIRED_DISP], make_int (old_pointm), old_buffer);
      Fset_marker (w->start[DESIRED_DISP], make_int (old_startp), old_buffer);
    }

  /* These also have to be set before calling redisplay_output_window
     since it sets the CURRENT_DISP values based on them. */
  w->last_modified[DESIRED_DISP] = make_int (BUF_MODIFF (b));
  w->last_facechange[DESIRED_DISP] = make_int (BUF_FACECHANGE (b));
  Fset_marker (w->last_start[DESIRED_DISP], make_int (startp), w->buffer);
  Fset_marker (w->last_point[DESIRED_DISP], make_int (pointm), w->buffer);

  if (!skip_output)
    {
      Bufpos start = marker_position (w->start[DESIRED_DISP]);
      Bufpos end = (w->window_end_pos[DESIRED_DISP] == -1
		    ? BUF_ZV (b)
		    : BUF_Z (b) - w->window_end_pos[DESIRED_DISP] - 1);

      update_line_start_cache (w, start, end, pointm, 1);
      redisplay_output_window (w);
      /*
       * If we just displayed the echo area, the line start cache is
       * no longer valid, because the minibuffer window is associated
       * with the window now.
       */
      if (echo_active)
	w->line_cache_last_updated = make_int (-1);
    }

  /* #### This should be dependent on face changes and will need to be
     somewhere else once tty updates occur on a per-frame basis. */
  mark_face_cachels_as_clean (w);

  w->windows_changed = 0;
}

/* Call buffer_reset_changes for all buffers present in any window
   currently visible in all frames on all devices.  #### There has to
   be a better way to do this. */

static int
reset_buffer_changes_mapfun (struct window *w, void *ignored_closure)
{
  buffer_reset_changes (XBUFFER (w->buffer));
  return 0;
}

static void
reset_buffer_changes (void)
{
  Lisp_Object frmcons, devcons, concons;

  FRAME_LOOP_NO_BREAK (frmcons, devcons, concons)
    {
      struct frame *f = XFRAME (XCAR (frmcons));

      if (FRAME_REPAINT_P (f))
	map_windows (f, reset_buffer_changes_mapfun, 0);
    }
}

/* Ensure that all windows underneath the given window in the window
   hierarchy are correctly displayed. */

static void
redisplay_windows (Lisp_Object window, int skip_selected)
{
  for (; !NILP (window) ; window = XWINDOW (window)->next)
    {
      redisplay_window (window, skip_selected);
    }
}

static int
call_redisplay_end_triggers (struct window *w, void *closure)
{
  Bufpos lrpos = w->last_redisplay_pos;
  w->last_redisplay_pos = 0;
  if (!NILP (w->buffer)
      && !NILP (w->redisplay_end_trigger)
      && lrpos > 0)
    {
      Bufpos pos;

      if (MARKERP (w->redisplay_end_trigger)
	  && XMARKER (w->redisplay_end_trigger)->buffer != 0)
	pos = marker_position (w->redisplay_end_trigger);
      else if (INTP (w->redisplay_end_trigger))
	pos = XINT (w->redisplay_end_trigger);
      else
	{
	  w->redisplay_end_trigger = Qnil;
	  return 0;
	}

      if (lrpos >= pos)
	{
	  Lisp_Object window;
	  XSETWINDOW (window, w);
	  va_run_hook_with_args_in_buffer (XBUFFER (w->buffer),
					   Qredisplay_end_trigger_functions,
					   2, window,
					   w->redisplay_end_trigger);
	  w->redisplay_end_trigger = Qnil;
	}
    }

  return 0;
}

/* Ensure that all windows on the given frame are correctly displayed. */

static int
redisplay_frame (struct frame *f, int preemption_check)
{
  struct device *d = XDEVICE (f->device);

  if (preemption_check)
    {
      /* The preemption check itself takes a lot of time,
	 so normally don't do it here.  We do it if called
	 from Lisp, though (`redisplay-frame'). */
      int preempted;

      REDISPLAY_PREEMPTION_CHECK;
      if (preempted)
	return 1;
    }

  /* Before we put a hold on frame size changes, attempt to process
     any which are already pending. */
  if (f->size_change_pending)
    change_frame_size (f, f->new_height, f->new_width, 0);

  /* If frame size might need to be changed, due to changed size
     of toolbars, scrollbars etc, change it now */
  if (f->size_slipped)
    {
      adjust_frame_size (f);
      assert (!f->size_slipped);
    }

  /* The menubar, toolbar, and icon updates must be done before
     hold_frame_size_changes is called and we are officially
     'in_display'.  They may eval lisp code which may call Fsignal.
     If in_display is set Fsignal will abort. */

#ifdef HAVE_MENUBARS
  /* Update the menubar.  It is done first since it could change
     the menubar's visibility.  This way we avoid having flashing
     caused by an Expose event generated by the visibility change
     being handled. */
  update_frame_menubars (f);
#endif /* HAVE_MENUBARS */
  /* widgets are similar to menus in that they can call lisp to
     determine activation etc. Therefore update them before we get
     into redisplay. This is primarily for connected widgets such as
     radio buttons. */
  update_frame_subwindows (f);
#ifdef HAVE_TOOLBARS
  /* Update the toolbars. */
  update_frame_toolbars (f);
#endif /* HAVE_TOOLBARS */

  hold_frame_size_changes ();

  /* ----------------- BEGIN CRITICAL REDISPLAY SECTION ---------------- */
  /* Within this section, we are defenseless and assume that the
     following cannot happen:

     1) garbage collection
     2) Lisp code evaluation
     3) frame size changes

     We ensure (3) by calling hold_frame_size_changes(), which
     will cause any pending frame size changes to get put on hold
     till after the end of the critical section.  (1) follows
     automatically if (2) is met.  #### Unfortunately, there are
     some places where Lisp code can be called within this section.
     We need to remove them.

     If Fsignal() is called during this critical section, we
     will abort().

     If garbage collection is called during this critical section,
     we simply return. #### We should abort instead.

     #### If a frame-size change does occur we should probably
     actually be preempting redisplay. */

  /* If we clear the frame we have to force its contents to be redrawn. */
  if (f->clear)
    f->frame_changed = 1;

  /* Erase the frame before outputting its contents. */
  if (f->clear)
    {
      DEVMETH (d, clear_frame, (f));
    }

  /* invalidate the subwindow cache. we are going to reuse the glyphs
     flag here to cause subwindows to get instantiated. This is
     because subwindows changed is less strict - dealing with things
     like the clicked state of button. */
  if (!Dynarr_length (f->subwindow_cachels)
      || f->glyphs_changed
      || f->frame_changed)
    reset_subwindow_cachels (f);
  else
    mark_subwindow_cachels_as_not_updated (f);

  /* Do the selected window first. */
  redisplay_window (FRAME_SELECTED_WINDOW (f), 0);

  /* Then do the rest. */
  redisplay_windows (f->root_window, 1);

  /* We now call the output_end routine for tty frames.  We delay
     doing so in order to avoid cursor flicker.  So much for 100%
     encapsulation. */
  if (FRAME_TTY_P (f))
    DEVMETH (d, output_end, (d));

  update_frame_title (f);

  f->buffers_changed  = 0;
  f->clip_changed     = 0;
  f->extents_changed  = 0;
  f->faces_changed    = 0;
  f->frame_changed    = 0;
  f->glyphs_changed   = 0;
  f->subwindows_changed   = 0;
  f->icon_changed     = 0;
  f->menubar_changed  = 0;
  f->modeline_changed = 0;
  f->point_changed    = 0;
  f->toolbar_changed  = 0;
  f->windows_changed  = 0;
  f->windows_structure_changed = 0;
  f->window_face_cache_reset = 0;
  f->echo_area_garbaged = 0;

  f->clear = 0;

  if (!f->size_change_pending)
    f->size_changed = 0;

  /* ----------------- END CRITICAL REDISPLAY SECTION ---------------- */

  /* Allow frame size changes to occur again.

     #### what happens if changes to other frames happen? */
  unhold_one_frame_size_changes (f);

  map_windows (f, call_redisplay_end_triggers, 0);
  return 0;
}

/* Ensure that all frames on the given device are correctly displayed. */

static int
redisplay_device (struct device *d)
{
  Lisp_Object frame, frmcons;
  int preempted = 0;
  int size_change_failed = 0;
  struct frame *f;

  if (DEVICE_STREAM_P (d)) /* nothing to do */
    return 0;

  /* It is possible that redisplay has been called before the
     device is fully initialized.  If so then continue with the
     next device. */
  if (NILP (DEVICE_SELECTED_FRAME (d)))
    return 0;

  REDISPLAY_PREEMPTION_CHECK;
  if (preempted)
    return 1;

  /* Always do the selected frame first. */
  frame = DEVICE_SELECTED_FRAME (d);

  f = XFRAME (frame);

  if (f->icon_changed || f->windows_changed)
    update_frame_icon (f);

  if (FRAME_REPAINT_P (f))
    {
      if (f->buffers_changed  || f->clip_changed  || f->extents_changed ||
	  f->faces_changed    || f->frame_changed || f->menubar_changed ||
	  f->modeline_changed || f->point_changed || f->size_changed    ||
	  f->toolbar_changed  || f->windows_changed || f->size_slipped  ||
	  f->windows_structure_changed || f->glyphs_changed || f->subwindows_changed)
	{
	  preempted = redisplay_frame (f, 0);
	}

      if (preempted)
	return 1;

      /* If the frame redisplay did not get preempted, then this flag
         should have gotten set to 0.  It might be possible for that
         not to happen if a size change event were to occur at an odd
         time.  To make sure we don't miss anything we simply don't
         reset the top level flags until the condition ends up being
         in the right state. */
      if (f->size_changed)
	size_change_failed = 1;
    }

  DEVICE_FRAME_LOOP (frmcons, d)
    {
      f = XFRAME (XCAR (frmcons));

      if (f == XFRAME (DEVICE_SELECTED_FRAME (d)))
	continue;

      if (f->icon_changed || f->windows_changed)
	update_frame_icon (f);

      if (FRAME_REPAINT_P (f))
	{
	  if (f->buffers_changed  || f->clip_changed  || f->extents_changed ||
	      f->faces_changed    || f->frame_changed || f->menubar_changed ||
	      f->modeline_changed || f->point_changed || f->size_changed    ||
	      f->toolbar_changed  || f->windows_changed ||
	      f->windows_structure_changed ||
	      f->glyphs_changed || f->subwindows_changed)
	    {
	      preempted = redisplay_frame (f, 0);
	    }

	  if (preempted)
	    return 1;

	  if (f->size_change_pending)
	    size_change_failed = 1;
	}
    }

  /* If we get here then we redisplayed all of our frames without
     getting preempted so mark ourselves as clean. */
  d->buffers_changed  = 0;
  d->clip_changed     = 0;
  d->extents_changed  = 0;
  d->faces_changed    = 0;
  d->frame_changed    = 0;
  d->glyphs_changed   = 0;
  d->subwindows_changed   = 0;
  d->icon_changed     = 0;
  d->menubar_changed  = 0;
  d->modeline_changed = 0;
  d->point_changed    = 0;
  d->toolbar_changed  = 0;
  d->windows_changed  = 0;
  d->windows_structure_changed = 0;

  if (!size_change_failed)
    d->size_changed = 0;

  return 0;
}

static Lisp_Object
restore_profiling_redisplay_flag (Lisp_Object val)
{
  profiling_redisplay_flag = XINT (val);
  return Qnil;
}

/* Ensure that all windows on all frames on all devices are displaying
   the current contents of their respective buffers. */

static void
redisplay_without_hooks (void)
{
  Lisp_Object devcons, concons;
  int size_change_failed = 0;
  int count = specpdl_depth ();

  if (profiling_active)
    {
      record_unwind_protect (restore_profiling_redisplay_flag,
			     make_int (profiling_redisplay_flag));
      profiling_redisplay_flag = 1;
    }

  if (asynch_device_change_pending)
    handle_asynch_device_change ();

  if (!buffers_changed && !clip_changed     && !extents_changed &&
      !faces_changed   && !frame_changed    && !icon_changed    &&
      !menubar_changed && !modeline_changed && !point_changed   &&
      !size_changed    && !toolbar_changed  && !windows_changed &&
      !glyphs_changed  && !subwindows_changed &&
      !windows_structure_changed && !disable_preemption &&
      preemption_count < max_preempts)
    goto done;

  DEVICE_LOOP_NO_BREAK (devcons, concons)
    {
      struct device *d = XDEVICE (XCAR (devcons));
      int preempted;

      if (d->buffers_changed  || d->clip_changed     || d->extents_changed ||
	  d->faces_changed    || d->frame_changed    || d->icon_changed    ||
	  d->menubar_changed  || d->modeline_changed || d->point_changed   ||
	  d->size_changed     || d->toolbar_changed  || d->windows_changed ||
	  d->windows_structure_changed ||
	  d->glyphs_changed || d->subwindows_changed)
	{
	  preempted = redisplay_device (d);

	  if (preempted)
	    {
	      preemption_count++;
	      RESET_CHANGED_SET_FLAGS;
	      goto done;
	    }

	  /* See comment in redisplay_device. */
	  if (d->size_changed)
	    size_change_failed = 1;
	}
    }
  preemption_count = 0;

  /* Mark redisplay as accurate */
  buffers_changed  = 0;
  clip_changed     = 0;
  extents_changed  = 0;
  frame_changed    = 0;
  glyphs_changed   = 0;
  subwindows_changed   = 0;
  icon_changed     = 0;
  menubar_changed  = 0;
  modeline_changed = 0;
  point_changed    = 0;
  toolbar_changed  = 0;
  windows_changed  = 0;
  windows_structure_changed = 0;
  RESET_CHANGED_SET_FLAGS;

  if (faces_changed)
    {
      mark_all_faces_as_clean ();
      faces_changed = 0;
    }

  if (!size_change_failed)
    size_changed = 0;

  reset_buffer_changes ();

 done:
  unbind_to (count, Qnil);
}

void
redisplay (void)
{
  if (last_display_warning_tick != display_warning_tick &&
      !inhibit_warning_display)
    {
      /* If an error occurs during this function, oh well.
         If we report another warning, we could get stuck in an
	 infinite loop reporting warnings. */
      call0_trapping_errors (0, Qdisplay_warning_buffer);
      last_display_warning_tick = display_warning_tick;
    }
  /* The run_hook_trapping_errors functions are smart enough not
     to do any evalling if the hook function is empty, so there
     should not be any significant time loss.  All places in the
     C code that call redisplay() are prepared to handle GCing,
     so we should be OK. */
#ifndef INHIBIT_REDISPLAY_HOOKS
  run_hook_trapping_errors ("Error in pre-redisplay-hook",
			    Qpre_redisplay_hook);
#endif /* INHIBIT_REDISPLAY_HOOKS */

  redisplay_without_hooks ();

#ifndef INHIBIT_REDISPLAY_HOOKS
  run_hook_trapping_errors ("Error in post-redisplay-hook",
			    Qpost_redisplay_hook);
#endif /* INHIBIT_REDISPLAY_HOOKS */
}


static char window_line_number_buf[32];

/* Efficiently determine the window line number, and return a pointer
   to its printed representation.  Do this regardless of whether
   line-number-mode is on.  The first line in the buffer is counted as
   1.  If narrowing is in effect, the lines are counted from the
   beginning of the visible portion of the buffer.  */
static char *
window_line_number (struct window *w, int type)
{
  struct device *d = XDEVICE (XFRAME (w->frame)->device);
  struct buffer *b = XBUFFER (w->buffer);
  /* Be careful in the order of these tests. The first clause will
     fail if DEVICE_SELECTED_FRAME == Qnil (since w->frame cannot be).
     This can occur when the frame title is computed really early */
  Bufpos pos =
    ((EQ(DEVICE_SELECTED_FRAME(d), w->frame) &&
       (w == XWINDOW (FRAME_SELECTED_WINDOW (device_selected_frame(d)))) &&
      EQ(DEVICE_CONSOLE(d), Vselected_console) &&
      XDEVICE(CONSOLE_SELECTED_DEVICE(XCONSOLE(DEVICE_CONSOLE(d)))) == d )
     ? BUF_PT (b)
     : marker_position (w->pointm[type]));
  EMACS_INT line;

  line = buffer_line_number (b, pos, 1);

  long_to_string (window_line_number_buf, line + 1);

  return window_line_number_buf;
}


/* Given a character representing an object in a modeline
   specification, return a string (stored into the global array
   `mode_spec_bufbyte_string') with the information that object
   represents.

   This function is largely unchanged from previous versions of the
   redisplay engine.

   Warning! This code is also used for frame titles and can be called
   very early in the device/frame update process!  JV
*/

static void
decode_mode_spec (struct window *w, Emchar spec, int type)
{
  Lisp_Object obj = Qnil;
  CONST char *str = NULL;
  struct buffer *b = XBUFFER (w->buffer);

  Dynarr_reset (mode_spec_bufbyte_string);

  switch (spec)
    {
      /* print buffer name */
    case 'b':
      obj = b->name;
      break;

      /* print visited file name */
    case 'f':
      obj = b->filename;
      break;

      /* print the current column */
    case 'c':
      {
        Bufpos pt = (w == XWINDOW (Fselected_window (Qnil)))
                    ? BUF_PT (b)
                    : marker_position (w->pointm[type]);
	int col = column_at_point (b, pt, 1) + !!column_number_start_at_one;
	char buf[32];

	long_to_string (buf, col);

	Dynarr_add_many (mode_spec_bufbyte_string,
			 (CONST Bufbyte *) buf, strlen (buf));

	goto decode_mode_spec_done;
      }
      /* print the file coding system */
    case 'C':
#ifdef FILE_CODING
      {
        Lisp_Object codesys = b->buffer_file_coding_system;
        /* Be very careful here not to get an error. */
	if (NILP (codesys) || SYMBOLP (codesys) || CODING_SYSTEMP (codesys))
          {
            codesys = Ffind_coding_system (codesys);
	    if (CODING_SYSTEMP (codesys))
              obj = XCODING_SYSTEM_MNEMONIC (codesys);
          }
      }
#endif /* FILE_CODING */
      break;

      /* print the current line number */
    case 'l':
      str = window_line_number (w, type);
      break;

      /* print value of mode-name (obsolete) */
    case 'm':
      obj = b->mode_name;
      break;

      /* print hyphen and frame number, if != 1 */
    case 'N':
#ifdef HAVE_TTY
      {
	struct frame *f = XFRAME (w->frame);
	if (FRAME_TTY_P (f) && f->order_count > 1 && f->order_count <= 99999999)
	  {
	    /* Naughty, naughty */
	    char * writable_str = alloca_array (char, 10);
	    sprintf (writable_str, "-%d", f->order_count);
	    str = writable_str;
	  }
      }
#endif /* HAVE_TTY */
      break;

      /* print Narrow if appropriate */
    case 'n':
      if (BUF_BEGV (b) > BUF_BEG (b)
	  || BUF_ZV (b) < BUF_Z (b))
	str = " Narrow";
      break;

      /* print %, * or hyphen, if buffer is read-only, modified or neither */
    case '*':
      str = (!NILP (b->read_only)
	     ? "%"
	     : ((BUF_MODIFF (b) > BUF_SAVE_MODIFF (b))
		? "*"
		: "-"));
      break;

      /* print * or hyphen -- XEmacs change to allow a buffer to be
         read-only but still indicate whether it is modified. */
    case '+':
      str = ((BUF_MODIFF (b) > BUF_SAVE_MODIFF (b))
	     ? "*"
	     : (!NILP (b->read_only)
		? "%"
		: "-"));
      break;

      /* #### defined in 19.29 decode_mode_spec, but not in
         modeline-format doc string. */
      /* This differs from %* in that it ignores read-only-ness. */
    case '&':
      str = ((BUF_MODIFF (b) > BUF_SAVE_MODIFF (b))
	     ? "*"
	     : "-");
      break;

      /* print process status */
    case 's':
      obj = Fget_buffer_process (w->buffer);
      if (NILP (obj))
	str = GETTEXT ("no process");
      else
	obj = Fsymbol_name (Fprocess_status (obj));
      break;

      /* Print name of selected frame.  */
    case 'S':
      obj = XFRAME (w->frame)->name;
      break;

      /* indicate TEXT or BINARY */
    case 't':
      /* #### NT does not use this any more. Now what? */
      str = "T";
      break;

      /* print percent of buffer above top of window, or Top, Bot or All */
    case 'p':
    {
      Bufpos pos = marker_position (w->start[type]);
      Charcount total = BUF_ZV (b) - BUF_BEGV (b);

      /* This had better be while the desired lines are being done. */
      if (w->window_end_pos[type] <= BUF_Z (b) - BUF_ZV (b))
	{
	  if (pos <= BUF_BEGV (b))
	    str = "All";
	  else
	    str = "Bottom";
	}
      else if (pos <= BUF_BEGV (b))
	str = "Top";
      else
	{
	  /* This hard limit is ok since the string it will hold has a
             fixed maximum length of 3.  But just to be safe... */
	  char buf[10];

	  total = ((pos - BUF_BEGV (b)) * 100 + total - 1) / total;

	  /* We can't normally display a 3-digit number, so get us a
             2-digit number that is close. */
	  if (total == 100)
	    total = 99;

	  sprintf (buf, "%2d%%", total);
	  Dynarr_add_many (mode_spec_bufbyte_string, (Bufbyte *) buf,
			   strlen (buf));

	  goto decode_mode_spec_done;
	}
      break;
    }

    /* print percent of buffer above bottom of window, perhaps plus
       Top, or print Bottom or All */
    case 'P':
    {
      Bufpos toppos = marker_position (w->start[type]);
      Bufpos botpos = BUF_Z (b) - w->window_end_pos[type];
      Charcount total = BUF_ZV (b) - BUF_BEGV (b);

      /* botpos is only accurate as of the last redisplay, so we can
         only treat it as a hint.  In particular, after erase-buffer,
         botpos may be negative. */
      if (botpos < toppos)
	botpos = toppos;

      if (botpos >= BUF_ZV (b))
	{
	  if (toppos <= BUF_BEGV (b))
	    str = "All";
	  else
	    str = "Bottom";
	}
      else
	{
	  /* This hard limit is ok since the string it will hold has a
             fixed maximum length of around 6.  But just to be safe... */
	  char buf[10];

	  total = ((botpos - BUF_BEGV (b)) * 100 + total - 1) / total;

	  /* We can't normally display a 3-digit number, so get us a
             2-digit number that is close. */
	  if (total == 100)
	    total = 99;

	  if (toppos <= BUF_BEGV (b))
	    sprintf (buf, "Top%2d%%", total);
	  else
	    sprintf (buf, "%2d%%", total);

	  Dynarr_add_many (mode_spec_bufbyte_string, (Bufbyte *) buf,
			   strlen (buf));

	  goto decode_mode_spec_done;
	}
      break;
    }

    /* print % */
    case '%':
      str = "%";
      break;

      /* print one [ for each recursive editing level. */
    case '[':
    {
      int i;

      if (command_loop_level > 5)
	{
	  str = "[[[... ";
	  break;
	}

      for (i = 0; i < command_loop_level; i++)
	Dynarr_add (mode_spec_bufbyte_string, '[');

      goto decode_mode_spec_done;
    }

    /* print one ] for each recursive editing level. */
    case ']':
    {
      int i;

      if (command_loop_level > 5)
	{
	  str = "...]]]";
	  break;
	}

      for (i = 0; i < command_loop_level; i++)
	Dynarr_add (mode_spec_bufbyte_string, ']');

      goto decode_mode_spec_done;
    }

    /* print infinitely many dashes -- handle at top level now */
    case '-':
      break;

    }

  if (STRINGP (obj))
    Dynarr_add_many (mode_spec_bufbyte_string,
		     XSTRING_DATA   (obj),
		     XSTRING_LENGTH (obj));
  else if (str)
    Dynarr_add_many (mode_spec_bufbyte_string, (Bufbyte *) str, strlen (str));

decode_mode_spec_done:
  Dynarr_add (mode_spec_bufbyte_string, '\0');
}

/* Given a display line, free all of its data structures. */

static void
free_display_line (struct display_line *dl)
{
  int block;

  if (dl->display_blocks)
    {
      for (block = 0; block < Dynarr_largest (dl->display_blocks); block++)
	{
	  struct display_block *db = Dynarr_atp (dl->display_blocks, block);

	  Dynarr_free (db->runes);
	}

      Dynarr_free (dl->display_blocks);
      dl->display_blocks = NULL;
    }

  if (dl->left_glyphs)
    {
      Dynarr_free (dl->left_glyphs);
      dl->left_glyphs = NULL;
    }

  if (dl->right_glyphs)
    {
      Dynarr_free (dl->right_glyphs);
      dl->right_glyphs = NULL;
    }
}


/* Given an array of display lines, free them and all data structures
   contained within them. */

static void
free_display_lines (display_line_dynarr *dla)
{
  int line;

  for (line = 0; line < Dynarr_largest (dla); line++)
    {
      free_display_line (Dynarr_atp (dla, line));
    }

  Dynarr_free (dla);
}

/* Call internal free routine for each set of display lines. */

void
free_display_structs (struct window_mirror *mir)
{
  if (mir->current_display_lines)
    {
      free_display_lines (mir->current_display_lines);
      mir->current_display_lines = 0;
    }

  if (mir->desired_display_lines)
    {
      free_display_lines (mir->desired_display_lines);
      mir->desired_display_lines = 0;
    }
}


static void
mark_glyph_block_dynarr (glyph_block_dynarr *gba, void (*markobj) (Lisp_Object))
{
  if (gba)
    {
      glyph_block *gb = Dynarr_atp (gba, 0);
      glyph_block *gb_last = Dynarr_atp (gba, Dynarr_length (gba));

      for (; gb < gb_last; gb++)
	{
	  if (!NILP (gb->glyph))
	    markobj (gb->glyph);
	  if (!NILP (gb->extent))
	    markobj (gb->extent);
	}
    }
}

static void
mark_redisplay_structs (display_line_dynarr *dla, void (*markobj) (Lisp_Object))
{
  display_line *dl = Dynarr_atp (dla, 0);
  display_line *dl_last = Dynarr_atp (dla, Dynarr_length (dla));

  for (; dl < dl_last; dl++)
    {
      display_block_dynarr *dba = dl->display_blocks;
      display_block *db = Dynarr_atp (dba, 0);
      display_block *db_last = Dynarr_atp (dba, Dynarr_length (dba));

      for (; db < db_last; db++)
	{
	  rune_dynarr *ra = db->runes;
	  rune *r = Dynarr_atp (ra, 0);
	  rune *r_last = Dynarr_atp (ra, Dynarr_length (ra));

	  for (; r < r_last; r++)
	    {
	      if (r->type == RUNE_DGLYPH)
		{
		  if (!NILP (r->object.dglyph.glyph))
		    markobj (r->object.dglyph.glyph);
		  if (!NILP (r->object.dglyph.extent))
		    markobj (r->object.dglyph.extent);
		}
	    }
	}

      mark_glyph_block_dynarr (dl->left_glyphs,  markobj);
      mark_glyph_block_dynarr (dl->right_glyphs, markobj);
    }
}

static void
mark_window_mirror (struct window_mirror *mir, void (*markobj)(Lisp_Object))
{
  mark_redisplay_structs (mir->current_display_lines, markobj);
  mark_redisplay_structs (mir->desired_display_lines, markobj);

  if (mir->next)
    mark_window_mirror (mir->next, markobj);

  if (mir->hchild)
    mark_window_mirror (mir->hchild, markobj);
  else if (mir->vchild)
    mark_window_mirror (mir->vchild, markobj);
}

void
mark_redisplay (void (*markobj)(Lisp_Object))
{
  Lisp_Object frmcons, devcons, concons;

  FRAME_LOOP_NO_BREAK (frmcons, devcons, concons)
    {
      struct frame *f = XFRAME (XCAR (frmcons));
      update_frame_window_mirror (f);
      mark_window_mirror (f->root_mirror, markobj);
    }
}

/*****************************************************************************
 Line Start Cache Description and Rationale

 The traditional scrolling code in Emacs breaks in a variable height world.
 It depends on the key assumption that the number of lines that can be
 displayed at any given time is fixed.  This led to a complete separation
 of the scrolling code from the redisplay code.  In order to fully support
 variable height lines, the scrolling code must actually be tightly
 integrated with redisplay.  Only redisplay can determine how many lines
 will be displayed on a screen for any given starting point.

 What is ideally wanted is a complete list of the starting buffer position
 for every possible display line of a buffer along with the height of that
 display line.  Maintaining such a full list would be very expensive.  We
 settle for having it include information for all areas which we happen to
 generate anyhow (i.e. the region currently being displayed) and for those
 areas we need to work with.

 In order to ensure that the cache accurately represents what redisplay
 would actually show, it is necessary to invalidate it in many situations.
 If the buffer changes, the starting positions may no longer be correct.
 If a face or an extent has changed then the line heights may have altered.
 These events happen frequently enough that the cache can end up being
 constantly disabled.  With this potentially constant invalidation when is
 the cache ever useful?

 Even if the cache is invalidated before every single usage, it is
 necessary.  Scrolling often requires knowledge about display lines which
 are actually above or below the visible region.  The cache provides a
 convenient light-weight method of storing this information for multiple
 display regions.  This knowledge is necessary for the scrolling code to
 always obey the First Golden Rule of Redisplay.

 If the cache already contains all of the information that the scrolling
 routines happen to need so that it doesn't have to go generate it, then we
 are able to obey the Third Golden Rule of Redisplay.  The first thing we
 do to help out the cache is to always add the displayed region.  This
 region had to be generated anyway, so the cache ends up getting the
 information basically for free.  In those cases where a user is simply
 scrolling around viewing a buffer there is a high probability that this is
 sufficient to always provide the needed information.  The second thing we
 can do is be smart about invalidating the cache.

 TODO -- Be smart about invalidating the cache.  Potential places:

 + Insertions at end-of-line which don't cause line-wraps do not alter the
   starting positions of any display lines.  These types of buffer
   modifications should not invalidate the cache.  This is actually a large
   optimization for redisplay speed as well.

 + Buffer modifications frequently only affect the display of lines at and
   below where they occur.  In these situations we should only invalidate
   the part of the cache starting at where the modification occurs.

 In case you're wondering, the Second Golden Rule of Redisplay is not
 applicable.
 ****************************************************************************/

/* This will get used quite a bit so we don't want to be constantly
   allocating and freeing it. */
line_start_cache_dynarr *internal_cache;

/* Makes internal_cache represent the TYPE display structs and only
   the TYPE display structs. */

static void
update_internal_cache_list (struct window *w, int type)
{
  int line;
  display_line_dynarr *dla = window_display_lines (w, type);

  Dynarr_reset (internal_cache);
  for (line = 0; line < Dynarr_length (dla); line++)
    {
      struct display_line *dl = Dynarr_atp (dla, line);

      if (dl->modeline)
	continue;
      else
	{
	  struct line_start_cache lsc;

	  lsc.start = dl->bufpos;
	  lsc.end = dl->end_bufpos;
	  lsc.height = dl->ascent + dl->descent;

	  Dynarr_add (internal_cache, lsc);
	}
    }
}

/* Reset the line cache if necessary.  This should be run at the
   beginning of any function which access the cache. */

static void
validate_line_start_cache (struct window *w)
{
  struct buffer *b = XBUFFER (w->buffer);
  struct frame *f = XFRAME (w->frame);

  if (!w->line_cache_validation_override)
    {
      /* f->extents_changed used to be in here because extent face and
         size changes can cause text shifting.  However, the extent
         covering the region is constantly having its face set and
         priority altered by the mouse code.  This means that the line
         start cache is constantly being invalidated.  This is bad
         since the mouse code also triggers heavy usage of the cache.
         Since it is an unlikely that f->extents being changed
         indicates that the cache really needs to be updated and if it
         does redisplay will catch it pretty quickly we no longer
         invalidate the cache if it is set.  This greatly speeds up
         dragging out regions with the mouse. */
      if (XINT (w->line_cache_last_updated) < BUF_MODIFF (b)
	  || f->faces_changed
	  || f->clip_changed)
	{
	  Dynarr_reset (w->line_start_cache);
	}
    }
}

/* Return the very first buffer position contained in the given
   window's cache, or -1 if the cache is empty.  Assumes that the
   cache is valid. */

static Bufpos
line_start_cache_start (struct window *w)
{
  line_start_cache_dynarr *cache = w->line_start_cache;

  if (!Dynarr_length (cache))
    return -1;
  else
    return Dynarr_atp (cache, 0)->start;
}

/* Return the very last buffer position contained in the given
   window's cache, or -1 if the cache is empty.  Assumes that the
   cache is valid. */

static Bufpos
line_start_cache_end (struct window *w)
{
  line_start_cache_dynarr *cache = w->line_start_cache;

  if (!Dynarr_length (cache))
    return -1;
  else
    return Dynarr_atp (cache, Dynarr_length (cache) - 1)->end;
}

/* Return the index of the line POINT is contained within in window
   W's line start cache.  It will enlarge the cache or move the cache
   window in order to have POINT be present in the cache.  MIN_PAST is
   a guarantee of the number of entries in the cache present on either
   side of POINT (unless a buffer boundary is hit).  If MIN_PAST is -1
   then it will be treated as 0, but the cache window will not be
   allowed to shift.  Returns -1 if POINT cannot be found in the cache
   for any reason. */

int
point_in_line_start_cache (struct window *w, Bufpos point, int min_past)
{
  struct buffer *b = XBUFFER (w->buffer);
  line_start_cache_dynarr *cache = w->line_start_cache;
  unsigned int top, bottom, pos;

  validate_line_start_cache (w);
  w->line_cache_validation_override++;

  /* Let functions pass in negative values, but we still treat -1
     specially. */
  /* #### bogosity alert */
  if (min_past < 0 && min_past != -1)
    min_past = -min_past;

  if (!Dynarr_length (cache) || line_start_cache_start (w) > point
      || line_start_cache_end (w) < point)
    {
      int loop;
      int win_char_height = window_char_height (w, 1);

      /* Occasionally we get here with a 0 height
         window. find_next_newline_no_quit will abort if we pass it a
         count of 0 so handle that case. */
      if (!win_char_height)
	win_char_height = 1;

      if (!Dynarr_length (cache))
	{
	  Bufpos from = find_next_newline_no_quit (b, point, -1);
	  Bufpos to = find_next_newline_no_quit (b, from, win_char_height);

	  update_line_start_cache (w, from, to, point, 0);

	  if (!Dynarr_length (cache))
	    {
	      w->line_cache_validation_override--;
	      return -1;
	    }
	}

      assert (Dynarr_length (cache));

      loop = 0;
      while (line_start_cache_start (w) > point
	     && (loop < cache_adjustment || min_past == -1))
	{
	  Bufpos from, to;

	  from = line_start_cache_start (w);
	  if (from <= BUF_BEGV (b))
	    break;

	  from = find_next_newline_no_quit (b, from, -win_char_height);
	  to = line_start_cache_end (w);

	  update_line_start_cache (w, from, to, point, 0);
	  loop++;
	}

      if (line_start_cache_start (w) > point)
	{
	  Bufpos from, to;

	  from = find_next_newline_no_quit (b, point, -1);
	  if (from >= BUF_ZV (b))
	    {
	      to = find_next_newline_no_quit (b, from, -win_char_height);
	      from = to;
	      to = BUF_ZV (b);
	    }
	  else
	    to = find_next_newline_no_quit (b, from, win_char_height);

	  update_line_start_cache (w, from, to, point, 0);
	}

      loop = 0;
      while (line_start_cache_end (w) < point
	     && (loop < cache_adjustment || min_past == -1))
	{
	  Bufpos from, to;

	  to = line_start_cache_end (w);
	  if (to >= BUF_ZV (b))
	    break;

	  from = line_start_cache_end (w);
	  to = find_next_newline_no_quit (b, from, win_char_height);

	  update_line_start_cache (w, from, to, point, 0);
	  loop++;
	}

      if (line_start_cache_end (w) < point)
	{
	  Bufpos from, to;

	  from = find_next_newline_no_quit (b, point, -1);
	  if (from >= BUF_ZV (b))
	    {
	      to = find_next_newline_no_quit (b, from, -win_char_height);
	      from = to;
	      to = BUF_ZV (b);
	    }
	  else
	    to = find_next_newline_no_quit (b, from, win_char_height);

	  update_line_start_cache (w, from, to, point, 0);
	}
    }

  assert (Dynarr_length (cache));

  if (min_past == -1)
    min_past = 0;

  /* This could happen if the buffer is narrowed. */
  if (line_start_cache_start (w) > point
      || line_start_cache_end (w) < point)
    {
      w->line_cache_validation_override--;
      return -1;
    }

find_point_loop:

  top = Dynarr_length (cache) - 1;
  bottom = 0;

  while (1)
    {
      unsigned int new_pos;
      Bufpos start, end;

      pos = (bottom + top + 1) >> 1;
      start = Dynarr_atp (cache, pos)->start;
      end = Dynarr_atp (cache, pos)->end;

      if (point >= start && point <= end)
	{
	  if (pos < min_past && line_start_cache_start (w) > BUF_BEGV (b))
	    {
	      Bufpos from =
		find_next_newline_no_quit (b, line_start_cache_start (w),
					   -min_past - 1);
	      Bufpos to = line_start_cache_end (w);

	      update_line_start_cache (w, from, to, point, 0);
	      goto find_point_loop;
	    }
	  else if ((Dynarr_length (cache) - pos - 1) < min_past
		   && line_start_cache_end (w) < BUF_ZV (b))
	    {
	      Bufpos from = line_start_cache_end (w);
	      Bufpos to = find_next_newline_no_quit (b, from,
						     (min_past
						      ? min_past
						      : 1));

	      update_line_start_cache (w, from, to, point, 0);
	      goto find_point_loop;
	    }
	  else
	    {
	      w->line_cache_validation_override--;
	      return pos;
	    }
	}
      else if (point > end)
	bottom = pos + 1;
      else if (point < start)
	top = pos - 1;
      else
	abort ();

      new_pos = (bottom + top + 1) >> 1;
      if (pos == new_pos)
	{
	  w->line_cache_validation_override--;
	  return -1;
	}
    }
}

/* Return a boolean indicating if POINT would be visible in window W
   if display of the window was to begin at STARTP. */

int
point_would_be_visible (struct window *w, Bufpos startp, Bufpos point)
{
  struct buffer *b = XBUFFER (w->buffer);
  int pixpos = 0;
  int bottom = WINDOW_TEXT_HEIGHT (w);
  int start_elt;

  /* If point is before the intended start it obviously can't be visible. */
  if (point < startp)
    return 0;

  /* If point or start are not in the accessible buffer range, then
     fail. */
  if (startp < BUF_BEGV (b) || startp > BUF_ZV (b)
      || point < BUF_BEGV (b) || point > BUF_ZV (b))
    return 0;

  validate_line_start_cache (w);
  w->line_cache_validation_override++;

  start_elt = point_in_line_start_cache (w, startp, 0);
  if (start_elt == -1)
    {
      w->line_cache_validation_override--;
      return 0;
    }

  assert (line_start_cache_start (w) <= startp
	  && line_start_cache_end (w) >= startp);

  while (1)
    {
      int height;

      /* Expand the cache if necessary. */
      if (start_elt == Dynarr_length (w->line_start_cache))
	{
	  Bufpos old_startp =
	    Dynarr_atp (w->line_start_cache, start_elt - 1)->start;

	  start_elt = point_in_line_start_cache (w, old_startp,
						 window_char_height (w, 0));

	  /* We've already actually processed old_startp, so increment
             immediately. */
	  start_elt++;

	  /* If this happens we didn't add any extra elements.  Bummer. */
	  if (start_elt == Dynarr_length (w->line_start_cache))
	    {
	      w->line_cache_validation_override--;
	      return 0;
	    }
	}

      height = Dynarr_atp (w->line_start_cache, start_elt)->height;

      if (pixpos + height > bottom)
	{
	  if (bottom - pixpos < VERTICAL_CLIP (w, 0))
	    {
	      w->line_cache_validation_override--;
	      return 0;
	    }
	}

      pixpos += height;
      if (point <= Dynarr_atp (w->line_start_cache, start_elt)->end)
	{
	  w->line_cache_validation_override--;
	  return 1;
	}

      start_elt++;
    }
}

/* For the given window W, if display starts at STARTP, what will be
   the buffer position at the beginning or end of the last line
   displayed.  The end of the last line is also know as the window end
   position.

   #### With a little work this could probably be reworked as just a
   call to start_with_line_at_pixpos. */

static Bufpos
start_end_of_last_line (struct window *w, Bufpos startp, int end)
{
  struct buffer *b = XBUFFER (w->buffer);
  line_start_cache_dynarr *cache = w->line_start_cache;
  int pixpos = 0;
  int bottom = WINDOW_TEXT_HEIGHT (w);
  Bufpos cur_start;
  int start_elt;

  validate_line_start_cache (w);
  w->line_cache_validation_override++;

  if (startp < BUF_BEGV (b))
    startp = BUF_BEGV (b);
  else if (startp > BUF_ZV (b))
    startp = BUF_ZV (b);
  cur_start = startp;

  start_elt = point_in_line_start_cache (w, cur_start, 0);
  if (start_elt == -1)
    abort ();	/* this had better never happen */

  while (1)
    {
      int height = Dynarr_atp (cache, start_elt)->height;

      cur_start = Dynarr_atp (cache, start_elt)->start;

      if (pixpos + height > bottom)
	{
	  /* Adjust for any possible clip. */
	  if (bottom - pixpos < VERTICAL_CLIP (w, 0))
	    start_elt--;

	  if (start_elt < 0)
	    {
	      w->line_cache_validation_override--;
	      if (end)
		return BUF_ZV (b);
	      else
		return BUF_BEGV (b);
	    }
	  else
	    {
	      w->line_cache_validation_override--;
	      if (end)
		return Dynarr_atp (cache, start_elt)->end;
	      else
		return Dynarr_atp (cache, start_elt)->start;
	    }
	}

      pixpos += height;
      start_elt++;
      if (start_elt == Dynarr_length (cache))
	{
	  Bufpos from = line_start_cache_end (w);
	  int win_char_height = window_char_height (w, 0);
	  Bufpos to = find_next_newline_no_quit (b, from,
						 (win_char_height
						  ? win_char_height
						  : 1));

	  /* We've hit the end of the bottom so that's what it is. */
	  if (from >= BUF_ZV (b))
	    {
	      w->line_cache_validation_override--;
	      return BUF_ZV (b);
	    }

	  update_line_start_cache (w, from, to, BUF_PT (b), 0);

	  /* Updating the cache invalidates any current indexes. */
	  start_elt = point_in_line_start_cache (w, cur_start, -1) + 1;
	}
    }
}

/* For the given window W, if display starts at STARTP, what will be
   the buffer position at the beginning of the last line displayed. */

Bufpos
start_of_last_line (struct window *w, Bufpos startp)
{
  return start_end_of_last_line (w, startp, 0);
}

/* For the given window W, if display starts at STARTP, what will be
   the buffer position at the end of the last line displayed.  This is
   also know as the window end position. */

Bufpos
end_of_last_line (struct window *w, Bufpos startp)
{
  return start_end_of_last_line (w, startp, 1);
}

/* For window W, what does the starting position have to be so that
   the line containing POINT will cover pixel position PIXPOS. */

Bufpos
start_with_line_at_pixpos (struct window *w, Bufpos point, int pixpos)
{
  struct buffer *b = XBUFFER (w->buffer);
  int cur_elt;
  Bufpos cur_pos, prev_pos = point;
  int point_line_height;
  int pixheight = pixpos - WINDOW_TEXT_TOP (w);

  validate_line_start_cache (w);
  w->line_cache_validation_override++;

  cur_elt = point_in_line_start_cache (w, point, 0);
  /* #### See comment in update_line_start_cache about big minibuffers. */
  if (cur_elt < 0)
    {
      w->line_cache_validation_override--;
      return point;
    }

  point_line_height = Dynarr_atp (w->line_start_cache, cur_elt)->height;

  while (1)
    {
      cur_pos = Dynarr_atp (w->line_start_cache, cur_elt)->start;

      pixheight -= Dynarr_atp (w->line_start_cache, cur_elt)->height;

      /* Do not take into account the value of vertical_clip here.
         That is the responsibility of the calling functions. */
      if (pixheight < 0)
	{
	  w->line_cache_validation_override--;
	  if (-pixheight > point_line_height)
	    /* We can't make the target line cover pixpos, so put it
	       above pixpos.  That way it will at least be visible. */
	    return prev_pos;
	  else
	    return cur_pos;
	}

      cur_elt--;
      if (cur_elt < 0)
	{
	  Bufpos from, to;
	  int win_char_height;

	  if (cur_pos <= BUF_BEGV (b))
	    {
	      w->line_cache_validation_override--;
	      return BUF_BEGV (b);
	    }

	  win_char_height = window_char_height (w, 0);
	  if (!win_char_height)
	    win_char_height = 1;

	  from = find_next_newline_no_quit (b, cur_pos, -win_char_height);
	  to = line_start_cache_end (w);
	  update_line_start_cache (w, from, to, point, 0);

	  cur_elt = point_in_line_start_cache (w, cur_pos, 2) - 1;
	  assert (cur_elt >= 0);
	}
      prev_pos = cur_pos;
    }
}

/* For window W, what does the starting position have to be so that
   the line containing point is on display line LINE.  If LINE is
   positive it is considered to be the number of lines from the top of
   the window (0 is the top line).  If it is negative the number is
   considered to be the number of lines from the bottom (-1 is the
   bottom line). */

Bufpos
start_with_point_on_display_line (struct window *w, Bufpos point, int line)
{
  validate_line_start_cache (w);
  w->line_cache_validation_override++;

  if (line >= 0)
    {
      int cur_elt = point_in_line_start_cache (w, point, line);

      if (cur_elt - line < 0)
	cur_elt = 0;		/* Hit the top */
      else
	cur_elt -= line;

      w->line_cache_validation_override--;
      return Dynarr_atp (w->line_start_cache, cur_elt)->start;
    }
  else
    {
      /* The calculated value of pixpos is correct for the bottom line
         or what we want when line is -1.  Therefore we subtract one
         because we have already handled one line. */
      int new_line = -line - 1;
      int cur_elt = point_in_line_start_cache (w, point, new_line);
      int pixpos = WINDOW_TEXT_BOTTOM (w);
      Bufpos retval, search_point;

      /* If scroll_on_clipped_lines is false, the last "visible" line of
 	 the window covers the pixel at WINDOW_TEXT_BOTTOM (w) - 1.
 	 If s_o_c_l is true, then we don't want to count a clipped
 	 line, so back up from the bottom by the height of the line
 	 containing point. */
      if (scroll_on_clipped_lines)
	pixpos -= Dynarr_atp (w->line_start_cache, cur_elt)->height;
      else
	pixpos -= 1;

      if (cur_elt + new_line >= Dynarr_length (w->line_start_cache))
	{
	  /* Hit the bottom of the buffer. */
	  int adjustment =
	    (cur_elt + new_line) - Dynarr_length (w->line_start_cache) + 1;
	  Lisp_Object window;
	  int defheight;

	  XSETWINDOW (window, w);
	  default_face_height_and_width (window, &defheight, 0);

	  cur_elt = Dynarr_length (w->line_start_cache) - 1;

	  pixpos -= (adjustment * defheight);
	  if (pixpos < WINDOW_TEXT_TOP (w))
	    pixpos = WINDOW_TEXT_TOP (w);
	}
      else
	cur_elt = cur_elt + new_line;

      search_point = Dynarr_atp (w->line_start_cache, cur_elt)->start;

      retval = start_with_line_at_pixpos (w, search_point, pixpos);
      w->line_cache_validation_override--;
      return retval;
    }
}

/* This is used to speed up vertical scrolling by caching the known
   buffer starting positions for display lines.  This allows the
   scrolling routines to avoid costly calls to regenerate_window.  If
   NO_REGEN is true then it will only add the values in the DESIRED
   display structs which are in the given range.

   Note also that the FROM/TO values are minimums.  It is possible
   that this function will actually add information outside of the
   lines containing those positions.  This can't hurt but it could
   possibly help.

   #### We currently force the cache to have only 1 contiguous region.
   It might help to make the cache a dynarr of caches so that we can
   cover more areas.  This might, however, turn out to be a lot of
   overhead for too little gain. */

static void
update_line_start_cache (struct window *w, Bufpos from, Bufpos to,
			 Bufpos point, int no_regen)
{
  struct buffer *b = XBUFFER (w->buffer);
  line_start_cache_dynarr *cache = w->line_start_cache;
  Bufpos low_bound, high_bound;

  validate_line_start_cache (w);
  w->line_cache_validation_override++;
  updating_line_start_cache = 1;

  if (from < BUF_BEGV (b))
    from = BUF_BEGV (b);
  if (to > BUF_ZV (b))
    to = BUF_ZV (b);

  if (from > to)
    {
      updating_line_start_cache = 0;
      w->line_cache_validation_override--;
      return;
    }

  if (Dynarr_length (cache))
    {
      low_bound = line_start_cache_start (w);
      high_bound = line_start_cache_end (w);

      /* Check to see if the desired range is already in the cache. */
      if (from >= low_bound && to <= high_bound)
	{
	  updating_line_start_cache = 0;
	  w->line_cache_validation_override--;
	  return;
	}

      /* Check to make sure that the desired range is adjacent to the
	 current cache.  If not, invalidate the cache. */
      if (to < low_bound || from > high_bound)
	{
	  Dynarr_reset (cache);
	  low_bound = high_bound = -1;
	}
    }
  else
    {
      low_bound = high_bound = -1;
    }

  w->line_cache_last_updated = make_int (BUF_MODIFF (b));

  /* This could be integrated into the next two sections, but it is easier
     to follow what's going on by having it separate. */
  if (no_regen)
    {
      Bufpos start, end;

      update_internal_cache_list (w, DESIRED_DISP);
      if (!Dynarr_length (internal_cache))
	{
	  updating_line_start_cache = 0;
	  w->line_cache_validation_override--;
	  return;
	}

      start = Dynarr_atp (internal_cache, 0)->start;
      end =
	Dynarr_atp (internal_cache, Dynarr_length (internal_cache) - 1)->end;

      /* We aren't allowed to generate additional information to fill in
         gaps, so if the DESIRED structs don't overlap the cache, reset the
         cache. */
      if (Dynarr_length (cache))
	{
	  if (end < low_bound || start > high_bound)
	    Dynarr_reset (cache);

	  /* #### What should really happen if what we are doing is
             extending a line (the last line)? */
	  if (Dynarr_length (cache) == 1
	      && Dynarr_length (internal_cache) == 1)
	    Dynarr_reset (cache);
	}

      if (!Dynarr_length (cache))
	{
	  Dynarr_add_many (cache, Dynarr_atp (internal_cache, 0),
			   Dynarr_length (internal_cache));
	  updating_line_start_cache = 0;
	  w->line_cache_validation_override--;
	  return;
	}

      /* An extra check just in case the calling function didn't pass in
         the bounds of the DESIRED structs in the first place. */
      if (start >= low_bound && end <= high_bound)
	{
	  updating_line_start_cache = 0;
	  w->line_cache_validation_override--;
	  return;
	}

      /* At this point we know that the internal cache partially overlaps
         the main cache. */
      if (start < low_bound)
	{
	  int ic_elt = Dynarr_length (internal_cache) - 1;
	  while (ic_elt >= 0)
	    {
	      if (Dynarr_atp (internal_cache, ic_elt)->start < low_bound)
		break;
	      else
		ic_elt--;
	    }

	  if (!(ic_elt >= 0))
	    {
	      Dynarr_reset (cache);
	      Dynarr_add_many (cache, Dynarr_atp (internal_cache, 0),
			       Dynarr_length (internal_cache));
	      updating_line_start_cache = 0;
	      w->line_cache_validation_override--;
	      return;
	    }

	  Dynarr_insert_many_at_start (cache, Dynarr_atp (internal_cache, 0),
			      ic_elt + 1);
	}

      if (end > high_bound)
	{
	  int ic_elt = 0;

	  while (ic_elt < Dynarr_length (internal_cache))
	    {
	      if (Dynarr_atp (internal_cache, ic_elt)->start > high_bound)
		break;
	      else
		ic_elt++;
	    }

	  if (!(ic_elt < Dynarr_length (internal_cache)))
	    {
	      Dynarr_reset (cache);
	      Dynarr_add_many (cache, Dynarr_atp (internal_cache, 0),
			       Dynarr_length (internal_cache));
	      updating_line_start_cache = 0;
	      w->line_cache_validation_override--;
	      return;
	    }

	  Dynarr_add_many (cache, Dynarr_atp (internal_cache, ic_elt),
			   Dynarr_length (internal_cache) - ic_elt);
	}

      updating_line_start_cache = 0;
      w->line_cache_validation_override--;
      return;
    }

  if (!Dynarr_length (cache) || from < low_bound)
    {
      Bufpos startp = find_next_newline_no_quit (b, from, -1);
      int marker = 0;
      int old_lb = low_bound;

      while (startp < old_lb || low_bound == -1)
	{
	  int ic_elt;
          Bufpos new_startp;

	  regenerate_window (w, startp, point, CMOTION_DISP);
	  update_internal_cache_list (w, CMOTION_DISP);

	  /* If this assert is triggered then regenerate_window failed
             to layout a single line.  That is not supposed to be
             possible because we impose a minimum height on the buffer
             and override vertical clip when we are in here. */
	  /* #### Ah, but it is because the window may temporarily
             exist but not have any lines at all if the minibuffer is
             real big.  Look into that situation better. */
	  if (!Dynarr_length (internal_cache))
	    {
	      if (old_lb == -1 && low_bound == -1)
		{
		  updating_line_start_cache = 0;
		  w->line_cache_validation_override--;
		  return;
		}

	      assert (Dynarr_length (internal_cache));
	    }
	  assert (startp == Dynarr_atp (internal_cache, 0)->start);

	  ic_elt = Dynarr_length (internal_cache) - 1;
	  if (low_bound != -1)
	    {
	      while (ic_elt >= 0)
		{
		  if (Dynarr_atp (internal_cache, ic_elt)->start < old_lb)
		    break;
		  else
		    ic_elt--;
		}
	    }
	  assert (ic_elt >= 0);

	  new_startp = Dynarr_atp (internal_cache, ic_elt)->end + 1;

          /*
           * Handle invisible text properly:
           * If the last line we're inserting has the same end as the
           * line before which it will be added, merge the two lines.
           */
          if (Dynarr_length (cache)  &&
              Dynarr_atp (internal_cache, ic_elt)->end ==
              Dynarr_atp (cache, marker)->end)
            {
              Dynarr_atp (cache, marker)->start
                = Dynarr_atp (internal_cache, ic_elt)->start;
              Dynarr_atp (cache, marker)->height
                = Dynarr_atp (internal_cache, ic_elt)->height;
              ic_elt--;
            }

          if (ic_elt >= 0)       /* we still have lines to add.. */
            {
              Dynarr_insert_many (cache, Dynarr_atp (internal_cache, 0),
                                  ic_elt + 1, marker);
              marker += (ic_elt + 1);
            }

	  if (startp < low_bound || low_bound == -1)
	    low_bound = startp;
	  startp = new_startp;
	  if (startp > BUF_ZV (b))
	    {
	      updating_line_start_cache = 0;
	      w->line_cache_validation_override--;
	      return;
	    }
	}
    }

  assert (Dynarr_length (cache));
  assert (from >= low_bound);

  /* Readjust the high_bound to account for any changes made while
     correcting the low_bound. */
  high_bound = Dynarr_atp (cache, Dynarr_length (cache) - 1)->end;

  if (to > high_bound)
    {
      Bufpos startp = Dynarr_atp (cache, Dynarr_length (cache) - 1)->end + 1;

      do
	{
	  regenerate_window (w, startp, point, CMOTION_DISP);
	  update_internal_cache_list (w, CMOTION_DISP);

	  /* See comment above about regenerate_window failing. */
	  assert (Dynarr_length (internal_cache));

	  Dynarr_add_many (cache, Dynarr_atp (internal_cache, 0),
			   Dynarr_length (internal_cache));
	  high_bound = Dynarr_atp (cache, Dynarr_length (cache) - 1)->end;
	  startp = high_bound + 1;
	}
      while (to > high_bound);
    }

  updating_line_start_cache = 0;
  w->line_cache_validation_override--;
  assert (to <= high_bound);
}


/* Given x and y coordinates in characters, relative to a window,
   return the pixel location corresponding to those coordinates.  The
   pixel location returned is the center of the given character
   position.  The pixel values are generated relative to the window,
   not the frame.

   The modeline is considered to be part of the window. */

void
glyph_to_pixel_translation (struct window *w, int char_x, int char_y,
			    int *pix_x, int *pix_y)
{
  display_line_dynarr *dla = window_display_lines (w, CURRENT_DISP);
  int num_disp_lines, modeline;
  Lisp_Object window;
  int defheight, defwidth;

  XSETWINDOW (window, w);
  default_face_height_and_width (window, &defheight, &defwidth);

  /* If we get a bogus value indicating somewhere above or to the left of
     the window, use the first window line or character position
     instead. */
  if (char_y < 0)
    char_y = 0;
  if (char_x < 0)
    char_x = 0;

  num_disp_lines = Dynarr_length (dla);
  modeline = 0;
  if (num_disp_lines)
    {
      if (Dynarr_atp (dla, 0)->modeline)
	{
	  num_disp_lines--;
	  modeline = 1;
	}
    }

  /* First check if the y position intersects the display lines. */
  if (char_y < num_disp_lines)
    {
      struct display_line *dl = Dynarr_atp (dla, char_y + modeline);
      struct display_block *db = get_display_block_from_line (dl, TEXT);

      *pix_y = (dl->ypos - dl->ascent +
		((unsigned int) (dl->ascent + dl->descent - dl->clip) >> 1));

      if (char_x < Dynarr_length (db->runes))
	{
	  struct rune *rb = Dynarr_atp (db->runes, char_x);

	  *pix_x = rb->xpos + (rb->width >> 1);
	}
      else
	{
	  int last_rune = Dynarr_length (db->runes) - 1;
	  struct rune *rb = Dynarr_atp (db->runes, last_rune);

	  char_x -= last_rune;

	  *pix_x = rb->xpos + rb->width;
	  *pix_x += ((char_x - 1) * defwidth);
	  *pix_x += (defwidth >> 1);
	}
    }
  else
    {
      /* It didn't intersect, so extrapolate.  #### For now, we include the
	 modeline in this since we don't have true character positions in
	 it. */

      if (!Dynarr_length (w->face_cachels))
	reset_face_cachels (w);

      char_y -= num_disp_lines;

      if (Dynarr_length (dla))
	{
	  struct display_line *dl = Dynarr_atp (dla, Dynarr_length (dla) - 1);
	  *pix_y = dl->ypos + dl->descent - dl->clip;
	}
      else
	*pix_y = WINDOW_TEXT_TOP (w);

      *pix_y += (char_y * defheight);
      *pix_y += (defheight >> 1);

      *pix_x = WINDOW_TEXT_LEFT (w);
      /* Don't adjust by one because this is still the unadjusted value. */
      *pix_x += (char_x * defwidth);
      *pix_x += (defwidth >> 1);
    }

  if (*pix_x > w->pixel_left + w->pixel_width)
      *pix_x = w->pixel_left + w->pixel_width;
  if (*pix_y > w->pixel_top + w->pixel_height)
      *pix_y = w->pixel_top + w->pixel_height;

  *pix_x -= w->pixel_left;
  *pix_y -= w->pixel_top;
}

/* Given a display line and a position, determine if there is a glyph
   there and return information about it if there is. */

static void
get_position_object (struct display_line *dl, Lisp_Object *obj1,
		     Lisp_Object *obj2, int x_coord, int *low_x_coord,
		     int *high_x_coord)
{
  struct display_block *db;
  int elt;
  int block =
    get_next_display_block (dl->bounds, dl->display_blocks, x_coord, 0);

  /* We use get_next_display_block to get the actual display block
     that would be displayed at x_coord. */

  if (block == NO_BLOCK)
    return;
  else
    db = Dynarr_atp (dl->display_blocks, block);

  for (elt = 0; elt < Dynarr_length (db->runes); elt++)
    {
      struct rune *rb = Dynarr_atp (db->runes, elt);

      if (rb->xpos <= x_coord && x_coord < (rb->xpos + rb->width))
	{
	  if (rb->type == RUNE_DGLYPH)
	    {
	      *obj1 = rb->object.dglyph.glyph;
	      *obj2 = rb->object.dglyph.extent;
	    }
	  else
	    {
	      *obj1 = Qnil;
	      *obj2 = Qnil;
	    }

	  if (low_x_coord)
	    *low_x_coord = rb->xpos;
	  if (high_x_coord)
	    *high_x_coord = rb->xpos + rb->width;

	  return;
	}
    }
}

#define UPDATE_CACHE_RETURN						\
  do {									\
    d->pixel_to_glyph_cache.valid = 1;					\
    d->pixel_to_glyph_cache.low_x_coord = low_x_coord;			\
    d->pixel_to_glyph_cache.high_x_coord = high_x_coord;		\
    d->pixel_to_glyph_cache.low_y_coord = low_y_coord;			\
    d->pixel_to_glyph_cache.high_y_coord = high_y_coord;		\
    d->pixel_to_glyph_cache.frame = f;					\
    d->pixel_to_glyph_cache.col = *col;					\
    d->pixel_to_glyph_cache.row = *row;					\
    d->pixel_to_glyph_cache.obj_x = *obj_x;				\
    d->pixel_to_glyph_cache.obj_y = *obj_y;				\
    d->pixel_to_glyph_cache.w = *w;					\
    d->pixel_to_glyph_cache.bufpos = *bufpos;				\
    d->pixel_to_glyph_cache.closest = *closest;				\
    d->pixel_to_glyph_cache.modeline_closest = *modeline_closest;	\
    d->pixel_to_glyph_cache.obj1 = *obj1;				\
    d->pixel_to_glyph_cache.obj2 = *obj2;				\
    d->pixel_to_glyph_cache.retval = position;				\
    RETURN_SANS_WARNINGS position;					\
  } while (0)

/* Given x and y coordinates in pixels relative to a frame, return
   information about what is located under those coordinates.

   The return value will be one of:

     OVER_TOOLBAR:	over one of the 4 frame toolbars
     OVER_MODELINE:	over a modeline
     OVER_BORDER:	over an internal border
     OVER_NOTHING:	over the text area, but not over text
     OVER_OUTSIDE:	outside of the frame border
     OVER_TEXT:		over text in the text area

   OBJ1 is one of

     -- a toolbar button
     -- a glyph
     -- nil if the coordinates are not over a glyph or a toolbar button.

   OBJ2 is one of

     -- an extent, if the coordinates are over a glyph in the text area
     -- nil otherwise.

   If the coordinates are over a glyph, OBJ_X and OBJ_Y give the
   equivalent coordinates relative to the upper-left corner of the glyph.

   If the coordinates are over a character, OBJ_X and OBJ_Y give the
   equivalent coordinates relative to the upper-left corner of the character.

   Otherwise, OBJ_X and OBJ_Y are undefined.
   */

int
pixel_to_glyph_translation (struct frame *f, int x_coord, int y_coord,
			    int *col, int *row, int *obj_x, int *obj_y,
			    struct window **w, Bufpos *bufpos,
			    Bufpos *closest, Charcount *modeline_closest,
			    Lisp_Object *obj1, Lisp_Object *obj2)
{
  struct device *d;
  struct pixel_to_glyph_translation_cache *cache;
  Lisp_Object window;
  int frm_left, frm_right, frm_top, frm_bottom;
  int low_x_coord, high_x_coord, low_y_coord, high_y_coord;
  int position = OVER_NOTHING;
  int device_check_failed = 0;
  display_line_dynarr *dla;

  /* This is a safety valve in case this got called with a frame in
     the middle of being deleted. */
  if (!DEVICEP (f->device) || !DEVICE_LIVE_P (XDEVICE (f->device)))
    {
      device_check_failed = 1;
      d = NULL, cache = NULL; /* Warning suppression */
    }
  else
    {
      d = XDEVICE (f->device);
      cache = &d->pixel_to_glyph_cache;
    }

  if (!device_check_failed
      && cache->valid
      && cache->frame == f
      && cache->low_x_coord <= x_coord
      && cache->high_x_coord > x_coord
      && cache->low_y_coord <= y_coord
      && cache->high_y_coord > y_coord)
    {
      *col = cache->col;
      *row = cache->row;
      *obj_x = cache->obj_x;
      *obj_y = cache->obj_y;
      *w = cache->w;
      *bufpos = cache->bufpos;
      *closest = cache->closest;
      *modeline_closest = cache->modeline_closest;
      *obj1 = cache->obj1;
      *obj2 = cache->obj2;

      return cache->retval;
    }
  else
    {
      *col = 0;
      *row = 0;
      *obj_x = 0;
      *obj_y = 0;
      *w = 0;
      *bufpos = 0;
      *closest = 0;
      *modeline_closest = -1;
      *obj1 = Qnil;
      *obj2 = Qnil;

      low_x_coord = x_coord;
      high_x_coord = x_coord + 1;
      low_y_coord = y_coord;
      high_y_coord = y_coord + 1;
    }

  if (device_check_failed)
    return OVER_NOTHING;

  frm_left = FRAME_LEFT_BORDER_END (f);
  frm_right = FRAME_RIGHT_BORDER_START (f);
  frm_top = FRAME_TOP_BORDER_END (f);
  frm_bottom = FRAME_BOTTOM_BORDER_START (f);

  /* Check if the mouse is outside of the text area actually used by
     redisplay. */
  if (y_coord < frm_top)
    {
      if (y_coord >= FRAME_TOP_BORDER_START (f))
	{
	  low_y_coord = FRAME_TOP_BORDER_START (f);
	  high_y_coord = frm_top;
	  position = OVER_BORDER;
	}
      else if (y_coord >= 0)
	{
	  low_y_coord = 0;
	  high_y_coord = FRAME_TOP_BORDER_START (f);
	  position = OVER_TOOLBAR;
	}
      else
	{
	  low_y_coord = y_coord;
	  high_y_coord = 0;
	  position = OVER_OUTSIDE;
	}
    }
  else if (y_coord >= frm_bottom)
    {
      if (y_coord < FRAME_BOTTOM_BORDER_END (f))
	{
	  low_y_coord = frm_bottom;
	  high_y_coord = FRAME_BOTTOM_BORDER_END (f);
	  position = OVER_BORDER;
	}
      else if (y_coord < FRAME_PIXHEIGHT (f))
	{
	  low_y_coord = FRAME_BOTTOM_BORDER_END (f);
	  high_y_coord = FRAME_PIXHEIGHT (f);
	  position = OVER_TOOLBAR;
	}
      else
	{
	  low_y_coord = FRAME_PIXHEIGHT (f);
	  high_y_coord = y_coord;
	  position = OVER_OUTSIDE;
	}
    }

  if (position != OVER_TOOLBAR && position != OVER_BORDER)
    {
      if (x_coord < frm_left)
	{
	  if (x_coord >= FRAME_LEFT_BORDER_START (f))
	    {
	      low_x_coord = FRAME_LEFT_BORDER_START (f);
	      high_x_coord = frm_left;
	      position = OVER_BORDER;
	    }
	  else if (x_coord >= 0)
	    {
	      low_x_coord = 0;
	      high_x_coord = FRAME_LEFT_BORDER_START (f);
	      position = OVER_TOOLBAR;
	    }
	  else
	    {
	      low_x_coord = x_coord;
	      high_x_coord = 0;
	      position = OVER_OUTSIDE;
	    }
	}
      else if (x_coord >= frm_right)
	{
	  if (x_coord < FRAME_RIGHT_BORDER_END (f))
	    {
	      low_x_coord = frm_right;
	      high_x_coord = FRAME_RIGHT_BORDER_END (f);
	      position = OVER_BORDER;
	    }
	  else if (x_coord < FRAME_PIXWIDTH (f))
	    {
	      low_x_coord = FRAME_RIGHT_BORDER_END (f);
	      high_x_coord = FRAME_PIXWIDTH (f);
	      position = OVER_TOOLBAR;
	    }
	  else
	    {
	      low_x_coord = FRAME_PIXWIDTH (f);
	      high_x_coord = x_coord;
	      position = OVER_OUTSIDE;
	    }
	}
    }

#ifdef HAVE_TOOLBARS
  if (position == OVER_TOOLBAR)
    {
      *obj1 = toolbar_button_at_pixpos (f, x_coord, y_coord);
      *obj2 = Qnil;
      *w = 0;
      UPDATE_CACHE_RETURN;
    }
#endif /* HAVE_TOOLBARS */

  /* We still have to return the window the pointer is next to and its
     relative y position even if it is outside the x boundary. */
  if (x_coord < frm_left)
    x_coord = frm_left;
  else if (x_coord > frm_right)
    x_coord = frm_right;

  /* Same in reverse. */
  if (y_coord < frm_top)
    y_coord = frm_top;
  else if (y_coord > frm_bottom)
    y_coord = frm_bottom;

  /* Find what window the given coordinates are actually in. */
  window = f->root_window;
  *w = find_window_by_pixel_pos (x_coord, y_coord, window);

  /* If we didn't find a window, we're done. */
  if (!*w)
    {
      UPDATE_CACHE_RETURN;
    }
  else if (position != OVER_NOTHING)
    {
      *closest = 0;
      *modeline_closest = -1;

      if (high_y_coord <= frm_top || high_y_coord >= frm_bottom)
	{
	  *w = 0;
	  UPDATE_CACHE_RETURN;
	}
    }

  /* Check if the window is a minibuffer but isn't active. */
  if (MINI_WINDOW_P (*w) && !minibuf_level)
    {
      /* Must reset the window value since some callers will ignore
         the return value if it is set. */
      *w = 0;
      UPDATE_CACHE_RETURN;
    }

  /* See if the point is over window vertical divider */
  if (window_needs_vertical_divider (*w))
    {
      int div_x_high = WINDOW_RIGHT (*w);
      int div_x_low  = div_x_high - window_divider_width (*w);
      int div_y_high = WINDOW_BOTTOM (*w);
      int div_y_low  = WINDOW_TOP (*w);

      if (div_x_low < x_coord && x_coord <= div_x_high &&
	  div_y_low < y_coord && y_coord <= div_y_high)
	{
	  low_x_coord = div_x_low;
	  high_x_coord = div_x_high;
	  low_y_coord = div_y_low;
	  high_y_coord = div_y_high;
	  position = OVER_V_DIVIDER;
	  UPDATE_CACHE_RETURN;
	}
    }

  dla = window_display_lines (*w, CURRENT_DISP);

  for (*row = 0; *row < Dynarr_length (dla); (*row)++)
    {
      int really_over_nothing = 0;
      struct display_line *dl = Dynarr_atp (dla, *row);

      if ((int) (dl->ypos - dl->ascent) <= y_coord
	  && y_coord <= (int) (dl->ypos + dl->descent))
	{
	  int check_margin_glyphs = 0;
	  struct display_block *db = get_display_block_from_line (dl, TEXT);
	  struct rune *rb = 0;

	  if (x_coord < dl->bounds.left_white
	      || x_coord >= dl->bounds.right_white)
	    check_margin_glyphs = 1;

	  low_y_coord = dl->ypos - dl->ascent;
	  high_y_coord = dl->ypos + dl->descent + 1;

	  if (position == OVER_BORDER
	      || position == OVER_OUTSIDE
	      || check_margin_glyphs)
	    {
	      int x_check, left_bound;

	      if (check_margin_glyphs)
		{
		  x_check = x_coord;
		  left_bound = dl->bounds.left_white;
		}
	      else
		{
		  x_check = high_x_coord;
		  left_bound = frm_left;
		}

	      if (Dynarr_length (db->runes))
		{
		  if (x_check <= left_bound)
		    {
		      if (dl->modeline)
			*modeline_closest = Dynarr_atp (db->runes, 0)->bufpos;
		      else
			*closest = Dynarr_atp (db->runes, 0)->bufpos;
		    }
		  else
		    {
		      if (dl->modeline)
			*modeline_closest =
			  Dynarr_atp (db->runes,
				      Dynarr_length (db->runes) - 1)->bufpos;
		      else
			*closest =
			  Dynarr_atp (db->runes,
				      Dynarr_length (db->runes) - 1)->bufpos;
		    }

		  if (dl->modeline)
		    *modeline_closest += dl->offset;
		  else
		    *closest += dl->offset;
		}
	      else
		{
		  /* #### What should be here. */
		  if (dl->modeline)
		    *modeline_closest = 0;
		  else
		    *closest = 0;
		}

	      if (check_margin_glyphs)
		{
		  if (x_coord < dl->bounds.left_in
		      || x_coord >= dl->bounds.right_in)
		    {
		      /* If we are over the outside margins then we
                         know the loop over the text block isn't going
                         to accomplish anything.  So we go ahead and
                         set what information we can right here and
                         return. */
		      (*row)--;
		      *obj_y = y_coord - (dl->ypos - dl->ascent);
		      get_position_object (dl, obj1, obj2, x_coord,
					   &low_x_coord, &high_x_coord);

		      UPDATE_CACHE_RETURN;
		    }
		}
	      else
		UPDATE_CACHE_RETURN;
	    }

	  for (*col = 0; *col <= Dynarr_length (db->runes); (*col)++)
	    {
	      int past_end = (*col == Dynarr_length (db->runes));

	      if (!past_end)
		rb = Dynarr_atp (db->runes, *col);

	      if (past_end ||
		  (rb->xpos <= x_coord && x_coord < rb->xpos + rb->width))
		{
		  if (past_end)
		    {
		      (*col)--;
		      rb = Dynarr_atp (db->runes, *col);
		    }

		  *bufpos = rb->bufpos + dl->offset;
		  low_x_coord = rb->xpos;
		  high_x_coord = rb->xpos + rb->width;

		  if (rb->type == RUNE_DGLYPH)
		    {
		      int elt = *col + 1;

		      /* Find the first character after the glyph. */
		      while (elt < Dynarr_length (db->runes))
			{
			  if (Dynarr_atp (db->runes, elt)->type != RUNE_DGLYPH)
			    {
			      if (dl->modeline)
				*modeline_closest =
				  (Dynarr_atp (db->runes, elt)->bufpos +
				   dl->offset);
			      else
				*closest =
				  (Dynarr_atp (db->runes, elt)->bufpos +
				   dl->offset);
			      break;
			    }

			  elt++;
			}

		      /* In this case we failed to find a non-glyph
                         character so we return the last position
                         displayed on the line. */
		      if (elt == Dynarr_length (db->runes))
			{
			  if (dl->modeline)
			    *modeline_closest = dl->end_bufpos + dl->offset;
			  else
			    *closest = dl->end_bufpos + dl->offset;
			  really_over_nothing = 1;
			}
		    }
		  else
		    {
		      if (dl->modeline)
			*modeline_closest = rb->bufpos + dl->offset;
		      else
			*closest = rb->bufpos + dl->offset;
		    }

		  if (dl->modeline)
		    {
		      *row = window_displayed_height (*w);

		      if (position == OVER_NOTHING)
			position = OVER_MODELINE;

		      if (rb->type == RUNE_DGLYPH)
			{
			  *obj1 = rb->object.dglyph.glyph;
			  *obj2 = rb->object.dglyph.extent;
			}
		      else if (rb->type == RUNE_CHAR)
			{
			  *obj1 = Qnil;
			  *obj2 = Qnil;
			}
		      else
			{
			  *obj1 = Qnil;
			  *obj2 = Qnil;
			}

		      UPDATE_CACHE_RETURN;
		    }
		  else if (past_end
			   || (rb->type == RUNE_CHAR
			       && rb->object.chr.ch == '\n'))
		    {
		      (*row)--;
		      /* At this point we may have glyphs in the right
                         inside margin. */
		      if (check_margin_glyphs)
			get_position_object (dl, obj1, obj2, x_coord,
					     &low_x_coord, &high_x_coord);
		      UPDATE_CACHE_RETURN;
		    }
		  else
		    {
		      (*row)--;
		      if (rb->type == RUNE_DGLYPH)
			{
			  *obj1 = rb->object.dglyph.glyph;
			  *obj2 = rb->object.dglyph.extent;
			}
		      else if (rb->type == RUNE_CHAR)
			{
			  *obj1 = Qnil;
			  *obj2 = Qnil;
			}
		      else
			{
			  *obj1 = Qnil;
			  *obj2 = Qnil;
			}

		      *obj_x = x_coord - rb->xpos;
		      *obj_y = y_coord - (dl->ypos - dl->ascent);

		      /* At this point we may have glyphs in the left
                         inside margin. */
		      if (check_margin_glyphs)
			get_position_object (dl, obj1, obj2, x_coord, 0, 0);

		      if (position == OVER_NOTHING && !really_over_nothing)
			position = OVER_TEXT;

		      UPDATE_CACHE_RETURN;
		    }
		}
	    }
	}
    }

  *row = Dynarr_length (dla) - 1;
  if (FRAME_WIN_P (f))
    {
      int bot_elt = Dynarr_length (dla) - 1;

      if (bot_elt >= 0)
	{
	  struct display_line *dl = Dynarr_atp (dla, bot_elt);
	  int adj_area = y_coord - (dl->ypos + dl->descent);
	  Lisp_Object lwin;
	  int defheight;

	  XSETWINDOW (lwin, *w);
	  default_face_height_and_width (lwin, 0, &defheight);

	  *row += (adj_area / defheight);
	}
    }

  /* #### This should be checked out some more to determine what
     should really be going on. */
  if (!MARKERP ((*w)->start[CURRENT_DISP]))
    *closest = 0;
  else
    *closest = end_of_last_line (*w,
				 marker_position ((*w)->start[CURRENT_DISP]));
  *col = 0;
  UPDATE_CACHE_RETURN;
}
#undef UPDATE_CACHE_RETURN


/***************************************************************************/
/*									   */
/*                             Lisp functions                              */
/*									   */
/***************************************************************************/

DEFUN ("redisplay-echo-area", Fredisplay_echo_area, 0, 0, 0, /*
Ensure that all minibuffers are correctly showing the echo area.
*/
       ())
{
  Lisp_Object devcons, concons;

  DEVICE_LOOP_NO_BREAK (devcons, concons)
    {
      struct device *d = XDEVICE (XCAR (devcons));
      Lisp_Object frmcons;

      DEVICE_FRAME_LOOP (frmcons, d)
	{
	  struct frame *f = XFRAME (XCAR (frmcons));

	  if (FRAME_REPAINT_P (f) && FRAME_HAS_MINIBUF_P (f))
	    {
	      Lisp_Object window = FRAME_MINIBUF_WINDOW (f);
	      /*
	       * If the frame size has changed, there may be random
	       * chud on the screen left from previous messages
	       * because redisplay_frame hasn't been called yet.
	       * Clear the screen to get rid of the potential mess.
	       */
	      if (f->echo_area_garbaged)
		{
		  DEVMETH (d, clear_frame, (f));
		  f->echo_area_garbaged = 0;
		}
	      redisplay_window (window, 0);
	      call_redisplay_end_triggers (XWINDOW (window), 0);
	    }
	}

      /* We now call the output_end routine for tty frames.  We delay
	 doing so in order to avoid cursor flicker.  So much for 100%
	 encapsulation. */
      if (DEVICE_TTY_P (d))
	DEVMETH (d, output_end, (d));
    }

  return Qnil;
}

static Lisp_Object
restore_disable_preemption_value (Lisp_Object value)
{
  disable_preemption = XINT (value);
  return Qnil;
}

DEFUN ("redraw-frame", Fredraw_frame, 0, 2, 0, /*
Clear frame FRAME and output again what is supposed to appear on it.
FRAME defaults to the selected frame if omitted.
Normally, redisplay is preempted as normal if input arrives.  However,
if optional second arg NO-PREEMPT is non-nil, redisplay will not stop for
input and is guaranteed to proceed to completion.
*/
       (frame, no_preempt))
{
  struct frame *f = decode_frame (frame);
  int count = specpdl_depth ();

  if (!NILP (no_preempt))
    {
      record_unwind_protect (restore_disable_preemption_value,
			     make_int (disable_preemption));
      disable_preemption++;
    }

  f->clear = 1;
  redisplay_frame (f, 1);

  return unbind_to (count, Qnil);
}

DEFUN ("redisplay-frame", Fredisplay_frame, 0, 2, 0, /*
Ensure that FRAME's contents are correctly displayed.
This differs from `redraw-frame' in that it only redraws what needs to
be updated, as opposed to unconditionally clearing and redrawing
the frame.
FRAME defaults to the selected frame if omitted.
Normally, redisplay is preempted as normal if input arrives.  However,
if optional second arg NO-PREEMPT is non-nil, redisplay will not stop for
input and is guaranteed to proceed to completion.
*/
       (frame, no_preempt))
{
  struct frame *f = decode_frame (frame);
  int count = specpdl_depth ();

  if (!NILP (no_preempt))
    {
      record_unwind_protect (restore_disable_preemption_value,
			     make_int (disable_preemption));
      disable_preemption++;
    }

  redisplay_frame (f, 1);

  return unbind_to (count, Qnil);
}

DEFUN ("redraw-device", Fredraw_device, 0, 2, 0, /*
Clear device DEVICE and output again what is supposed to appear on it.
DEVICE defaults to the selected device if omitted.
Normally, redisplay is preempted as normal if input arrives.  However,
if optional second arg NO-PREEMPT is non-nil, redisplay will not stop for
input and is guaranteed to proceed to completion.
*/
     (device, no_preempt))
{
  struct device *d = decode_device (device);
  Lisp_Object frmcons;
  int count = specpdl_depth ();

  if (!NILP (no_preempt))
    {
      record_unwind_protect (restore_disable_preemption_value,
			     make_int (disable_preemption));
      disable_preemption++;
    }

  DEVICE_FRAME_LOOP (frmcons, d)
    {
      XFRAME (XCAR (frmcons))->clear = 1;
    }
  redisplay_device (d);

  return unbind_to (count, Qnil);
}

DEFUN ("redisplay-device", Fredisplay_device, 0, 2, 0, /*
Ensure that DEVICE's contents are correctly displayed.
This differs from `redraw-device' in that it only redraws what needs to
be updated, as opposed to unconditionally clearing and redrawing
the device.
DEVICE defaults to the selected device if omitted.
Normally, redisplay is preempted as normal if input arrives.  However,
if optional second arg NO-PREEMPT is non-nil, redisplay will not stop for
input and is guaranteed to proceed to completion.
*/
       (device, no_preempt))
{
  struct device *d = decode_device (device);
  int count = specpdl_depth ();

  if (!NILP (no_preempt))
    {
      record_unwind_protect (restore_disable_preemption_value,
			     make_int (disable_preemption));
      disable_preemption++;
    }

  redisplay_device (d);

  return unbind_to (count, Qnil);
}

/* Big lie.  Big lie.  This will force all modelines to be updated
   regardless if the all flag is set or not.  It remains in existence
   solely for backwards compatibility. */
DEFUN ("redraw-modeline", Fredraw_modeline, 0, 1, 0, /*
Force the modeline of the current buffer to be redisplayed.
With optional non-nil ALL, force redisplay of all modelines.
*/
       (all))
{
  MARK_MODELINE_CHANGED;
  return Qnil;
}

DEFUN ("force-cursor-redisplay", Fforce_cursor_redisplay, 0, 1, 0, /*
Force an immediate update of the cursor on FRAME.
FRAME defaults to the selected frame if omitted.
*/
  (frame))
{
  redisplay_redraw_cursor (decode_frame (frame), 1);
  return Qnil;
}


/***************************************************************************/
/*									   */
/*                     Lisp-variable change triggers                       */
/*									   */
/***************************************************************************/

static void
margin_width_changed_in_frame (Lisp_Object specifier, struct frame *f,
			       Lisp_Object oldval)
{
  /* Nothing to be done? */
}

int
redisplay_variable_changed (Lisp_Object sym, Lisp_Object *val,
			    Lisp_Object in_object, int flags)
{
  /* #### clip_changed should really be renamed something like
     global_redisplay_change. */
  MARK_CLIP_CHANGED;
  return 0;
}

void
redisplay_glyph_changed (Lisp_Object glyph, Lisp_Object property,
			 Lisp_Object locale)
{
  if (WINDOWP (locale))
    {
      MARK_FRAME_GLYPHS_CHANGED (XFRAME (WINDOW_FRAME (XWINDOW (locale))));
    }
  else if (FRAMEP (locale))
    {
      MARK_FRAME_GLYPHS_CHANGED (XFRAME (locale));
    }
  else if (DEVICEP (locale))
    {
      Lisp_Object frmcons;
      DEVICE_FRAME_LOOP (frmcons, XDEVICE (locale))
	MARK_FRAME_GLYPHS_CHANGED (XFRAME (XCAR (frmcons)));
    }
  else if (CONSOLEP (locale))
    {
      Lisp_Object frmcons, devcons;
      CONSOLE_FRAME_LOOP_NO_BREAK (frmcons, devcons, XCONSOLE (locale))
	MARK_FRAME_GLYPHS_CHANGED (XFRAME (XCAR (frmcons)));
    }
  else /* global or buffer */
    {
      Lisp_Object frmcons, devcons, concons;
      FRAME_LOOP_NO_BREAK (frmcons, devcons, concons)
	MARK_FRAME_GLYPHS_CHANGED (XFRAME (XCAR (frmcons)));
    }
}

static void
text_cursor_visible_p_changed (Lisp_Object specifier, struct window *w,
			       Lisp_Object oldval)
{
  if (XFRAME (w->frame)->init_finished)
    Fforce_cursor_redisplay (w->frame);
}

#ifdef MEMORY_USAGE_STATS


/***************************************************************************/
/*									   */
/*                        memory usage computation                         */
/*									   */
/***************************************************************************/

static int
compute_rune_dynarr_usage (rune_dynarr *dyn, struct overhead_stats *ovstats)
{
  return dyn ? Dynarr_memory_usage (dyn, ovstats) : 0;
}

static int
compute_display_block_dynarr_usage (display_block_dynarr *dyn,
				    struct overhead_stats *ovstats)
{
  int total, i;

  if (!dyn)
    return 0;

  total = Dynarr_memory_usage (dyn, ovstats);
  for (i = 0; i < Dynarr_largest (dyn); i++)
    total += compute_rune_dynarr_usage (Dynarr_at (dyn, i).runes, ovstats);

  return total;
}

static int
compute_glyph_block_dynarr_usage (glyph_block_dynarr *dyn,
				  struct overhead_stats *ovstats)
{
  return dyn ? Dynarr_memory_usage (dyn, ovstats) : 0;
}

int
compute_display_line_dynarr_usage (display_line_dynarr *dyn,
				   struct overhead_stats *ovstats)
{
  int total, i;

  if (!dyn)
    return 0;

  total = Dynarr_memory_usage (dyn, ovstats);
  for (i = 0; i < Dynarr_largest (dyn); i++)
    {
      struct display_line *dl = &Dynarr_at (dyn, i);
      total += compute_display_block_dynarr_usage(dl->display_blocks, ovstats);
      total += compute_glyph_block_dynarr_usage  (dl->left_glyphs,    ovstats);
      total += compute_glyph_block_dynarr_usage  (dl->right_glyphs,   ovstats);
    }

  return total;
}

int
compute_line_start_cache_dynarr_usage (line_start_cache_dynarr *dyn,
				       struct overhead_stats *ovstats)
{
  return dyn ? Dynarr_memory_usage (dyn, ovstats) : 0;
}

#endif /* MEMORY_USAGE_STATS */


/***************************************************************************/
/*									   */
/*                              initialization                             */
/*									   */
/***************************************************************************/

void
init_redisplay (void)
{
  disable_preemption = 0;
  preemption_count = 0;
  max_preempts = INIT_MAX_PREEMPTS;

  if (!initialized)
    {
      cmotion_display_lines = Dynarr_new (display_line);
      mode_spec_bufbyte_string = Dynarr_new (Bufbyte);
      formatted_string_emchar_dynarr = Dynarr_new (Emchar);
      formatted_string_extent_dynarr = Dynarr_new (EXTENT);
      formatted_string_extent_start_dynarr = Dynarr_new (Bytecount);
      formatted_string_extent_end_dynarr = Dynarr_new (Bytecount);
      internal_cache = Dynarr_new (line_start_cache);
      xzero (formatted_string_display_line);
    }

  /* window system is nil when in -batch mode */
  if (!initialized || noninteractive)
    return;

  /* If the user wants to use a window system, we shouldn't bother
     initializing the terminal.  This is especially important when the
     terminal is so dumb that emacs gives up before and doesn't bother
     using the window system.

     If the DISPLAY environment variable is set, try to use X, and die
     with an error message if that doesn't work.  */

#ifdef HAVE_X_WINDOWS
  if (!strcmp (display_use, "x"))
    {
      /* Some stuff checks this way early. */
      Vwindow_system = Qx;
      Vinitial_window_system = Qx;
      return;
    }
#endif /* HAVE_X_WINDOWS */

#ifdef HAVE_MS_WINDOWS
  if (!strcmp (display_use, "mswindows"))
    {
      /* Some stuff checks this way early. */
      Vwindow_system = Qmswindows;
      Vinitial_window_system = Qmswindows;
      return;
    }
#endif /* HAVE_MS_WINDOWS */

#ifdef HAVE_TTY
  /* If no window system has been specified, try to use the terminal.  */
  if (!isatty (0))
    {
      stderr_out ("XEmacs: standard input is not a tty\n");
      exit (1);
    }

  /* Look at the TERM variable */
  if (!getenv ("TERM"))
    {
      stderr_out ("Please set the environment variable TERM; see tset(1).\n");
      exit (1);
    }

  Vinitial_window_system = Qtty;
  return;
#else  /* not HAVE_TTY */
  /* No DISPLAY specified, and no TTY support. */
  stderr_out ("XEmacs: Cannot open display.\n\
Please set the environmental variable DISPLAY to an appropriate value.\n");
  exit (1);
#endif
  /* Unreached. */
}

void
syms_of_redisplay (void)
{
  defsymbol (&Qcursor_in_echo_area, "cursor-in-echo-area");
#ifndef INHIBIT_REDISPLAY_HOOKS
  defsymbol (&Qpre_redisplay_hook, "pre-redisplay-hook");
  defsymbol (&Qpost_redisplay_hook, "post-redisplay-hook");
#endif /* INHIBIT_REDISPLAY_HOOKS */
  defsymbol (&Qdisplay_warning_buffer, "display-warning-buffer");
  defsymbol (&Qbar_cursor, "bar-cursor");
  defsymbol (&Qwindow_scroll_functions, "window-scroll-functions");
  defsymbol (&Qredisplay_end_trigger_functions,
	     "redisplay-end-trigger-functions");

  DEFSUBR (Fredisplay_echo_area);
  DEFSUBR (Fredraw_frame);
  DEFSUBR (Fredisplay_frame);
  DEFSUBR (Fredraw_device);
  DEFSUBR (Fredisplay_device);
  DEFSUBR (Fredraw_modeline);
  DEFSUBR (Fforce_cursor_redisplay);
}

void
vars_of_redisplay (void)
{
#if 0
  staticpro (&last_arrow_position);
  staticpro (&last_arrow_string);
  last_arrow_position = Qnil;
  last_arrow_string = Qnil;
#endif /* 0 */

  updating_line_start_cache = 0;

  /* #### Probably temporary */
  DEFVAR_INT ("redisplay-cache-adjustment", &cache_adjustment /*
\(Temporary) Setting this will impact the performance of the internal
line start cache.
*/ );
  cache_adjustment = 2;

  DEFVAR_INT_MAGIC ("pixel-vertical-clip-threshold", &vertical_clip /*
Minimum pixel height for clipped bottom display line.
A clipped line shorter than this won't be displayed.
*/ ,
		    redisplay_variable_changed);
  vertical_clip = 5;

  DEFVAR_INT_MAGIC ("pixel-horizontal-clip-threshold", &horizontal_clip /*
Minimum visible area for clipped glyphs at right boundary.
Clipped glyphs shorter than this won't be displayed.
Only pixmap glyph instances are currently allowed to be clipped.
*/ ,
		    redisplay_variable_changed);
  horizontal_clip = 5;

  DEFVAR_LISP ("global-mode-string", &Vglobal_mode_string /*
String displayed by modeline-format's "%m" specification.
*/ );
  Vglobal_mode_string = Qnil;

  DEFVAR_LISP_MAGIC ("overlay-arrow-position", &Voverlay_arrow_position /*
Marker for where to display an arrow on top of the buffer text.
This must be the beginning of a line in order to work.
See also `overlay-arrow-string'.
*/ ,
		     redisplay_variable_changed);
  Voverlay_arrow_position = Qnil;

  DEFVAR_LISP_MAGIC ("overlay-arrow-string", &Voverlay_arrow_string /*
String to display as an arrow.  See also `overlay-arrow-position'.
*/ ,
		     redisplay_variable_changed);
  Voverlay_arrow_string = Qnil;

  DEFVAR_INT ("scroll-step", &scroll_step /*
*The number of lines to try scrolling a window by when point moves out.
If that fails to bring point back on frame, point is centered instead.
If this is zero, point is always centered after it moves off screen.
*/ );
  scroll_step = 0;

  DEFVAR_INT ("scroll-conservatively", &scroll_conservatively /*
*Scroll up to this many lines, to bring point back on screen.
*/ );
  scroll_conservatively = 0;

  DEFVAR_BOOL_MAGIC ("truncate-partial-width-windows",
		     &truncate_partial_width_windows /*
*Non-nil means truncate lines in all windows less than full frame wide.
*/ ,
		     redisplay_variable_changed);
  truncate_partial_width_windows = 1;

  DEFVAR_BOOL ("visible-bell", &visible_bell /*
*Non-nil means try to flash the frame to represent a bell.
*/ );
  visible_bell = 0;

  DEFVAR_BOOL ("no-redraw-on-reenter", &no_redraw_on_reenter /*
*Non-nil means no need to redraw entire frame after suspending.
A non-nil value is useful if the terminal can automatically preserve
Emacs's frame display when you reenter Emacs.
It is up to you to set this variable if your terminal can do that.
*/ );
  no_redraw_on_reenter = 0;

  DEFVAR_LISP ("window-system", &Vwindow_system /*
A symbol naming the window-system under which Emacs is running,
such as `x', or nil if emacs is running on an ordinary terminal.

Do not use this variable, except for GNU Emacs compatibility, as it
gives wrong values in a multi-device environment.  Use `console-type'
instead.
*/ );
  Vwindow_system = Qnil;

  /* #### Temporary shit until window-system is eliminated. */
  DEFVAR_LISP ("initial-window-system", &Vinitial_window_system /*
DON'T TOUCH
*/ );
  Vinitial_window_system = Qnil;

  DEFVAR_BOOL ("cursor-in-echo-area", &cursor_in_echo_area /*
Non-nil means put cursor in minibuffer, at end of any message there.
*/ );
  cursor_in_echo_area = 0;

  /* #### Shouldn't this be generalized as follows:

     if nil, use block cursor.
     if a number, use a bar cursor of that width.
     Otherwise, use a 1-pixel bar cursor.

     #### Or better yet, this variable should be trashed entirely
     (use a Lisp-magic variable to maintain compatibility)
     and a specifier `cursor-shape' added, which allows a block
     cursor, a bar cursor, a flashing block or bar cursor,
     maybe a caret cursor, etc. */

  DEFVAR_LISP ("bar-cursor", &Vbar_cursor /*
Use vertical bar cursor if non-nil.  If t width is 1 pixel, otherwise 2.
*/ );
  Vbar_cursor = Qnil;

#ifndef INHIBIT_REDISPLAY_HOOKS
  xxDEFVAR_LISP ("pre-redisplay-hook", &Vpre_redisplay_hook /*
Function or functions to run before every redisplay.
Functions on this hook must be careful to avoid signalling errors!
*/ );
  Vpre_redisplay_hook = Qnil;

  xxDEFVAR_LISP ("post-redisplay-hook", &Vpost_redisplay_hook /*
Function or functions to run after every redisplay.
Functions on this hook must be careful to avoid signalling errors!
*/ );
  Vpost_redisplay_hook = Qnil;
#endif /* INHIBIT_REDISPLAY_HOOKS */

  DEFVAR_INT ("display-warning-tick", &display_warning_tick /*
Bump this to tell the C code to call `display-warning-buffer'
at next redisplay.  You should not normally change this; the function
`display-warning' automatically does this at appropriate times.
*/ );
  display_warning_tick = 0;

  DEFVAR_BOOL ("inhibit-warning-display", &inhibit_warning_display /*
Non-nil means inhibit display of warning messages.
You should *bind* this, not set it.  Any pending warning messages
will be displayed when the binding no longer applies.
*/ );
  /* reset to 0 by startup.el after the splash screen has displayed.
     This way, the warnings don't obliterate the splash screen. */
  inhibit_warning_display = 1;

  DEFVAR_LISP ("window-size-change-functions",
               &Vwindow_size_change_functions /*
Not currently implemented.
Functions called before redisplay, if window sizes have changed.
The value should be a list of functions that take one argument.
Just before redisplay, for each frame, if any of its windows have changed
size since the last redisplay, or have been split or deleted,
all the functions in the list are called, with the frame as argument.
*/ );
  Vwindow_size_change_functions = Qnil;

  DEFVAR_LISP ("window-scroll-functions", &Vwindow_scroll_functions /*
Not currently implemented.
Functions to call before redisplaying a window with scrolling.
Each function is called with two arguments, the window
and its new display-start position.  Note that the value of `window-end'
is not valid when these functions are called.
*/ );
  Vwindow_scroll_functions = Qnil;

  DEFVAR_LISP ("redisplay-end-trigger-functions",
               &Vredisplay_end_trigger_functions /*
See `set-window-redisplay-end-trigger'.
*/ );
  Vredisplay_end_trigger_functions = Qnil;

  DEFVAR_BOOL ("column-number-start-at-one", &column_number_start_at_one /*
*Non-nil means column display number starts at 1.
*/ );
  column_number_start_at_one = 0;
}

void
specifier_vars_of_redisplay (void)
{
  DEFVAR_SPECIFIER ("left-margin-width", &Vleft_margin_width /*
*Width of left margin.
This is a specifier; use `set-specifier' to change it.
*/ );
  Vleft_margin_width = Fmake_specifier (Qnatnum);
  set_specifier_fallback (Vleft_margin_width, list1 (Fcons (Qnil, Qzero)));
  set_specifier_caching (Vleft_margin_width,
			 slot_offset (struct window, left_margin_width),
			 some_window_value_changed,
			 slot_offset (struct frame, left_margin_width),
			 margin_width_changed_in_frame);

  DEFVAR_SPECIFIER ("right-margin-width", &Vright_margin_width /*
*Width of right margin.
This is a specifier; use `set-specifier' to change it.
*/ );
  Vright_margin_width = Fmake_specifier (Qnatnum);
  set_specifier_fallback (Vright_margin_width, list1 (Fcons (Qnil, Qzero)));
  set_specifier_caching (Vright_margin_width,
			 slot_offset (struct window, right_margin_width),
			 some_window_value_changed,
			 slot_offset (struct frame, right_margin_width),
			 margin_width_changed_in_frame);

  DEFVAR_SPECIFIER ("minimum-line-ascent", &Vminimum_line_ascent /*
*Minimum ascent height of lines.
This is a specifier; use `set-specifier' to change it.
*/ );
  Vminimum_line_ascent = Fmake_specifier (Qnatnum);
  set_specifier_fallback (Vminimum_line_ascent, list1 (Fcons (Qnil, Qzero)));
  set_specifier_caching (Vminimum_line_ascent,
			 slot_offset (struct window, minimum_line_ascent),
			 some_window_value_changed,
			 0, 0);

  DEFVAR_SPECIFIER ("minimum-line-descent", &Vminimum_line_descent /*
*Minimum descent height of lines.
This is a specifier; use `set-specifier' to change it.
*/ );
  Vminimum_line_descent = Fmake_specifier (Qnatnum);
  set_specifier_fallback (Vminimum_line_descent, list1 (Fcons (Qnil, Qzero)));
  set_specifier_caching (Vminimum_line_descent,
			 slot_offset (struct window, minimum_line_descent),
			 some_window_value_changed,
			 0, 0);

  DEFVAR_SPECIFIER ("use-left-overflow", &Vuse_left_overflow /*
*Non-nil means use the left outside margin as extra whitespace when
displaying 'whitespace or 'inside-margin glyphs.
This is a specifier; use `set-specifier' to change it.
*/ );
  Vuse_left_overflow = Fmake_specifier (Qboolean);
  set_specifier_fallback (Vuse_left_overflow, list1 (Fcons (Qnil, Qnil)));
  set_specifier_caching (Vuse_left_overflow,
			 slot_offset (struct window, use_left_overflow),
			 some_window_value_changed,
			 0, 0);

  DEFVAR_SPECIFIER ("use-right-overflow", &Vuse_right_overflow /*
*Non-nil means use the right outside margin as extra whitespace when
displaying 'whitespace or 'inside-margin glyphs.
This is a specifier; use `set-specifier' to change it.
*/ );
  Vuse_right_overflow = Fmake_specifier (Qboolean);
  set_specifier_fallback (Vuse_right_overflow, list1 (Fcons (Qnil, Qnil)));
  set_specifier_caching (Vuse_right_overflow,
			 slot_offset (struct window, use_right_overflow),
			 some_window_value_changed,
			 0, 0);

  DEFVAR_SPECIFIER ("text-cursor-visible-p", &Vtext_cursor_visible_p /*
*Non-nil means the text cursor is visible (this is usually the case).
This is a specifier; use `set-specifier' to change it.
*/ );
  Vtext_cursor_visible_p = Fmake_specifier (Qboolean);
  set_specifier_fallback (Vtext_cursor_visible_p, list1 (Fcons (Qnil, Qt)));
  set_specifier_caching (Vtext_cursor_visible_p,
			 slot_offset (struct window, text_cursor_visible_p),
			 text_cursor_visible_p_changed,
			 0, 0);

}
