/* Sunpro-specific routines.

   Copyright (C) 1994 Sun Microsystems, Inc.

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

#include <config.h>
#include "lisp.h"

/* ####

  The following junk used to be in lisp/prim/files.el.  It obviously
  doesn't belong there, but should go somewhere.

  (if (fboundp 'ut-log-text)	;; #### Sun stuff; what is this?
      (ut-log-text "Reading a file."))
*/

/* Whether usage tracking is turned on (Sun only) */
Lisp_Object Vusage_tracking;
#ifdef USAGE_TRACKING
#include <ut.h>
#endif

DEFUN  ("ut-log-text", 
	Fut_log_text, 
	Sut_log_text, 
	1, MANY, 0 /*
Log a usage-tracking message if `usage-tracking' is non-nil.
Args are the same as to `format'.  Returns whether the message was
actually logged.  If usage-tracking support was not compiled in, this
function has no effect and always returns `nil'.  See function
`has-usage-tracking-p'.
*/ )
     (nargs, args)
     int nargs;
     Lisp_Object *args;
{
#ifdef USAGE_TRACKING
  Lisp_Object xs;
  unsigned char *s; /* #### Does not support I18N4. */

  if (!NILP (Vusage_tracking))
    {
      xs = Fformat (nargs, args);
      CHECK_STRING (xs);
      s = string_data (XSTRING (xs));
      ut_log_text ((char *) s);
    }
  return Vusage_tracking;
#else
  return Qnil;
#endif
}


/************************************************************************/
/*                            initialization                            */
/************************************************************************/

void
syms_of_sunpro (void)
{
  defsubr (&Sut_log_text);
}

void
vars_of_sunpro (void)
{
  DEFVAR_LISP ("usage-tracking", &Vusage_tracking /*
Whether usage tracking is turned on (Sun internal use only).
Has no effect if usage tracking support has not been compiled in.
*/ );
  Vusage_tracking = Qnil;

  Fprovide (intern ("sparcworks"));
#ifdef USAGE_TRACKING
  Fprovide (intern ("usage-tracking"));
#endif
}

void
init_sunpro (void)
{
#ifdef USAGE_TRACKING
  if (!purify_flag)
    {	       /* Enabled only when not dumping an executable */
      Vusage_tracking = Qt;
      ut_initialize ("xemacs", NULL, NULL);
    }
  else
    {
      Vusage_tracking = Qnil;
    }
#else
  Vusage_tracking = Qnil;
#endif
}
