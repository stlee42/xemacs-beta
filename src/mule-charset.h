/* Header for multilingual functions.
   Copyright (C) 1992, 1995 Free Software Foundation, Inc.
   Copyright (C) 1995 Sun Microsystems, Inc.

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

/* Synched up with: Mule 2.3.  Not in FSF. */

/* Rewritten by Ben Wing <ben@xemacs.org>. */

#ifndef _XEMACS_MULE_CHARSET_H
#define _XEMACS_MULE_CHARSET_H

/*
   1. Character Sets
   =================

   A character set (or "charset") is an ordered set of characters.
   A particular character in a charset is indexed using one or
   more "position codes", which are non-negative integers.
   The number of position codes needed to identify a particular
   character in a charset is called the "dimension" of the
   charset.  In XEmacs/Mule, all charsets have 1 or 2 dimensions,
   and the size of all charsets (except for a few special cases)
   is either 94, 96, 94 by 94, or 96 by 96.  The range of
   position codes used to index characters from any of these
   types of character sets is as follows:

   Charset type		Position code 1		Position code 2
   ------------------------------------------------------------
   94			33 - 126		N/A
   96			32 - 127		N/A
   94x94		33 - 126		33 - 126
   96x96		32 - 127		32 - 127

   Note that in the above cases position codes do not start at
   an expected value such as 0 or 1.  The reason for this will
   become clear later.

   For example, Latin-1 is a 96-character charset, and JISX0208
   (the Japanese national character set) is a 94x94-character
   charset.

   [Note that, although the ranges above define the *valid*
   position codes for a charset, some of the slots in a particular
   charset may in fact be empty.  This is the case for JISX0208,
   for example, where (e.g.) all the slots whose first
   position code is in the range 118 - 127 are empty.]

   There are three charsets that do not follow the above rules.
   All of them have one dimension, and have ranges of position
   codes as follows:

   Charset name		Position code 1
   ------------------------------------
   ASCII		0 - 127
   Control-1		0 - 31
   Composite		0 - some large number

   (The upper bound of the position code for composite characters
   has not yet been determined, but it will probably be at
   least 16,383).

   ASCII is the union of two subsidiary character sets:
   Printing-ASCII (the printing ASCII character set,
   consisting of position codes 33 - 126, like for a standard
   94-character charset) and Control-ASCII (the non-printing
   characters that would appear in a binary file with codes 0
   - 32 and 127).

   Control-1 contains the non-printing characters that would
   appear in a binary file with codes 128 - 159.

   Composite contains characters that are generated by
   overstriking one or more characters from other charsets.

   Note that some characters in ASCII, and all characters
   in Control-1, are "control" (non-printing) characters.
   These have no printed representation but instead control
   some other function of the printing (e.g. TAB or 8 moves
   the current character position to the next tab stop).
   All other characters in all charsets are "graphic"
   (printing) characters.

   When a binary file is read in, the bytes in the file are
   assigned to character sets as follows:

   Bytes		Character set		Range
   --------------------------------------------------
   0 - 127		ASCII			0 - 127
   128 - 159		Control-1		0 - 31
   160 - 255		Latin-1			32 - 127

   This is a bit ad-hoc but gets the job done.

   2. Encodings
   ============

   An "encoding" is a way of numerically representing
   characters from one or more character sets.  If an encoding
   only encompasses one character set, then the position codes
   for the characters in that character set could be used
   directly.  This is not possible, however, if more than one
   character set is to be used in the encoding.

   For example, the conversion detailed above between bytes in
   a binary file and characters is effectively an encoding
   that encompasses the three character sets ASCII, Control-1,
   and Latin-1 in a stream of 8-bit bytes.

   Thus, an encoding can be viewed as a way of encoding
   characters from a specified group of character sets using a
   stream of bytes, each of which contains a fixed number of
   bits (but not necessarily 8, as in the common usage of
   "byte").

   Here are descriptions of a couple of common
   encodings:


   A. Japanese EUC (Extended Unix Code)

   This encompasses the character sets:
   - Printing-ASCII,
   - Katakana-JISX0201 (half-width katakana, the right half of JISX0201).
   - Japanese-JISX0208
   - Japanese-JISX0212
   It uses 8-bit bytes.

   Note that Printing-ASCII and Katakana-JISX0201 are 94-character
   charsets, while Japanese-JISX0208 is a 94x94-character charset.

   The encoding is as follows:

   Character set	Representation  (PC == position-code)
   -------------	--------------
   Printing-ASCII	PC1
   Japanese-JISX0208	PC1 + 0x80 | PC2 + 0x80
   Katakana-JISX0201	0x8E       | PC1 + 0x80


   B. JIS7

   This encompasses the character sets:
   - Printing-ASCII
   - Latin-JISX0201 (the left half of JISX0201; this character set is
     very similar to Printing-ASCII and is a 94-character charset)
   - Japanese-JISX0208
   - Katakana-JISX0201
   It uses 7-bit bytes.

   Unlike Japanese EUC, this is a "modal" encoding, which
   means that there are multiple states that the encoding can
   be in, which affect how the bytes are to be interpreted.
   Special sequences of bytes (called "escape sequences")
   are used to change states.

   The encoding is as follows:

   Character set	Representation
   -------------	--------------
   Printing-ASCII	PC1
   Latin-JISX0201	PC1
   Katakana-JISX0201	PC1
   Japanese-JISX0208	PC1 | PC2

   Escape sequence	ASCII equivalent  Meaning
   ---------------	----------------  -------
   0x1B 0x28 0x42	ESC ( B		  invoke Printing-ASCII
   0x1B 0x28 0x4A	ESC ( J		  invoke Latin-JISX0201
   0x1B 0x28 0x49	ESC ( I		  invoke Katakana-JISX0201
   0x1B 0x24 0x42	ESC $ B		  invoke Japanese-JISX0208

   Initially, Printing-ASCII is invoked.

   3. Internal Mule Encodings
   ==========================

   In XEmacs/Mule, each character set is assigned a unique number,
   called a "leading byte".  This is used in the encodings of a
   character.  Leading bytes are in the range 0x80 - 0xFF
   (except for ASCII, which has a leading byte of 0), although
   some leading bytes are reserved.

   Charsets whose leading byte is in the range 0x80 - 0x9F are
   called "official" and are used for built-in charsets.
   Other charsets are called "private" and have leading bytes
   in the range 0xA0 - 0xFF; these are user-defined charsets.

   More specifically:

   Character set		Leading byte
   -------------		------------
   ASCII			0
   Composite			0x80
   Dimension-1 Official		0x81 - 0x8D
				  (0x8E is free)
   Control			0x8F
   Dimension-2 Official		0x90 - 0x99
				  (0x9A - 0x9D are free;
				  0x9E and 0x9F are reserved)
   Dimension-1 Private		0xA0 - 0xEF
   Dimension-2 Private		0xF0 - 0xFF

   There are two internal encodings for characters in XEmacs/Mule.
   One is called "string encoding" and is an 8-bit encoding that
   is used for representing characters in a buffer or string.
   It uses 1 to 4 bytes per character.  The other is called
   "character encoding" and is a 19-bit encoding that is used
   for representing characters individually in a variable.

   (In the following descriptions, we'll ignore composite
   characters for the moment.  We also give a general (structural)
   overview first, followed later by the exact details.)

   A. Internal String Encoding

   ASCII characters are encoded using their position code directly.
   Other characters are encoded using their leading byte followed
   by their position code(s) with the high bit set.  Characters
   in private character sets have their leading byte prefixed with
   a "leading byte prefix", which is either 0x9E or 0x9F. (No
   character sets are ever assigned these leading bytes.) Specifically:

   Character set		Encoding (PC == position-code)
   -------------		-------- (LB == leading-byte)
   ASCII			PC1 |
   Control-1			LB   | PC1 + 0xA0
   Dimension-1 official		LB   | PC1 + 0x80
   Dimension-1 private		0x9E | LB         | PC1 + 0x80
   Dimension-2 official		LB   | PC1        | PC2 + 0x80
   Dimension-2 private		0x9F | LB         | PC1 + 0x80 | PC2 + 0x80

   The basic characteristic of this encoding is that the first byte
   of all characters is in the range 0x00 - 0x9F, and the second and
   following bytes of all characters is in the range 0xA0 - 0xFF.
   This means that it is impossible to get out of sync, or more
   specifically:

   1. Given any byte position, the beginning of the character it is
      within can be determined in constant time.
   2. Given any byte position at the beginning of a character, the
      beginning of the next character can be determined in constant
      time.
   3. Given any byte position at the beginning of a character, the
      beginning of the previous character can be determined in constant
      time.
   4. Textual searches can simply treat encoded strings as if they
      were encoded in a one-byte-per-character fashion rather than
      the actual multi-byte encoding.

   None of the standard non-modal encodings meet all of these
   conditions.  For example, EUC satisfies only (2) and (3), while
   Shift-JIS and Big5 (not yet described) satisfy only (2). (All
   non-modal encodings must satisfy (2), in order to be unambiguous.)

   B. Internal Character Encoding

   One 19-bit word represents a single character.  The word is
   separated into three fields:

   Bit number:	18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
		<------------> <------------------> <------------------>
   Field:	      1		         2		      3

   Note that fields 2 and 3 hold 7 bits each, while field 1 holds 5 bits.

   Character set		Field 1		Field 2		Field 3
   -------------		-------		-------		-------
   ASCII			   0		   0              PC1
      range:                                                   (00 - 7F)
   Control-1			   0		   1              PC1
      range:                                                   (00 - 1F)
   Dimension-1 official            0            LB - 0x80         PC1
      range:                                    (01 - 0D)      (20 - 7F)
   Dimension-1 private             0            LB - 0x80         PC1
      range:                                    (20 - 6F)      (20 - 7F)
   Dimension-2 official		LB - 0x8F          PC1            PC2
      range:                    (01 - 0A)       (20 - 7F)      (20 - 7F)
   Dimension-2 private          LB - 0xE1          PC1            PC2
      range:                    (0F - 1E)       (20 - 7F)      (20 - 7F)
   Composite			  0x1F              ?              ?

   Note that character codes 0 - 255 are the same as the "binary encoding"
   described above.
*/

