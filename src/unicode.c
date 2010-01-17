/* Code to handle Unicode conversion.
   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2009, 2010 Ben Wing.

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

/* Synched up with: FSF 20.3.  Not in FSF. */

/* Authorship:

   Current primary author: Ben Wing <ben@xemacs.org>

   Written by Ben Wing <ben@xemacs.org>, June, 2001.
   Separated out into this file, August, 2001.
   Includes Unicode coding systems, some parts of which have been written
   by someone else.  #### Morioka and Hayashi, I think.

   As of September 2001, the detection code is here and abstraction of the
   detection system is finished.  The unicode detectors have been rewritten
   to include multiple levels of likelihood.
   */

#include <config.h>
#include "lisp.h"

#include "charset.h"
#include "elhash.h"
#include "file-coding.h"
#include "opaque.h"

#include "buffer.h"
#include "rangetab.h"
#include "extents.h"

#include "sysfile.h"

/* For more info about how Unicode works under Windows, see intl-win32.c. */

/* Info about Unicode translation tables [ben]:

   FORMAT:
   -------

   We currently use the following format for tables:

   If dimension == 1, to_unicode_table is a CHARSET_MAX_SIZE-element array
   of ints (Unicode code points); else, it's a CHARSET_MAX_SIZE-element
   array of int * pointers, each of which points to a
   CHARSET_MAX_SIZE-element array of ints.  If no elements in a row have
   been filled in, the pointer will point to a default empty table; that
   way, memory usage is more reasonable but lookup still fast.

   -- If from_unicode_levels == 1, from_unicode_table is a 256-element
   array of UINT_16_BITs (octet 1 in high byte, octet 2 in low byte; we don't
   store Ichars directly to save space).

   -- If from_unicode_levels == 2, from_unicode_table is a 256-element
   array of UINT_16_BIT * pointers, each of which points to a 256-element array
   of UINT_16_BITs.

   -- If from_unicode_levels == 3, from_unicode_table is a 256-element
   array of UINT_16_BIT ** pointers, each of which points to a 256-element
   array of UINT_16_BIT * pointers, each of which points to a 256-element
   array of UINT_16_BITs.

   -- If from_unicode_levels == 4, same thing but one level deeper.

   Just as for to_unicode_table, we use default tables to fill in all
   entries with no values in them.

   #### An obvious space-saving optimization is to use variable-sized
   tables, where each table instead of just being a 256-element array, is a
   structure with a start value, an end value, and a variable number of
   entries (END - START + 1).  Only 8 bits are needed for END and START,
   and could be stored at the end to avoid alignment problems.  However,
   before charging off and implementing this, we need to consider whether
   it's worth it:

   (1) Most tables will be highly localized in which code points are
   defined, heavily reducing the possible memory waste.  Before doing any
   rewriting, write some code to see how much memory is actually being
   wasted (i.e. ratio of empty entries to total # of entries) and only
   start rewriting if it's unacceptably high.  You have to check over all
   charsets.

   (2) Since entries are usually added one at a time, you have to be very
   careful when creating the tables to avoid realloc()/free() thrashing in
   the common case when you are in an area of high localization and are
   going to end up using most entries in the table.  You'd certainly want
   to allow only certain sizes, not arbitrary ones (probably powers of 2,
   where you want the entire block including the START/END values to fit
   into a power of 2, minus any malloc overhead if there is any -- there's
   none under gmalloc.c, and probably most system malloc() functions are
   quite smart nowadays and also have no overhead).  You could optimize
   somewhat during the in-C initializations, because you can compute the
   actual usage of various tables by scanning the entries you're going to
   add in a separate pass before adding them. (You could actually do the
   same thing when entries are added on the Lisp level by making the
   assumption that all the entries will come in one after another before
   any use is made of the data.  So as they're coming in, you just store
   them in a big long list, and the first time you need to retrieve an
   entry, you compute the whole table at once.) You'd still have to deal
   with the possibility of later entries coming in, though.

   (3) You do lose some speed using START/END values, since you need a
   couple of comparisons at each level.  This could easily make each single
   lookup become 3-4 times slower.  The Unicode book considers this a big
   issue, and recommends against variable-sized tables for this reason;
   however, they almost certainly have in mind applications that primarily
   involve conversion of large amounts of data.  Most Unicode strings that
   are translated in XEmacs are fairly small.  The only place where this
   might matter is in loading large files -- e.g. a 3-megabyte
   Unicode-encoded file.  So think about this, and maybe do a trial
   implementation where you don't worry too much about the intricacies of
   (2) and just implement some basic "multiply by 1.5" trick or something
   to do the resizing.  There is a very good FAQ on Unicode called
   something like the Linux-Unicode How-To (it should be part of the Linux
   How-To's, I think), that lists the url of a guy with a whole bunch of
   unicode files you can use to stress-test your implementations, and he's
   highly likely to have a good multi-megabyte Unicode-encoded file (with
   normal text in it -- if you created your own just by creating repeated
   strings of letters and numbers, you probably wouldn't get accurate
   results).

   INITIALIZATION:
   ---------------

   There are advantages and disadvantages to loading the tables at
   run-time.

   Advantages:

   They're big, and it's very fast to recreate them (a fraction of a second
   on modern processors).

   Disadvantages:

   (1) User-defined charsets: It would be inconvenient to require all
   dumped user-defined charsets to be reloaded at init time.

   NB With run-time loading, we load in init-mule-at-startup, in
   mule-cmds.el.  This is called from startup.el, which is quite late in
   the initialization process -- but data-directory isn't set until then.
   With dump-time loading, you still can't dump in a Japanese directory
   (again, until we move to Unicode internally), but this is not such an
   imposition.

   
*/

/* #### WARNING!  The current sledgehammer routines have a fundamental
   problem in that they can't handle two characters mapping to a
   single Unicode codepoint or vice-versa in a single charset table.
   It's not clear there is any way to handle this and still make the
   sledgehammer routines useful.

   Inquiring Minds Want To Know Dept: does the above WARNING mean that
   _if_ it happens, then it will signal error, or then it will do
   something evil and unpredictable?  Signaling an error is OK: for
   all national standards, the national to Unicode map is an inclusion
   (1-to-1).  Any character set that does not behave that way is
   broken according to the Unicode standard.

   Answer: You will get an ABORT(), since the purpose of the sledgehammer
   routines is self-checking.  The above problem with non-1-to-1 mapping
   occurs in the Big5 tables, as provided by the Unicode Consortium. */

/* #define SLEDGEHAMMER_CHECK_UNICODE */

/* When MULE is not defined, we may still need some Unicode support --
   in particular, some Windows API's always want Unicode, and the way
   we've set up the Unicode encapsulation, we may as well go ahead and
   always use the Unicode versions of split API's. (It would be
   trickier to not use them, and pointless -- under NT, the ANSI API's
   call the Unicode ones anyway, so in the case of structures, we'd be
   converting from Unicode to ANSI structures, only to have the OS
   convert them back.) */

Lisp_Object Qunicode;
Lisp_Object Qutf_16, Qutf_8, Qucs_4, Qutf_7, Qutf_32;
Lisp_Object Qneed_bom;

Lisp_Object Qutf_16_little_endian, Qutf_16_bom;
Lisp_Object Qutf_16_little_endian_bom;

Lisp_Object Qutf_8_bom;

extern int firstbyte_mask[];

#ifdef MULE
/* These range tables are not directly accessible from Lisp: */
static Lisp_Object Vunicode_invalid_and_query_skip_chars;
static Lisp_Object Vutf_8_invalid_and_query_skip_chars;
static Lisp_Object Vunicode_query_skip_chars;

static Lisp_Object Vunicode_query_string, Vunicode_invalid_string,
  Vutf_8_invalid_string;
#endif /* MULE */

/* See the Unicode FAQ, http://www.unicode.org/faq/utf_bom.html#35 for this
   algorithm. 
 
   (They also give another, really verbose one, as part of their explanation
   of the various planes of the encoding, but we won't use that.) */
 
#define UTF_16_LEAD_OFFSET (0xD800 - (0x10000 >> 10))
#define UTF_16_SURROGATE_OFFSET (0x10000 - (0xD800 << 10) - 0xDC00)

#define utf_16_surrogates_to_code(lead, trail) \
  (((lead) << 10) + (trail) + UTF_16_SURROGATE_OFFSET)

#define CODE_TO_UTF_16_SURROGATES(codepoint, lead, trail) do {	\
    int __ctu16s_code = (codepoint);				\
    lead = UTF_16_LEAD_OFFSET + (__ctu16s_code >> 10);		\
    trail = 0xDC00 + (__ctu16s_code & 0x3FF);			\
} while (0)

#ifdef MULE 

/* There is no single badval that will work for all cases with the from tables,
   because we allow arbitrary 256x256 charsets. #### This is a real problem;
   need a better fix.  One possibility is to compute a bad value that is
   outside the range of a particular charset, and have separate blank tables
   for each charset.  This still chokes on 256x256, but not anywhere else.
   The value of 0x0001 will not be valid in any dimension-1 charset (they
   always are of the form 0xXX00), not valid in a ku-ten style charset, and
   not valid in any ISO-2022-like charset, or Shift-JIS, Big5, JOHAB, etc.;
   or any related charset, all of which try to avoid using the control
   character ranges.  Of course it *is* valid in Unicode, if someone tried
   to create a national unicode charset; but if we chose a value that is
   invalid in Unicode, it's likely to be valid for many other charsets; no
   win. */
#define BADVAL_FROM_TABLE ((UINT_16_BIT) 1)
/* For the to tables we are safe, because -1 is never a valid Unicode
   codepoint. */
#define BADVAL_TO_TABLE (-1)

/* We use int for to_unicode; Unicode codepoints always fit into a signed
   32-bit value.

   We use UINT_16_BIT to store a charset codepoint, up to 256x256,
   unsigned to avoid problems.
*/
static int *to_unicode_blank_1;
static int **to_unicode_blank_2;

/* The lowest table is always null.  We do it this way so that the table index
   corresponds to the number of levels of the table, i.e. how many indices
   before you get an actual value rather than a pointer. */
static void *from_unicode_blank[5];

static const struct memory_description to_unicode_level_0_desc_1[] = {
  { XD_END }
};

static const struct sized_memory_description to_unicode_level_0_desc = {
  sizeof (int), to_unicode_level_0_desc_1
};

static const struct memory_description to_unicode_level_1_desc_1[] = {
  { XD_BLOCK_PTR, 0, CHARSET_MAX_SIZE, { &to_unicode_level_0_desc } },
  { XD_END }
};

static const struct sized_memory_description to_unicode_level_1_desc = {
  sizeof (void *), to_unicode_level_1_desc_1
};

static const struct memory_description to_unicode_description_1[] = {
  { XD_BLOCK_PTR, 1, CHARSET_MAX_SIZE, { &to_unicode_level_0_desc } },
  { XD_BLOCK_PTR, 2, CHARSET_MAX_SIZE, { &to_unicode_level_1_desc } },
  { XD_END }
};

/* Not static because each charset has a set of to and from tables and
   needs to describe them to pdump. */
const struct sized_memory_description to_unicode_description = {
  sizeof (void *), to_unicode_description_1
};

/* Used only for to_unicode_blank_2 */
static const struct memory_description to_unicode_level_2_desc_1[] = {
  { XD_BLOCK_PTR, 0, CHARSET_MAX_SIZE, { &to_unicode_level_1_desc } },
  { XD_END }
};

static const struct memory_description from_unicode_level_0_desc_1[] = {
  { XD_END }
};

static const struct sized_memory_description from_unicode_level_0_desc = {
   sizeof (UINT_16_BIT), from_unicode_level_0_desc_1
};

static const struct memory_description from_unicode_level_1_desc_1[] = {
  { XD_BLOCK_PTR, 0, 256, { &from_unicode_level_0_desc } },
  { XD_END }
};

static const struct sized_memory_description from_unicode_level_1_desc = {
  sizeof (void *), from_unicode_level_1_desc_1
};

static const struct memory_description from_unicode_level_2_desc_1[] = {
  { XD_BLOCK_PTR, 0, 256, { &from_unicode_level_1_desc } },
  { XD_END }
};

static const struct sized_memory_description from_unicode_level_2_desc = {
  sizeof (void *), from_unicode_level_2_desc_1
};

static const struct memory_description from_unicode_level_3_desc_1[] = {
  { XD_BLOCK_PTR, 0, 256, { &from_unicode_level_2_desc } },
  { XD_END }
};

static const struct sized_memory_description from_unicode_level_3_desc = {
  sizeof (void *), from_unicode_level_3_desc_1
};

static const struct memory_description from_unicode_description_1[] = {
  { XD_BLOCK_PTR, 1, 256, { &from_unicode_level_0_desc } },
  { XD_BLOCK_PTR, 2, 256, { &from_unicode_level_1_desc } },
  { XD_BLOCK_PTR, 3, 256, { &from_unicode_level_2_desc } },
  { XD_BLOCK_PTR, 4, 256, { &from_unicode_level_3_desc } },
  { XD_END }
};

/* Not static because each charset has a set of to and from tables and
   needs to describe them to pdump. */
const struct sized_memory_description from_unicode_description = {
  sizeof (void *), from_unicode_description_1
};

/* Used only for from_unicode_blank[4] */
static const struct memory_description from_unicode_level_4_desc_1[] = {
  { XD_BLOCK_PTR, 0, 256, { &from_unicode_level_3_desc } },
  { XD_END }
};

static Lisp_Object_dynarr *global_unicode_precedence_dynarr;

Lisp_Object Vlanguage_unicode_precedence_list;
Lisp_Object Vdefault_unicode_precedence_list;
/* Used internally in the generation of precedence-list dynarrs, to keep
   track of charsets already seen */
Lisp_Object Vprecedence_list_charsets_seen_hash;

Lisp_Object Qignore_first_column;

#ifndef UNICODE_INTERNAL
Lisp_Object Vcurrent_jit_charset;
/* The following are stored as Lisp objects instead of just ints so they
   are preserved across dumping */
Lisp_Object Vlast_allocated_jit_c1, Vlast_allocated_jit_c2;
Lisp_Object Vnumber_of_jit_charsets;
Lisp_Object Vcharset_descr;
#endif

/* Break up a 32-bit character code into 8-bit parts. */

#ifdef MAXIMIZE_UNICODE_TABLE_DEPTH
#define TO_TABLE_SIZE_FROM_CHARSET(charset) 2
#define UNICODE_BREAKUP_CHAR_CODE(val, u1, u2, u3, u4, levels)	\
do {								\
  int buc_val = (val);						\
								\
  (u1) = buc_val >> 24;						\
  (u2) = (buc_val >> 16) & 255;					\
  (u3) = (buc_val >> 8) & 255;					\
  (u4) = buc_val & 255;						\
} while (0)
#else /* not MAXIMIZE_UNICODE_TABLE_DEPTH */
#define TO_TABLE_SIZE_FROM_CHARSET(charset) XCHARSET_DIMENSION (charset)
#define UNICODE_BREAKUP_CHAR_CODE(val, u1, u2, u3, u4, levels)	\
do {								\
  int buc_val = (val);						\
								\
  (u1) = buc_val >> 24;						\
  (u2) = (buc_val >> 16) & 255;					\
  (u3) = (buc_val >> 8) & 255;					\
  (u4) = buc_val & 255;						\
  (levels) = (buc_val <= 0xFF ? 1 :				\
	      buc_val <= 0xFFFF ? 2 :				\
	      buc_val <= 0xFFFFFF ? 3 :				\
	      4);						\
} while (0)
#endif /* (not) MAXIMIZE_UNICODE_TABLE_DEPTH */
#endif /* MULE */


