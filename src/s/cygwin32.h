/* system description file for cygwin32.
   Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.
   Copyright (C) 2001 Ben Wing.

This file is part of XEmacs.

XEmacs is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

XEmacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XEmacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

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
 * windows '95 - I haven't tested this under '95, it will probably
 * build but I konw there are some limitations with cygwin under 95 so
 * YMMV. I build with NT4 SP3.
 *
 * Andy Piper <andy@xemacs.org> 8/1/98 
 * http://www.xemacs.freeserve.co.uk/ */

/* Identify ourselves */
#define CYGWIN

/* cheesy way to determine cygwin version */
#ifndef NOT_C_CODE
# include <signal.h>
# include <cygwin/version.h>

void cygwin_win32_to_posix_path_list (const char *, char *);
int cygwin_win32_to_posix_path_list_buf_size (const char *);
void cygwin_posix_to_win32_path_list (const char *, char *);
int cygwin_posix_to_win32_path_list_buf_size (const char *);

/* Still left out of 1.1! */
double logb (double);
int killpg (int pgrp, int sig);

#endif

#ifndef ORDINARY_LINK
#define ORDINARY_LINK
#endif

#define C_SWITCH_SYSTEM -fno-caller-saves
#define LIBS_SYSTEM -lwinmm
#define WIN32_LEAN_AND_MEAN

#define TEXT_START -1
#define TEXT_END -1
#define DATA_END -1
#define HEAP_IN_DATA
#define NO_LIM_DATA
#define UNEXEC "unexcw.o"

#define BROKEN_SIGIO

#define CYGWIN_BROKEN_SIGNALS

#define strnicmp strncasecmp
#ifndef HAVE_SOCKETS
#define HAVE_SOCKETS
#endif

#undef MAIL_USE_SYSTEM_LOCK

/* Do not define LOAD_AVE_TYPE or LOAD_AVE_CVT
   since there is no load average available. */

/* Define VIRT_ADDR_VARIES if the virtual addresses of
   pure and impure space as loaded can vary, and even their
   relative order cannot be relied on.

   Otherwise Emacs assumes that text space precedes data space,
   numerically.  */

/* Text does precede data space, but this is never a safe assumption.  */
#define VIRT_ADDR_VARIES

/* If you are compiling with a non-C calling convention but need to
   declare vararg routines differently, put it here */
#define _VARARGS_ __cdecl

/* If you are providing a function to something that will call the
   function back (like a signal handler and signal, or main) its calling
   convention must be whatever standard the libraries expect */
#define _CALLBACK_ __cdecl

/* SYSTEM_TYPE should indicate the kind of system you are using.
 It sets the Lisp variable system-type.  */

#define SYSTEM_TYPE "cygwin32"

#define NO_MATHERR

/* define MAIL_USE_FLOCK if the mailer uses flock
   to interlock access to /usr/spool/mail/$USER.
   The alternative is that a lock file named
   /usr/spool/mail/$USER.lock.  */

/* If the character used to separate elements of the executable path
   is not ':', #define this to be the appropriate character constant.  */
#define SEPCHAR ':'

/* ============================================================ */

/* Here, add any special hacks needed
   to make Emacs work on this system.  For example,
   you might define certain system call names that don't
   exist on your system, or that do different things on
   your system and must be used only through an encapsulation
   (Which you should place, by convention, in sysdep.c).  */

/* Define this to be the separator between path elements */
/* #define DIRECTORY_SEP XINT (Vdirectory_sep_char) */

/* Define this to be the separator between devices and paths */
#define DEVICE_SEP ':'

/* We'll support either convention on NT.  */
#define IS_DIRECTORY_SEP(_c_) ((_c_) == '/' || (_c_) == '\\')
#define IS_ANY_SEP(_c_) (IS_DIRECTORY_SEP (_c_) || IS_DEVICE_SEP (_c_))
#define EXEC_SUFFIXES   ".exe:.com:.bat:.cmd:"

/* We need a little extra space, see ../../lisp/loadup.el */
#define SYSTEM_PURESIZE_EXTRA 15000

#define CYGWIN_CONV_PATH(src, dst) \
dst = alloca (cygwin_win32_to_posix_path_list_buf_size(src)); \
cygwin_win32_to_posix_path_list(src, dst)
     /* !!#### more mule bogosity */
#define CYGWIN_WIN32_PATH(src, dst) \
dst = (Extbyte *) alloca (cygwin_posix_to_win32_path_list_buf_size(src)); \
cygwin_posix_to_win32_path_list(src, dst)