/*
   About Unicode support:

   Adding Unicode support is very desirable.  Unicode will likely be a
   very common representation in the future, and thus we should
   represent Unicode characters using three bytes instead of four.
   This means we need to find leading bytes for Unicode.  Given that
   there are 65,536 characters in Unicode and we can attach 96x96 =
   9,216 characters per leading byte, we need eight leading bytes for
   Unicode.  We currently have four free (0x9A - 0x9D), and with a
   little bit of rearranging we can get five: ASCII doesn't really
   need to take up a leading byte. (We could just as well use 0x7F,
   with a little change to the functions that assume that 0x80 is the
   lowest leading byte.) This means we still need to dump three
   leading bytes and move them into private space.  The CNS charsets
   are good candidates since they are rarely used, and
   JAPANESE_JISX0208_1978 is becoming less and less used and could
   also be dumped. */


/************************************************************************/
/*                    Definition of leading bytes                       */
/************************************************************************/

#define MIN_LEADING_BYTE		0x80
/* These need special treatment in a string and/or character */
#define LEADING_BYTE_ASCII		0x8E /* Omitted in a buffer */
#define LEADING_BYTE_COMPOSITE		0x80 /* for a composite character */
#define LEADING_BYTE_CONTROL_1		0x8F /* represent normal 80-9F */

