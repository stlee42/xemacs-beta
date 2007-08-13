/* X-specific Lisp objects.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.
   Copyright (C) 1995 Board of Trustees, University of Illinois.
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

/* Synched up with:  Not in FSF. */

#ifndef _XEMACS_OBJECTS_X_H_
#define _XEMACS_OBJECTS_X_H_

#include "objects.h"

#ifdef HAVE_X_WINDOWS

/*****************************************************************************
 Color-Instance
 ****************************************************************************/

struct x_color_instance_data
{
  XColor color;
};

#define X_COLOR_INSTANCE_DATA(c) ((struct x_color_instance_data *) (c)->data)
#define COLOR_INSTANCE_X_COLOR(c) (X_COLOR_INSTANCE_DATA (c)->color)

int allocate_nearest_color (Display *display, Colormap screen_colormap,
			    XColor *color_def);
int x_parse_nearest_color (struct device *d, XColor *color, Bufbyte *name,
			   Bytecount len, Error_behavior errb);

/*****************************************************************************
 Font-Instance
 ****************************************************************************/

struct x_font_instance_data
{
  /* X-specific information */
  Lisp_Object truename;
  XFontStruct *font;
};

#define X_FONT_INSTANCE_DATA(f) ((struct x_font_instance_data *) (f)->data)
#define FONT_INSTANCE_X_FONT(f) (X_FONT_INSTANCE_DATA (f)->font)
#define FONT_INSTANCE_X_TRUENAME(f) (X_FONT_INSTANCE_DATA (f)->truename)

#endif /* HAVE_X_WINDOWS */
#endif /* _XEMACS_OBJECTS_X_H_ */