/************************************************************************/
/*                        Unicode implementation                        */
/************************************************************************/

/* Given a Lisp_Object that is supposed to represent a Unicode codepoint,
   make sure it does, and return it. */

int
decode_unicode (Lisp_Object unicode)
{
  EMACS_INT val;
  CHECK_INT (unicode);
  val = XINT (unicode);
  if (!valid_unicode_codepoint_p (val, UNICODE_ALLOW_PRIVATE))
    invalid_argument ("Invalid unicode codepoint (range 0 .. 2^31 - 1)",
		      unicode);
  return (int) val;
}

#ifndef MULE

Lisp_Object_dynarr *
get_unicode_precedence (void)
{
  return NULL;
}

Lisp_Object_dynarr *
get_buffer_unicode_precedence (struct buffer *UNUSED (buf))
{
  return NULL;
}

void
free_precedence_dynarr (Lisp_Object_dynarr *precdyn)
{
  text_checking_assert (!precdyn);
}

/* Convert the given list of charsets (not previously validated) into a
   precedence dynarr for use with unicode_to_charset_codepoint().  When
   done, free the dynarr with free_precedence_dynarr(). */

Lisp_Object_dynarr *
convert_charset_list_to_precedence_dynarr (Lisp_Object UNUSED (precedence_list))
{
  return NULL;
}

#else /* MULE */

static void
init_blank_unicode_tables (void)
{
  int i;

  from_unicode_blank[0] = NULL;
  from_unicode_blank[1] = xnew_array (UINT_16_BIT, 256);
  from_unicode_blank[2] = xnew_array (UINT_16_BIT *, 256);
  from_unicode_blank[3] = xnew_array (UINT_16_BIT **, 256);
  from_unicode_blank[4] = xnew_array (UINT_16_BIT ***, 256);
  for (i = 0; i < 256; i++)
    {
      /* See comment above on BADVAL_FROM_TABLE */
      ((UINT_16_BIT *) from_unicode_blank[1])[i] = BADVAL_FROM_TABLE;
      ((void **) from_unicode_blank[2])[i] = from_unicode_blank[1];
      ((void **) from_unicode_blank[3])[i] = from_unicode_blank[2];
      ((void **) from_unicode_blank[4])[i] = from_unicode_blank[3];
    }

  to_unicode_blank_1 = xnew_array (int, CHARSET_MAX_SIZE);
  to_unicode_blank_2 = xnew_array (int *, CHARSET_MAX_SIZE);
  for (i = 0; i < CHARSET_MAX_SIZE; i++)
    {
      /* Likewise for BADVAL_TO_TABLE */
      to_unicode_blank_1[i] = BADVAL_TO_TABLE;
      to_unicode_blank_2[i] = to_unicode_blank_1;
    }
}

static void *
create_new_from_unicode_table (int level)
{
  /* WARNING: sizeof (UINT_16_BIT) != sizeof (UINT_16_BIT *). */
  Bytecount size = level == 1 ? sizeof (UINT_16_BIT) : sizeof (void *);
  void *newtab;

  text_checking_assert (level >= 1 && level <= 4);
  newtab = xmalloc (256 * size);
  memcpy (newtab, from_unicode_blank[level], 256 * size);
  return newtab;
}

/* Allocate and blank the tables.
   Loading them up is done by load-unicode-mapping-table. */
void
init_charset_unicode_tables (Lisp_Object charset)
{
  if (TO_TABLE_SIZE_FROM_CHARSET (charset) == 1)
    {
      int *to_table = xnew_array (int, CHARSET_MAX_SIZE);
      memcpy (to_table, to_unicode_blank_1, CHARSET_MAX_SIZE * sizeof (int));
      XCHARSET_TO_UNICODE_TABLE (charset) = to_table;
    }
  else
    {
      int **to_table = xnew_array (int *, CHARSET_MAX_SIZE);
      memcpy (to_table, to_unicode_blank_2, CHARSET_MAX_SIZE * sizeof (int *));
      XCHARSET_TO_UNICODE_TABLE (charset) = to_table;
    }

#ifdef MAXIMIZE_UNICODE_TABLE_DEPTH
  XCHARSET_FROM_UNICODE_TABLE (charset) = create_new_from_unicode_table (4);
  XCHARSET_FROM_UNICODE_LEVELS (charset) = 4;
#else
  XCHARSET_FROM_UNICODE_TABLE (charset) = create_new_from_unicode_table (1);
  XCHARSET_FROM_UNICODE_LEVELS (charset) = 1;
#endif /* MAXIMIZE_UNICODE_TABLE_DEPTH */
}

static void
free_from_unicode_table (void *table, int level)
{
  if (level >= 2)
    {
      void **tab = (void **) table;
      int i;

      for (i = 0; i < 256; i++)
	{
	  if (tab[i] != from_unicode_blank[level - 1])
	    free_from_unicode_table (tab[i], level - 1);
	}
    }

  xfree (table, void *);
}

static void
free_to_unicode_table (void *table, int level)
{
  if (level == 2)
    {
      int i;
      int **tab = (int **) table;

      for (i = 0; i < CHARSET_MAX_SIZE; i++)
	{
	  if (tab[i] != to_unicode_blank_1)
	    free_to_unicode_table (tab[i], 1);
	}
    }

  xfree (table, void *);
}

void
free_charset_unicode_tables (Lisp_Object charset)
{
  free_to_unicode_table (XCHARSET_TO_UNICODE_TABLE (charset),
			 TO_TABLE_SIZE_FROM_CHARSET (charset));
  free_from_unicode_table (XCHARSET_FROM_UNICODE_TABLE (charset),
			   XCHARSET_FROM_UNICODE_LEVELS (charset));
}

#ifdef MEMORY_USAGE_STATS

static Bytecount
compute_from_unicode_table_size_1 (void *table, int level,
				   struct overhead_stats *stats)
{
  Bytecount size = 0;

  if (level >= 2)
    {
      int i;
      void **tab = (void **) table;
      for (i = 0; i < 256; i++)
	{
	  if (tab[i] != from_unicode_blank[level - 1])
	    size += compute_from_unicode_table_size_1 (tab[i], level - 1,
						       stats);
	}
    }

  size += malloced_storage_size (table,
				 256 * (level == 1 ? sizeof (UINT_16_BIT) :
					sizeof (void *)),
				 stats);
  return size;
}

static Bytecount
compute_to_unicode_table_size_1 (void *table, int level,
				 struct overhead_stats *stats)
{
  Bytecount size = 0;

  if (level == 2)
    {
      int i;
      int **tab = (int **) table;

      for (i = 0; i < CHARSET_MAX_SIZE; i++)
	{
	  if (tab[i] != to_unicode_blank_1)
	    size += compute_to_unicode_table_size_1 (tab[i], 1, stats);
	}
    }

  size += malloced_storage_size (table,
				 CHARSET_MAX_SIZE *
				 (level == 1 ? sizeof (int) :
				  sizeof (void *)),
				 stats);
  return size;
}

Bytecount
compute_from_unicode_table_size (Lisp_Object charset,
				 struct overhead_stats *stats)
{
  return (compute_from_unicode_table_size_1
	  (XCHARSET_FROM_UNICODE_TABLE (charset),
	   XCHARSET_FROM_UNICODE_LEVELS (charset),
	   stats));
}

Bytecount
compute_to_unicode_table_size (Lisp_Object charset,
			       struct overhead_stats *stats)
{
  return (compute_to_unicode_table_size_1
	  (XCHARSET_TO_UNICODE_TABLE (charset),
	   TO_TABLE_SIZE_FROM_CHARSET (charset),
	   stats));
}

#endif

#ifdef SLEDGEHAMMER_CHECK_UNICODE

/* "Sledgehammer checks" are checks that verify the self-consistency
   of an entire structure every time a change is about to be made or
   has been made to the structure.  Not fast but a pretty much
   sure-fire way of flushing out any incorrectnesses in the algorithms
   that create the structure.

   Checking only after a change has been made will speed things up by
   a factor of 2, but it doesn't absolutely prove that the code just
   checked caused the problem; perhaps it happened elsewhere, either
   in some code you forgot to sledgehammer check or as a result of
   data corruption. */

static void
assert_not_any_blank_table (void *tab)
{
  assert (tab != from_unicode_blank[1]);
  assert (tab != from_unicode_blank[2]);
  assert (tab != from_unicode_blank[3]);
  assert (tab != from_unicode_blank[4]);
  assert (tab != to_unicode_blank_1);
  assert (tab != to_unicode_blank_2);
  assert (tab);
}

static void
sledgehammer_check_from_table (Lisp_Object charset, void *table, int level,
			       int codetop)
{
  int i;

  switch (level)
    {
    case 1:
      {
	UINT_16_BIT *tab = (UINT_16_BIT *) table;
	for (i = 0; i < 256; i++)
	  {
	    if (tab[i] != BADVAL_FROM_TABLE)
	      {
		int c1, c2;

		c1 = tab[i] >> 8;
		c2 = tab[i] & 0xFF;
		assert_codepoint_in_range (charset, c1, c2);
		if (TO_TABLE_SIZE_FROM_CHARSET (charset) == 1)
		  {
		    int *to_table =
		      (int *) XCHARSET_TO_UNICODE_TABLE (charset);
		    assert_not_any_blank_table (to_table);
		    assert (to_table[c2 - CHARSET_MIN_OFFSET] ==
			    (codetop << 8) + i);
		  }
		else
		  {
		    int **to_table =
		      (int **) XCHARSET_TO_UNICODE_TABLE (charset);
		    assert_not_any_blank_table (to_table);
		    assert_not_any_blank_table
		      (to_table[c1 - CHARSET_MIN_OFFSET]);
		    assert (to_table[c1 - CHARSET_MIN_OFFSET]
			    [c2 - CHARSET_MIN_OFFSET] == (codetop << 8) + i);
		  }
	      }
	  }
	break;
      }
    case 2:
    case 3:
    case 4:
      {
	void **tab = (void **) table;
	for (i = 0; i < 256; i++)
	  {
	    if (tab[i] != from_unicode_blank[level - 1])
	      sledgehammer_check_from_table (charset, tab[i], level - 1,
					     (codetop << 8) + i);
	  }
	break;
      }
    default:
      ABORT ();
    }
}

static void
sledgehammer_check_to_table (Lisp_Object charset, void *table, int level,
			     int codetop)
{
  int i;
  int low1, high1, low2, high2;

  get_charset_limits (charset, &low1, &high1, &low2, &high2);

  switch (level)
    {
    case 1:
      {
	int *tab = (int *) table;

	if (TO_TABLE_SIZE_FROM_CHARSET (charset) == 2)
	  /* This means we're traversing a nested table */
	  low1 = low2, high1 = high2;
	for (i = 0; i < CHARSET_MAX_SIZE; i++)
	  {
	    /* Make sure no out-of-bounds characters were set */
	    if (i + CHARSET_MIN_OFFSET < low1 ||
		i + CHARSET_MIN_OFFSET > high1)
	      assert (tab[i] == BADVAL_TO_TABLE);
	    if (tab[i] != BADVAL_TO_TABLE)
	      {
		int u4, u3, u2, u1, levels;
		UINT_16_BIT val;
		void *frtab = XCHARSET_FROM_UNICODE_TABLE (charset);

		assert (tab[i] >= 0);
		UNICODE_BREAKUP_CHAR_CODE (tab[i], u4, u3, u2, u1, levels);
#ifdef MAXIMIZE_UNICODE_TABLE_DEPTH
		levels = 4;
#endif /* MAXIMIZE_UNICODE_TABLE_DEPTH */
		assert (levels <= XCHARSET_FROM_UNICODE_LEVELS (charset));

		switch (XCHARSET_FROM_UNICODE_LEVELS (charset))
		  {
		  case 1: val = ((UINT_16_BIT *) frtab)[u1]; break;
		  case 2: val = ((UINT_16_BIT **) frtab)[u2][u1]; break;
		  case 3: val = ((UINT_16_BIT ***) frtab)[u3][u2][u1]; break;
		  case 4: val = ((UINT_16_BIT ****) frtab)[u4][u3][u2][u1];
		    break;
		  default: ABORT ();
		  }

		if (TO_TABLE_SIZE_FROM_CHARSET (charset) == 1)
		  {
		    assert (i + CHARSET_MIN_OFFSET == (val >> 8));
		    assert (0 == (val & 0xFF));
		  }
		else
		  {
		    assert (codetop + CHARSET_MIN_OFFSET == (val >> 8));
		    assert (i + CHARSET_MIN_OFFSET == (val & 0xFF));
		  }

		switch (XCHARSET_FROM_UNICODE_LEVELS (charset))
		  {
		  case 4:
		    assert_not_any_blank_table (frtab);
		    frtab = ((UINT_16_BIT ****) frtab)[u4];
		    /* fall through */
		  case 3:
		    assert_not_any_blank_table (frtab);
		    frtab = ((UINT_16_BIT ***) frtab)[u3];
		    /* fall through */
		  case 2:
		    assert_not_any_blank_table (frtab);
		    frtab = ((UINT_16_BIT **) frtab)[u2];
		    /* fall through */
		  case 1:
		    assert_not_any_blank_table (frtab);
		    break;
		  default: ABORT ();
		  }
	      }
	  }
	break;
      }
    case 2:
      {
	int **tab = (int **) table;

	for (i = 0; i < CHARSET_MAX_SIZE; i++)
	  {
	    /* Make sure no out-of-bounds characters were set */
	    if (i + CHARSET_MIN_OFFSET < low1 ||
		i + CHARSET_MIN_OFFSET > high1)
	      assert (tab[i] == to_unicode_blank_1);
	    if (tab[i] != to_unicode_blank_1)
	      sledgehammer_check_to_table (charset, tab[i], 1, i);
	  }
	break;
      }
    default:
      ABORT ();
    }
}

static void
sledgehammer_check_unicode_tables (Lisp_Object charset)
{
  /* verify that the blank tables have not been modified */
  int i;
  int from_level = XCHARSET_FROM_UNICODE_LEVELS (charset);
  int to_level = XCHARSET_FROM_UNICODE_LEVELS (charset);

  for (i = 0; i < 256; i++)
    {
      assert (((UINT_16_BIT *) from_unicode_blank[1])[i] ==
	      BADVAL_FROM_TABLE);
      assert (((void **) from_unicode_blank[2])[i] == from_unicode_blank[1]);
      assert (((void **) from_unicode_blank[3])[i] == from_unicode_blank[2]);
      assert (((void **) from_unicode_blank[4])[i] == from_unicode_blank[3]);
    }

  for (i = 0; i < CHARSET_MAX_SIZE; i++)
    {
      assert (to_unicode_blank_1[i] == BADVAL_TO_TABLE);
      assert (to_unicode_blank_2[i] == to_unicode_blank_1);
    }

  assert (from_level >= 1 && from_level <= 4);

  sledgehammer_check_from_table (charset,
				 XCHARSET_FROM_UNICODE_TABLE (charset),
				 from_level, 0);

  sledgehammer_check_to_table (charset,
			       XCHARSET_TO_UNICODE_TABLE (charset),
			       TO_TABLE_SIZE_FROM_CHARSET (charset), 0);
}

#endif /* SLEDGEHAMMER_CHECK_UNICODE */

