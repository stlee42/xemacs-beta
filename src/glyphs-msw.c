/* mswindows-specific Lisp objects.
   Copyright (C) 1998 Free Software Foundation, Inc.
   
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

/* written by Andy Piper <andyp@parallax.co.uk> plagerising buts from
   glyphs-x.c */

#include <config.h>
#include "lisp.h"
#include "lstream.h"
#include "console-msw.h"
#include "glyphs-msw.h"
#include "objects-msw.h"

#include "buffer.h"
#include "frame.h"
#include "insdel.h"
#include "opaque.h"
#include "sysfile.h"
#include "faces.h"
#include "imgproc.h"

#ifdef FILE_CODING
#include "file-coding.h"
#endif

DEFINE_IMAGE_INSTANTIATOR_FORMAT (bmp);
Lisp_Object Qbmp;
Lisp_Object Vmswindows_bitmap_file_path;

static void
mswindows_initialize_dibitmap_image_instance (struct Lisp_Image_Instance *ii,
					    enum image_instance_type type);

COLORREF mswindows_string_to_color (CONST char *name);

/************************************************************************/
/* convert from a series of RGB triples to a BITMAPINFO formated for the*/
/* proper display 							*/
/************************************************************************/
BITMAPINFO* EImage2DIBitmap (Lisp_Object device, int width, int height,
			    unsigned char *pic,
			    int *bit_count,
			    unsigned char** bmp_data)
{
  struct device *d = XDEVICE (device);
  int i,j;
  RGBQUAD* colortbl;
  int		ncolors;
  BITMAPINFO*	bmp_info;
  unsigned char *ip, *dp;

  if (DEVICE_MSWINDOWS_BITSPIXEL (d) > 0)
    {
      int bpline=(int)(~3UL & (unsigned long)((width*3) +3));
      /* FIXME: we can do this because 24bpp implies no colour table, once
       * we start paletizing this is no longer true. The X versions of
       * this function quantises to 256 colours or bit masks down to a
       * long. Windows can actually handle rgb triples in the raw so I
       * don't see much point trying to optimize down to the best
       * structure - unless it has memory / color allocation implications
       * .... */
      bmp_info=xnew_and_zero (BITMAPINFO);
      
      if (!bmp_info)
	{
	  return NULL;
	}

      bmp_info->bmiHeader.biBitCount=24; /* just RGB triples for now */
      bmp_info->bmiHeader.biCompression=BI_RGB; /* just RGB triples for now */
      bmp_info->bmiHeader.biSizeImage=width*height*3; 

      /* bitmap data needs to be in blue, green, red triples - in that
	 order, eimage is in RGB format so we need to convert */
      *bmp_data = xnew_array_and_zero (unsigned char, bpline * height);
      *bit_count = width * height * 3;

      if (!bmp_data)
	{
	  xfree (bmp_info);
	  return NULL;
	}

      ip = pic;
      for (i = height-1; i >= 0; i--) {
	dp = (*bmp_data) + (i * bpline);
	for (j = 0; j < width; j++) {
	  dp[2] =*ip++;
	  dp[1] =*ip++;
	  *dp   =*ip++;
	  dp += 3;
	}
      }
    }
  else				/* scale to 256 colors */
    {
      int rd,gr,bl;
      quant_table *qtable;
      int bpline= (int)(~3UL & (unsigned long)(width +3));
      /* Quantize the image and get a histogram while we're at it.
	 Do this first to save memory */
      qtable = build_EImage_quantable(pic, width, height, 256);
      if (qtable == NULL) return NULL;

      /* use our quantize table to allocate the colors */
      ncolors = qtable->num_active_colors;
      bmp_info=(BITMAPINFO*)xmalloc_and_zero (sizeof(BITMAPINFOHEADER) + 
					     sizeof(RGBQUAD) * ncolors);
      if (!bmp_info)
	{
	  xfree (qtable);
	  return NULL;
	}

      colortbl=(RGBQUAD*)(((unsigned char*)bmp_info)+sizeof(BITMAPINFOHEADER));

      bmp_info->bmiHeader.biBitCount=8; 
      bmp_info->bmiHeader.biCompression=BI_RGB; 
      bmp_info->bmiHeader.biSizeImage=bpline*height;
      bmp_info->bmiHeader.biClrUsed=ncolors; 
      bmp_info->bmiHeader.biClrImportant=ncolors; 
      
      *bmp_data = (unsigned char *) xmalloc_and_zero (bpline * height);
      *bit_count = bpline * height;

      if (!*bmp_data)
	{
	  xfree (qtable);
	  xfree (bmp_info);
	  return NULL;
	}
      
      /* build up an RGBQUAD colortable */
      for (i = 0; i < qtable->num_active_colors; i++) {
	colortbl[i].rgbRed = qtable->rm[i];
	colortbl[i].rgbGreen = qtable->gm[i];
	colortbl[i].rgbBlue = qtable->bm[i];
	colortbl[i].rgbReserved = 0;
      }

      /* now build up the data. picture has to be upside-down and
         back-to-front for msw bitmaps */
      ip = pic;
      for (i = height-1; i >= 0; i--) {
	dp = (*bmp_data) + (i * bpline);
	for (j = 0; j < width; j++) {
	  rd = *ip++;
	  gr = *ip++;
	  bl = *ip++;
	  *dp++ = QUANT_GET_COLOR (qtable,rd,gr,bl);
	}
      }
      xfree (qtable);
    } 
  /* fix up the standard stuff */
  bmp_info->bmiHeader.biWidth=width;
  bmp_info->bmiHeader.biHeight=height;
  bmp_info->bmiHeader.biPlanes=1;
  bmp_info->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  bmp_info->bmiHeader.biXPelsPerMeter=0; /* unless you know better */
  bmp_info->bmiHeader.biYPelsPerMeter=0; 

  return bmp_info;
}