/** The following are for 1-byte characters in an official charset. **/

#define LEADING_BYTE_LATIN_ISO8859_1	0x81 /* Right half of ISO 8859-1 */
#define LEADING_BYTE_LATIN_ISO8859_2	0x82 /* Right half of ISO 8859-2 */
#define LEADING_BYTE_LATIN_ISO8859_3	0x83 /* Right half of ISO 8859-3 */
#define LEADING_BYTE_LATIN_ISO8859_4	0x84 /* Right half of ISO 8859-4 */
#define LEADING_BYTE_THAI_TIS620	0x85 /* TIS620-2533 */
#define LEADING_BYTE_GREEK_ISO8859_7	0x86 /* Right half of ISO 8859-7 */
#define LEADING_BYTE_ARABIC_ISO8859_6	0x87 /* Right half of ISO 8859-6 */
#define LEADING_BYTE_HEBREW_ISO8859_8	0x88 /* Right half of ISO 8859-8 */
#define LEADING_BYTE_KATAKANA_JISX0201	0x89 /* Right half of JIS X0201-1976 */
#define LEADING_BYTE_LATIN_JISX0201	0x8A /* Left  half of JIS X0201-1976 */
#define LEADING_BYTE_CYRILLIC_ISO8859_5	0x8C /* Right half of ISO 8859-5 */
#define LEADING_BYTE_LATIN_ISO8859_9	0x8D /* Right half of ISO 8859-9 */