static void
set_unicode_conversion (int code, Lisp_Object charset, int c1, int c2)
{
  ASSERT_VALID_CHARSET_CODEPOINT (charset, c1, c2);
  /* @@#### Is UNICODE_ALLOW_PRIVATE correct here?  If so, replace with
     ASSERT_VALID_UNICODE_CODEPOINT(). */
  text_checking_assert (valid_unicode_codepoint_p (code,
						   UNICODE_ALLOW_PRIVATE));

  /* I tried an assert on code > 255 || chr == code, but that fails because
     Mule gives many Latin characters separate code points for different
     ISO 8859 coded character sets.  Obvious in hindsight.... */
  text_checking_assert (!EQ (charset, Vcharset_ascii) || code == c2);
  text_checking_assert (!EQ (charset, Vcharset_control_1) || code == c2);
  text_checking_assert (!EQ (charset, Vcharset_latin_iso8859_1) ||
			code == c2);

  /* This assert is needed because it is simply unimplemented. */
  text_checking_assert (!EQ (charset, Vcharset_composite));

#ifdef SLEDGEHAMMER_CHECK_UNICODE
  sledgehammer_check_unicode_tables (charset);
#endif

  /* First, the char -> unicode translation */

  if (TO_TABLE_SIZE_FROM_CHARSET (charset) == 1)
    {
      int *to_table = (int *) XCHARSET_TO_UNICODE_TABLE (charset);
      to_table[c2 - CHARSET_MIN_OFFSET] = code;
    }
  else
    {
      int **to_table_2 = (int **) XCHARSET_TO_UNICODE_TABLE (charset);
      int *to_table_1;

      to_table_1 = to_table_2[c1 - CHARSET_MIN_OFFSET];
      if (to_table_1 == to_unicode_blank_1)
	{
	  to_table_1 = xnew_array (int, CHARSET_MAX_SIZE);
	  memcpy (to_table_1, to_unicode_blank_1,
		  CHARSET_MAX_SIZE * sizeof (int));
	  to_table_2[c1 - CHARSET_MIN_OFFSET] = to_table_1;
	}
      to_table_1[c2 - CHARSET_MIN_OFFSET] = code;
    }

  /* Then, unicode -> char: much harder */

  {
    int levels;
    int u4, u3, u2, u1;
#ifndef MAXIMIZE_UNICODE_TABLE_DEPTH
    int code_levels;
#endif /* not MAXIMIZE_UNICODE_TABLE_DEPTH */
    UNICODE_BREAKUP_CHAR_CODE (code, u4, u3, u2, u1, code_levels);

    levels = XCHARSET_FROM_UNICODE_LEVELS (charset);
    text_checking_assert (levels >= 1 && levels <= 4);

#ifndef MAXIMIZE_UNICODE_TABLE_DEPTH
    text_checking_assert (code_levels <= 4);
    /* Make sure the charset's tables have at least as many levels as
       the code point has: Note that the charset is guaranteed to have
       at least one level, because it was created that way */
    if (levels < code_levels)
      {
	int i;

	for (i = 2; i <= code_levels; i++)
	  {
	    if (levels < i)
	      {
		void *old_table = XCHARSET_FROM_UNICODE_TABLE (charset);
		void *table = create_new_from_unicode_table (i);
		XCHARSET_FROM_UNICODE_TABLE (charset) = table;
		((void **) table)[0] = old_table;
	      }
	  }

	levels = code_levels;
	XCHARSET_FROM_UNICODE_LEVELS (charset) = code_levels;
      }
#endif /* not MAXIMIZE_UNICODE_TABLE_DEPTH */

    /* Now, make sure there is a non-default table at each level */
    {
      int i;
      void *table = XCHARSET_FROM_UNICODE_TABLE (charset);

      for (i = levels; i >= 2; i--)
	{
	  int ind;

	  switch (i)
	    {
	    case 4: ind = u4; break;
	    case 3: ind = u3; break;
	    case 2: ind = u2; break;
	    default: ind = 0; ABORT ();
	    }

	  if (((void **) table)[ind] == from_unicode_blank[i - 1])
	    ((void **) table)[ind] =
	      ((void *) create_new_from_unicode_table (i - 1));
	  table = ((void **) table)[ind];
	}
    }

    /* Finally, set the character */
	  
    {
      void *table = XCHARSET_FROM_UNICODE_TABLE (charset);
#ifndef MAXIMIZE_UNICODE_TABLE_DEPTH
      switch (levels)
	{
	case 4: ((UINT_16_BIT ****) table)[u4][u3][u2][u1] = (c1 << 8) + c2; break;
	case 3: ((UINT_16_BIT ***) table)[u3][u2][u1] = (c1 << 8) + c2; break;
	case 2: ((UINT_16_BIT **) table)[u2][u1] = (c1 << 8) + c2; break;
	case 1: ((UINT_16_BIT *) table)[u1] = (c1 << 8) + c2; break;
	default:  ABORT ();
	}
#else /* MAXIMIZE_UNICODE_TABLE_DEPTH */
      ((UINT_16_BIT ****) table)[u4][u3][u2][u1] = (c1 << 8) + c2;
#endif /* not MAXIMIZE_UNICODE_TABLE_DEPTH */
    }
  }

#ifdef SLEDGEHAMMER_CHECK_UNICODE
  sledgehammer_check_unicode_tables (charset);
#endif
}

#ifndef UNICODE_INTERNAL

/* Return a free JIT codepoint.  Return 1 on success, 0 on failure.
   (Currently never returns 0.  Presumably if we ever run out of JIT charsets,
   we will signal an error in Fmake_charset().) */

static int
get_free_jit_codepoint (Lisp_Object *charset, int *c1, int *c2)
{
  int last_allocated_jit_c1 = XINT (Vlast_allocated_jit_c1);
  int last_allocated_jit_c2 = XINT (Vlast_allocated_jit_c2);
  int number_of_jit_charsets = XINT (Vnumber_of_jit_charsets);

  if (!NILP (Vcurrent_jit_charset) &&
      !(last_allocated_jit_c1 == 127 && last_allocated_jit_c2 == 127))
    {
      if (127 == last_allocated_jit_c2)
	{
	  ++last_allocated_jit_c1;
	  last_allocated_jit_c2 = 0x20;
	}
      else
	{
	  ++last_allocated_jit_c2;
	}
    }
  else
    {
      Ibyte setname[100];
      qxesprintf (setname, "jit-ucs-charset-%d", number_of_jit_charsets);

      Vcurrent_jit_charset = Fmake_charset 
	(intern ((const CIbyte *) setname), Vcharset_descr, 
	 nconc2 (list6 (Qcolumns, make_int (1), Qchars,
			make_int (96),
			Qdimension, make_int (2)),
		 list2 (Qregistries, Qunicode_registries)));
      XCHARSET (Vcurrent_jit_charset)->jit_charset_p = 1;
      last_allocated_jit_c1 = last_allocated_jit_c2 = 32;

      number_of_jit_charsets++;
    }

  *charset = Vcurrent_jit_charset;
  *c1 = last_allocated_jit_c1;
  *c2 = last_allocated_jit_c2;
  ASSERT_VALID_CHARSET_CODEPOINT (*charset, *c1, *c2);
  
  Vlast_allocated_jit_c1 = make_int (last_allocated_jit_c1);
  Vlast_allocated_jit_c2 = make_int (last_allocated_jit_c2);
  Vnumber_of_jit_charsets = make_int (number_of_jit_charsets);

  return 1;
}

#endif /* not UNICODE_INTERNAL */

/* The just-in-time creation of XEmacs characters that correspond to unknown
   Unicode code points happens when: 

   1. The lookup would otherwise fail. 

   2. The precedence_list array is the nil or the default. 

   If there are no free code points in the just-in-time Unicode character
   set, and the precedence_list array is the default unicode precedence list,
   create a new just-in-time Unicode character set, add it at the end of the
   unicode precedence list, create the XEmacs character in that character
   set, and return it. */

/* Convert a Unicode codepoint to a charset codepoint.  PRECEDENCE_LIST is
   a list of charsets.  The charsets will be consulted in order for
   characters that match the Unicode codepoint.  If PREDICATE is non-NULL,
   only charsets that pass the predicate will be considered. */

void
non_ascii_unicode_to_charset_codepoint (int code,
					Lisp_Object_dynarr *precedence_list,
					int (*predicate) (Lisp_Object),
					Lisp_Object *charset,
					int *c1, int *c2)
{
  int u1, u2, u3, u4;
#ifndef MAXIMIZE_UNICODE_TABLE_DEPTH
  int code_levels;
#endif
  int i;
  int n = Dynarr_length (precedence_list);

  ASSERT_VALID_UNICODE_CODEPOINT (code);
  text_checking_assert (code >= 128);

  /* @@#### This optimization is not necessarily correct.  See comment in
     unicode_to_charset_codepoint(). */
  if (code <= 0x9F)
    {
      *charset = Vcharset_control_1;
      *c1 = 0;
      *c2 = code;
      goto done;
    }

  UNICODE_BREAKUP_CHAR_CODE (code, u4, u3, u2, u1, code_levels);

  for (i = 0; i < n; i++)
    {
      void *table;

      *charset = Dynarr_at (precedence_list, i);
      if (predicate && !(*predicate) (*charset))
	continue;
      table = XCHARSET_FROM_UNICODE_TABLE (*charset);
#ifdef ALLOW_ALGORITHMIC_CONVERSION_TABLES
      if (!table)
	{
	  int algo_low = XCHARSET_ALGO_LOW (*charset);
	  text_checking_assert (algo_low >= 0);
	  if (code >= algo_low &&
	      code < algo_low +
	      XCHARSET_CHARS (*charset, 0) * XCHARSET_CHARS (*charset, 1))
	    {
	      code -= algo_low;
	      *c1 = code / XCHARSET_CHARS (*charset, 1);
	      *c2 = code % XCHARSET_CHARS (*charset, 1);
	      *c1 += XCHARSET_OFFSET (*charset, 0);
	      *c2 += XCHARSET_OFFSET (*charset, 1);
	      goto done;
	    }
	  continue;
	}
#endif /* ALLOW_ALGORITHMIC_CONVERSION_TABLES */
#ifdef MAXIMIZE_UNICODE_TABLE_DEPTH
      UINT_16_BIT retval = ((UINT_16_BIT ****) table)[u4][u3][u2][u1];
      if (retval != BADVAL_FROM_TABLE)
	{
	  *c1 = retval >> 8;
	  *c2 = retval & 0xFF;
	  goto done;
	}
#else
      int levels = XCHARSET_FROM_UNICODE_LEVELS (*charset);
      if (levels >= code_levels)
	{
	  UINT_16_BIT retval;

	  switch (levels)
	    {
	    case 1: retval = ((UINT_16_BIT *) table)[u1]; break;
	    case 2: retval = ((UINT_16_BIT **) table)[u2][u1]; break;
	    case 3: retval = ((UINT_16_BIT ***) table)[u3][u2][u1]; break;
	    case 4: retval = ((UINT_16_BIT ****) table)[u4][u3][u2][u1]; break;
	    default: ABORT (); retval = 0;
	    }

	  if (retval != BADVAL_FROM_TABLE)
	    {
	      c1 = retval >> 8;
	      c2 = retval & 0xFF;
	      goto done;
	    }
	}
#endif /* MAXIMIZE_UNICODE_TABLE_DEPTH */
    }

  /* Unable to convert */

#ifndef UNICODE_INTERNAL
  /* Non-Unicode-internal: Maybe do just-in-time assignment */

  /* Only do the magic just-in-time assignment if we're using the default
     list.  This check is done because we don't want to do this assignment
     if we're using a partial list of charsets and if we're not using the
     default list, we don't know whether the list is full.

     @@#### We might want to rethink this, and will have to if/when we
     have buffer-local precedence lists.
     */ 
  if (global_unicode_precedence_dynarr == precedence_list) 
    {
      if (get_free_jit_codepoint (charset, c1, c2))
	{
	  set_unicode_conversion (code, *charset, *c1, *c2);
	  goto done;
	}
    }
  
#endif /* not UNICODE_INTERNAL */
  /* Unable to convert; try the private codepoint range */
  private_unicode_to_charset_codepoint (code, charset, c1, c2);
  return;

done:
  ASSERT_VALID_CHARSET_CODEPOINT (*charset, *c1, *c2);
  return;
}

/* @@####
 there has to be a way for Lisp callers to *always* request the private
 codepoint if they want it.

 Possibly, we want always-private characters to behave somewhat like
 their normal equivalents, when such exists; e.g. when retrieving
 a property from a char table using such a char, try to retrieve a
 normal Unicode value and use its properties instead.  When setting,
 do a similar switcheroo.  Maybe at redisplay time also, and anywhere
 else we access properties of a char, do similar switching.
 This might be necessary to get recompilation to really work right,
 I'm not sure.  Hold off on this until necessary.
*/

int
charset_codepoint_to_private_unicode (Lisp_Object charset, int c1, int c2)
{
  /* If the charset is ISO2022-compatible, try to use a representation
     that is portable, in case we are writing out to an external
     Unicode representation.  Otherwise, just do something.

     What we use is this:

     If ISO2022-compatible:

       23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
        1 <---> <------------------> <------------------> <------------------>
            1            2                    3                    4

     Field 1 is the type (94, 96, 94x94, 96x96).
     Field 2 is the final byte.
     Field 3 is the first octet.
     Field 4 is the second octet.
     Bit 23 is set so that we are above all possible UTF-16 chars.

 <-... 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
 <-..------------------------> <---------------------> <--------------------->
              1                           2                       3

     If non-ISO2022-compatible:

     Field 1 (extends up to 31 bits) is the charset ID + 256, to place it
     above all the others. (NOTE: It's true that non-encodable charset ID's
     are always >= 256; but the correspondence between encodable charsets
     and ISO2022-compatible charsets is not one-to-one in either direction;
     see get_charset_iso2022_type().)  Fields 2 and 3 are the octet values.
  */
  int type = get_charset_iso2022_type (charset);

  if (type >= 0)
    {
      /* The types are defined between 0 and 3 in charset.h; make sure
	 someone doesn't change them */
      text_checking_assert (type <= 3);
      c1 &= 127;
      c2 &= 127;
      return 0x800000 + (type << 21) + (XCHARSET_FINAL (charset) << 14)
	+ (c1 << 7) + c2;
    }
  else
    {
      int retval = ((256 + XCHARSET_ID (charset)) << 16) + (c1 << 8) + c2;
      /* Check for overflow */
      if (!valid_unicode_codepoint_p (retval, UNICODE_ALLOW_PRIVATE))
	retval = CANT_CONVERT_CHAR_WHEN_ENCODING_UNICODE;
      return retval;
    }
}

