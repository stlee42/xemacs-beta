/* Header for code conversion stuff
   Copyright (C) 1991, 1995 Free Software Foundation, Inc.
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

/* 91.10.09 written by K.Handa <handa@etl.go.jp> */
/* Rewritten by Ben Wing <ben@xemacs.org>. */

#ifndef _XEMACS_MULE_CODING_H_
#define _XEMACS_MULE_CODING_H_

struct decoding_stream;
struct encoding_stream;

/* Coding system types.  These go into the TYPE field of a
   struct Lisp_Coding_System. */

enum coding_system_type
{
  CODESYS_AUTODETECT,	/* Automatic conversion. */
  CODESYS_SHIFT_JIS,	/* Shift-JIS; Hankaku (half-width) KANA
			   is also supported. */
  CODESYS_ISO2022,	/* Any ISO2022-compliant coding system.
			   Includes JIS, EUC, CTEXT */
  CODESYS_BIG5,		/* BIG5 (used for Taiwanese). */
  CODESYS_CCL,		/* Converter written in CCL. */
  CODESYS_NO_CONVERSION	/* "No conversion"; used for binary files.
			   We use quotes because there really
			   is some conversion being applied,
			   but it appears to the user as if
			   the text is read in without conversion. */
#ifdef DEBUG_XEMACS
  ,CODESYS_INTERNAL	/* Raw (internally-formatted) data. */
#endif
};

enum eol_type
{
  EOL_AUTODETECT,
  EOL_LF,
  EOL_CRLF,
  EOL_CR
};

typedef struct charset_conversion_spec charset_conversion_spec;
struct charset_conversion_spec
{
  Lisp_Object from_charset;
  Lisp_Object to_charset;
};

typedef struct
{
  Dynarr_declare (charset_conversion_spec);
} charset_conversion_spec_dynarr;

struct Lisp_Coding_System
{
  struct lcrecord_header header;

  /* Name and doc string of this coding system. */
  Lisp_Object name, doc_string;

  /* This is the major type of the coding system -- one of Big5, ISO2022,
     Shift-JIS, etc.  See the constants above. */
  enum coding_system_type type;

  /* Mnemonic string displayed in the modeline when this coding
     system is active for a particular buffer. */
  Lisp_Object mnemonic;

  Lisp_Object post_read_conversion, pre_write_conversion;

  enum eol_type eol_type;

  /* Subsidiary coding systems that specify a particular type of EOL
     marking, rather than autodetecting it.  These will only be non-nil
     if (eol_type == EOL_AUTODETECT). */
  Lisp_Object eol_lf, eol_crlf, eol_cr;

  struct
  {
    /* What are the charsets to be initially designated to G0, G1,
       G2, G3?  If t, no charset is initially designated.  If nil,
       no charset is initially designated and no charset is allowed
       to be designated. */
    Lisp_Object initial_charset[4];

    /* If true, a designation escape sequence needs to be sent on output
       for the charset in G[0-3] before that charset is used. */
    unsigned char force_charset_on_output[4];

    charset_conversion_spec_dynarr *input_conv;
    charset_conversion_spec_dynarr *output_conv;

    unsigned int shoort		:1; /* C makes you speak Dutch */
    unsigned int no_ascii_eol	:1;
    unsigned int no_ascii_cntl	:1;
    unsigned int seven		:1;
    unsigned int lock_shift	:1;
    unsigned int no_iso6429	:1;
    unsigned int escape_quoted	:1;
  } iso2022;

  struct
  {
    /* For a CCL coding system, these specify the CCL programs used for
       decoding (input) and encoding (output). */
    Lisp_Object decode, encode;
  } ccl;
};

DECLARE_LRECORD (coding_system, struct Lisp_Coding_System);
#define XCODING_SYSTEM(x) XRECORD (x, coding_system, struct Lisp_Coding_System)
#define XSETCODING_SYSTEM(x, p) XSETRECORD (x, p, coding_system)
#define CODING_SYSTEMP(x) RECORDP (x, coding_system)
#define GC_CODING_SYSTEMP(x) GC_RECORDP (x, coding_system)
#define CHECK_CODING_SYSTEM(x) CHECK_RECORD (x, coding_system)
#define CONCHECK_CODING_SYSTEM(x) CONCHECK_RECORD (x, coding_system)