#define MIN_LEADING_BYTE_OFFICIAL_1	LEADING_BYTE_LATIN_ISO8859_1
#define MAX_LEADING_BYTE_OFFICIAL_1	LEADING_BYTE_LATIN_ISO8859_9

/** The following are for 2-byte characters in an official charset. **/

#define LEADING_BYTE_JAPANESE_JISX0208_1978 0x90/* Japanese JIS X0208-1978 */
#define LEADING_BYTE_CHINESE_GB2312	0x91	/* Chinese Hanzi GB2312-1980 */
#define LEADING_BYTE_JAPANESE_JISX0208	0x92	/* Japanese JIS X0208-1983 */
#define LEADING_BYTE_KOREAN_KSC5601	0x93	/* Hangul KS C5601-1987 */
#define LEADING_BYTE_JAPANESE_JISX0212	0x94	/* Japanese JIS X0212-1990 */
#define LEADING_BYTE_CHINESE_CNS11643_1	0x95	/* Chinese CNS11643 Set 1 */
#define LEADING_BYTE_CHINESE_CNS11643_2	0x96	/* Chinese CNS11643 Set 2 */
#define LEADING_BYTE_CHINESE_BIG5_1	0x97	/* Big5 Level 1 */
#define LEADING_BYTE_CHINESE_BIG5_2	0x98	/* Big5 Level 2 */
				     /* 0x99	   unused */
				     /* 0x9A       unused */
				     /* 0x9B       unused */
				     /* 0x9C       unused */
				     /* 0x9D       unused */

#define MIN_LEADING_BYTE_OFFICIAL_2	LEADING_BYTE_JAPANESE_JISX0208_1978
#define MAX_LEADING_BYTE_OFFICIAL_2	LEADING_BYTE_CHINESE_BIG5_2

/** The following are for 1- and 2-byte characters in a private charset. **/

#define PRE_LEADING_BYTE_PRIVATE_1	0x9E	/* 1-byte char-set */
#define PRE_LEADING_BYTE_PRIVATE_2	0x9F	/* 2-byte char-set */

#define MIN_LEADING_BYTE_PRIVATE_1	0xA0
#define MAX_LEADING_BYTE_PRIVATE_1	0xEF
#define MIN_LEADING_BYTE_PRIVATE_2	0xF0
#define MAX_LEADING_BYTE_PRIVATE_2	0xFF

#define NUM_LEADING_BYTES 128


/************************************************************************/
/*                    Operations on leading bytes                       */
/************************************************************************/

/* Is this leading byte for a private charset? */

#define LEADING_BYTE_PRIVATE_P(lb) ((lb) >= MIN_LEADING_BYTE_PRIVATE_1)

/* Is this a prefix for a private leading byte? */

INLINE int LEADING_BYTE_PREFIX_P (unsigned char lb);
INLINE int
LEADING_BYTE_PREFIX_P (unsigned char lb)
{
  return (lb == PRE_LEADING_BYTE_PRIVATE_1 ||
	  lb == PRE_LEADING_BYTE_PRIVATE_2);
}

/* Given a private leading byte, return the leading byte prefix stored
   in a string */

#define PRIVATE_LEADING_BYTE_PREFIX(lb)	\
  ((lb) < MIN_LEADING_BYTE_PRIVATE_2 ?	\
   PRE_LEADING_BYTE_PRIVATE_1 :		\
   PRE_LEADING_BYTE_PRIVATE_2)


/************************************************************************/
/*                     Operations on individual bytes                   */
/*                             of any format                            */
/************************************************************************/

/* Argument `c' should be (unsigned int) or (unsigned char). */
/* Note that SP and DEL are not included. */