/* Given a pixmap filename, look through all of the "standard" places
   where the file might be located.  Return a full pathname if found;
   otherwise, return Qnil. */

static Lisp_Object
locate_pixmap_file (Lisp_Object name)
{
  /* This function can GC if IN_REDISPLAY is false */
  Lisp_Object found;

  /* Check non-absolute pathnames with a directory component relative to
     the search path; that's the way Xt does it. */
  if (IS_DIRECTORY_SEP(XSTRING_BYTE (name, 0)) ||
      (XSTRING_BYTE (name, 0) == '.' &&
       (IS_DIRECTORY_SEP(XSTRING_BYTE (name, 1)) ||
	(XSTRING_BYTE (name, 1) == '.' &&
	 (IS_DIRECTORY_SEP(XSTRING_BYTE (name, 2)))))))
    {
      if (!NILP (Ffile_readable_p (name)))
	return name;
      else
	return Qnil;
    }

  if (locate_file (Vmswindows_bitmap_file_path, name, "", &found, R_OK) < 0)
    {
      Lisp_Object temp = list1 (Vdata_directory);
      struct gcpro gcpro1;

      GCPRO1 (temp);
      locate_file (temp, name, "", &found, R_OK);
      UNGCPRO;
    }
    
  return found;
}

/* If INSTANTIATOR refers to inline data, return Qnil.
   If INSTANTIATOR refers to data in a file, return the full filename
   if it exists; otherwise, return a cons of (filename).

   FILE_KEYWORD and DATA_KEYWORD are symbols specifying the
   keywords used to look up the file and inline data,
   respectively, in the instantiator.  Normally these would
   be Q_file and Q_data, but might be different for mask data. */

static Lisp_Object
potential_pixmap_file_instantiator (Lisp_Object instantiator,
				    Lisp_Object file_keyword,
				    Lisp_Object data_keyword)
{
  Lisp_Object file;
  Lisp_Object data;

  assert (VECTORP (instantiator));

  data = find_keyword_in_vector (instantiator, data_keyword);
  file = find_keyword_in_vector (instantiator, file_keyword);

  if (!NILP (file) && NILP (data))
    {
      Lisp_Object retval = locate_pixmap_file(file);
      if (!NILP (retval))
	return retval;
      else
	return Fcons (file, Qnil); /* should have been file */
    }

  return Qnil;
}