#define CODING_SYSTEM_NAME(codesys) ((codesys)->name)
#define CODING_SYSTEM_DOC_STRING(codesys) ((codesys)->doc_string)
#define CODING_SYSTEM_TYPE(codesys) ((codesys)->type)
#define CODING_SYSTEM_MNEMONIC(codesys) ((codesys)->mnemonic)
#define CODING_SYSTEM_POST_READ_CONVERSION(codesys) \
  ((codesys)->post_read_conversion)
#define CODING_SYSTEM_PRE_WRITE_CONVERSION(codesys) \
  ((codesys)->pre_write_conversion)
#define CODING_SYSTEM_EOL_TYPE(codesys) ((codesys)->eol_type)
#define CODING_SYSTEM_EOL_LF(codesys)   ((codesys)->eol_lf)
#define CODING_SYSTEM_EOL_CRLF(codesys) ((codesys)->eol_crlf)
#define CODING_SYSTEM_EOL_CR(codesys)   ((codesys)->eol_cr)
#define CODING_SYSTEM_ISO2022_INITIAL_CHARSET(codesys, g) \
  ((codesys)->iso2022.initial_charset[g])
#define CODING_SYSTEM_ISO2022_FORCE_CHARSET_ON_OUTPUT(codesys, g) \
  ((codesys)->iso2022.force_charset_on_output[g])
#define CODING_SYSTEM_ISO2022_SHORT(codesys) ((codesys)->iso2022.shoort)
#define CODING_SYSTEM_ISO2022_NO_ASCII_EOL(codesys) \
  ((codesys)->iso2022.no_ascii_eol)
#define CODING_SYSTEM_ISO2022_NO_ASCII_CNTL(codesys) \
  ((codesys)->iso2022.no_ascii_cntl)
#define CODING_SYSTEM_ISO2022_SEVEN(codesys) ((codesys)->iso2022.seven)
#define CODING_SYSTEM_ISO2022_LOCK_SHIFT(codesys) \
  ((codesys)->iso2022.lock_shift)
#define CODING_SYSTEM_ISO2022_NO_ISO6429(codesys) \
  ((codesys)->iso2022.no_iso6429)
#define CODING_SYSTEM_ISO2022_ESCAPE_QUOTED(codesys) \
  ((codesys)->iso2022.escape_quoted)
#define CODING_SYSTEM_CCL_DECODE(codesys) ((codesys)->ccl.decode)
#define CODING_SYSTEM_CCL_ENCODE(codesys) ((codesys)->ccl.encode)