#define BYTE_ASCII_P(c) ((c) < 0x80)
#define BYTE_C0_P(c) ((c) < 0x20)
/* Do some forced casting just to make *sure* things are gotten right. */
#define BYTE_C1_P(c) ((unsigned int) ((unsigned int) (c) - 0x80) < 0x20)


/************************************************************************/
/*                     Operations on individual bytes                   */
/*                       in a Mule-formatted string                     */
/************************************************************************/

/* Does this byte represent the first byte of a character? */

#define BUFBYTE_FIRST_BYTE_P(c) ((c) < 0xA0)

/* Does this byte represent the first byte of a multi-byte character? */

#define BUFBYTE_LEADING_BYTE_P(c) BYTE_C1_P (c)


/************************************************************************/
/*            Information about a particular character set              */
/************************************************************************/

struct Lisp_Charset
{
  struct lcrecord_header header;

  int id;
  Lisp_Object name;
  Lisp_Object doc_string, registry;

  Lisp_Object reverse_direction_charset;

  Lisp_Object ccl_program;

  Bufbyte leading_byte;

  /* Final byte of this character set in ISO2022 designating escape sequence */
  Bufbyte final;

  /* Number of bytes (1 - 4) required in the internal representation
     for characters in this character set.  This is *not* the
     same as the dimension of the character set). */
  unsigned int rep_bytes;

  /* Number of columns a character in this charset takes up, on TTY
     devices.  Not used for X devices. */
  unsigned int columns;

  /* Direction of this character set */
  unsigned int direction;

  /* Type of this character set (94, 96, 94x94, 96x96) */
  unsigned int type;

  /* Number of bytes used in encoding of this character set (1 or 2) */
  unsigned int dimension;

  /* Number of chars in each dimension (usually 94 or 96) */
  unsigned int chars;

  /* Which half of font to be used to display this character set */
  unsigned int graphic;
};

DECLARE_LRECORD (charset, struct Lisp_Charset);
#define XCHARSET(x) XRECORD (x, charset, struct Lisp_Charset)
#define XSETCHARSET(x, p) XSETRECORD (x, p, charset)
#define CHARSETP(x) RECORDP (x, charset)
#define GC_CHARSETP(x) GC_RECORDP (x, charset)
#define CHECK_CHARSET(x) CHECK_RECORD (x, charset)
#define CONCHECK_CHARSET(x) CONCHECK_RECORD (x, charset)

#define CHARSET_TYPE_94    0	/* This charset includes 94    characters. */
#define CHARSET_TYPE_96    1	/* This charset includes 96    characters. */
#define CHARSET_TYPE_94X94 2	/* This charset includes 94x94 characters. */
#define CHARSET_TYPE_96X96 3	/* This charset includes 96x96 characters. */

#define CHARSET_LEFT_TO_RIGHT	0
#define CHARSET_RIGHT_TO_LEFT	1

#define CHARSET_ID(cs)		 ((cs)->id)
#define CHARSET_NAME(cs)	 ((cs)->name)
#define CHARSET_LEADING_BYTE(cs) ((cs)->leading_byte)
#define CHARSET_REP_BYTES(cs)	 ((cs)->rep_bytes)
#define CHARSET_COLUMNS(cs)	 ((cs)->columns)
#define CHARSET_GRAPHIC(cs)	 ((cs)->graphic)
#define CHARSET_TYPE(cs)	 ((cs)->type)
#define CHARSET_DIRECTION(cs)	 ((cs)->direction)
#define CHARSET_FINAL(cs)	 ((cs)->final)
#define CHARSET_DOC_STRING(cs)	 ((cs)->doc_string)
#define CHARSET_REGISTRY(cs)	 ((cs)->registry)
#define CHARSET_CCL_PROGRAM(cs)  ((cs)->ccl_program)
#define CHARSET_DIMENSION(cs)	 ((cs)->dimension)
#define CHARSET_CHARS(cs)	 ((cs)->chars)
#define CHARSET_REVERSE_DIRECTION_CHARSET(cs) ((cs)->reverse_direction_charset)


#define CHARSET_PRIVATE_P(cs) LEADING_BYTE_PRIVATE_P (CHARSET_LEADING_BYTE (cs))

