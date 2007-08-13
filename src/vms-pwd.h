/* XEmacs password definition file.
   Copyright (C) 1986 Free Software Foundation.

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

#ifdef VMS
/* On VMS, we read the UAF file and emulate some of the necessary
   fields for Emacs. */
#include "uaf.h"

struct passwd {
  char pw_name[UAF$S_USERNAME+1];
  char pw_passwd[UAF$S_PWD];
  short pw_uid;
  short pw_gid;
  char pw_gecos[UAF$S_OWNER+1];
  char pw_dir[UAF$S_DEFDEV+UAF$S_DEFDIR+1];
  char pw_shell[UAF$S_DEFCLI+1];
};
#endif /* VMS */