static Lisp_Object
simple_image_type_normalize (Lisp_Object inst, Lisp_Object console_type,
			     Lisp_Object image_type_tag)
{
  /* This function can call lisp */
  Lisp_Object file = Qnil;
  struct gcpro gcpro1, gcpro2;
  Lisp_Object alist = Qnil;

  GCPRO2 (file, alist);

  /* Now, convert any file data into inline data.  At the end of this,
     `data' will contain the inline data (if any) or Qnil, and `file'
     will contain the name this data was derived from (if known) or
     Qnil.

     Note that if we cannot generate any regular inline data, we
     skip out. */

  file = potential_pixmap_file_instantiator (inst, Q_file, Q_data);

  if (CONSP (file)) /* failure locating filename */
    signal_double_file_error ("Opening pixmap file",
			      "no such file or directory",
			      Fcar (file));

  if (NILP (file)) /* no conversion necessary */
    RETURN_UNGCPRO (inst);

  alist = tagged_vector_to_alist (inst);

  {
    Lisp_Object data = make_string_from_file (file);
    alist = remassq_no_quit (Q_file, alist);
    /* there can't be a :data at this point. */
    alist = Fcons (Fcons (Q_file, file),
		   Fcons (Fcons (Q_data, data), alist));
  }

  {
    Lisp_Object result = alist_to_tagged_vector (image_type_tag, alist);
    free_alist (alist);
    RETURN_UNGCPRO (result);
  }
}


/* Initialize an image instance from a bitmap

   DEST_MASK specifies the mask of allowed image types.

   If this fails, signal an error.  INSTANTIATOR is only used
   in the error message. */

static void
init_image_instance_from_dibitmap (struct Lisp_Image_Instance *ii,
				   BITMAPINFO *bmp_info,
				   int dest_mask,
				   void *bmp_data,
				   int bmp_bits,
				   Lisp_Object instantiator)
{
  Lisp_Object device = IMAGE_INSTANCE_DEVICE (ii);
  struct device *d = XDEVICE (device);
  struct frame *f = XFRAME (DEVICE_SELECTED_FRAME (d));
  void* bmp_buf=0;
  HBITMAP bitmap;
  HDC hdc;

  if (!DEVICE_MSWINDOWS_P (d))
    signal_simple_error ("Not an mswindows device", device);

  if (NILP (DEVICE_SELECTED_FRAME (d)))
    signal_simple_error ("No selected frame on mswindows device", device);

  if (!(dest_mask & IMAGE_COLOR_PIXMAP_MASK))
    incompatible_image_types (instantiator, dest_mask,
			      IMAGE_COLOR_PIXMAP_MASK);
  hdc = FRAME_MSWINDOWS_DC (f);

  bitmap=CreateDIBSection (hdc,  
			  bmp_info,
			  DIB_RGB_COLORS,
			  &bmp_buf, 
			  0,0);

  if (!bitmap || !bmp_buf)
    signal_simple_error ("Unable to create bitmap", instantiator);

  /* copy in the actual bitmap */
  memcpy (bmp_buf, bmp_data, bmp_bits);

  mswindows_initialize_dibitmap_image_instance (ii, IMAGE_COLOR_PIXMAP);

  IMAGE_INSTANCE_PIXMAP_FILENAME (ii) =
    find_keyword_in_vector (instantiator, Q_file);

  IMAGE_INSTANCE_MSWINDOWS_BITMAP (ii) = bitmap;
  IMAGE_INSTANCE_MSWINDOWS_MASK (ii) = bitmap;
  IMAGE_INSTANCE_PIXMAP_WIDTH (ii) = bmp_info->bmiHeader.biWidth;
  IMAGE_INSTANCE_PIXMAP_HEIGHT (ii) = bmp_info->bmiHeader.biHeight;
  IMAGE_INSTANCE_PIXMAP_DEPTH (ii) = bmp_info->bmiHeader.biBitCount;
}