#define XCHARSET_ID(cs)		  CHARSET_ID           (XCHARSET (cs))
#define XCHARSET_NAME(cs)	  CHARSET_NAME         (XCHARSET (cs))
#define XCHARSET_REP_BYTES(cs)	  CHARSET_REP_BYTES    (XCHARSET (cs))
#define XCHARSET_COLUMNS(cs)	  CHARSET_COLUMNS      (XCHARSET (cs))
#define XCHARSET_GRAPHIC(cs)      CHARSET_GRAPHIC      (XCHARSET (cs))
#define XCHARSET_TYPE(cs)	  CHARSET_TYPE         (XCHARSET (cs))
#define XCHARSET_DIRECTION(cs)	  CHARSET_DIRECTION    (XCHARSET (cs))
#define XCHARSET_FINAL(cs)	  CHARSET_FINAL        (XCHARSET (cs))
#define XCHARSET_DOC_STRING(cs)	  CHARSET_DOC_STRING   (XCHARSET (cs))
#define XCHARSET_REGISTRY(cs)	  CHARSET_REGISTRY     (XCHARSET (cs))
#define XCHARSET_LEADING_BYTE(cs) CHARSET_LEADING_BYTE (XCHARSET (cs))
#define XCHARSET_CCL_PROGRAM(cs)  CHARSET_CCL_PROGRAM  (XCHARSET (cs))
#define XCHARSET_DIMENSION(cs)	  CHARSET_DIMENSION    (XCHARSET (cs))
#define XCHARSET_CHARS(cs)	  CHARSET_CHARS        (XCHARSET (cs))
#define XCHARSET_PRIVATE_P(cs)	  CHARSET_PRIVATE_P    (XCHARSET (cs))
#define XCHARSET_REVERSE_DIRECTION_CHARSET(cs) \
  CHARSET_REVERSE_DIRECTION_CHARSET (XCHARSET (cs))

/* Table of charsets indexed by (leading byte - 128). */
extern Lisp_Object charset_by_leading_byte[128];

/* Table of charsets indexed by type/final-byte/direction. */
extern Lisp_Object charset_by_attributes[4][128][2];

/* Table of number of bytes in the string representation of a character
   indexed by the first byte of that representation.

   This value can be derived other ways -- e.g. something like

   (BYTE_ASCII_P (first_byte) ? 1 :
    XCHARSET_REP_BYTES (CHARSET_BY_LEADING_BYTE (first_byte)))

   but it's faster this way. */
extern Bytecount rep_bytes_by_first_byte[0xA0];

#ifdef ERROR_CHECK_TYPECHECK
/* int not Bufbyte even though that is the actual type of a leading byte.
   This way, out-ot-range values will get caught rather than automatically
   truncated. */
INLINE Lisp_Object CHARSET_BY_LEADING_BYTE (int lb);
INLINE Lisp_Object
CHARSET_BY_LEADING_BYTE (int lb)
{
  assert (lb >= 0x80 && lb <= 0xFF);
  return charset_by_leading_byte[lb - 128];
}

#else

#define CHARSET_BY_LEADING_BYTE(lb) (charset_by_leading_byte[(lb) - 128])

#endif

#define CHARSET_BY_ATTRIBUTES(type, final, dir) \
  (charset_by_attributes[type][final][dir])

#ifdef ERROR_CHECK_TYPECHECK

/* Number of bytes in the string representation of a character */
INLINE int REP_BYTES_BY_FIRST_BYTE (int fb);
INLINE int
REP_BYTES_BY_FIRST_BYTE (int fb)
{
  assert (fb >= 0 && fb < 0xA0);
  return rep_bytes_by_first_byte[fb];
}

#else
#define REP_BYTES_BY_FIRST_BYTE(fb) (rep_bytes_by_first_byte[fb])
#endif


/************************************************************************/
/*                        Dealing with characters                       */
/************************************************************************/

/* Is this character represented by more than one byte in a string? */

#define CHAR_MULTIBYTE_P(c) ((c) >= 0x80)

#define CHAR_ASCII_P(c) (!CHAR_MULTIBYTE_P (c))

/* The bit fields of character are divided into 3 parts:
   FIELD1(5bits):FIELD2(7bits):FIELD3(7bits) */

