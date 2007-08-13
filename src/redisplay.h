/* Redisplay data structures.
   Copyright (C) 1994, 1995 Board of Trustees, University of Illinois.
   Copyright (C) 1996 Chuck Thompson.
   Copyright (C) 1995, 1996 Ben Wing.

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

#ifndef _XEMACS_REDISPLAY_H_
#define _XEMACS_REDISPLAY_H_

/* Redisplay DASSERT types */
#define DB_DISP_POS		1
#define DB_DISP_TEXT_LAYOUT	2
#define DB_DISP_REDISPLAY	4

/* These are the possible return values from pixel_to_glyph_translation. */
#define OVER_MODELINE		0
#define OVER_TEXT		1
#define OVER_OUTSIDE		2
#define OVER_NOTHING		3
#define OVER_BORDER		4
#define OVER_TOOLBAR		5

#define NO_BLOCK	-1

/* Imagine that the text in the buffer is displayed on a piece of paper
   the width of the frame and very very tall.  The line start cache is
   an array of struct line_start_cache's, describing the start and
   end buffer positions for a contiguous set of lines on that piece
   of paper. */

typedef struct line_start_cache line_start_cache;
struct line_start_cache
{
  Bufpos start, end;
  int height;
};

typedef struct
{
  Dynarr_declare (line_start_cache);
} line_start_cache_dynarr;

/* The possible types of runes.

   #### The Lisp_Glyph type is broken.  There should instead be a pixmap
   type.  Currently the device-specific output routines have to worry
   about whether the glyph is textual or not, etc.  For Mule this is
   a big problem because you might need multiple fonts to display the
   text.  It also eliminates optimizations that could come from glumping
   the text of multiple text glyphs together -- this makes displaying
   binary files (with lots of control chars, etc.) very very slow. */

#define RUNE_BLANK	0
#define RUNE_CHAR	1
#define RUNE_DGLYPH	2
#define RUNE_HLINE	3
#define RUNE_VLINE	4

#define CURSOR_ON	0
#define CURSOR_OFF	1
#define NO_CURSOR	2
#define NEXT_CURSOR	3
#define IGNORE_CURSOR	4

#define DEFAULT_INDEX	(face_index) 0
#define MODELINE_INDEX	(face_index) 1

/* A rune is a single display element, such as a printable character
   or pixmap.  Any single character in a buffer has one or more runes
   (or zero, if the character is invisible) corresponding to it.
   (Printable characters typically have one rune associated with them,
   but control characters have two -- a ^ and a letter -- and other
   non-printing characters (those displayed in octal) have four. */

typedef struct rune rune;
struct rune
{
  face_index findex;		/* face rune is displayed with.  The
				   face_index is an index into a
				   window-specific array of face cache
				   elements.  Each face cache element
				   corresponds to one "merged face"
				   (the result of merging all the
				   faces that overlap the rune) and
				   contains the instance values for
				   each of the face properties in this
				   particular window. */

  short xpos;			/* horizontal starting position in pixels */
  short width;			/* pixel width of rune */


  Bufpos bufpos;		/* buffer position this rune is displaying;
				   for the modeline, the value here is a
				   Charcount, but who's looking? */
  Bufpos endpos;		/* if set this rune covers a range of pos */
 				/* #### Chuck, what does it mean for a rune
				   to cover a range of pos?  I don't get
				   this. */
  unsigned int cursor_type :3;	/* is this rune covered by the cursor? */
  unsigned int type :3;		/* type of rune object */

  union				/* Information specific to the type of rune */
  {
    /* DGLYPH */
    struct
    {
      Lisp_Object glyph;
      Lisp_Object extent;	/* extent rune is attached to, if any.
                                   If this is a rune in the modeline
                                   then this might be nil. */

      int xoffset;		/* Number of pixels that need to be
				   chopped off the left of the glyph.
				   This has the effect of shifting the
				   glyph to the left while still clipping
				   at XPOS. */
    } dglyph;

    /* CHAR */
    struct
    {
      Emchar ch;		/* Cbaracter of this rune. */
    } chr;

    /* HLINE */
    struct
    {
      int thickness;		/* how thick to make hline */
      int yoffset;		/* how far down from top of line to put top */
    } hline;
  } object;			/* actual rune object */
};

typedef struct
{
  Dynarr_declare (rune);
} rune_dynarr;

/* These must have distinct values.  Note that the ordering actually
   represents priority levels.  TEXT has the lowest priority level. */