void
private_unicode_to_charset_codepoint (int priv, Lisp_Object *charset,
				      int *c1, int *c2)
{
  if (priv >= 0x1000000)
    {
      *charset = charset_by_id ((priv >> 16) - 256);
      *c1 = (priv >> 8) & 0xFF;
      *c2 = priv & 0xFF;
    }
  else if (priv >= 0x800000)
    {
      int type;
      int final;
      
      priv -= 0x800000;
      type = (priv >> 21) & 3;
      final = (priv >> 14) & 0x7F;
      *c1 = (priv >> 7) & 0x7F;
      *c2 = priv & 0x7F;
      *charset = charset_by_attributes (type, final, CHARSET_LEFT_TO_RIGHT);
      if (NILP (*charset))
	*charset = charset_by_attributes (type, final, CHARSET_RIGHT_TO_LEFT);
      if (!NILP (*charset))
	{
	  if (XCHARSET_OFFSET (*charset, 0) >= 128)
	    *c1 += 128;
	  if (XCHARSET_OFFSET (*charset, 1) >= 128)
	    *c2 += 128;
	}
    }
  else
    {
      /* @@#### Better error recovery? */
      *charset = Qnil;
      *c1 = 0;
      *c2 = 0;
    }
  if (!NILP (*charset) && !valid_charset_codepoint_p (*charset, *c1, *c2))
    /* @@#### Better error recovery? */
    {
      /* @@#### Better error recovery? */
      *charset = Qnil;
      *c1 = 0;
      *c2 = 0;
    }
}

/***************************************************************************/
/*                                                                         */
/*                code to handle Unicode precedence lists                  */
/*                                                                         */
/***************************************************************************/

/************

  NOTE: Unicode precedence lists are used when converting Unicode
  codepoints to charset codepoints.  There may be more than one charset
  containing a character matching a given Unicode codepoint; to determine
  which charset to use, we use a precedence list.  Externally, precedence
  lists are just lists, but internally we use a Lisp_Object_dynarr.

************/

void
begin_precedence_list_generation (void)
{
  Fclrhash (Vprecedence_list_charsets_seen_hash);
}

/* Add a single charset to a precedence list.  Charsets already present
   are not added.  To keep track of charsets already seen, this makes use
   of a hash table.  At the beginning of generating the list, you must
   call begin_precedence_list_generation(). */

void
add_charset_to_precedence_list (Lisp_Object charset,
				Lisp_Object_dynarr *preclist)
{
  if (NILP (Fgethash (charset, Vprecedence_list_charsets_seen_hash, Qnil)))
    {
      Dynarr_add (preclist, charset);
      Fputhash (charset, Qt, Vprecedence_list_charsets_seen_hash);
    }
}

/* Add a list of charsets to a precedence list.  LIST must be a list of
   charsets or charset names.  Charsets already present are not added.  To
   keep track of charsets already seen, this makes use of a hash table.  At
   the beginning of generating the list, you must call
   begin_precedence_list_generation(). */

static void
add_charsets_to_precedence_list (Lisp_Object list,
				 Lisp_Object_dynarr *preclist)
{
  {
    EXTERNAL_LIST_LOOP_2 (elt, list)
      {
	Lisp_Object charset = Fget_charset (elt);
	add_charset_to_precedence_list (charset, preclist);
      }
  }
}

/* Go through ORIG_PRECLIST and add all charsets to NEW_PRECLIST that pass
   the predicate, if not already added.  To keep track of charsets already
   seen, this makes use of a hash table.  At the beginning of generating
   the list, you must call begin_precedence_list_generation().  PREDICATE
   is passed a charset and should return non-zero if the charset is to be
   added.  If PREDICATE is NULL, always add the charset. */
void
filter_precedence_list (Lisp_Object_dynarr *orig_preclist,
			Lisp_Object_dynarr *new_preclist,
			int (*predicate) (Lisp_Object))
{
  int i;
  for (i = 0; i < Dynarr_length (orig_preclist); i++)
    {
      Lisp_Object charset = Dynarr_at (orig_preclist, i);
      if (!predicate || (*predicate) (charset))
	add_charset_to_precedence_list (charset, new_preclist);
    }
}

/* Called for each pair of (symbol, charset) in the hash table tracking
   charsets. */
static int
rup_mapper (Lisp_Object UNUSED (key), Lisp_Object value,
	    void * UNUSED (closure))
{
  if (NILP (Fgethash (value, Vprecedence_list_charsets_seen_hash, Qnil)))
    {
      Dynarr_add (global_unicode_precedence_dynarr, value);
      Fputhash (value, Qt, Vprecedence_list_charsets_seen_hash);
    }
  return 0;
}

/* Rebuild the charset precedence array.
   The "charsets preferred for the current language" get highest precedence,
   followed by the "charsets preferred by default", ordered as in
   Vlanguage_unicode_precedence_list and Vdefault_unicode_precedence_list,
   respectively.  All remaining charsets follow in an arbitrary order. */
void
recalculate_unicode_precedence (void)
{
  Dynarr_reset (global_unicode_precedence_dynarr);

  begin_precedence_list_generation ();
  add_charsets_to_precedence_list (Vlanguage_unicode_precedence_list,
				   global_unicode_precedence_dynarr);
  add_charsets_to_precedence_list (Vdefault_unicode_precedence_list,
				   global_unicode_precedence_dynarr);


  /* Now add all remaining charsets to global_unicode_precedence_dynarr */
  elisp_maphash (rup_mapper, Vcharset_hash_table, NULL);
}

Lisp_Object_dynarr *
get_unicode_precedence (void)
{
  return global_unicode_precedence_dynarr;
}

Lisp_Object_dynarr *
get_buffer_unicode_precedence (struct buffer *UNUSED (buf))
{
  /* @@####  Implement me */
  return global_unicode_precedence_dynarr;
}

void
free_precedence_dynarr (Lisp_Object_dynarr *dynarr)
{
  if (dynarr != global_unicode_precedence_dynarr)
    Dynarr_free (dynarr);
}

/* Convert the given list of charsets (not previously validated) into a
   precedence dynarr for use with unicode_to_charset_codepoint().  When
   done, free the dynarr with free_precedence_dynarr(). */

Lisp_Object_dynarr *
convert_charset_list_to_precedence_dynarr (Lisp_Object precedence_list)
{
  Lisp_Object_dynarr *dyn;

  if (NILP (precedence_list))
    return get_unicode_precedence ();

  /* Must validate before allocating (or use unwind-protect) */
  {
    EXTERNAL_LIST_LOOP_2 (elt, precedence_list)
      Fget_charset (elt);
  }

  dyn = Dynarr_new (Lisp_Object);

  begin_precedence_list_generation ();
  add_charsets_to_precedence_list (precedence_list, dyn);

  return dyn;
}

DEFUN ("unicode-precedence-list", 
       Funicode_precedence_list,
       0, 0, 0, /*
Return the precedence order among charsets used for Unicode decoding.

Value is a list of charsets, which are searched in order for a translation
matching a given Unicode character.

The highest precedence is given to the language-specific precedence list of
charsets, defined by `set-language-unicode-precedence-list'.  These are
followed by charsets in the default precedence list, defined by
`set-default-unicode-precedence-list'.  Charsets occurring multiple times are
given precedence according to their first occurrance in either list.  These
are followed by the remaining charsets, in some arbitrary order.

The language-specific precedence list is meant to be set as part of the
language environment initialization; the default precedence list is meant
to be set by the user.

#### NOTE: This interface may be changed.
*/
       ())
{
  int i;
  Lisp_Object list = Qnil;
  Lisp_Object_dynarr *preclist = get_unicode_precedence();

  for (i = Dynarr_length (preclist) - 1; i >= 0; i--)
    list = Fcons (Dynarr_at (preclist, i), list);
  return list;
}


/* #### This interface is wrong.  Cyrillic users and Chinese users are going
   to have varying opinions about whether ISO Cyrillic, KOI8-R, or Windows
   1251 should take precedence, and whether Big Five or CNS should take
   precedence, respectively.  This means that users are sometimes going to
   want to set Vlanguage_unicode_precedence_list.
   Furthermore, this should be language-local (buffer-local would be a
   reasonable approximation).

   Answer: You are right, this needs rethinking. */
DEFUN ("set-language-unicode-precedence-list",
       Fset_language_unicode_precedence_list,
       1, 1, 0, /*
Set the language-specific precedence of charsets in Unicode decoding.
LIST is a list of charsets.
See `unicode-precedence-list' for more information.

#### NOTE: This interface may be changed.
*/
       (list))
{
  {
    EXTERNAL_LIST_LOOP_2 (elt, list)
      Fget_charset (elt);
  }

  Vlanguage_unicode_precedence_list = list;
  recalculate_unicode_precedence ();
  return Qnil;
}

DEFUN ("language-unicode-precedence-list",
       Flanguage_unicode_precedence_list,
       0, 0, 0, /*
Return the language-specific precedence list used for Unicode decoding.
See `unicode-precedence-list' for more information.

#### NOTE: This interface may be changed.
*/
       ())
{
  return Vlanguage_unicode_precedence_list;
}

DEFUN ("set-default-unicode-precedence-list",
       Fset_default_unicode_precedence_list,
       1, 1, 0, /*
Set the default precedence list used for Unicode decoding.
This is intended to be set by the user.  See
`unicode-precedence-list' for more information.

#### NOTE: This interface may be changed.
*/
       (list))
{
  {
    EXTERNAL_LIST_LOOP_2 (elt, list)
      Fget_charset (elt);
  }

  Vdefault_unicode_precedence_list = list;
  recalculate_unicode_precedence ();
  return Qnil;
}

DEFUN ("default-unicode-precedence-list",
       Fdefault_unicode_precedence_list,
       0, 0, 0, /*
Return the default precedence list used for Unicode decoding.
See `unicode-precedence-list' for more information.

#### NOTE: This interface may be changed.
*/
       ())
{
  return Vdefault_unicode_precedence_list;
}

/***************************************************************************/
/*                set up Unicode <-> charset codepoint conversion          */
/***************************************************************************/

DEFUN ("set-unicode-conversion", Fset_unicode_conversion,
       3, 4, 0, /*
Add conversion information between Unicode and charset codepoints.
A single Unicode codepoint may have multiple corresponding charset
codepoints, but each codepoint in a charset corresponds to only one
Unicode codepoint.  Further calls to this function with the same
values for (CHARSET, C1 [, C2]) and a different value for UNICODE
will overwrite the previous value.

Note that the Unicode codepoints corresponding to the ASCII, Control-1,
and Latin-1 charsets are hard-wired.  Attempts to set these values
will raise an error.

C2 either must or may not be specified, depending on the dimension of
CHARSET (see `make-char').
*/
       (unicode, charset, c1, c2))
{
  int a1, a2;
  int ucp = decode_unicode (unicode);
  charset = get_external_charset_codepoint (charset, c1, c2, &a1, &a2, 0);
  
  /* The translations of ASCII, Control-1, and Latin-1 code points are
     hard-coded in ichar_to_unicode and unicode_to_ichar.

     #### But they shouldn't be; see comments elsewhere.

     Checking for all unicode < 256 && c1 | 0x80 != unicode is wrong
     because Mule gives many Latin characters code points in a few
     different character sets. */
  if ((EQ (charset, Vcharset_ascii) ||
       EQ (charset, Vcharset_latin_iso8859_1) ||
       EQ (charset, Vcharset_control_1)))
    {
      if (ucp != (a2 | 0x80))
	invalid_argument
	  ("Can't change Unicode translation for ASCII, Control-1 or Latin-1 character",
           unicode);
      return Qnil;
    }
  
  /* #### Composite characters are not properly implemented yet. */
  if (EQ (charset, Vcharset_composite))
    invalid_argument ("Can't set Unicode translation for Composite char",
		      unicode);

#ifdef ALLOW_ALGORITHMIC_CONVERSION_TABLES
  if (!XCHARSET_FROM_UNICODE_TABLE (charset))
    {
      text_checking_assert (XCHARSET_ALGO_LOW (charset) >= 0);
      invalid_argument
	("Can't set Unicode translation of charset with automatic translation",
	 charset);
    }
#endif

  set_unicode_conversion (ucp, charset, a1, a2);
  return Qnil;
}

/* "cerrar el fulano" = close the so-and-so */
static Lisp_Object
cerrar_el_fulano (Lisp_Object fulano)
{
  FILE *file = (FILE *) get_opaque_ptr (fulano);
  retry_fclose (file);
  return Qnil;
}