void
mswindows_create_icon_from_image(Lisp_Object image, struct frame* f, int size)
{
  HBITMAP mask, bmp;
  HDC hcdc = FRAME_MSWINDOWS_CDC (f);
  HDC hdcDst = CreateCompatibleDC (hcdc);  
  ICONINFO x_icon;
  
  if (size!=16 && size!=32)
    {
      signal_simple_error("Icons must be 16x16 or 32x32", image);
    }

#if 0
  iIconWidth = GetSystemMetrics(SM_CXICON);
  iIconHeight = GetSystemMetrics(SM_CYICON);
#endif

  SelectObject(hcdc, XIMAGE_INSTANCE_MSWINDOWS_BITMAP (image)); 

  bmp = CreateCompatibleBitmap(hcdc, size, size);
  DeleteObject( SelectObject(hdcDst, bmp) );
  
  if (!StretchBlt(hdcDst, 0, 0, size, size,
		  hcdc, 0, 0, 
		  XIMAGE_INSTANCE_PIXMAP_WIDTH (image), 
		  XIMAGE_INSTANCE_PIXMAP_HEIGHT (image), 
		  SRCCOPY))
    {
      printf("StretchBlt failed\n");
    }
  
  if (!(mask = CreateBitmap(size, size, 1, 1, NULL)))
    {
      printf("CreateBitmap() failed\n");
    }
  if (!SelectObject(hdcDst, mask)
      ||
      !SelectObject(hcdc, bmp))
    {
      printf("SelectObject() failed\n");
    }
  
  if (!BitBlt(hdcDst, 0, 0, size, size,
	     hcdc, 0, 0, 
	     NOTSRCCOPY))
    {
      printf("BitBlt failed\n");
    }

  SelectObject(hdcDst, 0);
  SelectObject(hcdc, 0);
  /*   PatBlt(hdcDst, 0, 0, size, size, WHITENESS);*/
  
  x_icon.fIcon=TRUE;
  x_icon.xHotspot=XIMAGE_INSTANCE_PIXMAP_HOTSPOT_X (image);
  x_icon.yHotspot=XIMAGE_INSTANCE_PIXMAP_HOTSPOT_Y (image);
  x_icon.hbmMask=mask;
  x_icon.hbmColor=bmp;
  
  XIMAGE_INSTANCE_MSWINDOWS_ICON (image)=
    CreateIconIndirect (&x_icon);
  XIMAGE_INSTANCE_MSWINDOWS_MASK (image)=mask;
  
  DeleteDC(hdcDst);
}

int
mswindows_resize_dibitmap_instance (struct Lisp_Image_Instance* ii,
				    struct frame* f,
				    int newx, int newy)
{
  HBITMAP newbmp;
  HDC hcdc = FRAME_MSWINDOWS_CDC (f);
  HDC hdcDst = CreateCompatibleDC (hcdc);  
  
  SelectObject(hcdc, IMAGE_INSTANCE_MSWINDOWS_BITMAP (ii)); 
  
  newbmp = CreateCompatibleBitmap(hcdc, newx, newy);

  DeleteObject( SelectObject(hdcDst, newbmp) );
  
  if (!StretchBlt(hdcDst, 0, 0, newx, newy,
		  hcdc, 0, 0, 
		  IMAGE_INSTANCE_PIXMAP_WIDTH (ii), 
		  IMAGE_INSTANCE_PIXMAP_HEIGHT (ii), 
		  SRCCOPY))
    {
      return FALSE;
    }
  
  SelectObject(hdcDst, 0);
  SelectObject(hcdc, 0);
  
  if (IMAGE_INSTANCE_MSWINDOWS_BITMAP (ii))
    DeleteObject (IMAGE_INSTANCE_MSWINDOWS_BITMAP (ii));
  if (IMAGE_INSTANCE_MSWINDOWS_MASK (ii))
    DeleteObject (IMAGE_INSTANCE_MSWINDOWS_MASK (ii));

  IMAGE_INSTANCE_MSWINDOWS_BITMAP (ii) = newbmp;
  IMAGE_INSTANCE_MSWINDOWS_MASK (ii) = newbmp;
  IMAGE_INSTANCE_PIXMAP_WIDTH (ii) = newx;
  IMAGE_INSTANCE_PIXMAP_HEIGHT (ii) = newy;

  DeleteDC(hdcDst);

  return TRUE;
}

/**********************************************************************
 *                               XPM                                  *
 **********************************************************************/