#define CHAR_FIELD1_MASK (0x1F << 14)
#define CHAR_FIELD2_MASK (0x7F << 7)
#define CHAR_FIELD3_MASK 0x7F

/* Macros to access each field of a character code of C.  */

#define CHAR_FIELD1(c) (((c) & CHAR_FIELD1_MASK) >> 14)
#define CHAR_FIELD2(c) (((c) & CHAR_FIELD2_MASK) >> 7)
#define CHAR_FIELD3(c)  ((c) & CHAR_FIELD3_MASK)

/* Field 1, if non-zero, usually holds a leading byte for a
   dimension-2 charset.  Field 2, if non-zero, usually holds a leading
   byte for a dimension-1 charset. */

/* Converting between field values and leading bytes.  */

#define FIELD2_TO_OFFICIAL_LEADING_BYTE 0x80
#define FIELD2_TO_PRIVATE_LEADING_BYTE  0x80

#define FIELD1_TO_OFFICIAL_LEADING_BYTE 0x8F
#define FIELD1_TO_PRIVATE_LEADING_BYTE  0xE1

/* Minimum and maximum allowed values for the fields. */

#define MIN_CHAR_FIELD2_OFFICIAL \
  (MIN_LEADING_BYTE_OFFICIAL_1 - FIELD2_TO_OFFICIAL_LEADING_BYTE)
#define MAX_CHAR_FIELD2_OFFICIAL \
  (MAX_LEADING_BYTE_OFFICIAL_1 - FIELD2_TO_OFFICIAL_LEADING_BYTE)

#define MIN_CHAR_FIELD1_OFFICIAL \
  (MIN_LEADING_BYTE_OFFICIAL_2 - FIELD1_TO_OFFICIAL_LEADING_BYTE)
#define MAX_CHAR_FIELD1_OFFICIAL \
  (MAX_LEADING_BYTE_OFFICIAL_2 - FIELD1_TO_OFFICIAL_LEADING_BYTE)

#define MIN_CHAR_FIELD2_PRIVATE \
  (MIN_LEADING_BYTE_PRIVATE_1 - FIELD2_TO_PRIVATE_LEADING_BYTE)
#define MAX_CHAR_FIELD2_PRIVATE \
  (MAX_LEADING_BYTE_PRIVATE_1 - FIELD2_TO_PRIVATE_LEADING_BYTE)

#define MIN_CHAR_FIELD1_PRIVATE \
  (MIN_LEADING_BYTE_PRIVATE_2 - FIELD1_TO_PRIVATE_LEADING_BYTE)
#define MAX_CHAR_FIELD1_PRIVATE \
  (MAX_LEADING_BYTE_PRIVATE_2 - FIELD1_TO_PRIVATE_LEADING_BYTE)

/* Minimum character code of each <type> character.  */

#define MIN_CHAR_OFFICIAL_TYPE9N    (MIN_CHAR_FIELD2_OFFICIAL <<  7)
#define MIN_CHAR_PRIVATE_TYPE9N     (MIN_CHAR_FIELD2_PRIVATE  <<  7)
#define MIN_CHAR_OFFICIAL_TYPE9NX9N (MIN_CHAR_FIELD1_OFFICIAL << 14)
#define MIN_CHAR_PRIVATE_TYPE9NX9N  (MIN_CHAR_FIELD1_PRIVATE  << 14)
#define MIN_CHAR_COMPOSITION        (0x1F << 14)

/* Leading byte of a character.

   NOTE: This takes advantage of the fact that
   FIELD2_TO_OFFICIAL_LEADING_BYTE and
   FIELD2_TO_PRIVATE_LEADING_BYTE are the same.
   */

INLINE Bufbyte CHAR_LEADING_BYTE (Emchar c);
INLINE Bufbyte
CHAR_LEADING_BYTE (Emchar c)
{
  if (CHAR_ASCII_P (c))
    return LEADING_BYTE_ASCII;
  else if (c < 0xA0)
    return LEADING_BYTE_CONTROL_1;
  else if (c < MIN_CHAR_OFFICIAL_TYPE9NX9N)
    return CHAR_FIELD2 (c) + FIELD2_TO_OFFICIAL_LEADING_BYTE;
  else if (c < MIN_CHAR_PRIVATE_TYPE9NX9N)
    return CHAR_FIELD1 (c) + FIELD1_TO_OFFICIAL_LEADING_BYTE;
  else if (c < MIN_CHAR_COMPOSITION)
    return CHAR_FIELD1 (c) + FIELD1_TO_PRIVATE_LEADING_BYTE;
  else
    return LEADING_BYTE_COMPOSITE;
}