enum display_type
{
  TEXT,
  LEFT_OUTSIDE_MARGIN,
  LEFT_INSIDE_MARGIN,
  RIGHT_INSIDE_MARGIN,
  RIGHT_OUTSIDE_MARGIN,
  OVERWRITE
};

/* A display block represents a run of text on a single line.
   Apparently there is only one display block per line for each
   of the types listed in `enum display_type'.

   A display block consists mostly of an array of runes, one per
   atomic display element (printable character, pixmap, etc.). */

/* #### Yuckity yuckity yuck yuck yuck yuck yuck!!

   Chuck, I think you should redo this.  It should not be the
   responsibility of the device-specific code to worry about
   the different faces.  The generic stuff in redisplay-output.c
   should glump things up into sub-blocks, each of which
   corresponds to a single pixmap or a single run of text in
   the same font.

   It might still make sense for the device-specific output routine
   to get passed an entire display line.  That way, it can make
   calls to XDrawText() (which draws multiple runs of single-font
   data) instead of XDrawString().  The reason for this is to
   reduce the amount of X traffic, which will help things significantly
   on a slow line. */

typedef struct display_block display_block;
struct display_block
{
  enum display_type type;	/* type of display block */

  int start_pos;		/* starting pixel position of block */
  int end_pos;			/* ending pixel position of block */

  rune_dynarr *runes;		/* Dynamic array of runes */
};

typedef struct
{
  Dynarr_declare (display_block);
} display_block_dynarr;

typedef struct layout_bounds_type
{
  int left_out;
  int left_in;
  int left_white;
  int right_white;
  int right_in;
  int right_out;
} layout_bounds;

typedef struct glyph_block glyph_block;
struct glyph_block
{
  Lisp_Object glyph;
  Lisp_Object extent;
  /* The rest are only used by margin routines. */
  face_index findex;
  int active;
  int width;
};

typedef struct
{
  Dynarr_declare (glyph_block);
} glyph_block_dynarr;

typedef struct display_line display_line;
struct display_line
{
  short ypos;				/* vertical position in pixels
					   of the baseline for this line. */
  unsigned short ascent, descent;	/* maximum values for this line.
					   The ascent is the number of
					   pixels above the baseline, and
					   the descent is the number of
					   pixels below the baseline.
					   The descent includes the baseline
					   pixel-row itself, I think. */
  unsigned short clip;			/* amount of bottom of line to clip
					   in pixels.*/
  Bufpos bufpos;			/* first buffer position on line */
  Bufpos end_bufpos;			/* last buffer position on line */
  Charcount offset;			/* adjustment to bufpos vals */
  Charcount num_chars;			/* # of chars on line
					   including expansion of tabs
					   and control chars */
  int cursor_elt;			/* rune block of TEXT display
					   block cursor is at or -1 */
  char used_prop_data;			/* can't incrementally update if line
					   used propogation data */

  layout_bounds bounds;			/* line boundary positions */

  char modeline;			/* t if this line is a modeline */

  /* Dynamic array of display blocks */
  display_block_dynarr *display_blocks;

  /* Dynamic arrays of left and right glyph blocks */
  glyph_block_dynarr *left_glyphs;
  glyph_block_dynarr *right_glyphs;
};

typedef struct
{
  Dynarr_declare (display_line);
} display_line_dynarr;

/* It could be argued that the following two structs belong in
   extents.h, but they're only used by redisplay and it simplifies
   the header files to put them here. */

typedef struct
{
  Dynarr_declare (EXTENT);
} EXTENT_dynarr;

struct font_metric_info
{
  int width;
  int height;			/* always ascent + descent; for convenience */
  int ascent;
  int descent;

  int proportional_p;
};

/* NOTE NOTE NOTE: Currently the positions in an extent fragment
   structure are Bytind's, not Bufpos's.  This could change. */

struct extent_fragment
{
  Lisp_Object object; /* buffer or string */
  struct frame *frm;
  Bytind pos, end;
  EXTENT_dynarr *extents;
  glyph_block_dynarr *begin_glyphs, *end_glyphs;
  unsigned int invisible:1;
  unsigned int invisible_ellipses:1;
  unsigned int previously_invisible:1;
  unsigned int invisible_ellipses_already_displayed:1;
};


/*************************************************************************/
/*                              change flags                             */
/*************************************************************************/

/* Quick flags to signal redisplay.  redisplay() sets them all to 0
   when it finishes.  If none of them are set when it starts, it
   assumes that nothing needs to be done.  Functions that make a change
   that is (potentially) visible on the screen should set the
   appropriate flag.

   If any of these flags are set, redisplay will look more carefully
   to see if anything has really changed. */