DEFUN ("load-unicode-mapping-table", Fload_unicode_mapping_table,
       2, 6, 0, /*
Load Unicode tables with the Unicode mapping data in FILENAME for CHARSET.
Data is text, in the form of one translation per line -- charset
codepoint followed by Unicode codepoint.  Numbers are decimal or hex
\(preceded by 0x).  Comments are marked with a #.  Charset codepoints
for two-dimensional charsets have the first octet stored in the
high 8 bits of the hex number and the second in the low 8 bits.

If START and END are given, only charset codepoints within the given
range will be processed.  (START and END apply to the codepoints in the
file, before OFFSET is applied.)

If OFFSET is given, that value will be added to all charset codepoints
in the file to obtain the internal charset codepoint.  \(We assume
that octets in the table are in the range 33 to 126 or 32 to 127.  If
you have a table in ku-ten form, with octets in the range 1 to 94, you
will have to use an offset of 5140, i.e. 0x2020.)

FLAGS, if specified, control further how the tables are interpreted
and are used to special-case certain known format deviations in the
Unicode tables or in the charset:

`ignore-first-column'
  The JIS X 0208 tables have 3 columns of data instead of 2.  The first
  column contains the Shift-JIS codepoint, which we ignore.
`big5'
  The charset codepoints are Big Five codepoints; convert it to the
  hacked-up Mule codepoint in `chinese-big5-1' or `chinese-big5-2'.
  Not when (featurep 'unicode-internal).
*/
     (filename, charset, start, end, offset, flags))
{
  int st = 0, en = INT_MAX, of = 0;
  FILE *file;
  struct gcpro gcpro1;
  char line[1025];
  int fondo = specpdl_depth (); /* "fondo" = depth */
  int ignore_first_column = 0;
#ifndef UNICODE_INTERNAL
  int big5 = 0;
#endif /* not UNICODE_INTERNAL */

  CHECK_STRING (filename);
  charset = Fget_charset (charset);
  if (!NILP (start))
    {
      CHECK_INT (start);
      st = XINT (start);
    }
  if (!NILP (end))
    {
      CHECK_INT (end);
      en = XINT (end);
    }
  if (!NILP (offset))
    {
      CHECK_INT (offset);
      of = XINT (offset);
    }

  if (!LISTP (flags))
    flags = list1 (flags);

  {
    EXTERNAL_LIST_LOOP_2 (elt, flags)
      {
	if (EQ (elt, Qignore_first_column))
	  ignore_first_column = 1;
#ifndef UNICODE_INTERNAL
	else if (EQ (elt, Qbig5))
	  {
	    big5 = 1;
	    /* At this point the charsets haven't been initialzied
	       yet, so at least set the values for big5-1 and big5-2
	       so we can use big5_char_to_fake_codepoint(). */
	    Vcharset_chinese_big5_1 = Fget_charset (Qchinese_big5_1);
	    Vcharset_chinese_big5_2 = Fget_charset (Qchinese_big5_2);
	  }
#endif /* not UNICODE_INTERNAL */
	else
	  invalid_constant
	    ("Unrecognized `load-unicode-mapping-table' flag", elt);
      }
  }

  GCPRO1 (filename);
  filename = Fexpand_file_name (filename, Qnil);
  file = qxe_fopen (XSTRING_DATA (filename), READ_TEXT);
  if (!file)
    report_file_error ("Cannot open", filename);
  record_unwind_protect (cerrar_el_fulano, make_opaque_ptr (file));
  while (fgets (line, sizeof (line), file))
    {
      char *p = line;
      int cp1, cp2, endcount;
      int cp1high, cp1low;
      int dummy;
      int scanf_count, garbage_after_scanf;

      while (*p) /* erase all comments out of the line */
	{
	  if (*p == '#')
	    *p = '\0';
	  else
	    p++;
	}
      /* see if line is nothing but whitespace and skip if so;
         count ^Z among this because it appears at the end of some
         Microsoft translation tables. */
      p = line + strspn (line, " \t\n\r\f\032");
      if (!*p)
	continue;
      /* NOTE: It appears that MS Windows and Newlib sscanf() have
	 different interpretations for whitespace (== "skip all whitespace
	 at processing point"): Newlib requires at least one corresponding
	 whitespace character in the input, but MS allows none.  The
	 following would be easier to write if we could count on the MS
	 interpretation.

	 Also, the return value does NOT include %n storage. */
      scanf_count =
	(!ignore_first_column ?
	 sscanf (p, "%i %i%n", &cp1, &cp2, &endcount) :
	 sscanf (p, "%i %i %i%n", &dummy, &cp1, &cp2, &endcount) - 1);
      /* #### Temporary code!  Cygwin newlib fucked up scanf() handling
	 of numbers beginning 0x0... starting in 04/2004, in an attempt
	 to fix another bug.  A partial fix for this was put in in
	 06/2004, but as of 10/2004 the value of ENDCOUNT returned in
	 such case is still wrong.  If this gets fixed soon, remove
	 this code. --ben */
      if (endcount > (int) strlen (p))
	/* We know we have a broken sscanf in this case!!! */
	garbage_after_scanf = 0;
      else
	{
#ifndef CYGWIN_SCANF_BUG
	  garbage_after_scanf =
	    *(p + endcount + strspn (p + endcount, " \t\n\r\f\032"));
#else
	  garbage_after_scanf = 0;
#endif
	}

      /* #### Hack.  A number of the CP###.TXT files from Microsoft contain
	 lines with a charset codepoint and no corresponding Unicode
	 codepoint, representing undefined values in the code page.

	 Skip them so we don't get a raft of warnings. */
      if (scanf_count == 1 && !garbage_after_scanf)
	continue;
      if (scanf_count < 2 || garbage_after_scanf)
	{
	  warn_when_safe (Qunicode, Qwarning,
			  "Unrecognized line in translation file %s:\n%s",
			  XSTRING_DATA (filename), line);
	  continue;
	}
      if (cp1 >= st && cp1 <= en)
	{
	  cp1 += of;
	  if (cp1 < 0 || cp1 >= 65536)
	    {
	    out_of_range:
	      warn_when_safe (Qunicode, Qwarning,
			      "Out of range first codepoint 0x%x in "
			      "translation file %s:\n%s",
			      cp1, XSTRING_DATA (filename), line);
	      continue;
	    }

	  cp1high = cp1 >> 8;
	  cp1low = cp1 & 255;

#ifndef UNICODE_INTERNAL
	  if (big5)
	    {
	      Lisp_Object fake_charset;
	      int c1, c2;
	      big5_char_to_fake_codepoint (cp1high, cp1low, &fake_charset,
					   &c1, &c2);
	      if (NILP (fake_charset))
		warn_when_safe (Qunicode, Qwarning,
				"Out of range Big5 codepoint 0x%x in "
				"translation file %s:\n%s",
				cp1, XSTRING_DATA (filename), line);
	      else
		set_unicode_conversion (cp2, fake_charset, c1, c2);
	    }
	  else
#endif /* not UNICODE_INTERNAL */
	    {
	      int l1, h1, l2, h2;
	      int c1 = cp1high, c2 = cp1low;

	      get_charset_limits (charset, &l1, &h1, &l2, &h2);

	      if (c1 < l1 || c1 > h1 || c2 < l2 || c2 > h2)
		goto out_of_range;

	      set_unicode_conversion (cp2, charset, c1, c2);
	    }
	}
    }

  if (ferror (file))
    report_file_error ("IO error when reading", filename);

  unbind_to (fondo); /* close file */
  UNGCPRO;
  return Qnil;
}

void
init_charset_unicode_map (Lisp_Object charset, Lisp_Object map)
{
  Lisp_Object savemap = map;

  CHECK_TRUE_LIST (map);
  if (STRINGP (XCAR (map)))
    {
      Lisp_Object filename = XCAR (map);
      Lisp_Object start = Qnil, end = Qnil, offset = Qnil, flags = Qnil;
      map = XCDR (map);
      if (!NILP (map))
	{
	  start = XCAR (map);
	  map = XCDR (map);
	}
      if (!NILP (map))
	{
	  end = XCAR (map);
	  map = XCDR (map);
	}
      if (!NILP (map))
	{
	  offset = XCAR (map);
	  map = XCDR (map);
	}
      if (!NILP (map))
	{
	  flags = XCAR (map);
	  map = XCDR (map);
	}
      if (!NILP (map))
	invalid_argument ("Unicode map can have at most 5 arguments",
			  savemap);
      Fload_unicode_mapping_table (filename, charset, start, end,
				   offset, flags);
    }
  else
    {
      EXTERNAL_LIST_LOOP_2 (entry, map)
	{
	  int len;
	  CHECK_TRUE_LIST (entry);
	  len = XINT (Flength (entry));
	  if (XCHARSET_DIMENSION (charset) == 1)
	    {
	      if (len != 2)
		invalid_argument ("Unicode map entry must have length 2 for dimension-1 charset", entry);
	      Fset_unicode_conversion (XCADR (entry), charset, XCAR (entry),
				       Qnil);
	    }
	  else
	    {
	      if (len != 3)
		invalid_argument ("Unicode map entry must have length 3 for dimension-1 charset", entry);
	      Fset_unicode_conversion (XCADDR (entry), charset, XCAR (entry),
				       XCADR (entry));
	    }
	}
    }

  /* Only set map after we have gone through everything and gotten
     no errors */
  XCHARSET_UNICODE_MAP (charset) = savemap;
}

#endif /* MULE */


/************************************************************************/
/*                      Properties of Unicode chars                     */
/************************************************************************/


int
unicode_char_columns (int code)
{
#if defined (HAVE_WCWIDTH) && defined (__STDC_ISO_10646__)
  return wcwidth ((wchar_t) code);
#else
  /* #### We need to do a much better job here.  Although maybe wcwidth()
     is available everywhere we care.  @@#### Copy the source for wcwidth().
     Also check under Windows for an equivalent. */
  /* #### Use a range table for this! */
  if (
      /* Tibetan */
      (code >= 0x0F00 && code <= 0x0FFF) ||
      /* Ethiopic, Ethiopic Supplement */
      (code >= 0x1200 && code <= 0x139F) ||
      /* Unified Canadian Aboriginal Syllabic */
      (code >= 0x1400 && code <= 0x167F) ||
      /* Ethiopic Extended */
      (code >= 0x2D80 && code <= 0x2DDF) ||
      /* Do not combine the previous range with this one, as
	 0x2E00 .. 0x2E7F is Supplemental Punctuation (Ancient Greek, etc.) */
      /* CJK Radicals Supplement ... Hangul Syllables */
      (code >= 0x2E80 && code <= 0xD7AF) ||
      /* CJK Compatibility Ideographs */
      (code >= 0xF900 && code <= 0xFAFF) ||
      /* CJK Compatibility Forms */
      (code >= 0xFE30 && code <= 0xFE4F) ||
      /* CJK Unified Ideographs Extension B, CJK Compatibility Ideographs
	 Supplement, any other crap in this region */
      (code >= 0x20000 && code <= 0x2FFFF))
    return 2;
  return 1;
#endif /* defined (HAVE_WCWIDTH) && defined (__STDC_ISO_10646__) */
}


/************************************************************************/
/*                         Unicode coding system                        */
/************************************************************************/

struct unicode_coding_system
{
  enum unicode_type type;
  unsigned int little_endian :1;
  unsigned int need_bom :1;
};

#define CODING_SYSTEM_UNICODE_TYPE(codesys) \
  (CODING_SYSTEM_TYPE_DATA (codesys, unicode)->type)
#define XCODING_SYSTEM_UNICODE_TYPE(codesys) \
  CODING_SYSTEM_UNICODE_TYPE (XCODING_SYSTEM (codesys))
#define CODING_SYSTEM_UNICODE_LITTLE_ENDIAN(codesys) \
  (CODING_SYSTEM_TYPE_DATA (codesys, unicode)->little_endian)
#define XCODING_SYSTEM_UNICODE_LITTLE_ENDIAN(codesys) \
  CODING_SYSTEM_UNICODE_LITTLE_ENDIAN (XCODING_SYSTEM (codesys))
#define CODING_SYSTEM_UNICODE_NEED_BOM(codesys) \
  (CODING_SYSTEM_TYPE_DATA (codesys, unicode)->need_bom)
#define XCODING_SYSTEM_UNICODE_NEED_BOM(codesys) \
  CODING_SYSTEM_UNICODE_NEED_BOM (XCODING_SYSTEM (codesys))

struct unicode_coding_stream
{
  /* decode */
  unsigned char counter;
  unsigned char indicated_length;
  int seen_char;
  int first_surrogate;
  unsigned int ch;
  /* encode */
  int wrote_bom;
};

static const struct memory_description unicode_coding_system_description[] = {
  { XD_END }
};

DEFINE_CODING_SYSTEM_TYPE_WITH_DATA (unicode);

/* Decode a UCS-2 or UCS-4 character (or -1 for error) into a buffer. */
static void
decode_unicode_char (int ch, unsigned_char_dynarr *dst,
		     struct unicode_coding_stream *data,
		     unsigned int ignore_bom)
{
  ASSERT_VALID_UNICODE_CODEPOINT (ch);
  if (ch == 0xFEFF && !data->seen_char && ignore_bom)
    ;
  else
    {
#ifdef MULE
      /* @@####
         FIXME: This following comment may not be correct, given the
         just-in-time Unicode character handling 
         [[
         #### If the lookup fails, we will currently use a replacement char
	 (e.g. <GETA MARK> (U+3013) of JIS X 0208).
	 #### Danger, Will Robinson!  Data loss. Should we signal user?
         ]]
       */
      Ichar chr = unicode_to_ichar (ch, get_unicode_precedence (),
				    CONVERR_SUCCEED);
      Dynarr_add_ichar (dst, chr);
#else
      if (ch < 256)
	Dynarr_add (dst, (Ibyte) ch);
      else
	/* This is OK since we are non-Mule and this becomes a single byte */
	Dynarr_add (dst, CANT_CONVERT_CHAR_WHEN_DECODING);
#endif /* MULE */
    }

  data->seen_char = 1;
}

#define DECODE_ERROR_OCTET(octet, dst, data, ignore_bom) \
  decode_unicode_char ((octet) + UNICODE_ERROR_OCTET_RANGE_START, \
                       dst, data, ignore_bom)

static inline void
indicate_invalid_utf_8 (unsigned char indicated_length,
                        unsigned char counter,
                        int ch, unsigned_char_dynarr *dst,
                        struct unicode_coding_stream *data,
                        unsigned int ignore_bom)
{
  Binbyte stored = indicated_length - counter; 
  Binbyte mask = "\x00\x00\xC0\xE0\xF0\xF8\xFC"[indicated_length];

  while (stored > 0)
    {
      DECODE_ERROR_OCTET (((ch >> (6 * (stored - 1))) & 0x3f) | mask,
                        dst, data, ignore_bom);
      mask = 0x80, stored--;
    }
}

inline static void
add_16_bit_char (int code, unsigned_char_dynarr *dst, int little_endian)
{
  if (little_endian)
    {
      Dynarr_add (dst, (unsigned char) (code & 255));
      Dynarr_add (dst, (unsigned char) ((code >> 8) & 255));
    }
  else
    {
      Dynarr_add (dst, (unsigned char) ((code >> 8) & 255));
      Dynarr_add (dst, (unsigned char) (code & 255));
    }
}

/* Also used in mule-coding.c for UTF-8 handling in ISO 2022-oriented
   encodings. */
void
encode_unicode_char (int code, unsigned_char_dynarr *dst,
		     enum unicode_type type, unsigned int little_endian,
                     int write_error_characters_as_such)
{
  ASSERT_VALID_UNICODE_CODEPOINT (code);
  switch (type)
    {
    case UNICODE_UTF_16:
      /* Handle surrogates */
      if (code < 0x10000)
	add_16_bit_char (code, dst, little_endian);
      else if (write_error_characters_as_such && 
	       code >= UNICODE_ERROR_OCTET_RANGE_START &&
	       code < (UNICODE_ERROR_OCTET_RANGE_START + 0x100))
	{
	  Dynarr_add (dst, (unsigned char) ((code & 0xFF)));
	}
      else if (code < 0x110000)
	{
	  int first, second;
	  
	  CODE_TO_UTF_16_SURROGATES (code, first, second);

	  add_16_bit_char (first, dst, little_endian);
	  add_16_bit_char (second, dst, little_endian);
	}
      else
	{
	  /* Not valid Unicode. Pass the replacement char (U+FFFD). */
	  add_16_bit_char (UNICODE_REPLACEMENT_CHAR, dst, little_endian);
	}
      break;

    case UNICODE_UCS_4:
    case UNICODE_UTF_32:
      if (little_endian)
	{
          if (write_error_characters_as_such && 
              code >= UNICODE_ERROR_OCTET_RANGE_START &&
              code < (UNICODE_ERROR_OCTET_RANGE_START + 0x100))
            {
              Dynarr_add (dst, (unsigned char) ((code & 0xFF)));
            }
          else
            {
              /* We generate and accept incorrect sequences here, which is
                 okay, in the interest of preservation of the user's
                 data.  */
              Dynarr_add (dst, (unsigned char) (code & 255));
              Dynarr_add (dst, (unsigned char) ((code >> 8) & 255));
              Dynarr_add (dst, (unsigned char) ((code >> 16) & 255));
              Dynarr_add (dst, (unsigned char) (code >> 24));
            }
	}
      else
	{
          if (write_error_characters_as_such && 
              code >= UNICODE_ERROR_OCTET_RANGE_START &&
              code < (UNICODE_ERROR_OCTET_RANGE_START + 0x100))
            {
              Dynarr_add (dst, (unsigned char) ((code & 0xFF)));
            }
          else
            {
              /* We generate and accept incorrect sequences here, which is okay,
                 in the interest of preservation of the user's data.  */
              Dynarr_add (dst, (unsigned char) (code >> 24));
              Dynarr_add (dst, (unsigned char) ((code >> 16) & 255));
              Dynarr_add (dst, (unsigned char) ((code >> 8) & 255));
              Dynarr_add (dst, (unsigned char) (code & 255));
            }
	}
      break;

    case UNICODE_UTF_8:
      {
	/* #### This code is duplicated in non_ascii_set_itext_ichar() in
	   text.c.  There should be a better way. */
	if (code <= 0x7f)
	  Dynarr_add (dst, (unsigned char) code);
	else
	  {
	    register int bytes;
	    register unsigned char *str;

	    if (code <= 0x7ff) bytes = 2;
	    else if (code <= 0xffff) bytes = 3;
	    else if (code <= 0x1fffff) bytes = 4;
	    else if (code <= 0x3ffffff)
             {
#if !(UNICODE_ERROR_OCTET_RANGE_START > 0x1fffff \
          && UNICODE_ERROR_OCTET_RANGE_START < 0x3ffffff)
#error "This code needs to be rewritten. " 
#endif
	       if (write_error_characters_as_such && 
		   code >= UNICODE_ERROR_OCTET_RANGE_START &&
		   code < (UNICODE_ERROR_OCTET_RANGE_START + 0x100))
		 {
		   Dynarr_add (dst, (unsigned char) ((code & 0xFF)));
		   break;
		 }
	       else
		 bytes = 5;
	     }
	    else bytes = 6;

	    Dynarr_add_many (dst, 0, bytes);
	    str = Dynarr_past_lastp (dst);
	    switch (bytes)
	      {
	      case 6:*--str = (code | 0x80) & 0xBF; code >>= 6;
	      case 5:*--str = (code | 0x80) & 0xBF; code >>= 6;
	      case 4:*--str = (code | 0x80) & 0xBF; code >>= 6;
	      case 3:*--str = (code | 0x80) & 0xBF; code >>= 6;
	      case 2:*--str = (code | 0x80) & 0xBF; code >>= 6;
	      case 1:*--str = code | firstbyte_mask[bytes];
	      }
	  }
  
	break;
      }

    case UNICODE_UTF_7: ABORT ();

    default: ABORT ();
    }
}