#define CHAR_CHARSET(c) CHARSET_BY_LEADING_BYTE (CHAR_LEADING_BYTE (c))

/* Return a character whose charset is CHARSET and position-codes
   are C1 and C2.  TYPE9N character ignores C2.

   NOTE: This takes advantage of the fact that
   FIELD2_TO_OFFICIAL_LEADING_BYTE and
   FIELD2_TO_PRIVATE_LEADING_BYTE are the same.
   */

INLINE Emchar MAKE_CHAR (Lisp_Object charset, int c1, int c2);
INLINE Emchar
MAKE_CHAR (Lisp_Object charset, int c1, int c2)
{
  if (EQ (charset, Vcharset_ascii))
    return c1;
  else if (EQ (charset, Vcharset_control_1))
    return c1 | 0x80;
  else if (EQ (charset, Vcharset_composite))
    return (0x1F << 14) | ((c1) << 7) | (c2);
  else if (XCHARSET_DIMENSION (charset) == 1)
    return ((XCHARSET_LEADING_BYTE (charset) -
	     FIELD2_TO_OFFICIAL_LEADING_BYTE) << 7) | (c1);
  else if (!XCHARSET_PRIVATE_P (charset))
    return ((XCHARSET_LEADING_BYTE (charset) -
	     FIELD1_TO_OFFICIAL_LEADING_BYTE) << 14) | ((c1) << 7) | (c2);
  else
    return ((XCHARSET_LEADING_BYTE (charset) -
	     FIELD1_TO_PRIVATE_LEADING_BYTE) << 14) | ((c1) << 7) | (c2);
}

/* The charset of character C is set to CHARSET, and the
   position-codes of C are set to C1 and C2.  C2 of TYPE9N character
   is 0.  */

/* BREAKUP_CHAR_1_UNSAFE assumes that the charset has already been
   calculated, and just computes c1 and c2.

   BREAKUP_CHAR also computes and stores the charset. */

#define BREAKUP_CHAR_1_UNSAFE(c, charset, c1, c2)	\
  XCHARSET_DIMENSION (charset) == 1			\
  ? ((c1) = CHAR_FIELD3 (c), (c2) = 0)			\
  : ((c1) = CHAR_FIELD2 (c),				\
     (c2) = CHAR_FIELD3 (c))

INLINE void breakup_char_1 (Emchar c, Lisp_Object *charset, int *c1, int *c2);
INLINE void
breakup_char_1 (Emchar c, Lisp_Object *charset, int *c1, int *c2)
{
  *charset = CHAR_CHARSET (c);
  BREAKUP_CHAR_1_UNSAFE (c, *charset, *c1, *c2);
}

#define BREAKUP_CHAR(c, charset, c1, c2) \
  breakup_char_1 (c, &(charset), &(c1), &(c2))



/************************************************************************/
/*                           Composite characters                       */
/************************************************************************/

Emchar lookup_composite_char (Bufbyte *str, int len);
Lisp_Object composite_char_string (Emchar ch);


/************************************************************************/
/*                            Exported functions                        */
/************************************************************************/

EXFUN (Ffind_charset, 1);
EXFUN (Fget_charset, 1);

extern Lisp_Object Vcharset_chinese_big5_1;
extern Lisp_Object Vcharset_chinese_big5_2;
extern Lisp_Object Vcharset_japanese_jisx0208;

Emchar Lstream_get_emchar_1 (Lstream *stream, int first_char);
int Lstream_fput_emchar (Lstream *stream, Emchar ch);
void Lstream_funget_emchar (Lstream *stream, Emchar ch);

int copy_internal_to_external (CONST Bufbyte *internal, Bytecount len,
			       unsigned char *external);
Bytecount copy_external_to_internal (CONST unsigned char *external,
				     int len, Bufbyte *internal);

#endif /* _XEMACS_MULE_CHARSET_H */