#ifdef HAVE_XPM
static int xpm_to_eimage (Lisp_Object image, CONST Extbyte *buffer,
			  unsigned char** data,
			  int* width, int* height,
			  int* x_hot, int* y_hot,
			  COLORREF bg)
{
  XpmImage xpmimage;
  XpmInfo xpminfo;
  int result, i, transp_idx, maskbpline;
  unsigned char* dptr;
  unsigned int* sptr;
  COLORREF color; /* the american spelling virus hits again .. */
  COLORREF* colortbl; 

  xzero (xpmimage);
  xzero (xpminfo);
  xpminfo.valuemask=XpmHotspot;

  result = XpmCreateXpmImageFromBuffer ((char*)buffer,
				       &xpmimage,
				       &xpminfo);
  switch (result)
    {
    case XpmSuccess:
      break;
    case XpmFileInvalid:
      {
	signal_simple_error ("invalid XPM data", image);
      }
    case XpmNoMemory:
      {
	signal_double_file_error ("Parsing pixmap data",
				  "out of memory", image);
      }
    default:
      {
	signal_double_file_error_2 ("Parsing pixmap data",
				    "unknown error code",
				    make_int (result), image);
      }
    }
  
  *width = xpmimage.width;
  *height = xpmimage.height;
  maskbpline = (int)(~3UL & (unsigned long)
		     (((~7UL & (unsigned long)(*width + 7)) / 8) + 3));
  
  *data = xnew_array_and_zero (unsigned char, *width * *height * 3);

  if (!*data)
    {
      XpmFreeXpmImage (&xpmimage);
      XpmFreeXpmInfo (&xpminfo);
      return 0;
    }

  /* build a color table to speed things up */
  colortbl = xnew_array_and_zero (COLORREF, xpmimage.ncolors);
  if (!colortbl)
    {
      xfree (*data);
      XpmFreeXpmImage (&xpmimage);
      XpmFreeXpmInfo (&xpminfo);
      return 0;
    }

  for (i=0; i<xpmimage.ncolors; i++)
    {
				/* pick up transparencies */
      if (!strcmp (xpmimage.colorTable[i].c_color,"None"))
	{
	  colortbl[i]=bg; /* PALETTERGB(0,0,0); */
	  transp_idx=i;
	}
      else
	{
	  colortbl[i]=
	    mswindows_string_to_color (xpmimage.colorTable[i].c_color);
	}
    }

  /* convert the image */
  sptr=xpmimage.data;
  dptr=*data;
  for (i = 0; i< *width * *height; i++)
    {
      color = colortbl[*sptr++];

      /* split out the 0x02bbggrr colorref into an rgb triple */
      *dptr++=GetRValue (color); /* red */
      *dptr++=GetGValue (color); /* green */
      *dptr++=GetBValue (color); /* blue */
    }

  *x_hot=xpminfo.x_hotspot;
  *y_hot=xpminfo.y_hotspot;

  XpmFreeXpmImage (&xpmimage);
  XpmFreeXpmInfo (&xpminfo);
  xfree (colortbl);
  return TRUE;
}

Lisp_Object
mswindows_xpm_normalize (Lisp_Object inst, Lisp_Object console_type)
{
  return simple_image_type_normalize (inst, console_type, Qxpm);
}

void
mswindows_xpm_instantiate (Lisp_Object image_instance,
			   Lisp_Object instantiator,
			   Lisp_Object pointer_fg, Lisp_Object pointer_bg,
			   int dest_mask, Lisp_Object domain)
{
  struct Lisp_Image_Instance *ii = XIMAGE_INSTANCE (image_instance);
  Lisp_Object device = IMAGE_INSTANCE_DEVICE (ii);
  CONST Extbyte		*bytes;
  Extcount 		len;
  unsigned char		*eimage;
  int			width, height, x_hot, y_hot;
  BITMAPINFO*		bmp_info;
  unsigned char*	bmp_data;
  int			bmp_bits;
  COLORREF		bkcolor;
  
  Lisp_Object data = find_keyword_in_vector (instantiator, Q_data);

  if (!DEVICE_MSWINDOWS_P (XDEVICE (device)))
    signal_simple_error ("Not an mswindows device", device);

  assert (!NILP (data));

  GET_STRING_BINARY_DATA_ALLOCA (data, bytes, len);

  /* this is a hack but MaskBlt and TransparentBlt are not supported
     on most windows variants */
  bkcolor = COLOR_INSTANCE_MSWINDOWS_COLOR 
    (XCOLOR_INSTANCE (FACE_BACKGROUND (Vdefault_face, domain)));

  /* convert to an eimage to make processing easier */
  if (!xpm_to_eimage (image_instance, bytes, &eimage, &width, &height,
		      &x_hot, &y_hot, bkcolor))
    {
      signal_simple_error ("XPM to EImage conversion failed", 
			   image_instance);
    }
  
  /* build a bitmap from the eimage */
  if (!(bmp_info=EImage2DIBitmap (device, width, height, eimage,
				 &bmp_bits, &bmp_data)))
    {
      signal_simple_error ("XPM to EImage conversion failed",
			   image_instance);
    }
  xfree (eimage);

  /* Now create the pixmap and set up the image instance */
  init_image_instance_from_dibitmap (ii, bmp_info, dest_mask,
				     bmp_data, bmp_bits, instantiator);

  XSETINT (IMAGE_INSTANCE_PIXMAP_HOTSPOT_X (ii), x_hot);
  XSETINT (IMAGE_INSTANCE_PIXMAP_HOTSPOT_Y (ii), y_hot);
  
  xfree (bmp_info);
  xfree (bmp_data);
}
#endif /* HAVE_XPM */