#define XCODING_SYSTEM_NAME(codesys) \
  CODING_SYSTEM_NAME (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_DOC_STRING(codesys) \
  CODING_SYSTEM_DOC_STRING (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_TYPE(codesys) \
  CODING_SYSTEM_TYPE (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_MNEMONIC(codesys) \
  CODING_SYSTEM_MNEMONIC (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_POST_READ_CONVERSION(codesys) \
  CODING_SYSTEM_POST_READ_CONVERSION (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_PRE_WRITE_CONVERSION(codesys) \
  CODING_SYSTEM_PRE_WRITE_CONVERSION (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_EOL_TYPE(codesys) \
  CODING_SYSTEM_EOL_TYPE (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_EOL_LF(codesys) \
  CODING_SYSTEM_EOL_LF (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_EOL_CRLF(codesys) \
  CODING_SYSTEM_EOL_CRLF (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_EOL_CR(codesys) \
  CODING_SYSTEM_EOL_CR (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_ISO2022_INITIAL_CHARSET(codesys, g) \
  CODING_SYSTEM_ISO2022_INITIAL_CHARSET (XCODING_SYSTEM (codesys), g)
#define XCODING_SYSTEM_ISO2022_FORCE_CHARSET_ON_OUTPUT(codesys, g) \
  CODING_SYSTEM_ISO2022_FORCE_CHARSET_ON_OUTPUT (XCODING_SYSTEM (codesys), g)
#define XCODING_SYSTEM_ISO2022_SHORT(codesys) \
  CODING_SYSTEM_ISO2022_SHORT (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_ISO2022_NO_ASCII_EOL(codesys) \
  CODING_SYSTEM_ISO2022_NO_ASCII_EOL (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_ISO2022_NO_ASCII_CNTL(codesys) \
  CODING_SYSTEM_ISO2022_NO_ASCII_CNTL (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_ISO2022_SEVEN(codesys) \
  CODING_SYSTEM_ISO2022_SEVEN (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_ISO2022_LOCK_SHIFT(codesys) \
  CODING_SYSTEM_ISO2022_LOCK_SHIFT (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_ISO2022_NO_ISO6429(codesys) \
  CODING_SYSTEM_ISO2022_NO_ISO6429 (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_ISO2022_ESCAPE_QUOTED(codesys) \
  CODING_SYSTEM_ISO2022_ESCAPE_QUOTED (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_CCL_DECODE(codesys) \
  CODING_SYSTEM_CCL_DECODE (XCODING_SYSTEM (codesys))
#define XCODING_SYSTEM_CCL_ENCODE(codesys) \
  CODING_SYSTEM_CCL_ENCODE (XCODING_SYSTEM (codesys))

extern Lisp_Object Qbuffer_file_coding_system, Qcoding_system_error;

extern Lisp_Object Vkeyboard_coding_system;
extern Lisp_Object Vterminal_coding_system;
extern Lisp_Object Vcoding_system_for_read;
extern Lisp_Object Vcoding_system_for_write;
extern Lisp_Object Vpathname_coding_system;

extern Lisp_Object Qescape_quoted;

/* Flags indicating current state while converting code. */

/* Used by everyone. */

#define CODING_STATE_END	(1 << 0) /* If set, this is the last chunk of
					    data being processed.  When this
					    is finished, output any necessary
					    terminating control characters,
					    escape sequences, etc. */
#define CODING_STATE_CR		(1 << 1) /* If set, we just saw a CR. */


/* Used by Big 5 on output. */

#define CODING_STATE_BIG5_1	(1 << 2) /* If set, we just encountered
					    LEADING_BYTE_BIG5_1. */
#define CODING_STATE_BIG5_2	(1 << 3) /* If set, we just encountered
					    LEADING_BYTE_BIG5_2. */


/* Used by ISO2022 on input and output. */

#define CODING_STATE_R2L	(1 << 4)  /* If set, the current
					     directionality is right-to-left.
					     Otherwise, it's left-to-right. */


/* Used by ISO2022 on input. */

#define CODING_STATE_ESCAPE	(1 << 5)  /* If set, we're currently parsing
					     an escape sequence and the upper
					     16 bits should be looked at to
					     indicate what partial escape
					     sequence we've seen so far.
					     Otherwise, we're running
					     through actual text. */
#define CODING_STATE_SS2	(1 << 6)  /* If set, G2 is invoked into GL, but
					     only for the next character. */
#define CODING_STATE_SS3	(1 << 7)  /* If set, G3 is invoked into GL,
					     but only for the next character.
					     If both CODING_STATE_SS2 and
					     CODING_STATE_SS3 are set,
					     CODING_STATE_SS2 overrides; but
					     this probably indicates an error
					     in the text encoding. */
#define CODING_STATE_COMPOSITE  (1 << 8)  /* If set, we're currently processing
					     a composite character (i.e. a
					     character constructed by
					     overstriking two or more
					     characters). */


/* CODING_STATE_ISO2022_LOCK is the mask of flags that remain on until
   explicitly turned off when in the ISO2022 encoder/decoder.  Other flags are
   turned off at the end of processing each character or escape sequence. */
# define CODING_STATE_ISO2022_LOCK \
  (CODING_STATE_END | CODING_STATE_COMPOSITE | CODING_STATE_R2L)
#define CODING_STATE_BIG5_LOCK \
  CODING_STATE_END

/* Flags indicating what we've seen so far when parsing an
   ISO2022 escape sequence. */
enum iso_esc_flag
{
  /* Partial sequences */
  ISO_ESC_NOTHING,	/* Nothing has been seen. */
  ISO_ESC,		/* We've seen ESC. */
  ISO_ESC_2_4,		/* We've seen ESC $.  This indicates
			   that we're designating a multi-byte, rather
			   than a single-byte, character set. */
  ISO_ESC_2_8,		/* We've seen ESC 0x28, i.e. ESC (.
			   This means designate a 94-character
			   character set into G0. */
  ISO_ESC_2_9,		/* We've seen ESC 0x29 -- designate a
			   94-character character set into G1. */
  ISO_ESC_2_10,		/* We've seen ESC 0x2A. */
  ISO_ESC_2_11,		/* We've seen ESC 0x2B. */
  ISO_ESC_2_12,		/* We've seen ESC 0x2C -- designate a
			   96-character character set into G0.
			   (This is not ISO2022-standard.
			   The following 96-character
			   control sequences are standard,
			   though.) */
  ISO_ESC_2_13,		/* We've seen ESC 0x2D -- designate a
			   96-character character set into G1.
			   */
  ISO_ESC_2_14,		/* We've seen ESC 0x2E. */
  ISO_ESC_2_15,		/* We've seen ESC 0x2F. */
  ISO_ESC_2_4_8,	/* We've seen ESC $ 0x28 -- designate
			   a 94^N character set into G0. */
  ISO_ESC_2_4_9,	/* We've seen ESC $ 0x29. */
  ISO_ESC_2_4_10,	/* We've seen ESC $ 0x2A. */
  ISO_ESC_2_4_11,	/* We've seen ESC $ 0x2B. */
  ISO_ESC_2_4_12,	/* We've seen ESC $ 0x2C. */
  ISO_ESC_2_4_13,	/* We've seen ESC $ 0x2D. */
  ISO_ESC_2_4_14,	/* We've seen ESC $ 0x2E. */
  ISO_ESC_2_4_15,	/* We've seen ESC $ 0x2F. */
  ISO_ESC_5_11,		/* We've seen ESC [ or 0x9B.  This
			   starts a directionality-control
			   sequence.  The next character
			   must be 0, 1, 2, or ]. */
  ISO_ESC_5_11_0,	/* We've seen 0x9B 0.  The next
			   character must be ]. */
  ISO_ESC_5_11_1,	/* We've seen 0x9B 1.  The next
			   character must be ]. */
  ISO_ESC_5_11_2,	/* We've seen 0x9B 2.  The next
			   character must be ]. */

  /* Full sequences. */
  ISO_ESC_START_COMPOSITE, /* Private usage for START COMPOSING */
  ISO_ESC_END_COMPOSITE, /* Private usage for END COMPOSING */
  ISO_ESC_SINGLE_SHIFT, /* We've seen a complete single-shift sequence. */
  ISO_ESC_LOCKING_SHIFT,/* We've seen a complete locking-shift sequence. */
  ISO_ESC_DESIGNATE,	/* We've seen a complete designation sequence. */
  ISO_ESC_DIRECTIONALITY,/* We've seen a complete ISO6429 directionality
			   sequence. */
  ISO_ESC_LITERAL	/* We've seen a literal character ala
			   escape-quoting. */
};

/* Macros to define code of control characters for ISO2022's functions.  */
			/* code */	/* function */
#define ISO_CODE_LF	0x0A		/* line-feed */
#define ISO_CODE_CR	0x0D		/* carriage-return */
#define ISO_CODE_SO	0x0E		/* shift-out */
#define ISO_CODE_SI	0x0F		/* shift-in */
#define ISO_CODE_ESC	0x1B		/* escape */
#define ISO_CODE_DEL	0x7F		/* delete */
#define ISO_CODE_SS2	0x8E		/* single-shift-2 */
#define ISO_CODE_SS3	0x8F		/* single-shift-3 */
#define ISO_CODE_CSI	0x9B		/* control-sequence-introduce */

/* Macros to access an encoding stream or decoding stream */

#define CODING_STREAM_DECOMPOSE(str, flags, ch)	\
do {						\
  flags = (str)->flags;				\
  ch = (str)->ch;				\
} while (0)

#define CODING_STREAM_COMPOSE(str, flags, ch)	\
do {						\
  (str)->flags = flags;				\
  (str)->ch = ch;				\
} while (0)


/* For detecting the encoding of text */
enum coding_category_type
{
  CODING_CATEGORY_SHIFT_JIS,
  CODING_CATEGORY_ISO_7, /* ISO2022 system using only seven-bit bytes,
			    no locking shift */
  CODING_CATEGORY_ISO_8_DESIGNATE, /* ISO2022 system using eight-bit bytes,
				      no locking shift, no single shift,
				      using designation to switch charsets */
  CODING_CATEGORY_ISO_8_1, /* ISO2022 system using eight-bit bytes,
			      no locking shift, no designation sequences,
			      one-dimension characters in the upper half. */
  CODING_CATEGORY_ISO_8_2, /* ISO2022 system using eight-bit bytes,
			      no locking shift, no designation sequences,
			      two-dimension characters in the upper half. */
  CODING_CATEGORY_ISO_LOCK_SHIFT, /* ISO2022 system using locking shift */
  CODING_CATEGORY_BIG5,
  CODING_CATEGORY_NO_CONVERSION
};

#define CODING_CATEGORY_LAST CODING_CATEGORY_NO_CONVERSION

#define CODING_CATEGORY_SHIFT_JIS_MASK	\
  (1 << CODING_CATEGORY_SHIFT_JIS)
#define CODING_CATEGORY_ISO_7_MASK \
  (1 << CODING_CATEGORY_ISO_7)
#define CODING_CATEGORY_ISO_8_DESIGNATE_MASK \
  (1 << CODING_CATEGORY_ISO_8_DESIGNATE)
#define CODING_CATEGORY_ISO_8_1_MASK \
  (1 << CODING_CATEGORY_ISO_8_1)
#define CODING_CATEGORY_ISO_8_2_MASK \
  (1 << CODING_CATEGORY_ISO_8_2)
#define CODING_CATEGORY_ISO_LOCK_SHIFT_MASK \
  (1 << CODING_CATEGORY_ISO_LOCK_SHIFT)
#define CODING_CATEGORY_BIG5_MASK \
  (1 << CODING_CATEGORY_BIG5)
#define CODING_CATEGORY_NO_CONVERSION_MASK \
  (1 << CODING_CATEGORY_NO_CONVERSION)
#define CODING_CATEGORY_NOT_FINISHED_MASK \
  (1 << 30)

/* Convert shift-JIS code (sj1, sj2) into internal string
   representation (c1, c2). (The leading byte is assumed.) */

#define DECODE_SJIS(sj1, sj2, c1, c2)			\
do {							\
  int I1 = sj1, I2 = sj2;				\
  if (I2 >= 0x9f)					\
    c1 = (I1 << 1) - ((I1 >= 0xe0) ? 0xe0 : 0x60),	\
    c2 = I2 + 2;					\
  else							\
    c1 = (I1 << 1) - ((I1 >= 0xe0) ? 0xe1 : 0x61),	\
    c2 = I2 + ((I2 >= 0x7f) ? 0x60 : 0x61);		\
} while (0)

/* Convert the internal string representation of a Shift-JIS character
   (c1, c2) into Shift-JIS code (sj1, sj2).  The leading byte is
   assumed. */

#define ENCODE_SJIS(c1, c2, sj1, sj2)			\
do {							\
  int I1 = c1, I2 = c2;					\
  if (I1 & 1)						\
    sj1 = (I1 >> 1) + ((I1 < 0xdf) ? 0x31 : 0x71),	\
    sj2 = I2 - ((I2 >= 0xe0) ? 0x60 : 0x61);		\
  else							\
    sj1 = (I1 >> 1) + ((I1 < 0xdf) ? 0x30 : 0x70),	\
    sj2 = I2 - 2;					\
} while (0)

Lisp_Object make_decoding_input_stream  (Lstream *stream, Lisp_Object codesys);
Lisp_Object make_encoding_input_stream  (Lstream *stream, Lisp_Object codesys);
Lisp_Object make_decoding_output_stream (Lstream *stream, Lisp_Object codesys);
Lisp_Object make_encoding_output_stream (Lstream *stream, Lisp_Object codesys);
Lisp_Object decoding_stream_coding_system (Lstream *stream);
Lisp_Object encoding_stream_coding_system (Lstream *stream);
void set_decoding_stream_coding_system (Lstream *stream, Lisp_Object codesys);
void set_encoding_stream_coding_system (Lstream *stream, Lisp_Object codesys);
void determine_real_coding_system (Lstream *stream, Lisp_Object *codesys_in_out,
				   enum eol_type *eol_type_in_out);
#endif /* _XEMACS_MULE_CODING_H_ */
