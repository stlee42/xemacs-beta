/* system description file for both 32-bit and 64-bit cygwin.
   Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.
   Copyright (C) 2001 Ben Wing.

This file is part of XEmacs.

XEmacs is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

XEmacs is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with XEmacs.  If not, see <http://www.gnu.org/licenses/>. */

/* Building under cygwin
 *
 * The approach I have taken with this port is to use primarily the
 * UNIX code base adding stuff that is MS-Windows specific. This works
 * quite well, and is in keeping with my perception of the cygwin
 * philosophy.  Note that if you make changes to this file you do NOT
 * want to define WIN32_NATIVE (formerly "WINDOWSNT"), I repeat - do
 * not define this, it will break everything horribly. What does get
 * defined is HAVE_MS_WINDOWS, but this is done by configure and only
 * applies to the window system.
 *
 * When building make sure your HOME path is unix style - i.e. without
 * a drive letter.
 *
 * once you have done this, configure and make.
 *
 * Andy Piper <andy@xemacs.org> 8/1/98 
 * http://www.xemacs.freeserve.co.uk/ */

#include "win32-common.h"

/* Identify ourselves */
#define CYGWIN

/* We are using Cygwin-style headers in /usr/include, also used by MinGW */
#define CYGWIN_HEADERS

/* cheesy way to determine cygwin version */
#ifndef NOT_C_CODE
# include <signal.h>
# include <cygwin/version.h>

/* Still left out of 1.1! */
double logb (double);
int killpg (int pgrp, int sig);

#endif

#ifndef ORDINARY_LINK
#define ORDINARY_LINK
#endif

#if __GNUC__ >= 3
#define C_SWITCH_SYSTEM -fno-caller-saves
#else
#define C_SWITCH_SYSTEM -fno-caller-saves -fvtable-thunks
#endif

#define LIBS_SYSTEM -lwinmm
#define WIN32_LEAN_AND_MEAN

#define TEXT_START -1
#define HEAP_IN_DATA
#define NO_LIM_DATA

#define BROKEN_SIGIO

#define CYGWIN_BROKEN_SIGNALS

#define strnicmp strncasecmp

#undef MAIL_USE_SYSTEM_LOCK

/* Cygwin bogusly forgets to copy mmap()ed regions into the child when
   a fork is done; thus, any reference to anything in mmap()ed space
   (under PDUMP, in particular, this bites, since all data loaded from
   PDUMP is normally done using mmap()) will cause an immediate segfault. */
#undef HAVE_MMAP
