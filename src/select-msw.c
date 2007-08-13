/* mswindows selection processing for XEmacs
   Copyright (C) 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.

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

/* Synched up with: Not synched with FSF. */

/* Authorship:

   Written by Kevin Gallo for FSF Emacs.
   Rewritten for mswindows by Jonathan Harris, December 1997 for 20.4.
 */


#include <config.h>
#include "lisp.h"

#include "console-msw.h"

DEFUN ("mswindows-set-clipboard", Fmswindows_set_clipboard, 1, 1, 0, /*
Copy STRING to the mswindows clipboard.
*/
       (string))
{
  int rawsize, size, i;
  unsigned char *src, *dst, *next;
  HGLOBAL h = NULL;

  CHECK_STRING (string);

  /* Calculate size with LFs converted to CRLFs because
   * CF_TEXT format uses CRLF delimited ASCIIZ */
  src = XSTRING_DATA (string);
  size = rawsize = XSTRING_LENGTH (string) + 1;
  for (i=0; i<rawsize; i++)
    if (src[i] == '\n')
      size++;

  if (!OpenClipboard (NULL))
    return Qnil;

  if (!EmptyClipboard () ||
      (h = GlobalAlloc (GMEM_MOVEABLE | GMEM_DDESHARE, size)) == NULL ||
      (dst = (unsigned char *) GlobalLock (h)) == NULL)
    {
      if (h != NULL) GlobalFree (h);
      CloseClipboard ();
      return Qnil;
    }
    
  /* Convert LFs to CRLFs */
  do
    {
      /* copy next line or remaining bytes including '\0' */
      next = memccpy (dst, src, '\n', rawsize);
      if (next)
	{
	  /* copied one line ending with '\n' */
	  int copied = next - dst;
	  rawsize -= copied;
	  src += copied;
	  /* insert '\r' before '\n' */
	  next[-1] = '\r';
	  next[0] = '\n';
	  dst = next+1;
	}	    
    }
  while (next);
    
  GlobalUnlock (h);
  
  i = (SetClipboardData (CF_TEXT, h) != NULL);
  
  CloseClipboard ();
  GlobalFree (h);
  
  return i ? Qt : Qnil;
}

DEFUN ("mswindows-get-clipboard", Fmswindows_get_clipboard, 0, 0, 0, /*
Return the contents of the mswindows clipboard.
*/
       ())
{
  HANDLE h;
  unsigned char *src, *dst, *next;
  Lisp_Object ret = Qnil;

  if (!OpenClipboard (NULL))
    return Qnil;

  if ((h = GetClipboardData (CF_TEXT)) != NULL &&
      (src = (unsigned char *) GlobalLock (h)) != NULL)
    {
      int i;
      int size, rawsize;
      size = rawsize = strlen (src);

      for (i=0; i<rawsize; i++)
	if (src[i] == '\r' && src[i+1] == '\n')
	  size--;

      /* Convert CRLFs to LFs */
      ret = make_uninit_string (size);
      dst = XSTRING_DATA (ret);
      do
	{
	  /* copy next line or remaining bytes excluding '\0' */
	  next = _memccpy (dst, src, '\r', rawsize);
	  if (next)
	    {
	      /* copied one line ending with '\r' */
	      int copied = next - dst;
	      rawsize -= copied;
	      src += copied;
	      if (*src == '\n')
		dst += copied - 1;		/* overwrite '\r' */
	      else
		dst += copied;
	    }	    
	}
      while (next);

      GlobalUnlock (h);
    }

  CloseClipboard ();

  return ret;
}


/************************************************************************/
/*                            initialization                            */
/************************************************************************/

void
syms_of_select_mswindows (void)
{
  DEFSUBR (Fmswindows_set_clipboard);
  DEFSUBR (Fmswindows_get_clipboard);
}

void
vars_of_select_mswindows (void)
{
}