/**********************************************************************
 *                               BMP                                  *
 **********************************************************************/

static void
bmp_validate (Lisp_Object instantiator)
{
  file_or_data_must_be_present (instantiator);
}

static Lisp_Object
bmp_normalize (Lisp_Object inst, Lisp_Object console_type)
{
  return simple_image_type_normalize (inst, console_type, Qbmp);
}

static int
bmp_possible_dest_types (void)
{
  return IMAGE_COLOR_PIXMAP_MASK;
}

static void
bmp_instantiate (Lisp_Object image_instance, Lisp_Object instantiator,
		 Lisp_Object pointer_fg, Lisp_Object pointer_bg,
		 int dest_mask, Lisp_Object domain)
{
  struct Lisp_Image_Instance *ii = XIMAGE_INSTANCE (image_instance);
  Lisp_Object device = IMAGE_INSTANCE_DEVICE (ii);
  CONST Extbyte		*bytes;
  Extcount 		len;
  BITMAPFILEHEADER*	bmp_file_header;
  BITMAPINFO*		bmp_info;
  void*			bmp_data;
  int			bmp_bits;
  Lisp_Object data = find_keyword_in_vector (instantiator, Q_data);

  if (!DEVICE_MSWINDOWS_P (XDEVICE (device)))
    signal_simple_error ("Not an mswindows device", device);

  assert (!NILP (data));

  GET_STRING_BINARY_DATA_ALLOCA (data, bytes, len);
  
  /* Then slurp the image into memory, decoding along the way.
     The result is the image in a simple one-byte-per-pixel
     format. */
  
  bmp_file_header=(BITMAPFILEHEADER*)bytes;
  bmp_info = (BITMAPINFO*)(bytes + sizeof(BITMAPFILEHEADER));
  bmp_data = (Extbyte*)bytes + bmp_file_header->bfOffBits;
  bmp_bits = bmp_file_header->bfSize - bmp_file_header->bfOffBits;

  /* Now create the pixmap and set up the image instance */
  init_image_instance_from_dibitmap (ii, bmp_info, dest_mask,
				     bmp_data, bmp_bits, instantiator);
}


/************************************************************************/
/*                      image instance methods                          */
/************************************************************************/

static void
mswindows_print_image_instance (struct Lisp_Image_Instance *p,
				Lisp_Object printcharfun,
				int escapeflag)
{
  char buf[100];

  switch (IMAGE_INSTANCE_TYPE (p))
    {
    case IMAGE_MONO_PIXMAP:
    case IMAGE_COLOR_PIXMAP:
    case IMAGE_POINTER:
      sprintf (buf, " (0x%lx", 
	       (unsigned long) IMAGE_INSTANCE_MSWINDOWS_BITMAP (p));
      write_c_string (buf, printcharfun);
      if (IMAGE_INSTANCE_MSWINDOWS_MASK (p))
	{
	  sprintf (buf, "/0x%lx", 
		   (unsigned long) IMAGE_INSTANCE_MSWINDOWS_MASK (p));
	  write_c_string (buf, printcharfun);
	}
      write_c_string (")", printcharfun);
      break;
    default:
      break;
    }
}