static Bytecount
unicode_convert (struct coding_stream *str, const UExtbyte *src,
		 unsigned_char_dynarr *dst, Bytecount n)
{
  struct unicode_coding_stream *data = CODING_STREAM_TYPE_DATA (str, unicode);
  enum unicode_type type =
    XCODING_SYSTEM_UNICODE_TYPE (str->codesys);
  unsigned int little_endian =
    XCODING_SYSTEM_UNICODE_LITTLE_ENDIAN (str->codesys);
  unsigned int ignore_bom = XCODING_SYSTEM_UNICODE_NEED_BOM (str->codesys);
  Bytecount orign = n;

  if (str->direction == CODING_DECODE)
    {
      unsigned char counter = data->counter;
      int ch    = data->ch;
      unsigned char indicated_length
        = data->indicated_length;

      while (n--)
	{
	  UExtbyte c = *src++;

	  /* #### This duplicates code elsewhere.  Maybe it is possible
	     to combine them efficiently. */
	  switch (type)
	    {
	    case UNICODE_UTF_8:
              if (0 == counter)
                {
                  if (0 == (c & 0x80))
                    {
                      /* ASCII. */
                      decode_unicode_char (c, dst, data, ignore_bom);
                    }
                  else if (0 == (c & 0x40))
                    {
                      /* Highest bit set, second highest not--there's
                         something wrong. */
                      DECODE_ERROR_OCTET (c, dst, data, ignore_bom);
                    }
                  else if (0 == (c & 0x20))
                    {
                      ch = c & 0x1f; 
                      counter = 1;
                      indicated_length = 2;
                    }
                  else if (0 == (c & 0x10))
                    {
                      ch = c & 0x0f;
                      counter = 2;
                      indicated_length = 3;
                    }
                  else if (0 == (c & 0x08))
                    {
                      ch = c & 0x0f;
                      counter = 3;
                      indicated_length = 4;
                    }
                  else
                    {
                      /* We don't supports lengths longer than 4 in
                         external-format data. */
                      DECODE_ERROR_OCTET (c, dst, data, ignore_bom);

                    }
                }
              else
                {
                  /* counter != 0 */
                  if ((0 == (c & 0x80)) || (0 != (c & 0x40)))
                    {
                      indicate_invalid_utf_8(indicated_length, 
                                             counter, 
                                             ch, dst, data, ignore_bom);
                      if (c & 0x80)
                        {
                          DECODE_ERROR_OCTET (c, dst, data, ignore_bom);
                        }
                      else
                        {
                          /* The character just read is ASCII. Treat it as
                             such.  */
                          decode_unicode_char (c, dst, data, ignore_bom);
                        }
                      ch = 0;
                      counter = 0;
                    }
                  else 
                    {
                      ch = (ch << 6) | (c & 0x3f);
                      counter--;
                      /* Just processed the final byte. Emit the character. */
                      if (!counter)
                        {
			  /* Don't accept over-long sequences, surrogates,
                             or codes above #x10FFFF. */
                          if ((ch < 0x80) ||
                              ((ch < 0x800) && indicated_length > 2) || 
                              ((ch < 0x10000) && indicated_length > 3) || 
                              valid_utf_16_surrogate(ch) || (ch > 0x110000))
                            {
                              indicate_invalid_utf_8(indicated_length, 
                                                     counter, 
                                                     ch, dst, data,
                                                     ignore_bom);
                            }
                          else
                            {
                              decode_unicode_char (ch, dst, data, ignore_bom);
                            }
                          ch = 0;
                        }
                    }
		}
	      break;

	    case UNICODE_UTF_16:

	      if (little_endian)
		ch = (c << counter) | ch;
	      else
		ch = (ch << 8) | c;

	      counter += 8;

	      if (16 == counter)
                {
		  int tempch = ch;

                  if (valid_utf_16_first_surrogate (ch))
                    {
                      break;
                    }
		  ch = 0;
		  counter = 0;
		  decode_unicode_char (tempch, dst, data, ignore_bom);
		}
	      else if (32 == counter)
		{
		  int tempch;

                  if (little_endian)
                    {
                      if (!valid_utf_16_last_surrogate (ch >> 16))
                        {
                          DECODE_ERROR_OCTET (ch & 0xFF, dst, data,
                                              ignore_bom);
                          DECODE_ERROR_OCTET ((ch >> 8) & 0xFF, dst, data,
                                              ignore_bom);
                          DECODE_ERROR_OCTET ((ch >> 16) & 0xFF, dst, data,
                                              ignore_bom);
                          DECODE_ERROR_OCTET ((ch >> 24) & 0xFF, dst, data,
                                              ignore_bom);
                        }
                      else
                        {
                          tempch = utf_16_surrogates_to_code ((ch & 0xffff),
							      (ch >> 16));
                          decode_unicode_char (tempch, dst, data, ignore_bom); 
                        }
                    }
                  else
                    {
                      if (!valid_utf_16_last_surrogate (ch & 0xFFFF))
                        {
                          DECODE_ERROR_OCTET ((ch >> 24) & 0xFF, dst, data,
                                              ignore_bom);
                          DECODE_ERROR_OCTET ((ch >> 16) & 0xFF, dst, data,
                                              ignore_bom);
                          DECODE_ERROR_OCTET ((ch >> 8) & 0xFF, dst, data,
                                              ignore_bom);
                          DECODE_ERROR_OCTET (ch & 0xFF, dst, data,
                                              ignore_bom);
                        }
                      else 
                        {
                          tempch = utf_16_surrogates_to_code ((ch >> 16), 
							      (ch & 0xffff));
                          decode_unicode_char (tempch, dst, data, ignore_bom); 
                        }
                    }

		  ch = 0;
		  counter = 0;
                }
              else
                assert (8 == counter || 24 == counter);
	      break;

	    case UNICODE_UCS_4:
            case UNICODE_UTF_32:
	      if (little_endian)
		ch = (c << counter) | ch;
	      else
		ch = (ch << 8) | c;
	      counter += 8;
	      if (counter == 32)
		{
		  if (ch > 0x10ffff)
		    {
                      /* ch is not a legal Unicode character. We're fine
                         with that in UCS-4, though not in UTF-32. */
                      if (UNICODE_UCS_4 == type &&
			  (unsigned long) ch < 0x80000000L)
                        {
                          decode_unicode_char (ch, dst, data, ignore_bom);
                        }
                      else if (little_endian)
                        {
                          DECODE_ERROR_OCTET (ch & 0xFF, dst, data, 
                                            ignore_bom);
                          DECODE_ERROR_OCTET ((ch >> 8) & 0xFF, dst, data,
                                            ignore_bom);
                          DECODE_ERROR_OCTET ((ch >> 16) & 0xFF, dst, data,
                                            ignore_bom);
                          DECODE_ERROR_OCTET ((ch >> 24) & 0xFF, dst, data,
                                            ignore_bom);
                        }
                      else
                        {
                          DECODE_ERROR_OCTET ((ch >> 24) & 0xFF, dst, data,
                                            ignore_bom);
                          DECODE_ERROR_OCTET ((ch >> 16) & 0xFF, dst, data,
                                            ignore_bom);
                          DECODE_ERROR_OCTET ((ch >> 8) & 0xFF, dst, data,
                                            ignore_bom);
                          DECODE_ERROR_OCTET (ch & 0xFF, dst, data, 
                                            ignore_bom);
                        }
		    }
                  else
                    {
                      decode_unicode_char (ch, dst, data, ignore_bom);
                    }
		  ch = 0;
		  counter = 0;
		}
	      break;

	    case UNICODE_UTF_7:
	      ABORT ();
	      break;

	    default: ABORT ();
	    }

	}

      if (str->eof && counter)
        {
          switch (type)
            {
	    case UNICODE_UTF_8:
              indicate_invalid_utf_8 (indicated_length, 
				      counter, ch, dst, data, 
				      ignore_bom);
              break;

            case UNICODE_UTF_16:
            case UNICODE_UCS_4:
            case UNICODE_UTF_32:
              if (8 == counter)
                {
                  DECODE_ERROR_OCTET (ch, dst, data, ignore_bom);
                }
              else if (16 == counter)
                {
                  if (little_endian)
                    {
                      DECODE_ERROR_OCTET (ch & 0xFF, dst, data, ignore_bom); 
                      DECODE_ERROR_OCTET ((ch >> 8) & 0xFF, dst, data,
                                          ignore_bom); 
                    }
                  else
                    {
                      DECODE_ERROR_OCTET ((ch >> 8) & 0xFF, dst, data,
                                          ignore_bom); 
                      DECODE_ERROR_OCTET (ch & 0xFF, dst, data, ignore_bom); 
                    }
                }
              else if (24 == counter)
                {
                  if (little_endian)
                    {
                      DECODE_ERROR_OCTET ((ch >> 16) & 0xFF, dst, data,
                                          ignore_bom);
                      DECODE_ERROR_OCTET (ch & 0xFF, dst, data, ignore_bom); 
                      DECODE_ERROR_OCTET ((ch >> 8) & 0xFF, dst, data,
                                          ignore_bom); 
                    }
                  else
                    {
                      DECODE_ERROR_OCTET ((ch >> 16) & 0xFF, dst, data,
                                          ignore_bom);
                      DECODE_ERROR_OCTET ((ch >> 8) & 0xFF, dst, data,
                                          ignore_bom); 
                      DECODE_ERROR_OCTET (ch & 0xFF, dst, data,
                                          ignore_bom); 
                    }
                }
              else assert(0);
              break;
            }
          ch = 0;
          counter = 0;
        }

      data->ch = ch;
      data->counter = counter;
      data->indicated_length = indicated_length;
    }
  else
    {
#ifdef ENABLE_COMPOSITE_CHARS
      /* flags for handling composite chars.  We do a little switcheroo
	 on the source while we're outputting the composite char. */
      Bytecount saved_n = 0;
      const Ibyte *saved_src = NULL;
      int in_composite = 0;

    back_to_square_n:
#endif /* ENABLE_COMPOSITE_CHARS */

      if (XCODING_SYSTEM_UNICODE_NEED_BOM (str->codesys) && !data->wrote_bom)
	{
	  encode_unicode_char (0xFEFF, dst, type, little_endian, 1);
	  data->wrote_bom = 1;
	}

      while (n--)
	{
	  Ibyte c = *src++;

#ifdef MULE
	  if (byte_ascii_p (c))
#endif /* MULE */
	    encode_unicode_char (c, dst, type, little_endian, 1);
#ifdef MULE
	  else
	    {
	      COPY_PARTIAL_CHAR_BYTE (c, str);
	      if (!str->pind_remaining)
		{
#ifdef UNICODE_INTERNAL
		  encode_unicode_char (non_ascii_itext_ichar (str->partial),
				       dst, type, little_endian, 1);
#else
		  Lisp_Object charset;
		  int c1, c2;
		  non_ascii_itext_to_charset_codepoint_raw (str->partial, 0,
							    &charset, &c1,
							    &c2);
#ifdef ENABLE_COMPOSITE_CHARS
		  if (EQ (charset, Vcharset_composite))
		    {
		      if (in_composite)
			{
			  /* #### Bother! We don't know how to
			     handle this yet. */
			  encode_unicode_char
			    (CANT_CONVERT_CHAR_WHEN_ENCODING_UNICODE,
			     dst, type, little_endian, 1);
			}
		      else
			{
			  Ichar emch =
			    charset_codepoint_to_unicode
			    (Vcharset_composite, c1, c2,
			     CONVERR_SUCCEED);
			  Lisp_Object lstr = composite_char_string (emch);
			  saved_n = n;
			  saved_src = src;
			  in_composite = 1;
			  src = XSTRING_DATA   (lstr);
			  n   = XSTRING_LENGTH (lstr);
			}
		    }
		  else
#endif /* ENABLE_COMPOSITE_CHARS */
		    {
		      int code =
			charset_codepoint_to_unicode
			(charset, c1, c2, CONVERR_SUCCEED);
		      encode_unicode_char (code, dst, type, little_endian, 1);
		    }
#endif /* UNICODE_INTERNAL */
		}
	    }
#endif /* MULE */
	}

#ifdef ENABLE_COMPOSITE_CHARS
      if (in_composite)
	{
	  n = saved_n;
	  src = saved_src;
	  in_composite = 0;
	  goto back_to_square_n; /* Wheeeeeeeee ..... */
	}
#endif /* ENABLE_COMPOSITE_CHARS */

      /* La palabra se hizo carne! */
      /* A palavra fez-se carne! */
      /* Whatever. */
    }

  return orign;
}

/* DEFINE_DETECTOR (utf_7); */
DEFINE_DETECTOR (utf_8);
DEFINE_DETECTOR_CATEGORY (utf_8, utf_8);
DEFINE_DETECTOR_CATEGORY (utf_8, utf_8_bom);
DEFINE_DETECTOR (ucs_4);
DEFINE_DETECTOR_CATEGORY (ucs_4, ucs_4);
DEFINE_DETECTOR (utf_16);
DEFINE_DETECTOR_CATEGORY (utf_16, utf_16);
DEFINE_DETECTOR_CATEGORY (utf_16, utf_16_little_endian);
DEFINE_DETECTOR_CATEGORY (utf_16, utf_16_bom);
DEFINE_DETECTOR_CATEGORY (utf_16, utf_16_little_endian_bom);

struct ucs_4_detector
{
  int in_ucs_4_byte;
};

static void
ucs_4_detect (struct detection_state *st, const UExtbyte *src,
	      Bytecount n)
{
  struct ucs_4_detector *data = DETECTION_STATE_DATA (st, ucs_4);

  while (n--)
    {
      UExtbyte c = *src++;
      switch (data->in_ucs_4_byte)
	{
	case 0:
	  if (c >= 128)
	    {
	      DET_RESULT (st, ucs_4) = DET_NEARLY_IMPOSSIBLE;
	      return;
	    }
	  else
	    data->in_ucs_4_byte++;
	  break;
	case 3:
	  data->in_ucs_4_byte = 0;
	  break;
	default:
	  data->in_ucs_4_byte++;
	}
    }

  /* !!#### write this for real */
  DET_RESULT (st, ucs_4) = DET_AS_LIKELY_AS_UNLIKELY;
}

struct utf_16_detector
{
  unsigned int seen_ffff:1;
  unsigned int seen_forward_bom:1;
  unsigned int seen_rev_bom:1;
  int byteno;
  int prev_char;
  int text, rev_text;
  int sep, rev_sep;
  int num_ascii;
};

