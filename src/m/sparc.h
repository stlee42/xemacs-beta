/* machine description file for Sun 4 SPARC.
   Copyright (C) 1987, 1994 Free Software Foundation, Inc.

This file is part of GNU Emacs.

GNU Emacs is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XEmacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Synched up with: FSF 19.31. */

/* The following line tells the configuration script what sort of 
   operating system this machine is likely to run.
   USUAL-OPSYS="note"

NOTE-START
Use -opsystem=sunos4 for operating system version 4, and
-opsystem=bsd4-2 for earlier versions.
NOTE-END  */

/* XEmacs change */
/* Say this machine is a sparc if we are not generating the Makefiles.
   In that case say we are a SPARC.  Otherwise people who have sparc
   in a path will not be happy. */

#ifdef NOT_C_CODE
# define SPARC
#else
# ifndef sparc
#  define sparc
# endif
#endif

#ifdef __GNUC__
# define C_OPTIMIZE_SWITCH -O
#else
/* XEmacs change */
# ifdef USE_LCC
#  define C_OPTIMIZE_SWITCH -O4 -Oi
# else
     /* This level of optimization is reported to work.  */
#  define C_OPTIMIZE_SWITCH -O2
# endif
#endif

/* XINT must explicitly sign-extend */

#define EXPLICIT_SIGN_EXTEND

/* Data type of load average, as read out of kmem.  */

#define LOAD_AVE_TYPE long

/* Convert that into an integer that is 100 for a load average of 1.0  */

#define LOAD_AVE_CVT(x) (int) (((double) (x)) * 100.0 / FSCALE)

/* Define C_ALLOCA if this machine does not support a true alloca
   and the one written in C should be used instead.
   Define HAVE_ALLOCA to say that the system provides a properly
   working alloca function and it should be used.
   Define neither one if an assembler-language alloca
   in the file alloca.s should be used.  */

#define HAVE_ALLOCA
#ifndef NOT_C_CODE
#if __GNUC__ < 2 /* Modern versions of GCC handle alloca directly.  */
#include <alloca.h>
#endif
#endif

/* Solaris defines alloca to __builtin_alloca & does not provide a prototype */
#ifdef __SUNPRO_C
#  ifndef NOT_C_CODE
#    include <alloca.h>
     void *__builtin_alloca (unsigned int);
#  endif
#endif

/* Must use the system's termcap, if we use any termcap.
   It does special things.  */

#ifndef TERMINFO
#define LIBS_TERMCAP -ltermcap
#endif

/* Mask for address bits within a memory segment */

#define SEGMENT_MASK (SEGSIZ - 1)

/* Arrange to link with sun windows, if requested.  */
/* For details on emacstool and sunfns, see etc/SUN-SUPPORT */
/* These programs require Sun UNIX 4.2 Release 3.2 or greater */

#ifdef HAVE_SUN_WINDOWS
#define OTHER_FILES  ${etcdir}emacstool
#define LIBS_MACHINE -lsuntool -lsunwindow -lpixrect
#define OBJECTS_MACHINE sunfns.o
#define SYMS_MACHINE syms_of_sunfns ()
#define SYSTEM_PURESIZE_EXTRA 10000
#endif

#ifndef __NetBSD__
#ifndef __linux__
/* This really belongs in s/sun.h.  */

/* Say that the text segment of a.out includes the header;
   the header actually occupies the first few bytes of the text segment
   and is counted in hdr.a_text.  */

#define A_TEXT_OFFSET(HDR) sizeof (HDR)

/* This is the offset of the executable's text, from the start of the file.  */

#define A_TEXT_SEEK(HDR) (N_TXTOFF (hdr) + sizeof (hdr))

#endif /* __linux__ */
#endif /* __NetBSD__ */
