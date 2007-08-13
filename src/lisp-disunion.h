/* Fundamental definitions for XEmacs Lisp interpreter -- non-union objects.
   Copyright (C) 1985, 1986, 1987, 1992, 1993 Free Software Foundation, Inc.

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

/* Synched up with: FSF 19.30.  Split out from lisp.h. */

/* If union type is not wanted, define Lisp_Object as just a number
   and define the macros below to extract fields by shifting */

#define Qzero 0

typedef EMACS_INT Lisp_Object;

#define VALMASK ((1L << (VALBITS)) - 1L)
#define GCTYPEMASK ((1L << (GCTYPEBITS)) - 1L)

/* comment from FSFmacs (perhaps not accurate here):

   This is set in the car of a cons and in the plist slot of a symbol
   to indicate it is marked.  Likewise in the plist slot of an interval,
   the chain slot of a marker, the type slot of a float, and the name
   slot of a buffer.

   In strings, this bit in the size field indicates that the string
   is a "large" one, one which was separately malloc'd
   rather than being part of a string block.  */

#define MARKBIT (1UL << (VALBITS))


/* These macros extract various sorts of values from a Lisp_Object.
   For example, if tem is a Lisp_Object whose type is Lisp_Type_Cons,
   XCONS (tem) is the struct Lisp_Cons * pointing to the memory for that cons. */

/* One needs to override this if there must be high bits set in data space
   (doing the result of the below & ((1 << (GCTYPE + 1)) - 1) would work
   on all machines, but would penalize machines which don't need it) */
#define XTYPE(a) ((enum Lisp_Type) (((EMACS_UINT)(a)) >> ((VALBITS) + 1)))

#define EQ(x,y) ((x) == (y))
#define GC_EQ(x,y) (XGCTYPE (x) == XGCTYPE (y) && XPNTR (x) == XPNTR (y))

/* Extract the value of a Lisp_Object as a signed integer.  */

#ifndef XREALINT   /* Some machines need to do this differently.  */
# define XREALINT(a) (((a) << (LONGBITS-VALBITS)) >> (LONGBITS-VALBITS))
#endif

/* Extract the value as an unsigned integer.  This is a basis
   for extracting it as a pointer to a structure in storage.  */

#define XUINT(a) ((a) & VALMASK)

#ifdef HAVE_SHM
/* In this representation, data is found in two widely separated segments.  */
extern int pure_size;
#  define XPNTR(a) \
  (XUINT (a) | (XUINT (a) > pure_size ? DATA_SEG_BITS : PURE_SEG_BITS))
# else /* not HAVE_SHM */
#  ifdef DATA_SEG_BITS
/* This case is used for the rt-pc.
   In the diffs I was given, it checked for ptr = 0
   and did not adjust it in that case.
   But I don't think that zero should ever be found
   in a Lisp object whose data type says it points to something. */
#   define XPNTR(a) (XUINT (a) | DATA_SEG_BITS)
#  else
#   define XPNTR(a) XUINT (a)
#  endif
#endif /* not HAVE_SHM */

#define XSETINT(a, b) XSETOBJ (a, Lisp_Type_Int, b)

#define XSETCHAR(var, value) XSETOBJ (var, Lisp_Type_Char, value)

/* XSETOBJ was formerly named XSET.  The name change was made to catch
   C code that attempts to use this macro.  You should always use the
   individual settor macros (XSETCONS, XSETBUFFER, etc.) instead. */

#define XSETOBJ(var, type_tag, value)			\
 ((void) ((var) = (((EMACS_INT) (type_tag) << ((VALBITS) + 1))	\
                 + ((EMACS_INT) (value) & VALMASK))))

/* During garbage collection, XGCTYPE must be used for extracting types
   so that the mark bit is ignored.  XMARKBIT accesses the markbit.
   Markbits are used only in particular slots of particular structure types.
   Other markbits are always zero.
   Outside of garbage collection, all mark bits are always zero.  */

#ifndef XGCTYPE
# define XGCTYPE(a) XTYPE(a)
#endif

# define XMARKBIT(a) ((a) & (MARKBIT))

# define XMARK(a) ((void) ((a) |= (MARKBIT)))
# define XUNMARK(a) ((void) ((a) &= (~(MARKBIT))))

/* Use this for turning a (void *) into a Lisp_Object, as when the
  Lisp_Object is passed into a toolkit callback function */
#define VOID_TO_LISP(larg,varg) ((void) ((larg) = ((Lisp_Object) (varg))))
#define CVOID_TO_LISP VOID_TO_LISP

/* Use this for turning a Lisp_Object into a (void *), as when the
   Lisp_Object is passed into a toolkit callback function */
#define LISP_TO_VOID(larg) ((void *) (larg))
#define LISP_TO_CVOID(varg) ((CONST void *) (larg))

/* Convert a Lisp_Object into something that can't be used as an
   lvalue.  Useful for type-checking. */
#define NON_LVALUE(larg) ((larg) + 0)