static void
utf_16_detect (struct detection_state *st, const UExtbyte *src,
	       Bytecount n)
{
  struct utf_16_detector *data = DETECTION_STATE_DATA (st, utf_16);
  
  while (n--)
    {
      UExtbyte c = *src++;
      int prevc = data->prev_char;
      if (data->byteno == 1 && c == 0xFF && prevc == 0xFE)
	data->seen_forward_bom = 1;
      else if (data->byteno == 1 && c == 0xFE && prevc == 0xFF)
	data->seen_rev_bom = 1;

      if (data->byteno & 1)
	{
	  if (c == 0xFF && prevc == 0xFF)
	    data->seen_ffff = 1;
	  if (prevc == 0
	      && (c == '\r' || c == '\n'
		  || (c >= 0x20 && c <= 0x7E)))
	    data->text++;
	  if (c == 0
	      && (prevc == '\r' || prevc == '\n'
		  || (prevc >= 0x20 && prevc <= 0x7E)))
	    data->rev_text++;
	  /* #### 0x2028 is LINE SEPARATOR and 0x2029 is PARAGRAPH SEPARATOR.
	     I used to count these in text and rev_text but that is very bad,
	     as 0x2028 is also space + left-paren in ASCII, which is extremely
	     common.  So, what do we do with these? */
	  if (prevc == 0x20 && (c == 0x28 || c == 0x29))
	    data->sep++;
	  if (c == 0x20 && (prevc == 0x28 || prevc == 0x29))
	    data->rev_sep++;
	}

      if ((c >= ' ' && c <= '~') || c == '\n' || c == '\r' || c == '\t' ||
	  c == '\f' || c == '\v')
	data->num_ascii++;
      data->byteno++;
      data->prev_char = c;
    }

  {
    int variance_indicates_big_endian =
      (data->text >= 10
       && (data->rev_text == 0
	   || data->text / data->rev_text >= 10));
    int variance_indicates_little_endian =
      (data->rev_text >= 10
       && (data->text == 0
	   || data->rev_text / data->text >= 10));

    if (data->seen_ffff)
      SET_DET_RESULTS (st, utf_16, DET_NEARLY_IMPOSSIBLE);
    else if (data->seen_forward_bom)
      {
	SET_DET_RESULTS (st, utf_16, DET_NEARLY_IMPOSSIBLE);
	if (variance_indicates_big_endian)
	  DET_RESULT (st, utf_16_bom) = DET_NEAR_CERTAINTY;
	else if (variance_indicates_little_endian)
	  DET_RESULT (st, utf_16_bom) = DET_SOMEWHAT_LIKELY;
	else
	  DET_RESULT (st, utf_16_bom) = DET_QUITE_PROBABLE;
      }
    else if (data->seen_forward_bom)
      {
	SET_DET_RESULTS (st, utf_16, DET_NEARLY_IMPOSSIBLE);
	if (variance_indicates_big_endian)
	  DET_RESULT (st, utf_16_bom) = DET_NEAR_CERTAINTY;
	else if (variance_indicates_little_endian)
	  /* #### may need to rethink */
	  DET_RESULT (st, utf_16_bom) = DET_SOMEWHAT_LIKELY;
	else
	  /* #### may need to rethink */
	  DET_RESULT (st, utf_16_bom) = DET_QUITE_PROBABLE;
      }
    else if (data->seen_rev_bom)
      {
	SET_DET_RESULTS (st, utf_16, DET_NEARLY_IMPOSSIBLE);
	if (variance_indicates_little_endian)
	  DET_RESULT (st, utf_16_little_endian_bom) = DET_NEAR_CERTAINTY;
	else if (variance_indicates_big_endian)
	  /* #### may need to rethink */
	  DET_RESULT (st, utf_16_little_endian_bom) = DET_SOMEWHAT_LIKELY;
	else
	  /* #### may need to rethink */
	  DET_RESULT (st, utf_16_little_endian_bom) = DET_QUITE_PROBABLE;
      }
    else if (variance_indicates_big_endian)
      {
	SET_DET_RESULTS (st, utf_16, DET_NEARLY_IMPOSSIBLE);
	DET_RESULT (st, utf_16) = DET_SOMEWHAT_LIKELY;
	DET_RESULT (st, utf_16_little_endian) = DET_SOMEWHAT_UNLIKELY;
      }
    else if (variance_indicates_little_endian)
      {
	SET_DET_RESULTS (st, utf_16, DET_NEARLY_IMPOSSIBLE);
	DET_RESULT (st, utf_16) = DET_SOMEWHAT_UNLIKELY;
	DET_RESULT (st, utf_16_little_endian) = DET_SOMEWHAT_LIKELY;
      }
    else
      {
	/* #### FUCKME!  There should really be an ASCII detector.  This
	   would rule out the need to have this built-in here as
	   well. --ben */
	int pct_ascii = data->byteno ? (100 * data->num_ascii) / data->byteno
		        : 100;

	if (pct_ascii > 90)
	  SET_DET_RESULTS (st, utf_16, DET_QUITE_IMPROBABLE);
	else if (pct_ascii > 75)
	  SET_DET_RESULTS (st, utf_16, DET_SOMEWHAT_UNLIKELY);
	else
	  SET_DET_RESULTS (st, utf_16, DET_AS_LIKELY_AS_UNLIKELY);
      }
  }
}

struct utf_8_detector
{
  int byteno;
  int first_byte;
  int second_byte;
  int prev_byte;
  int in_utf_8_byte;
  int recent_utf_8_sequence;
  int seen_bogus_utf8;
  int seen_really_bogus_utf8;
  int seen_2byte_sequence;
  int seen_longer_sequence;
  int seen_iso2022_esc;
  int seen_iso_shift;
  unsigned int seen_utf_bom:1;
};

static void
utf_8_detect (struct detection_state *st, const UExtbyte *src,
 	      Bytecount n)
{
  struct utf_8_detector *data = DETECTION_STATE_DATA (st, utf_8);

  while (n--)
    {
      UExtbyte c = *src++;
      switch (data->byteno)
	{
	case 0:
	  data->first_byte = c;
	  break;
	case 1:
	  data->second_byte = c;
	  break;
	case 2:
	  if (data->first_byte == 0xef &&
	      data->second_byte == 0xbb &&
	      c == 0xbf)
	    data->seen_utf_bom = 1;
	  break;
	}

      switch (data->in_utf_8_byte)
	{
	case 0:
	  if (data->prev_byte == ISO_CODE_ESC && c >= 0x28 && c <= 0x2F)
	    data->seen_iso2022_esc++;
	  else if (c == ISO_CODE_SI || c == ISO_CODE_SO)
	    data->seen_iso_shift++;
	  else if (c >= 0xfc)
	    data->in_utf_8_byte = 5;
	  else if (c >= 0xf8)
	    data->in_utf_8_byte = 4;
	  else if (c >= 0xf0)
	    data->in_utf_8_byte = 3;
	  else if (c >= 0xe0)
	    data->in_utf_8_byte = 2;
	  else if (c >= 0xc0)
	    data->in_utf_8_byte = 1;
	  else if (c >= 0x80)
	    data->seen_bogus_utf8++;
	  if (data->in_utf_8_byte > 0)
	    data->recent_utf_8_sequence = data->in_utf_8_byte;
	  break;
	default:
	  if ((c & 0xc0) != 0x80)
	    data->seen_really_bogus_utf8++;
	  else
	    {
	      data->in_utf_8_byte--;
	      if (data->in_utf_8_byte == 0)
		{
		  if (data->recent_utf_8_sequence == 1)
		    data->seen_2byte_sequence++;
		  else
		    {
		      assert (data->recent_utf_8_sequence >= 2);
		      data->seen_longer_sequence++;
		    }
		}
	    }
	}

      data->byteno++;
      data->prev_byte = c;
    }

  /* either BOM or no BOM, but not both */
  SET_DET_RESULTS (st, utf_8, DET_NEARLY_IMPOSSIBLE);


  if (data->seen_utf_bom)
    DET_RESULT (st, utf_8_bom) = DET_NEAR_CERTAINTY;
  else
    {
      if (data->seen_really_bogus_utf8 ||
	  data->seen_bogus_utf8 >= 2)
	; /* bogus */
      else if (data->seen_bogus_utf8)
	DET_RESULT (st, utf_8) = DET_SOMEWHAT_UNLIKELY;
      else if ((data->seen_longer_sequence >= 5 ||
		data->seen_2byte_sequence >= 10) &&
	       (!(data->seen_iso2022_esc + data->seen_iso_shift) ||
		(data->seen_longer_sequence * 2 + data->seen_2byte_sequence) /
		(data->seen_iso2022_esc + data->seen_iso_shift) >= 10))
	/* heuristics, heuristics, we love heuristics */
	DET_RESULT (st, utf_8) = DET_QUITE_PROBABLE;
      else if (data->seen_iso2022_esc ||
	       data->seen_iso_shift >= 3)
	DET_RESULT (st, utf_8) = DET_SOMEWHAT_UNLIKELY;
      else if (data->seen_longer_sequence ||
	       data->seen_2byte_sequence)
	DET_RESULT (st, utf_8) = DET_SOMEWHAT_LIKELY;
      else if (data->seen_iso_shift)
	DET_RESULT (st, utf_8) = DET_SOMEWHAT_UNLIKELY;
      else
	DET_RESULT (st, utf_8) = DET_AS_LIKELY_AS_UNLIKELY;
    }
}

static void
unicode_init_coding_stream (struct coding_stream *str)
{
  struct unicode_coding_stream *data =
    CODING_STREAM_TYPE_DATA (str, unicode);
  xzero (*data);
}

static void
unicode_rewind_coding_stream (struct coding_stream *str)
{
  unicode_init_coding_stream (str);
}

static int
unicode_putprop (Lisp_Object codesys, Lisp_Object key, Lisp_Object value)
{
  if (EQ (key, Qunicode_type))
    {
      enum unicode_type type;

      if (EQ (value, Qutf_8))
	type = UNICODE_UTF_8;
      else if (EQ (value, Qutf_16))
	type = UNICODE_UTF_16;
      else if (EQ (value, Qutf_7))
	type = UNICODE_UTF_7;
      else if (EQ (value, Qucs_4))
	type = UNICODE_UCS_4;
      else if (EQ (value, Qutf_32))
	type = UNICODE_UTF_32;
      else
	invalid_constant ("Invalid Unicode type", key);
      
      XCODING_SYSTEM_UNICODE_TYPE (codesys) = type;
    }
  else if (EQ (key, Qlittle_endian))
    XCODING_SYSTEM_UNICODE_LITTLE_ENDIAN (codesys) = !NILP (value);
  else if (EQ (key, Qneed_bom))
    XCODING_SYSTEM_UNICODE_NEED_BOM (codesys) = !NILP (value);
  else
    return 0;
  return 1;
}

static Lisp_Object
unicode_getprop (Lisp_Object coding_system, Lisp_Object prop)
{
  if (EQ (prop, Qunicode_type))
    {
      switch (XCODING_SYSTEM_UNICODE_TYPE (coding_system))
	{
	case UNICODE_UTF_16: return Qutf_16;
	case UNICODE_UTF_8: return Qutf_8;
	case UNICODE_UTF_7: return Qutf_7;
	case UNICODE_UCS_4: return Qucs_4;
	case UNICODE_UTF_32: return Qutf_32;
	default: ABORT ();
	}
    }
  else if (EQ (prop, Qlittle_endian))
    return XCODING_SYSTEM_UNICODE_LITTLE_ENDIAN (coding_system) ? Qt : Qnil;
  else if (EQ (prop, Qneed_bom))
    return XCODING_SYSTEM_UNICODE_NEED_BOM (coding_system) ? Qt : Qnil;
  return Qunbound;
}

static void
unicode_print (Lisp_Object cs, Lisp_Object printcharfun,
	       int UNUSED (escapeflag))
{
  write_fmt_string_lisp (printcharfun, "(%s", 1,
                         unicode_getprop (cs, Qunicode_type));
  if (XCODING_SYSTEM_UNICODE_LITTLE_ENDIAN (cs))
    write_c_string (printcharfun, ", little-endian");
  if (XCODING_SYSTEM_UNICODE_NEED_BOM (cs))
    write_c_string (printcharfun, ", need-bom");
  write_c_string (printcharfun, ")");
}

#ifdef MULE
DEFUN ("set-unicode-query-skip-chars-args", Fset_unicode_query_skip_chars_args,
       3, 3, 0, /*
Specify strings as matching characters known to Unicode coding systems.

QUERY-STRING is a string matching characters that can unequivocally be
encoded by the Unicode coding systems.

INVALID-STRING is a string to match XEmacs characters that represent known
octets on disk, but that are invalid sequences according to Unicode. 

UTF-8-INVALID-STRING is a more restrictive string to match XEmacs characters
that are invalid UTF-8 octets.

All three strings are in the format accepted by `skip-chars-forward'. 
*/
       (query_string, invalid_string, utf_8_invalid_string))
{
  CHECK_STRING (query_string);
  CHECK_STRING (invalid_string);
  CHECK_STRING (utf_8_invalid_string);

  Vunicode_query_string = query_string;
  Vunicode_invalid_string = invalid_string;
  Vutf_8_invalid_string = utf_8_invalid_string;

  return Qnil;
}

static void
add_lisp_string_to_skip_chars_range (Lisp_Object string, Lisp_Object rtab,
                                     Lisp_Object value)
{
  Ibyte *p, *pend;
  Ichar c;

  p = XSTRING_DATA (string);
  pend = p + XSTRING_LENGTH (string);

  while (p != pend)
    {
      c = itext_ichar (p);

      INC_IBYTEPTR (p);

      if (c == '\\')
        {
          if (p == pend) break;
          c = itext_ichar (p);
          INC_IBYTEPTR (p);
        }

      if (p != pend && *p == '-')
        {
          Ichar cend;

          /* Skip over the dash.  */
          p++;
          if (p == pend) break;
          cend = itext_ichar (p);

          Fput_range_table (make_int (c), make_int (cend), value,
                            rtab);

          INC_IBYTEPTR (p);
        }
      else
        {
          Fput_range_table (make_int (c), make_int (c), value, rtab);
        }
    }
}

/* This function wouldn't be necessary if initialised range tables were
   dumped properly; see
   http://mid.gmane.org/18179.49815.622843.336527@parhasard.net . */
static void
initialize_unicode_query_range_tables_from_strings (void)
{
  CHECK_STRING (Vunicode_query_string);
  CHECK_STRING (Vunicode_invalid_string);
  CHECK_STRING (Vutf_8_invalid_string);

  Vunicode_query_skip_chars = Fmake_range_table (Qstart_closed_end_closed);

  add_lisp_string_to_skip_chars_range (Vunicode_query_string,
                                       Vunicode_query_skip_chars,
                                       Qsucceeded);

  Vunicode_invalid_and_query_skip_chars
    = Fcopy_range_table (Vunicode_query_skip_chars);

  add_lisp_string_to_skip_chars_range (Vunicode_invalid_string,
                                       Vunicode_invalid_and_query_skip_chars,
                                       Qinvalid_sequence);

  Vutf_8_invalid_and_query_skip_chars
    = Fcopy_range_table (Vunicode_query_skip_chars);

  add_lisp_string_to_skip_chars_range (Vutf_8_invalid_string,
                                       Vutf_8_invalid_and_query_skip_chars, 
                                       Qinvalid_sequence);
}