static void
mswindows_finalize_image_instance (struct Lisp_Image_Instance *p)
{
  if (!p->data)
    return;

  if (DEVICE_LIVE_P (XDEVICE (p->device)))
    {
      if (IMAGE_INSTANCE_MSWINDOWS_BITMAP (p))
	DeleteObject (IMAGE_INSTANCE_MSWINDOWS_BITMAP (p));
      IMAGE_INSTANCE_MSWINDOWS_BITMAP (p) = 0;
      if (IMAGE_INSTANCE_MSWINDOWS_MASK (p))
	DeleteObject (IMAGE_INSTANCE_MSWINDOWS_MASK (p));
      IMAGE_INSTANCE_MSWINDOWS_MASK (p) = 0;
      if (IMAGE_INSTANCE_MSWINDOWS_ICON (p))
	DestroyIcon (IMAGE_INSTANCE_MSWINDOWS_ICON (p));
      IMAGE_INSTANCE_MSWINDOWS_ICON (p) = 0;
    }

  xfree (p->data);
  p->data = 0;
}

static int
mswindows_image_instance_equal (struct Lisp_Image_Instance *p1,
				struct Lisp_Image_Instance *p2, int depth)
{
  switch (IMAGE_INSTANCE_TYPE (p1))
    {
    case IMAGE_MONO_PIXMAP:
    case IMAGE_COLOR_PIXMAP:
    case IMAGE_POINTER:
      if (IMAGE_INSTANCE_MSWINDOWS_BITMAP (p1) 
	  != IMAGE_INSTANCE_MSWINDOWS_BITMAP (p2))
	return 0;
      break;
    default:
      break;
    }

  return 1;
}

static unsigned long
mswindows_image_instance_hash (struct Lisp_Image_Instance *p, int depth)
{
  switch (IMAGE_INSTANCE_TYPE (p))
    {
    case IMAGE_MONO_PIXMAP:
    case IMAGE_COLOR_PIXMAP:
    case IMAGE_POINTER:
      return (unsigned long) IMAGE_INSTANCE_MSWINDOWS_BITMAP (p);
    default:
      return 0;
    }
}

/* Set all the slots in an image instance structure to reasonable
   default values.  This is used somewhere within an instantiate
   method.  It is assumed that the device slot within the image
   instance is already set -- this is the case when instantiate
   methods are called. */

static void
mswindows_initialize_dibitmap_image_instance (struct Lisp_Image_Instance *ii,
					    enum image_instance_type type)
{
  ii->data = xnew_and_zero (struct mswindows_image_instance_data);
  IMAGE_INSTANCE_TYPE (ii) = type;
  IMAGE_INSTANCE_PIXMAP_FILENAME (ii) = Qnil;
  IMAGE_INSTANCE_PIXMAP_MASK_FILENAME (ii) = Qnil;
  IMAGE_INSTANCE_PIXMAP_HOTSPOT_X (ii) = Qnil;
  IMAGE_INSTANCE_PIXMAP_HOTSPOT_Y (ii) = Qnil;
  IMAGE_INSTANCE_PIXMAP_FG (ii) = Qnil;
  IMAGE_INSTANCE_PIXMAP_BG (ii) = Qnil;
}


/************************************************************************/
/*                            initialization                            */
/************************************************************************/

void
syms_of_glyphs_mswindows (void)
{
}

void
console_type_create_glyphs_mswindows (void)
{
  /* image methods */

  CONSOLE_HAS_METHOD (mswindows, print_image_instance);
  CONSOLE_HAS_METHOD (mswindows, finalize_image_instance);
  CONSOLE_HAS_METHOD (mswindows, image_instance_equal);
  CONSOLE_HAS_METHOD (mswindows, image_instance_hash);
}

void
image_instantiator_format_create_glyphs_mswindows (void)
{
  /* image-instantiator types */

  INITIALIZE_IMAGE_INSTANTIATOR_FORMAT (bmp, "bmp");

  IIFORMAT_HAS_METHOD (bmp, validate);
  IIFORMAT_HAS_METHOD (bmp, normalize);
  IIFORMAT_HAS_METHOD (bmp, possible_dest_types);
  IIFORMAT_HAS_METHOD (bmp, instantiate);

  IIFORMAT_VALID_KEYWORD (bmp, Q_data, check_valid_string);
  IIFORMAT_VALID_KEYWORD (bmp, Q_file, check_valid_string);
}

void
vars_of_glyphs_mswindows (void)
{
  Fprovide (Qbmp);
  DEFVAR_LISP ("mswindows-bitmap-file-path", &Vmswindows_bitmap_file_path /*
A list of the directories in which mswindows bitmap files may be found.
This is used by the `make-image-instance' function.
*/ );
  Vmswindows_bitmap_file_path = Qnil;
}

void
complex_vars_of_glyphs_mswindows (void)
{
}
