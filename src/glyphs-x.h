/* X-specific glyphs and related.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.
   Copyright (C) 1995 Board of Trustees, University of Illinois.
   Copyright (C) 1995, 1996 Ben Wing
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

/* Synched up with:  Not in FSF. */

#ifndef _XEMACS_GLYPHS_X_H_
#define _XEMACS_GLYPHS_X_H_

#include "glyphs.h"

#ifdef HAVE_X_WINDOWS

#include "xintrinsic.h"
#include "../lwlib/lwlib.h"

/****************************************************************************
 *                         Image-Instance Object                            *
 ****************************************************************************/

struct x_image_instance_data
{
  Pixmap pixmap;
  Pixmap mask;
  Cursor cursor;

  /* If depth>0, then that means that other colors were allocated when
     this pixmap was loaded.  These are they; we need to free them when
     finalizing the image instance. */
  unsigned long *pixels;
  int npixels;

  /* Should we hang on to the extra info from the XpmAttributes, like
     the textual color table and the comments?   Is that useful? */
};

#define X_IMAGE_INSTANCE_DATA(i) ((struct x_image_instance_data *) (i)->data)

#define IMAGE_INSTANCE_X_PIXMAP(i) (X_IMAGE_INSTANCE_DATA (i)->pixmap)
#define IMAGE_INSTANCE_X_MASK(i) (X_IMAGE_INSTANCE_DATA (i)->mask)
#define IMAGE_INSTANCE_X_CURSOR(i) (X_IMAGE_INSTANCE_DATA (i)->cursor)
#define IMAGE_INSTANCE_X_PIXELS(i) (X_IMAGE_INSTANCE_DATA (i)->pixels)
#define IMAGE_INSTANCE_X_NPIXELS(i) (X_IMAGE_INSTANCE_DATA (i)->npixels)

#define XIMAGE_INSTANCE_X_PIXMAP(i) \
  IMAGE_INSTANCE_X_PIXMAP (XIMAGE_INSTANCE (i))
#define XIMAGE_INSTANCE_X_MASK(i) \
  IMAGE_INSTANCE_X_MASK (XIMAGE_INSTANCE (i))
#define XIMAGE_INSTANCE_X_CURSOR(i) \
  IMAGE_INSTANCE_X_CURSOR (XIMAGE_INSTANCE (i))
#define XIMAGE_INSTANCE_X_PIXELS(i) \
  IMAGE_INSTANCE_X_PIXELS (XIMAGE_INSTANCE (i))
#define XIMAGE_INSTANCE_X_NPIXELS(i) \
  IMAGE_INSTANCE_X_NPIXELS (XIMAGE_INSTANCE (i))

/* Set to 1 if you wish to implement this feature */
# define HAVE_SUBWINDOWS 0
# if HAVE_SUBWINDOWS
/****************************************************************************
 *                            Subwindow Object                              *
 ****************************************************************************/

DECLARE_LRECORD (subwindow, struct Lisp_Subwindow);
#define XSUBWINDOW(x) XRECORD (x, subwindow, struct Lisp_Subwindow)
#define XSETSUBWINDOW(x, p) XSETRECORD (x, p, subwindow)
#define SUBWINDOWP(x) RECORDP (x, subwindow)
#define GC_SUBWINDOWP(x) GC_RECORDP (x, subwindow)
#define CHECK_SUBWINDOW(x) CHECK_RECORD (x, subwindow)

struct Lisp_Subwindow
{
  struct lcrecord_header header;
  Lisp_Object frame;
  Screen *xscreen;
  Window parent_window;

  unsigned int width, height;
  Window subwindow;

  int being_displayed;		/* used to detect when needs to be unmapped */
};
# endif

#endif /* HAVE_X_WINDOWS */
#endif /* _XEMACS_GLYPHS_X_H_ */