static Lisp_Object
unicode_query (Lisp_Object codesys, struct buffer *buf, Charbpos end,
               int flags)
{
  Charbpos pos = BUF_PT (buf), fail_range_start, fail_range_end;
  Charbpos pos_byte = BYTE_BUF_PT (buf);
  Lisp_Object skip_chars_range_table, result = Qnil;
  enum query_coding_failure_reasons failed_reason,
    previous_failed_reason = query_coding_succeeded;
  int checked_unicode,
    invalid_lower_limit = UNICODE_ERROR_OCTET_RANGE_START,
    invalid_upper_limit = -1,
    unicode_type = XCODING_SYSTEM_UNICODE_TYPE (codesys);

  if (flags & QUERY_METHOD_HIGHLIGHT && 
      /* If we're being called really early, live without highlights getting
         cleared properly: */
      !(UNBOUNDP (XSYMBOL (Qquery_coding_clear_highlights)->function)))
    {
      /* It's okay to call Lisp here, the only non-stack object we may have
         allocated up to this point is skip_chars_range_table, and that's
         reachable from its entry in Vfixed_width_query_ranges_cache. */
      call3 (Qquery_coding_clear_highlights, make_int (pos), make_int (end),
             wrap_buffer (buf));
    }

  if (NILP (Vunicode_query_skip_chars))
    {
      initialize_unicode_query_range_tables_from_strings();
    }

  if (flags & QUERY_METHOD_IGNORE_INVALID_SEQUENCES)
    {
      switch (unicode_type)
        {
        case UNICODE_UTF_8:
          skip_chars_range_table = Vutf_8_invalid_and_query_skip_chars;
          break;
        case UNICODE_UTF_7:
          /* #### See above. */
          return Qunbound;
          break;
        default:
          skip_chars_range_table = Vunicode_invalid_and_query_skip_chars;
          break;
        }
    }
  else
    {
      switch (unicode_type)
        {
        case UNICODE_UTF_8:
          invalid_lower_limit = UNICODE_ERROR_OCTET_RANGE_START + 0x80;
          invalid_upper_limit = UNICODE_ERROR_OCTET_RANGE_START + 0xFF;
          break;
        case UNICODE_UTF_7:
          /* #### Work out what to do here in reality, read the spec and decide
             which octets are invalid. */
          return Qunbound;
          break;
        default:
          invalid_lower_limit = UNICODE_ERROR_OCTET_RANGE_START;
          invalid_upper_limit = UNICODE_ERROR_OCTET_RANGE_START + 0xFF;
          break;
        }

      skip_chars_range_table = Vunicode_query_skip_chars;
    }

  while (pos < end)
    {
      Ichar ch = BYTE_BUF_FETCH_CHAR (buf, pos_byte);
      if ((ch < 0x100 ? 1 : 
           (!EQ (Qnil, Fget_range_table (make_int (ch), skip_chars_range_table,
                                         Qnil)))))
        {
          pos++;
          INC_BYTEBPOS (buf, pos_byte);
        }
      else
        {
          fail_range_start = pos;
          while ((pos < end) &&
		 /* @@#### Is CONVERR_FAIL correct? */
                 ((checked_unicode = ichar_to_unicode (ch, CONVERR_FAIL),
                   -1 == checked_unicode
                   && (failed_reason = query_coding_unencodable))
                  || (!(flags & QUERY_METHOD_IGNORE_INVALID_SEQUENCES) &&
                      (invalid_lower_limit <= checked_unicode) &&
                      (checked_unicode <= invalid_upper_limit)
                      && (failed_reason = query_coding_invalid_sequence)))
                 && (previous_failed_reason == query_coding_succeeded
                     || previous_failed_reason == failed_reason))
            {
              pos++;
              INC_BYTEBPOS (buf, pos_byte);
              ch = BYTE_BUF_FETCH_CHAR (buf, pos_byte);
              previous_failed_reason = failed_reason;
            }

          if (fail_range_start == pos)
            {
              /* The character can actually be encoded; move on. */
              pos++;
              INC_BYTEBPOS (buf, pos_byte);
            }
          else
            {
              assert (previous_failed_reason == query_coding_invalid_sequence
                      || previous_failed_reason == query_coding_unencodable);

              if (flags & QUERY_METHOD_ERRORP)
                {
                  DECLARE_EISTRING (error_details);

                  eicpy_ascii (error_details, "Cannot encode ");
                  eicat_lstr (error_details,
                              make_string_from_buffer (buf, fail_range_start, 
                                                       pos -
                                                       fail_range_start));
                  eicat_ascii (error_details, " using coding system");

                  text_conversion_error
		    ((const CIbyte *)(eidata (error_details)),
		     XCODING_SYSTEM_NAME (codesys));
                }

              if (NILP (result))
                {
                  result = Fmake_range_table (Qstart_closed_end_open);
                }

              fail_range_end = pos;

              Fput_range_table (make_int (fail_range_start), 
                                make_int (fail_range_end),
                                (previous_failed_reason
                                 == query_coding_unencodable ?
                                 Qunencodable : Qinvalid_sequence), 
                                result);
              previous_failed_reason = query_coding_succeeded;

              if (flags & QUERY_METHOD_HIGHLIGHT) 
                {
                  Lisp_Object extent
                    = Fmake_extent (make_int (fail_range_start),
                                    make_int (fail_range_end), 
                                    wrap_buffer (buf));
                  
                  Fset_extent_priority
                    (extent, make_int (2 + mouse_highlight_priority));
                  Fset_extent_face (extent, Qquery_coding_warning_face);
                }
            }
        }
    }

  return result;
}
#else /* !MULE */
static Lisp_Object
unicode_query (Lisp_Object UNUSED (codesys),
               struct buffer * UNUSED (buf),
               Charbpos UNUSED (end), int UNUSED (flags))
{
  return Qnil;
}
#endif

int
dfc_coding_system_is_unicode (
#ifdef WIN32_ANY
			      Lisp_Object codesys
#else
			      Lisp_Object UNUSED (codesys)
#endif
			      )
{
#ifdef WIN32_ANY
  codesys = Fget_coding_system (codesys);
  return (EQ (XCODING_SYSTEM_TYPE (codesys), Qunicode) &&
	  XCODING_SYSTEM_UNICODE_TYPE (codesys) == UNICODE_UTF_16 &&
	  XCODING_SYSTEM_UNICODE_LITTLE_ENDIAN (codesys));
	      
#else
  return 0;
#endif
}


/************************************************************************/
/*                             Initialization                           */
/************************************************************************/

#ifdef MULE

void
initialize_ascii_control_1_latin_1_unicode_translation (void)
{
  int i;

  for (i = 0; i < 128; i++)
    set_unicode_conversion (i, Vcharset_ascii, 0, i);
  for (i = 128; i < 160; i++)
    set_unicode_conversion (i, Vcharset_control_1, 0, i);
  for (i = 160; i < 256; i++)
    set_unicode_conversion (i, Vcharset_latin_iso8859_1, 0, i);
}

#endif

void
syms_of_unicode (void)
{
#ifdef MULE
  DEFSUBR (Funicode_precedence_list);
  DEFSUBR (Fset_language_unicode_precedence_list);
  DEFSUBR (Flanguage_unicode_precedence_list);
  DEFSUBR (Fset_default_unicode_precedence_list);
  DEFSUBR (Fdefault_unicode_precedence_list);
  DEFSUBR (Fset_unicode_conversion);

  DEFSUBR (Fload_unicode_mapping_table);

  DEFSUBR (Fset_unicode_query_skip_chars_args);

  DEFSYMBOL (Qignore_first_column);
  DEFSYMBOL (Qunicode_registries);
#endif /* MULE */

  DEFSYMBOL (Qunicode);
  DEFSYMBOL (Qucs_4);
  DEFSYMBOL (Qutf_16);
  DEFSYMBOL (Qutf_32);
  DEFSYMBOL (Qutf_8);
  DEFSYMBOL (Qutf_7);

  DEFSYMBOL (Qneed_bom);

  DEFSYMBOL (Qutf_16);
  DEFSYMBOL (Qutf_16_little_endian);
  DEFSYMBOL (Qutf_16_bom);
  DEFSYMBOL (Qutf_16_little_endian_bom);

  DEFSYMBOL (Qutf_8);
  DEFSYMBOL (Qutf_8_bom);
}

void
coding_system_type_create_unicode (void)
{
  INITIALIZE_CODING_SYSTEM_TYPE_WITH_DATA (unicode, "unicode-coding-system-p");
  CODING_SYSTEM_HAS_METHOD (unicode, print);
  CODING_SYSTEM_HAS_METHOD (unicode, convert);
  CODING_SYSTEM_HAS_METHOD (unicode, query);
  CODING_SYSTEM_HAS_METHOD (unicode, init_coding_stream);
  CODING_SYSTEM_HAS_METHOD (unicode, rewind_coding_stream);
  CODING_SYSTEM_HAS_METHOD (unicode, putprop);
  CODING_SYSTEM_HAS_METHOD (unicode, getprop);

  INITIALIZE_DETECTOR (utf_8);
  DETECTOR_HAS_METHOD (utf_8, detect);
  INITIALIZE_DETECTOR_CATEGORY (utf_8, utf_8);
  INITIALIZE_DETECTOR_CATEGORY (utf_8, utf_8_bom);

  INITIALIZE_DETECTOR (ucs_4);
  DETECTOR_HAS_METHOD (ucs_4, detect);
  INITIALIZE_DETECTOR_CATEGORY (ucs_4, ucs_4);

  INITIALIZE_DETECTOR (utf_16);
  DETECTOR_HAS_METHOD (utf_16, detect);
  INITIALIZE_DETECTOR_CATEGORY (utf_16, utf_16);
  INITIALIZE_DETECTOR_CATEGORY (utf_16, utf_16_little_endian);
  INITIALIZE_DETECTOR_CATEGORY (utf_16, utf_16_bom);
  INITIALIZE_DETECTOR_CATEGORY (utf_16, utf_16_little_endian_bom);
}

void
reinit_coding_system_type_create_unicode (void)
{
  REINITIALIZE_CODING_SYSTEM_TYPE (unicode);
}

void
vars_of_unicode (void)
{
  Fprovide (intern ("unicode"));

#ifdef MULE
#ifndef UNICODE_INTERNAL
  staticpro (&Vnumber_of_jit_charsets);
  Vnumber_of_jit_charsets = Qzero;
  staticpro (&Vcurrent_jit_charset);
  Vcurrent_jit_charset = Qnil;
  staticpro (&Vlast_allocated_jit_c1);
  Vlast_allocated_jit_c1 = Qzero;
  staticpro (&Vlast_allocated_jit_c2);
  Vlast_allocated_jit_c2 = Qzero;
  staticpro (&Vcharset_descr);
  Vcharset_descr
    = build_msg_string ("Mule charset for otherwise unknown Unicode code points.");
#endif
  staticpro (&Vlanguage_unicode_precedence_list);
  Vlanguage_unicode_precedence_list = Qnil;

  staticpro (&Vdefault_unicode_precedence_list);
  Vdefault_unicode_precedence_list = Qnil;

  staticpro (&Vprecedence_list_charsets_seen_hash);
  Vprecedence_list_charsets_seen_hash =
    make_lisp_hash_table (20, HASH_TABLE_NON_WEAK, HASH_TABLE_EQ);

  global_unicode_precedence_dynarr = Dynarr_new (Lisp_Object);
  dump_add_root_block_ptr (&global_unicode_precedence_dynarr,
			    &Lisp_Object_dynarr_description);

  
  
  init_blank_unicode_tables ();

  /* Note that the "block" we are describing is a single pointer, and hence
     we could potentially use dump_add_root_block_ptr().  However, given
     the way the descriptions are written, we couldn't use them, and would
     have to write new descriptions for each of the pointers below, since
     we would have to make use of a description with an XD_BLOCK_ARRAY
     in it. */

  dump_add_root_block (&to_unicode_blank_1, sizeof (void *),
		       to_unicode_level_1_desc_1);
  dump_add_root_block (&to_unicode_blank_2, sizeof (void *),
		       to_unicode_level_2_desc_1);

  dump_add_root_block (&from_unicode_blank[1], sizeof (void *),
		       from_unicode_level_1_desc_1);
  dump_add_root_block (&from_unicode_blank[2], sizeof (void *),
		       from_unicode_level_2_desc_1);
  dump_add_root_block (&from_unicode_blank[3], sizeof (void *),
		       from_unicode_level_3_desc_1);
  dump_add_root_block (&from_unicode_blank[4], sizeof (void *),
		       from_unicode_level_4_desc_1);

  DEFVAR_LISP ("unicode-registries", &Qunicode_registries /*
Vector describing the X11 registries searched when using fallback fonts.

"Fallback fonts" here includes by default those fonts used by redisplay
when displaying JIT charsets (used in non-Unicode-internal for holding
Unicode codepoints that can't be otherwise represented), and those used
when no font matching the charset's registries property has been found
(that is, they're probably Mule-specific charsets like Ethiopic or IPA).
*/ );
  Qunicode_registries = vector1 (build_string ("iso10646-1"));

  /* Initialised in lisp/mule/general-late.el, by a call to
     #'set-unicode-query-skip-chars-args. Or at least they would be, but we
     can't do this at dump time right now, initialised range tables aren't
     dumped properly. */
  staticpro (&Vunicode_invalid_and_query_skip_chars);
  Vunicode_invalid_and_query_skip_chars = Qnil;
  staticpro (&Vutf_8_invalid_and_query_skip_chars);
  Vutf_8_invalid_and_query_skip_chars = Qnil;
  staticpro (&Vunicode_query_skip_chars);
  Vunicode_query_skip_chars = Qnil;

  /* If we could dump the range table above these wouldn't be necessary: */
  staticpro (&Vunicode_query_string);
  Vunicode_query_string = Qnil;
  staticpro (&Vunicode_invalid_string);
  Vunicode_invalid_string = Qnil;
  staticpro (&Vutf_8_invalid_string);
  Vutf_8_invalid_string = Qnil;
#endif /* MULE */
}

void
complex_vars_of_unicode (void)
{
  /* We used to define this in unicode.el.  But we need it early for
     Cygwin 1.7 -- used in LOCAL_FILE_FORMAT_TO_TSTR() et al. */
  Fmake_coding_system_internal
    (Qutf_8, Qunicode,
     build_msg_string ("UTF-8"),
     nconc2 (list4 (Qdocumentation,
		    build_msg_string (
"UTF-8 Unicode encoding -- ASCII-compatible 8-bit variable-width encoding\n"
"sharing the following principles with the Mule-internal encoding:\n"
"\n"
"  -- All ASCII characters (codepoints 0 through 127) are represented\n"
"     by themselves (i.e. using one byte, with the same value as the\n"
"     ASCII codepoint), and these bytes are disjoint from bytes\n"
"     representing non-ASCII characters.\n"
"\n"
"     This means that any 8-bit clean application can safely process\n"
"     UTF-8-encoded text as it were ASCII, with no corruption (e.g. a\n"
"     '/' byte is always a slash character, never the second byte of\n"
"     some other character, as with Big5, so a pathname encoded in\n"
"     UTF-8 can safely be split up into components and reassembled\n"
"     again using standard ASCII processes).\n"
"\n"
"  -- Leading bytes and non-leading bytes in the encoding of a\n"
"     character are disjoint, so moving backwards is easy.\n"
"\n"
"  -- Given only the leading byte, you know how many following bytes\n"
"     are present.\n"
),
		    Qmnemonic, build_string ("UTF8")),
	     list2 (Qunicode_type, Qutf_8)));
}