/* non-nil if the contents of a buffer have changed since the last time
   redisplay completed */
extern int buffers_changed;
extern int buffers_changed_set;

/* Nonzero if head_clip or tail_clip of a buffer has changed
 since last redisplay that finished */
extern int clip_changed;
extern int clip_changed_set;

/* non-nil if any extent has changed since the last time redisplay completed */
extern int extents_changed;
extern int extents_changed_set;

/* non-nil if any face has changed since the last time redisplay completed */
extern int faces_changed;

/* Nonzero means one or more frames have been marked as garbaged */
extern int frame_changed;

/* True if any of the builtin display glyphs (continuation,
   hscroll, control-arrow, etc) is in need of updating
   somewhere. */
extern int glyphs_changed;
extern int glyphs_changed_set;

/* True if an icon is in need of updating somewhere. */
extern int icon_changed;
extern int icon_changed_set;

/* True if a menubar is in need of updating somewhere. */
extern int menubar_changed;
extern int menubar_changed_set;

/* true iff we should redraw the modelines on the next redisplay */
extern int modeline_changed;
extern int modeline_changed_set;

/* non-nil if point has changed in some buffer since the last time
   redisplay completed */
extern int point_changed;
extern int point_changed_set;

/* non-nil if some frame has changed its size */
extern int size_changed;

/* non-nil if some device has signaled that it wants to change size */
extern int asynch_device_change_pending;

/* non-nil if any toolbar has changed */
extern int toolbar_changed;
extern int toolbar_changed_set;

/* non-nil if any window has changed since the last time redisplay completed */
extern int windows_changed;

/* non-nil if any frame's window structure has changed since the last
   time redisplay completed */
extern int windows_structure_changed;

/* These macros can be relatively expensive.  Since they are often
   called numerous times between each call to redisplay, we keep track
   if each has already been called and don't bother doing most of the
   work if it is currently set. */

#define MARK_TYPE_CHANGED(object) do {					\
  if (!object##_changed_set) {						\
    Lisp_Object _devcons_, _concons_;					\
    DEVICE_LOOP_NO_BREAK (_devcons_, _concons_)				\
      {									\
        Lisp_Object _frmcons_;						\
        struct device *_d_ = XDEVICE (XCONS (_devcons_)->car);		\
        DEVICE_FRAME_LOOP (_frmcons_, _d_)				\
	  {								\
	    struct frame *_f_ = XFRAME (XCONS (_frmcons_)->car);	\
            _f_->object##_changed = 1;					\
	    _f_->modiff++;						\
	  }    								\
        _d_->object##_changed = 1;					\
      }									\
    object##_changed = 1;						\
    object##_changed_set = 1; }						\
  }  while (0)

#define MARK_BUFFERS_CHANGED MARK_TYPE_CHANGED (buffers)
#define MARK_CLIP_CHANGED MARK_TYPE_CHANGED (clip)
#define MARK_EXTENTS_CHANGED MARK_TYPE_CHANGED (extents)
#define MARK_ICON_CHANGED MARK_TYPE_CHANGED (icon)
#define MARK_MENUBAR_CHANGED MARK_TYPE_CHANGED (menubar)
#define MARK_MODELINE_CHANGED MARK_TYPE_CHANGED (modeline)
#define MARK_POINT_CHANGED MARK_TYPE_CHANGED (point)
#define MARK_TOOLBAR_CHANGED MARK_TYPE_CHANGED (toolbar)
#define MARK_GLYPHS_CHANGED MARK_TYPE_CHANGED (glyphs)

/* Anytime a console, device or frame is added or deleted we need to reset
   these flags. */
#define RESET_CHANGED_SET_FLAGS						\
  do {									\
    buffers_changed_set = 0;						\
    clip_changed_set = 0;						\
    extents_changed_set = 0;						\
    icon_changed_set = 0;						\
    menubar_changed_set = 0;						\
    modeline_changed_set = 0;						\
    point_changed_set = 0;						\
    toolbar_changed_set = 0;						\
    glyphs_changed_set = 0;						\
  } while (0)


/*************************************************************************/
/*                       redisplay global variables                      */
/*************************************************************************/

/* redisplay structre used by various utility routines. */
extern display_line_dynarr *cmotion_display_lines;

/* nil or a symbol naming the window system
   under which emacs is running
   ('x is the only current possibility) */
extern Lisp_Object Vwindow_system;

/* Nonzero means truncate lines in all windows less wide than the frame. */
extern int truncate_partial_width_windows;

/* Nonzero if we're in a display critical section. */
extern int in_display;

/* Nonzero means no need to redraw the entire frame on resuming
   a suspended Emacs.  This is useful on terminals with multiple pages,
   where one page is used for Emacs and another for all else. */
extern int no_redraw_on_reenter;

/* Nonzero means flash the frame instead of ringing the bell.  */
extern int visible_bell;

/* Thickness of shadow border around 3D modelines. */
extern Lisp_Object Vmodeline_shadow_thickness;

/* Scroll if point lands on the bottom line and that line is partially
   clipped. */
extern int scroll_on_clipped_lines;

extern Lisp_Object Vglobal_mode_string;

/* The following two variables are defined in emacs.c and are used
   to convey information discovered on the command line way early
   (before *anything* is initialized). */

/* If non-zero, a window-system was specified on the command line.
   Defined in emacs.c. */
extern int display_arg;

/* Type of display specified.  Defined in emacs.c. */
extern char *display_use;


/*************************************************************************/
/*                     redisplay exported functions                      */
/*************************************************************************/

int redisplay_text_width_string (struct window *w, int findex,
				 Bufbyte *nonreloc, Lisp_Object reloc,
				 Bytecount offset, Bytecount len);
int redisplay_frame_text_width_string (struct frame *f,
				       Lisp_Object face,
				       Bufbyte *nonreloc,
				       Lisp_Object reloc,
				       Bytecount offset, Bytecount len);
void redisplay (void);
struct display_block *get_display_block_from_line (struct display_line *dl,
						   enum display_type type);
layout_bounds calculate_display_line_boundaries (struct window *w,
						 int modeline);
Bufpos point_at_center (struct window *w, int type, Bufpos start,
			Bufpos point);
int line_at_center (struct window *w, int type, Bufpos start, Bufpos point);
int window_half_pixpos (struct window *w);
void redisplay_echo_area (void);
void free_display_structs (struct window_mirror *mir);
Bufbyte *generate_formatted_string (struct window *w, Lisp_Object format_str,
                                    Lisp_Object result_str, face_index findex,
                                    int type);
int real_current_modeline_height (struct window *w);
int pixel_to_glyph_translation (struct frame *f, int x_coord,
				int y_coord, int *col, int *row,
				int *obj_x, int *obj_y,
				struct window **w, Bufpos *bufpos,
				Bufpos *closest, Charcount *modeline_closest,
				Lisp_Object *obj1, Lisp_Object *obj2);
void glyph_to_pixel_translation (struct window *w, int char_x,
				 int char_y, int *pix_x, int *pix_y);
void mark_redisplay (void (*) (Lisp_Object));
int point_in_line_start_cache (struct window *w, Bufpos point,
			       int min_past);
int point_would_be_visible (struct window *w, Bufpos startp,
		    Bufpos point);
Bufpos start_of_last_line (struct window *w, Bufpos startp);
Bufpos end_of_last_line (struct window *w, Bufpos startp);
Bufpos start_with_line_at_pixpos (struct window *w, Bufpos point,
				  int pixpos);
Bufpos start_with_point_on_display_line (struct window *w, Bufpos point,
					 int line);
int redisplay_variable_changed (Lisp_Object sym, Lisp_Object *val,
				Lisp_Object in_object, int flags);
void redisplay_glyph_changed (Lisp_Object glyph, Lisp_Object property,
			      Lisp_Object locale);

#ifdef MEMORY_USAGE_STATS
int compute_display_line_dynarr_usage (display_line_dynarr *dyn,
				       struct overhead_stats *ovstats);
int compute_line_start_cache_dynarr_usage (line_start_cache_dynarr *dyn,
					   struct overhead_stats *ovstats);
#endif


/* defined in redisplay-output.c */
int get_next_display_block (layout_bounds bounds,
			    display_block_dynarr *dba, int start_pos,
			    int *next_start);
void redisplay_clear_bottom_of_window (struct window *w,
				       display_line_dynarr *ddla,
				       int min_start, int max_end);
void redisplay_update_line (struct window *w, int first_line,
			    int last_line, int update_values);
void redisplay_output_window (struct window *w);
int redisplay_move_cursor (struct window *w, Bufpos new_point,
			   int no_output_end);
void redisplay_redraw_cursor (struct frame *f, int run_begin_end_meths);
void output_display_line (struct window *w, display_line_dynarr *cdla,
			  display_line_dynarr *ddla, int line,
			  int force_start, int force_end);

#endif /* _XEMACS_REDISPLAY_H_ */
