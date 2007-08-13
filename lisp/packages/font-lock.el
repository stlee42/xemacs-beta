;;; font-lock.el --- decorating source files with fonts/colors based on syntax

;; Copyright (C) 1992-1995, 1997 Free Software Foundation, Inc.
;; Copyright (C) 1995 Amdahl Corporation.
;; Copyright (C) 1996 Ben Wing.

;; Author: Jamie Zawinski <jwz@netscape.com>, for the LISPM Preservation Society.
;; Minimally merged with FSF 19.34 by Barry Warsaw <bwarsaw@python.org>
;; Then (partially) synched with FSF 19.30, leading to:
;; Next Author: RMS
;; Next Author: Simon Marshall <simon@gnu.ai.mit.edu>
;; Latest XEmacs Author: Ben Wing
;; Maintainer: XEmacs Development Team (sigh :-( )
;; Keywords: languages, faces

;; This file is part of XEmacs.

;; XEmacs is free software; you can redistribute it and/or modify it
;; under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; XEmacs is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with XEmacs; see the file COPYING.  If not, write to the 
;; Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;;; Synched up with: FSF 19.30 except for the code to initialize the faces.

;;; Commentary:

;; Font-lock-mode is a minor mode that causes your comments to be
;; displayed in one face, strings in another, reserved words in another,
;; documentation strings in another, and so on.
;;
;; Comments will be displayed in `font-lock-comment-face'.
;; Strings will be displayed in `font-lock-string-face'.
;; Doc strings will be displayed in `font-lock-doc-string-face'.
;; Function and variable names (in their defining forms) will be
;;  displayed in `font-lock-function-name-face'.
;; Reserved words will be displayed in `font-lock-keyword-face'.
;;
;; Don't let the name fool you: you can highlight things using different
;; colors or background stipples instead of fonts, though that is not the
;; default.  See the variables `font-lock-use-colors' and
;; `font-lock-use-fonts' for broad control over this, or see the
;; documentation on faces and how to change their attributes for
;; fine-grained control.
;;
;; To make the text you type be fontified, use M-x font-lock-mode.  When
;; this minor mode is on, the fonts of the current line will be updated
;; with every insertion or deletion.
;;
;; By default, font-lock will automatically put newly loaded files
;; into font-lock-mode if it knows about the file's mode.  See the
;; variables `font-lock-auto-fontify', `font-lock-mode-enable-list',
;; and `font-lock-mode-disable-list' for control over this.
;;
;; The `font-lock-keywords' variable defines other patterns to highlight.
;; The default font-lock-mode-hook sets it to the value of the variables
;; lisp-font-lock-keywords, c-font-lock-keywords, etc, as appropriate.
;; The easiest way to change the highlighting patterns is to change the
;; values of c-font-lock-keywords and related variables.  See the doc
;; string of the variable `font-lock-keywords' for the appropriate syntax.
;;
;; The default value for `lisp-font-lock-keywords' is the value of the variable
;; `lisp-font-lock-keywords-1'.  You may like `lisp-font-lock-keywords-2' 
;; better; it highlights many more words, but is slower and makes your buffers
;; be very visually noisy.
;;
;; The same is true of `c-font-lock-keywords-1' and `c-font-lock-keywords-2';
;; the former is subdued, the latter is loud.
;;
;; You can make font-lock default to the gaudier variety of keyword
;; highlighting by setting the variable `font-lock-maximum-decoration'
;; before loading font-lock, or by calling the functions
;; `font-lock-use-default-maximal-decoration' or
;; `font-lock-use-default-minimal-decoration'.
;;
;; On a Sparc10, the initial fontification takes about 6 seconds for a typical
;; 140k file of C code, using the default configuration.  The actual speed
;; depends heavily on the type of code in the file, and how many non-syntactic
;; patterns match; for example, Xlib.h takes 23 seconds for 101k, because many
;; patterns match in it.  You can speed this up substantially by removing some
;; of the patterns that are highlighted by default.  Fontifying lisp code is
;; significantly faster, because lisp has a more regular syntax than C, so the
;; regular expressions don't have to be as complicated.
;;
;; It's called font-lock-mode here because on the Lispms it was called
;; "Electric Font Lock Mode."  It was called that because there was an older
;; mode called "Electric Caps Lock Mode" which had the function of causing all
;; of your source code to be in upper case except for strings and comments,
;; without you having to blip the caps lock key by hand all the time (thus the
;; "electric", as in `electric-c-brace'.)

;; See also the related packages `fast-lock' and `lazy-lock'.  Both
;; attempt to speed up the initial fontification.  `fast-lock' saves
;; the fontification info when you exit Emacs and reloads it next time
;; you load the file, so that the file doesn't have to be fontified
;; again.  `lazy-lock' does "lazy" fontification -- i.e. it only
;; fontifies the text as it becomes visible rather than fontifying
;; the whole file when it's first loaded in.

;; Further comments from the FSF:

;; Nasty regexps of the form "bar\\(\\|lo\\)\\|f\\(oo\\|u\\(\\|bar\\)\\)\\|lo"
;; are made thusly: (regexp-opt '("foo" "fu" "fubar" "bar" "barlo" "lo")) for
;; efficiency.

;; What is fontification for?  You might say, "It's to make my code look nice."
;; I think it should be for adding information in the form of cues.  These cues
;; should provide you with enough information to both (a) distinguish between
;; different items, and (b) identify the item meanings, without having to read
;; the items and think about it.  Therefore, fontification allows you to think
;; less about, say, the structure of code, and more about, say, why the code
;; doesn't work.  Or maybe it allows you to think less and drift off to sleep.
;;
;; So, here are my opinions/advice/guidelines:
;; 
;; - Use the same face for the same conceptual object, across all modes.
;;   i.e., (b) above, all modes that have items that can be thought of as, say,
;;   keywords, should be highlighted with the same face, etc.
;; - Keep the faces distinct from each other as far as possible.
;;   i.e., (a) above.
;; - Make the face attributes fit the concept as far as possible.
;;   i.e., function names might be a bold colour such as blue, comments might
;;   be a bright colour such as red, character strings might be brown, because,
;;   err, strings are brown (that was not the reason, please believe me).
;; - Don't use a non-nil OVERRIDE unless you have a good reason.
;;   Only use OVERRIDE for special things that are easy to define, such as the
;;   way `...' quotes are treated in strings and comments in Emacs Lisp mode.
;;   Don't use it to, say, highlight keywords in commented out code or strings.
;; - Err, that's it.


;;; Code:

(require 'fontl-hooks)

;;;;;;;;;;;;;;;;;;;;;;      user variables       ;;;;;;;;;;;;;;;;;;;;;;

(defvar font-lock-verbose t
  "*If non-nil, means show status messages when fontifying.
See also `font-lock-message-threshold'.")

(defvar font-lock-message-threshold 6000
  "*Minimum size of region being fontified for status messages to appear.

The size is measured in characters.  This affects `font-lock-fontify-region'
but not `font-lock-fontify-buffer'. (In other words, when you first visit
a file and it gets fontified, you will see status messages no matter what
size the file is.  However, if you do something else like paste a
chunk of text or revert a buffer, you will see status messages only if the
changed region is large enough.)

Note that setting `font-lock-verbose' to nil disables the status
messages entirely.")

;;;###autoload
(defvar font-lock-auto-fontify t
  "*Whether font-lock should automatically fontify files as they're loaded.
This will only happen if font-lock has fontifying keywords for the major
mode of the file.  You can get finer-grained control over auto-fontification
by using this variable in combination with `font-lock-mode-enable-list' or
`font-lock-mode-disable-list'.")

;;;###autoload
(defvar font-lock-mode-enable-list nil
  "*List of modes to auto-fontify, if `font-lock-auto-fontify' is nil.")

;;;###autoload
(defvar font-lock-mode-disable-list nil
  "*List of modes not to auto-fontify, if `font-lock-auto-fontify' is t.")

;;;###autoload
(defvar font-lock-use-colors '(color)
  "*Specification for when Font Lock will set up color defaults.
Normally this should be '(color), meaning that Font Lock will set up
color defaults that are only used on color displays.  Set this to nil
if you don't want Font Lock to set up color defaults at all.  This
should be one of

-- a list of valid tags, meaning that the color defaults will be used
   when all of the tags apply. (e.g. '(color x))
-- a list whose first element is 'or and whose remaining elements are
   lists of valid tags, meaning that the defaults will be used when
   any of the tag lists apply.
-- nil, meaning that the defaults should not be set up at all.

\(If you specify face values in your init file, they will override any
that Font Lock specifies, regardless of whether you specify the face
values before or after loading Font Lock.)

See also `font-lock-use-fonts'.  If you want more control over the faces
used for fontification, see the documentation of `font-lock-mode' for
how to do it.")

;;;###autoload
(defvar font-lock-use-fonts '(or (mono) (grayscale))
  "*Specification for when Font Lock will set up non-color defaults.

Normally this should be '(or (mono) (grayscale)), meaning that Font
Lock will set up non-color defaults that are only used on either mono
or grayscale displays.  Set this to nil if you don't want Font Lock to
set up non-color defaults at all.  This should be one of

-- a list of valid tags, meaning that the non-color defaults will be used
   when all of the tags apply. (e.g. '(grayscale x))
-- a list whose first element is 'or and whose remaining elements are
   lists of valid tags, meaning that the defaults will be used when
   any of the tag lists apply.
-- nil, meaning that the defaults should not be set up at all.

\(If you specify face values in your init file, they will override any
that Font Lock specifies, regardless of whether you specify the face
values before or after loading Font Lock.)

See also `font-lock-use-colors'.  If you want more control over the faces
used for fontification, see the documentation of `font-lock-mode' for
how to do it.")

;;;###autoload
(defvar font-lock-maximum-decoration nil
  "*If non-nil, the maximum decoration level for fontifying.
If nil, use the minimum decoration (equivalent to level 0).
If t, use the maximum decoration available.
If a number, use that level of decoration (or if not available the maximum).
If a list, each element should be a cons pair of the form (MAJOR-MODE . LEVEL),
where MAJOR-MODE is a symbol or t (meaning the default).  For example:
 ((c++-mode . 2) (c-mode . t) (t . 1))
means use level 2 decoration for buffers in `c++-mode', the maximum decoration
available for buffers in `c-mode', and level 1 decoration otherwise.")

;;;###autoload
(define-obsolete-variable-alias 'font-lock-use-maximal-decoration
  'font-lock-maximum-decoration)

;;;###autoload
(defvar font-lock-maximum-size (* 250 1024)
  "*If non-nil, the maximum size for buffers for fontifying.
Only buffers less than this can be fontified when Font Lock mode is turned on.
If nil, means size is irrelevant.
If a list, each element should be a cons pair of the form (MAJOR-MODE . SIZE),
where MAJOR-MODE is a symbol or t (meaning the default).  For example:
 ((c++-mode . 256000) (c-mode . 256000) (rmail-mode . 1048576))
means that the maximum size is 250K for buffers in `c++-mode' or `c-mode', one
megabyte for buffers in `rmail-mode', and size is irrelevant otherwise.")

;; Fontification variables:

;;;###autoload
(defvar font-lock-keywords nil
  "*A list of the keywords to highlight.
Each element should be of the form:

 MATCHER
 (MATCHER . MATCH)
 (MATCHER . FACENAME)
 (MATCHER . HIGHLIGHT)
 (MATCHER HIGHLIGHT ...)
 (eval . FORM)

where HIGHLIGHT should be either MATCH-HIGHLIGHT or MATCH-ANCHORED.

FORM is an expression, whose value should be a keyword element,
evaluated when the keyword is (first) used in a buffer.  This feature
can be used to provide a keyword that can only be generated when Font
Lock mode is actually turned on.

For highlighting single items, typically only MATCH-HIGHLIGHT is required.
However, if an item or (typically) items is to be highlighted following the
instance of another item (the anchor) then MATCH-ANCHORED may be required.

MATCH-HIGHLIGHT should be of the form:

 (MATCH FACENAME OVERRIDE LAXMATCH)

Where MATCHER can be either the regexp to search for, a variable
containing the regexp to search for, or the function to call to make
the search (called with one argument, the limit of the search).  MATCH
is the subexpression of MATCHER to be highlighted.  FACENAME is either
a symbol naming a face, or an expression whose value is the face name
to use.  If you want FACENAME to be a symbol that evaluates to a face,
use a form like \"(progn sym)\".

OVERRIDE and LAXMATCH are flags.  If OVERRIDE is t, existing fontification may
be overwritten.  If `keep', only parts not already fontified are highlighted.
If `prepend' or `append', existing fontification is merged with the new, in
which the new or existing fontification, respectively, takes precedence.
If LAXMATCH is non-nil, no error is signalled if there is no MATCH in MATCHER.

For example, an element of the form highlights (if not already highlighted):

 \"\\\\\\=<foo\\\\\\=>\"		Discrete occurrences of \"foo\" in the value of the
			variable `font-lock-keyword-face'.
 (\"fu\\\\(bar\\\\)\" . 1)	Substring \"bar\" within all occurrences of \"fubar\" in
			the value of `font-lock-keyword-face'.
 (\"fubar\" . fubar-face)	Occurrences of \"fubar\" in the value of `fubar-face'.
 (\"foo\\\\|bar\" 0 foo-bar-face t)
			Occurrences of either \"foo\" or \"bar\" in the value
			of `foo-bar-face', even if already highlighted.

MATCH-ANCHORED should be of the form:

 (MATCHER PRE-MATCH-FORM POST-MATCH-FORM MATCH-HIGHLIGHT ...)

Where MATCHER is as for MATCH-HIGHLIGHT with one exception; see below.
PRE-MATCH-FORM and POST-MATCH-FORM are evaluated before the first, and after
the last, instance MATCH-ANCHORED's MATCHER is used.  Therefore they can be
used to initialise before, and cleanup after, MATCHER is used.  Typically,
PRE-MATCH-FORM is used to move to some position relative to the original
MATCHER, before starting with MATCH-ANCHORED's MATCHER.  POST-MATCH-FORM might
be used to move, before resuming with MATCH-ANCHORED's parent's MATCHER.

For example, an element of the form highlights (if not already highlighted):

 (\"\\\\\\=<anchor\\\\\\=>\" (0 anchor-face) (\"\\\\\\=<item\\\\\\=>\" nil nil (0 item-face)))

 Discrete occurrences of \"anchor\" in the value of `anchor-face', and subsequent
 discrete occurrences of \"item\" (on the same line) in the value of `item-face'.
 (Here PRE-MATCH-FORM and POST-MATCH-FORM are nil.  Therefore \"item\" is
 initially searched for starting from the end of the match of \"anchor\", and
 searching for subsequent instance of \"anchor\" resumes from where searching
 for \"item\" concluded.)

The above-mentioned exception is as follows.  The limit of the MATCHER search
defaults to the end of the line after PRE-MATCH-FORM is evaluated.
However, if PRE-MATCH-FORM returns a position greater than the position after
PRE-MATCH-FORM is evaluated, that position is used as the limit of the search.
It is generally a bad idea to return a position greater than the end of the
line, i.e., cause the MATCHER search to span lines.

Note that the MATCH-ANCHORED feature is experimental; in the future, we may
replace it with other ways of providing this functionality.

These regular expressions should not match text which spans lines.  While
\\[font-lock-fontify-buffer] handles multi-line patterns correctly, updating
when you edit the buffer does not, since it considers text one line at a time.

Be very careful composing regexps for this list;
the wrong pattern can dramatically slow things down!")
;;;###autoload
(make-variable-buffer-local 'font-lock-keywords)

(defvar font-lock-defaults nil
  "The defaults font Font Lock mode for the current buffer.
Normally, do not set this directly.  If you are writing a major mode,
put a property of `font-lock-defaults' on the major-mode symbol with
the desired value.

It should be a list

\(KEYWORDS KEYWORDS-ONLY CASE-FOLD SYNTAX-ALIST SYNTAX-BEGIN)

KEYWORDS may be a symbol (a variable or function whose value is the keywords
to use for fontification) or a list of symbols.  If KEYWORDS-ONLY is non-nil,
syntactic fontification (strings and comments) is not performed.  If CASE-FOLD
is non-nil, the case of the keywords is ignored when fontifying.  If
SYNTAX-ALIST is non-nil, it should be a list of cons pairs of the form (CHAR
. STRING) used to set the local Font Lock syntax table, for keyword and
syntactic fontification (see `modify-syntax-entry').

If SYNTAX-BEGIN is non-nil, it should be a function with no args used to move
backwards outside any enclosing syntactic block, for syntactic fontification.
Typical values are `beginning-of-line' (i.e., the start of the line is known to
be outside a syntactic block), or `beginning-of-defun' for programming modes or
`backward-paragraph' for textual modes (i.e., the mode-dependent function is
known to move outside a syntactic block).  If nil, the beginning of the buffer
is used as a position outside of a syntactic block, in the worst case.

These item elements are used by Font Lock mode to set the variables
`font-lock-keywords', `font-lock-keywords-only',
`font-lock-keywords-case-fold-search', `font-lock-syntax-table' and
`font-lock-beginning-of-syntax-function', respectively.

Alternatively, if the value is a symbol, it should name a major mode,
and the defaults for that mode will apply.")
(make-variable-buffer-local 'font-lock-defaults)

;; FSF uses `font-lock-defaults-alist' and expects the major mode to
;; set a value for `font-lock-defaults', but I don't like either of
;; these -- requiring the mode to set `font-lock-defaults' makes it
;; impossible to have defaults for a minor mode, and using an alist is
;; generally a bad idea for information that really should be
;; decentralized. (Who knows what strange modes might want
;; font-locking?)

(defvar font-lock-keywords-only nil
  "Non-nil means Font Lock should not do syntactic fontification.
This is normally set via `font-lock-defaults'.

This should be nil for all ``language'' modes, but other modes, like
dired, do not have anything useful in the syntax tables (no comment
or string delimiters, etc) and so there is no need to use them and
this variable should have a value of t.

You should not set this variable directly; its value is computed
from `font-lock-defaults', or (if that does not specify anything)
by examining the syntax table to see whether it appears to contain
anything useful.")
(make-variable-buffer-local 'font-lock-keywords-only)

(defvar font-lock-keywords-case-fold-search nil
  "Whether the strings in `font-lock-keywords' should be case-folded.
This variable is automatically buffer-local, as the correct value depends
on the language in use.")
(make-variable-buffer-local 'font-lock-keywords-case-fold-search)

(defvar font-lock-after-fontify-buffer-hook nil
  "Function or functions to run after completion of font-lock-fontify-buffer.")

(defvar font-lock-syntax-table nil
  "Non-nil means use this syntax table for fontifying.
If this is nil, the major mode's syntax table is used.
This is normally set via `font-lock-defaults'.")
(make-variable-buffer-local 'font-lock-syntax-table)

;; These are used in the FSF version in syntactic font-locking.
;; We do this all in C.
;;; These record the parse state at a particular position, always the
;;; start of a line.  Used to make
;;; `font-lock-fontify-syntactically-region' faster.
;(defvar font-lock-cache-position nil)
;(defvar font-lock-cache-state nil)
;(make-variable-buffer-local 'font-lock-cache-position)
;(make-variable-buffer-local 'font-lock-cache-state)

;; If this is nil, we only use the beginning of the buffer if we can't use
;; `font-lock-cache-position' and `font-lock-cache-state'.
(defvar font-lock-beginning-of-syntax-function nil
  "Non-nil means use this function to move back outside of a syntactic block.
If this is nil, the beginning of the buffer is used (in the worst case).
This is normally set via `font-lock-defaults'.")
(make-variable-buffer-local 'font-lock-beginning-of-syntax-function)

;;;###autoload
(defvar font-lock-mode nil) ; for modeline
(defvar font-lock-fontified nil) ; whether we have hacked this buffer
(put 'font-lock-fontified 'permanent-local t)

;;;###autoload
(defvar font-lock-mode-hook nil
  "Function or functions to run on entry to font-lock-mode.")

; whether font-lock-set-defaults has already been run.
(defvar font-lock-defaults-computed nil)
(make-variable-buffer-local 'font-lock-defaults-computed)

;; #### barf gag retch.  Horrid FSF lossage that we need to
;; keep around for compatibility with font-lock-keywords that
;; forget to properly quote their faces.
(defvar font-lock-comment-face 'font-lock-comment-face
  "Don't even think of using this.")
(defvar font-lock-doc-string-face 'font-lock-doc-string-face
  "Don't even think of using this.")
(defvar font-lock-string-face 'font-lock-string-face
  "Don't even think of using this.")
(defvar font-lock-keyword-face 'font-lock-keyword-face
  "Don't even think of using this.")
(defvar font-lock-function-name-face 'font-lock-function-name-face
  "Don't even think of using this.")
(defvar font-lock-variable-name-face 'font-lock-variable-name-face
  "Don't even think of using this.")
(defvar font-lock-type-face 'font-lock-type-face
  "Don't even think of using this.")
(defvar font-lock-reference-face 'font-lock-reference-face
  "Don't even think of using this.")
(defvar font-lock-preprocessor-face 'font-lock-preprocessor-face
  "Don't even think of using this.")


;;;;;;;;;;;;;;;;;;;;;;        actual code        ;;;;;;;;;;;;;;;;;;;;;;

;;; To fontify the whole buffer by language syntax, we go through it a
;;; character at a time, creating extents on the boundary of each syntactic
;;; unit (that is, one extent for each block comment, one for each line
;;; comment, one for each string, etc.)  This is done with the C function
;;; syntactically-sectionize.  It's in C for speed (the speed of lisp function
;;; calls was a real bottleneck for this task since it involves examining each
;;; character in turn.)
;;;
;;; Then we make a second pass, to fontify the buffer based on other patterns
;;; specified by regexp.  When we find a match for a region of text, we need
;;; to change the fonts on those characters.  This is done with the
;;; put-text-property function, which knows how to efficiently share extents.
;;; Conceptually, we are attaching some particular face to each of the
;;; characters in a range, but the implementation of this involves creating
;;; extents, or resizing existing ones.
;;;
;;; Each time a modification happens to a line, we re-fontify the entire line.
;;; We do this by first removing the extents (text properties) on the line,
;;; and then doing the syntactic and keyword passes again on that line.  (More
;;; generally, each modified region is extended to include the preceding and
;;; following BOL or EOL.)
;;;
;;; This means that, as the user types, we repeatedly go back to the beginning
;;; of the line, doing more work the longer the line gets.  This doesn't cost
;;; much in practice, and if we don't, then we incorrectly fontify things when,
;;; for example, inserting spaces into `intfoo () {}'.
;;;


;; The user level functions

;;;###autoload
(defun font-lock-mode (&optional arg)
  "Toggle Font Lock Mode.
With arg, turn font-lock mode on if and only if arg is positive.

When Font Lock mode is enabled, text is fontified as you type it:

 - Comments are displayed in `font-lock-comment-face';
 - Strings are displayed in `font-lock-string-face';
 - Documentation strings (in Lisp-like languages) are displayed in
   `font-lock-doc-string-face';
 - Language keywords (\"reserved words\") are displayed in
   `font-lock-keyword-face';
 - Function names in their defining form are displayed in
   `font-lock-function-name-face';
 - Variable names in their defining form are displayed in
   `font-lock-variable-name-face';
 - Type names are displayed in `font-lock-type-face';
 - References appearing in help files and the like are displayed
   in `font-lock-reference-face';
 - Preprocessor declarations are displayed in
  `font-lock-preprocessor-face';

   and

 - Certain other expressions are displayed in other faces according
   to the value of the variable `font-lock-keywords'.

Where modes support different levels of fontification, you can use the variable
`font-lock-maximum-decoration' to specify which level you generally prefer.
When you turn Font Lock mode on/off the buffer is fontified/defontified, though
fontification occurs only if the buffer is less than `font-lock-maximum-size'.
To fontify a buffer without turning on Font Lock mode, and regardless of buffer
size, you can use \\[font-lock-fontify-buffer].

See the variable `font-lock-keywords' for customization."
  (interactive "P")
  (let ((on-p (if arg (> (prefix-numeric-value arg) 0) (not font-lock-mode)))
	(maximum-size (if (not (consp font-lock-maximum-size))
			  font-lock-maximum-size
			(cdr (or (assq major-mode font-lock-maximum-size)
				 (assq t font-lock-maximum-size))))))
    ;; Font-lock mode will refuse to turn itself on if in batch mode, or if
    ;; the current buffer is "invisible".  The latter is because packages
    ;; sometimes put their temporary buffers into some particular major mode
    ;; to get syntax tables and variables and whatnot, but we don't want the
    ;; fact that the user has font-lock-mode on a mode hook to slow these
    ;; things down.
    (if (or noninteractive (eq (aref (buffer-name) 0) ?\ ))
	(setq on-p nil))
    (if (equal (buffer-name) " *Compiler Input*") ; hack for bytecomp...
	(setq on-p nil))
    (cond (on-p
	   (make-local-hook 'after-change-functions)
	   (add-hook 'after-change-functions
		     'font-lock-after-change-function nil t)
	   (add-hook 'pre-idle-hook 'font-lock-pre-idle-hook))
	  (t
	   (remove-hook 'after-change-functions
			'font-lock-after-change-function t)
	   (setq font-lock-defaults-computed nil
		 font-lock-keywords nil)
	   ;; We have no business doing this here, since 
	   ;; pre-idle-hook is global.	Other buffers may
	   ;; still be in font-lock mode.  -dkindred@cs.cmu.edu
	   ;; (remove-hook 'pre-idle-hook 'font-lock-pre-idle-hook)
	   ))
    (set (make-local-variable 'font-lock-mode) on-p)
    (cond (on-p
	   (font-lock-set-defaults-1)
	   (make-local-hook 'before-revert-hook)
	   (make-local-hook 'after-revert-hook)
	   ;; If buffer is reverted, must clean up the state.
	   (add-hook 'before-revert-hook 'font-lock-revert-setup nil t)
	   (add-hook 'after-revert-hook 'font-lock-revert-cleanup nil t)
	   (run-hooks 'font-lock-mode-hook)
	   (cond (font-lock-fontified
		  nil)
		 ((or (null maximum-size) (<= (buffer-size) maximum-size))
		  (font-lock-fontify-buffer))
		 (font-lock-verbose
		  (display-message
		   'command
		   (format "Fontifying %s... buffer too big." (buffer-name))))))
	  (font-lock-fontified
	   (setq font-lock-fontified nil)
	   (remove-hook 'before-revert-hook 'font-lock-revert-setup t)
	   (remove-hook 'after-revert-hook 'font-lock-revert-cleanup t)
	   (font-lock-unfontify-region (point-min) (point-max))
	   (font-lock-thing-lock-cleanup))
	  (t
	   (remove-hook 'before-revert-hook 'font-lock-revert-setup t)
	   (remove-hook 'after-revert-hook 'font-lock-revert-cleanup t)
	   (font-lock-thing-lock-cleanup)))
    (redraw-modeline)))

;; For init-file hooks
;;;###autoload
(defun turn-on-font-lock ()
  "Unconditionally turn on Font Lock mode."
  (font-lock-mode 1))

;;;###autoload
(defun turn-off-font-lock ()
  "Unconditionally turn off Font Lock mode."
  (font-lock-mode 0))

;;;###autoload
(defun font-lock-fontify-buffer ()
  "Fontify the current buffer the way `font-lock-mode' would.
See `font-lock-mode' for details.

This can take a while for large buffers."
  (interactive)
  (let ((was-on font-lock-mode)
	(font-lock-verbose (or font-lock-verbose (interactive-p)))
	(font-lock-message-threshold 0)
	(aborted nil))
    ;; Turn it on to run hooks and get the right font-lock-keywords.
    (or was-on (font-lock-mode 1))
    (font-lock-unfontify-region (point-min) (point-max) t)
;;    (buffer-syntactic-context-flush-cache)
    
    ;; If a ^G is typed during fontification, abort the fontification, but
    ;; return normally (do not signal.)  This is to make it easy to abort
    ;; fontification if it's taking a long time, without also causing the
    ;; buffer not to pop up.  If a real abort is desired, the user can ^G
    ;; again.
    ;;
    ;; Possibly this should happen down in font-lock-fontify-region instead
    ;; of here, but since that happens from the after-change-hook (meaning
    ;; much more frequently) I'm afraid of the bad consequences of stealing
    ;; the interrupt character at inopportune times.
    ;;
    (condition-case nil
	(save-excursion
	  (font-lock-fontify-region (point-min) (point-max)))
      (quit
       (setq aborted t)))

    (or was-on		; turn it off if it was off.
	(let ((font-lock-fontified nil)) ; kludge to prevent defontification
	  (font-lock-mode 0)))
    (set (make-local-variable 'font-lock-fontified) t)
    (if (and aborted font-lock-verbose)
	(display-message 'command
			 (format "Fontifying %s... aborted." (buffer-name))))
    )
  (run-hooks 'font-lock-after-fontify-buffer-hook))

;; Fontification functions.

;; We first define some defsubsts to encapsulate the way we add
;; faces to a region of text.  I am planning on modifying the
;; text-property mechanism so that multiple independent classes
;; of text properties can exist.  That way, for example, ediff's
;; face text properties don't interfere with font lock's face
;; text properties.  Due to the XEmacs implementation of text
;; properties in terms of extents, doing this is fairly trivial:
;; instead of using the `text-prop' property, you just use a
;; specified property.

(defsubst font-lock-set-face (start end face)
  ;; Set the face on the characters in the range.
  (put-nonduplicable-text-property start end 'face face)
  (put-nonduplicable-text-property start end 'font-lock t))

(defsubst font-lock-remove-face (start end)
  ;; Remove any syntax highlighting on the characters in the range.
  (put-nonduplicable-text-property start end 'face nil)
  (put-nonduplicable-text-property start end 'font-lock nil))

(defsubst font-lock-any-faces-p (start end)
  ;; Return non-nil if we've put any syntax highlighting on
  ;; the characters in the range.
  ;;
  ;; used to look for 'text-prop property, but this has problems if
  ;; you put any other text properties in the vicinity.  Simon
  ;; Marshall suggested looking for the 'face property (this is what
  ;; FSF Emacs does) but that's equally bogus.  Only reliable way is
  ;; for font-lock to specially mark its extents.
  ;;
  ;; FSF's (equivalent) definition of this defsubst would be
  ;; (text-property-not-all start end 'font-lock nil)
  ;;
  ;; Perhaps our `map-extents' is faster than our definition
  ;; of `text-property-not-all'.  #### If so, `text-property-not-all'
  ;; should be fixed ...
  ;;
  (map-extents 'extent-property (current-buffer) start (1- end) 'font-lock))


;; Fontification functions.

;; We use this wrapper.  However, `font-lock-fontify-region' used to be the
;; name used for `font-lock-fontify-syntactically-region', so a change isn't
;; back-compatible.  But you shouldn't be calling these directly, should you?
(defun font-lock-fontify-region (beg end &optional loudly)
  (let ((modified (buffer-modified-p))
	(buffer-undo-list t) (inhibit-read-only t)
	(old-syntax-table (syntax-table))
	buffer-file-name buffer-file-truename)
    (unwind-protect
	(progn
	  ;; Use the fontification syntax table, if any.
	  (if font-lock-syntax-table (set-syntax-table font-lock-syntax-table))
	  ;; Now do the fontification.
	  (if font-lock-keywords-only
	      (font-lock-unfontify-region beg end)
	    (font-lock-fontify-syntactically-region beg end loudly))
	  (font-lock-fontify-keywords-region beg end loudly))
      ;; Clean up.
      (set-syntax-table old-syntax-table)
      (and (not modified) (buffer-modified-p) (set-buffer-modified-p nil)))))

;; The following must be rethought, since keywords can override fontification.
;      ;; Now scan for keywords, but not if we are inside a comment now.
;      (or (and (not font-lock-keywords-only)
;	       (let ((state (parse-partial-sexp beg end nil nil 
;						font-lock-cache-state)))
;		 (or (nth 4 state) (nth 7 state))))
;	  (font-lock-fontify-keywords-region beg end))

(defun font-lock-unfontify-region (beg end &optional maybe-loudly)
  (if (and maybe-loudly font-lock-verbose
	   (>= (- end beg) font-lock-message-threshold))
      (display-message
       'progress
       (format "Fontifying %s..." (buffer-name))))
  (let ((modified (buffer-modified-p))
	(buffer-undo-list t) (inhibit-read-only t)
	buffer-file-name buffer-file-truename)
    (font-lock-remove-face beg end)
    (and (not modified) (buffer-modified-p) (set-buffer-modified-p nil))))

;; Following is the original FSF version (similar to our original
;; version, before all the crap I added below).
;;
;; Probably that crap should either be fixed up so it works better,
;; or tossed away.
;;
;; I think that lazy-lock v2 tries to do something similar.
;; Those efforts should be merged.

;; Called when any modification is made to buffer text.
;(defun font-lock-after-change-function (beg end old-len)
;  (save-excursion
;    (save-match-data
;      ;; Rescan between start of line from `beg' and start of line after `end'.
;      (font-lock-fontify-region
;	(progn (goto-char beg) (beginning-of-line) (point))
;	(progn (goto-char end) (forward-line 1) (point))))))

(defvar font-lock-old-extent nil)
(defvar font-lock-old-len 0)

(defun font-lock-fontify-glumped-region ()
  ;; even if something goes wrong in the fontification, mark the glumped
  ;; region as fontified; otherwise, the same error might get signaled
  ;; after every command.
  (unwind-protect
      ;; buffer may be deleted.
      (if (buffer-live-p (extent-object font-lock-old-extent))
	  (save-excursion
	    (set-buffer (extent-object font-lock-old-extent))
	    (font-lock-after-change-function-1
	     (extent-start-position font-lock-old-extent)
	     (extent-end-position font-lock-old-extent)
	     font-lock-old-len)))
    (detach-extent font-lock-old-extent)
    (setq font-lock-old-extent nil)))

(defun font-lock-pre-idle-hook ()
  (condition-case nil
      (if font-lock-old-extent
	  (font-lock-fontify-glumped-region))
    (error (warn "Error caught in `font-lock-pre-idle-hook'"))))

(defvar font-lock-always-fontify-immediately nil
  "Set this to non-nil to disable font-lock deferral.")

;;; called when any modification is made to buffer text.  This function
;;; attempts to glump adjacent changes together so that excessive
;;; fontification is avoided.  This function could easily be adapted
;;; to other after-change-functions.

(defun font-lock-after-change-function (beg end old-len)
  (let ((obeg (and font-lock-old-extent
		   (extent-start-position font-lock-old-extent)))
	(oend (and font-lock-old-extent
		   (extent-end-position font-lock-old-extent)))
	(bc-end (+ beg old-len)))

    ;; If this change can't be merged into the glumped one,
    ;; we need to fontify the glumped one right now.
    (if (and font-lock-old-extent
	     (or (not (eq (current-buffer)
			  (extent-object font-lock-old-extent)))
		 (< bc-end obeg)
		 (> beg oend)))
	(font-lock-fontify-glumped-region))
  
    (if font-lock-old-extent
	;; Update glumped region.
	(progn
	  ;; Any characters in the before-change region that are
	  ;; outside the glumped region go into the glumped
	  ;; before-change region.
	  (if (> bc-end oend)
	      (setq font-lock-old-len (+ font-lock-old-len (- bc-end oend))))
	  (if (> obeg beg)
	      (setq font-lock-old-len (+ font-lock-old-len (- obeg beg))))
	  ;; New glumped region is the union of the glumped region
	  ;; and the new region.
	  (set-extent-endpoints font-lock-old-extent
				(min obeg beg)
				(max oend end)))

      ;; No glumped region, so create one.
      (setq font-lock-old-extent (make-extent beg end))
      (set-extent-property font-lock-old-extent 'detachable nil)
      (set-extent-property font-lock-old-extent 'end-open nil)
      (setq font-lock-old-len old-len))

    (if font-lock-always-fontify-immediately
	(font-lock-fontify-glumped-region))))

(defun font-lock-after-change-function-1 (beg end old-len)
  (if (null font-lock-mode)
      nil
    (save-excursion
      (save-restriction
	;; if we don't widen, then fill-paragraph (and any command that
	;; operates on a narrowed region) confuses things, because the C
	;; code will fail to realize that we're inside a comment.
	(widen)
	(save-match-data
	  (let ((zmacs-region-stays zmacs-region-stays)) ; protect from change!
	    (goto-char beg)
	    ;; Maybe flush the internal cache used by syntactically-sectionize.
	    ;; (It'd be nice if this was more automatic.)  Any deletions mean
	    ;; the cache is invalid, and insertions at beginning or end of line
	    ;; mean that the bol cache might be invalid.
;;	    (if (or (> old-len 0) (bobp) (= (preceding-char) ?\n))
;;		(buffer-syntactic-context-flush-cache))

	    ;; Always recompute the whole line.
	    (goto-char end)
	    (forward-line 1)
	    (setq end (point))
	    (goto-char beg)
	    (beginning-of-line)
	    (setq beg (point))
	    ;; Rescan between start of line from `beg' and start of line after
	    ;; `end'.
	    (font-lock-fontify-region beg end)))))))


;; Syntactic fontification functions.

;; Note: Here is the FSF version.  Our version is much faster because
;; of the C support we provide.  This may be useful for reference,
;; however, and perhaps there is something useful here that should
;; be merged into our version.
;;
;(defun font-lock-fontify-syntactically-region (start end &optional loudly)
;  "Put proper face on each string and comment between START and END.
;START should be at the beginning of a line."
;  (let ((synstart (if comment-start-skip
;		       (concat "\\s\"\\|" comment-start-skip)
;		     "\\s\""))
;	 (comstart (if comment-start-skip
;		       (concat "\\s<\\|" comment-start-skip)
;		     "\\s<"))
;	 state prev prevstate)
;    (if loudly (message "Fontifying %s... (syntactically...)" (buffer-name)))
;    (save-restriction
;      (widen)
;      (goto-char start)
;      ;;
;      ;; Find the state at the `beginning-of-line' before `start'.
;      (if (eq start font-lock-cache-position)
;	   ;; Use the cache for the state of `start'.
;	   (setq state font-lock-cache-state)
;	 ;; Find the state of `start'.
;	 (if (null font-lock-beginning-of-syntax-function)
;	     ;; Use the state at the previous cache position, if any, or
;	     ;; otherwise calculate from `point-min'.
;	     (if (or (null font-lock-cache-position)
;		     (< start font-lock-cache-position))
;		 (setq state (parse-partial-sexp (point-min) start))
;	       (setq state (parse-partial-sexp font-lock-cache-position start
;					       nil nil font-lock-cache-state)))
;	   ;; Call the function to move outside any syntactic block.
;	   (funcall font-lock-beginning-of-syntax-function)
;	   (setq state (parse-partial-sexp (point) start)))
;	 ;; Cache the state and position of `start'.
;	 (setq font-lock-cache-state state
;	       font-lock-cache-position start))
;      ;;
;      ;; If the region starts inside a string, show the extent of it.
;      (if (nth 3 state)
;	   (let ((beg (point)))
;	     (while (and (re-search-forward "\\s\"" end 'move)
;			 (nth 3 (parse-partial-sexp beg (point)
;						    nil nil state))))
;	     (put-text-property beg (point) 'face font-lock-string-face)
;	     (setq state (parse-partial-sexp beg (point) nil nil state))))
;      ;;
;      ;; Likewise for a comment.
;      (if (or (nth 4 state) (nth 7 state))
;	   (let ((beg (point)))
;	     (save-restriction
;	       (narrow-to-region (point-min) end)
;	       (condition-case nil
;		   (progn
;		     (re-search-backward comstart (point-min) 'move)
;		     (forward-comment 1)
;		     ;; forward-comment skips all whitespace,
;		     ;; so go back to the real end of the comment.
;		     (skip-chars-backward " \t"))
;		 (error (goto-char end))))
;	     (put-text-property beg (point) 'face font-lock-comment-face)
;	     (setq state (parse-partial-sexp beg (point) nil nil state))))
;      ;;
;      ;; Find each interesting place between here and `end'.
;      (while (and (< (point) end)
;		   (setq prev (point) prevstate state)
;		   (re-search-forward synstart end t)
;		   (progn
;		     ;; Clear out the fonts of what we skip over.
;		     (remove-text-properties prev (point) '(face nil))
;		     ;; Verify the state at that place
;		     ;; so we don't get fooled by \" or \;.
;		     (setq state (parse-partial-sexp prev (point)
;						     nil nil state))))
;	 (let ((here (point)))
;	   (if (or (nth 4 state) (nth 7 state))
;	       ;;
;	       ;; We found a real comment start.
;	       (let ((beg (match-beginning 0)))
;		 (goto-char beg)
;		 (save-restriction
;		   (narrow-to-region (point-min) end)
;		   (condition-case nil
;		       (progn
;			 (forward-comment 1)
;			 ;; forward-comment skips all whitespace,
;			 ;; so go back to the real end of the comment.
;			 (skip-chars-backward " \t"))
;		     (error (goto-char end))))
;		 (put-text-property beg (point) 'face
;				    font-lock-comment-face)
;		 (setq state (parse-partial-sexp here (point) nil nil state)))
;	     (if (nth 3 state)
;		 ;;
;		 ;; We found a real string start.
;		 (let ((beg (match-beginning 0)))
;		   (while (and (re-search-forward "\\s\"" end 'move)
;			       (nth 3 (parse-partial-sexp here (point)
;							  nil nil state))))
;		   (put-text-property beg (point) 'face font-lock-string-face)
;		   (setq state (parse-partial-sexp here (point)
;						   nil nil state))))))
;	 ;;
;	 ;; Make sure `prev' is non-nil after the loop
;	 ;; only if it was set on the very last iteration.
;	 (setq prev nil)))
;    ;;
;    ;; Clean up.
;    (and prev (remove-text-properties prev end '(face nil)))))

(defun font-lock-fontify-syntactically-region (start end &optional loudly)
  "Put proper face on each string and comment between START and END.
START should be at the beginning of a line."
  (if font-lock-keywords-only
      nil
    (if (and font-lock-verbose
	     (>= (- end start) font-lock-message-threshold))
	(display-message
	 'progress
	 (format "Fontifying %s... (syntactically...)" (buffer-name))))
    (font-lock-unfontify-region start end loudly)
    (goto-char start)
    (if (> end (point-max)) (setq end (point-max)))
    (syntactically-sectionize
      #'(lambda (s e context depth)
	  (let (face)
	    (cond ((eq context 'string)
		   ;;#### Should only do this is Lisp-like modes!
		   (setq face
			 (if (= depth 1)
			     ;; really we should only use this if
			     ;;  in position 3 depth 1, but that's
			     ;;  too expensive to compute.
			     'font-lock-doc-string-face
			   'font-lock-string-face)))
		  ((or (eq context 'comment)
		       (eq context 'block-comment))
		   (setq face 'font-lock-comment-face)
;		 ;; Don't fontify whitespace at the beginning of lines;
;		 ;;  otherwise comment blocks may not line up with code.
;		 ;; (This is sometimes a good idea, sometimes not; in any
;		 ;; event it should be in C for speed --jwz)
;		 (save-excursion
;		     (goto-char s)
;		     (while (prog1 (search-forward "\n" (1- e) 'move)
;			      (setq face 'font-lock-comment-face)
;			      (setq e (point)))
;		       (skip-chars-forward " \t\n")
;		       (setq s (point)))
		   ))
	    (font-lock-set-face s e face)))
      start end)
    ))

;;; Additional text property functions.

;; The following three text property functions are not generally available (and
;; it's not certain that they should be) so they are inlined for speed.
;; The case for `fillin-text-property' is simple; it may or not be generally
;; useful.  (Since it is used here, it is useful in at least one place.;-)
;; However, the case for `append-text-property' and `prepend-text-property' is
;; more complicated.  Should they remove duplicate property values or not?  If
;; so, should the first or last duplicate item remain?  Or the one that was
;; added?  In our implementation, the first duplicate remains.

;; XEmacs: modified all these functions to use
;; `put-nonduplicable-text-property' instead of `put-text-property', and
;; the first one to take both SETPROP and MARKPROP, in accordance with the
;; changed definitions of `font-lock-any-faces-p' and `font-lock-set-face'.

(defsubst font-lock-fillin-text-property (start end setprop markprop value &optional object)
  "Fill in one property of the text from START to END.
Arguments PROP and VALUE specify the property and value to put where none are
already in place.  Therefore existing property values are not overwritten.
Optional argument OBJECT is the string or buffer containing the text."
  (let ((start (text-property-any start end markprop nil object)) next)
    (while start
      (setq next (next-single-property-change start markprop object end))
      (put-nonduplicable-text-property start next setprop value object)
      (put-nonduplicable-text-property start next markprop value object)
      (setq start (text-property-any next end markprop nil object)))))

;; This function (from simon's unique.el) is rewritten and inlined for speed.
;(defun unique (list function)
;  "Uniquify LIST, deleting elements using FUNCTION.
;Return the list with subsequent duplicate items removed by side effects.
;FUNCTION is called with an element of LIST and a list of elements from LIST,
;and should return the list of elements with occurrences of the element removed,
;i.e., a function such as `delete' or `delq'.
;This function will work even if LIST is unsorted.  See also `uniq'."
;  (let ((list list))
;    (while list
;      (setq list (setcdr list (funcall function (car list) (cdr list))))))
;  list)

(defsubst font-lock-unique (list)
  "Uniquify LIST, deleting elements using `delq'.
Return the list with subsequent duplicate items removed by side effects."
  (let ((list list))
    (while list
      (setq list (setcdr list (delq (car list) (cdr list))))))
  list)

;; A generalisation of `facemenu-add-face' for any property, but without the
;; removal of inactive faces via `facemenu-discard-redundant-faces' and special
;; treatment of `default'.  Uses `unique' to remove duplicate property values.
(defsubst font-lock-prepend-text-property (start end prop value &optional object)
  "Prepend to one property of the text from START to END.
Arguments PROP and VALUE specify the property and value to prepend to the value
already in place.  The resulting property values are always lists, and unique.
Optional argument OBJECT is the string or buffer containing the text."
  (let ((val (if (listp value) value (list value))) next prev)
    (while (/= start end)
      (setq next (next-single-property-change start prop object end)
	    prev (get-text-property start prop object))
      (put-text-property
       start next prop
       (font-lock-unique (append val (if (listp prev) prev (list prev))))
       object)
      (setq start next))))

(defsubst font-lock-append-text-property (start end prop value &optional object)
  "Append to one property of the text from START to END.
Arguments PROP and VALUE specify the property and value to append to the value
already in place.  The resulting property values are always lists, and unique.
Optional argument OBJECT is the string or buffer containing the text."
  (let ((val (if (listp value) value (list value))) next prev)
    (while (/= start end)
      (setq next (next-single-property-change start prop object end)
	    prev (get-text-property start prop object))
      (put-text-property
       start next prop
       (font-lock-unique (append (if (listp prev) prev (list prev)) val))
       object)
      (setq start next))))

;;; Regexp fontification functions.

(defsubst font-lock-apply-highlight (highlight)
  "Apply HIGHLIGHT following a match.
HIGHLIGHT should be of the form MATCH-HIGHLIGHT, see `font-lock-keywords'."
  (let* ((match (nth 0 highlight))
	 (start (match-beginning match)) (end (match-end match))
	 (override (nth 2 highlight)))
    (let ((newface (nth 1 highlight)))
      (or (symbolp newface)
	  (setq newface (eval newface)))
      (cond ((not start)
	     ;; No match but we might not signal an error.
	     (or (nth 3 highlight)
		 (error "No match %d in highlight %S" match highlight)))
	    ((= start end) nil)
	    ((not override)
	     ;; Cannot override existing fontification.
	     (or (font-lock-any-faces-p start end)
		 (font-lock-set-face start end newface)))
	    ((eq override t)
	     ;; Override existing fontification.
	     (font-lock-set-face start end newface))
	    ((eq override 'keep)
	     ;; Keep existing fontification.
	     (font-lock-fillin-text-property start end 'face 'font-lock
					     newface))
	    ((eq override 'prepend)
	     ;; Prepend to existing fontification.
	     (font-lock-prepend-text-property start end 'face newface))
	    ((eq override 'append)
	     ;; Append to existing fontification.
	     (font-lock-append-text-property start end 'face newface))))))

(defsubst font-lock-fontify-anchored-keywords (keywords limit)
  "Fontify according to KEYWORDS until LIMIT.
KEYWORDS should be of the form MATCH-ANCHORED, see `font-lock-keywords',
LIMIT can be modified by the value of its PRE-MATCH-FORM."
  (let ((matcher (nth 0 keywords)) (lowdarks (nthcdr 3 keywords)) highlights
	;; Evaluate PRE-MATCH-FORM.
	(pre-match-value (eval (nth 1 keywords))))
    ;; Set LIMIT to value of PRE-MATCH-FORM or the end of line.
    (if (and (numberp pre-match-value) (> pre-match-value (point)))
	(setq limit pre-match-value)
      (save-excursion (end-of-line) (setq limit (point))))
    (save-match-data
      ;; Find an occurrence of `matcher' before `limit'.
      (while (if (stringp matcher)
		 (re-search-forward matcher limit t)
	       (funcall matcher limit))
	;; Apply each highlight to this instance of `matcher'.
	(setq highlights lowdarks)
	(while highlights
	  (font-lock-apply-highlight (car highlights))
	  (setq highlights (cdr highlights)))))
    ;; Evaluate POST-MATCH-FORM.
    (eval (nth 2 keywords))))

(defun font-lock-fontify-keywords-region (start end &optional loudvar)
  "Fontify according to `font-lock-keywords' between START and END.
START should be at the beginning of a line."
  (let ((loudly (and font-lock-verbose
		     (>= (- end start) font-lock-message-threshold))))
    (let ((case-fold-search font-lock-keywords-case-fold-search)
	  (keywords (cdr (if (eq (car-safe font-lock-keywords) t)
			     font-lock-keywords
			   (font-lock-compile-keywords))))
	  (bufname (buffer-name)) (count 0)
	  keyword matcher highlights)
      ;;
      ;; Fontify each item in `font-lock-keywords' from `start' to `end'.
      (while keywords
	(if loudly (display-message
		    'progress
		    (format "Fontifying %s... (regexps..%s)" bufname
			    (make-string (setq count (1+ count)) ?.))))
	;;
	;; Find an occurrence of `matcher' from `start' to `end'.
	(setq keyword (car keywords) matcher (car keyword))
	(goto-char start)
	(while (and (< (point) end)
		    (if (stringp matcher)
			(re-search-forward matcher end t)
		      (funcall matcher end)))
	  ;; Apply each highlight to this instance of `matcher', which may be
	  ;; specific highlights or more keywords anchored to `matcher'.
	  (setq highlights (cdr keyword))
	  (while highlights
	    (if (numberp (car (car highlights)))
		(let ((end (match-end (car (car highlights)))))
		  (font-lock-apply-highlight (car highlights))
		  ;; restart search just after the end of the
		  ;; keyword so keywords can share bracketing
		  ;; expressions.
		  (and end (goto-char end)))
	      (font-lock-fontify-anchored-keywords (car highlights) end))
	    (setq highlights (cdr highlights))))
	(setq keywords (cdr keywords))))
    (if loudly (display-message
		'progress
		(format "Fontifying %s... done." (buffer-name))))))


;; Various functions.

;; Turn off other related packages if they're on.  I prefer a hook. --sm.
;; These explicit calls are easier to understand
;; because people know what they will do.
;; A hook is a mystery because it might do anything whatever. --rms.
(defun font-lock-thing-lock-cleanup ()
  (cond ((and (boundp 'fast-lock-mode) fast-lock-mode)
	 (fast-lock-mode -1))
	((and (boundp 'lazy-lock-mode) lazy-lock-mode)
	 (lazy-lock-mode -1))
	((and (boundp 'lazy-shot-mode) lazy-shot-mode)
	 (lazy-shot-mode -1))))

;; Do something special for these packages after fontifying.  I prefer a hook.
(defun font-lock-after-fontify-buffer ()
  (cond ((and (boundp 'fast-lock-mode) fast-lock-mode)
	 (fast-lock-after-fontify-buffer))
	((and (boundp 'lazy-lock-mode) lazy-lock-mode)
	 (lazy-lock-after-fontify-buffer))))

;; If the buffer is about to be reverted, it won't be fontified afterward.
(defun font-lock-revert-setup ()
  (setq font-lock-fontified nil))

;; If the buffer has just been reverted, normally that turns off
;; Font Lock mode.  So turn the mode back on if necessary.
(defalias 'font-lock-revert-cleanup 'turn-on-font-lock)


(defun font-lock-compile-keywords (&optional keywords)
  ;; Compile `font-lock-keywords' into the form (t KEYWORD ...) where KEYWORD
  ;; is the (MATCHER HIGHLIGHT ...) shown in the variable's doc string.
  (let ((keywords (or keywords font-lock-keywords)))
    (setq font-lock-keywords 
     (if (eq (car-safe keywords) t)
	 keywords
       (cons t (mapcar 'font-lock-compile-keyword keywords))))))

(defun font-lock-compile-keyword (keyword)
  (cond ((nlistp keyword)		; Just MATCHER
	 (list keyword '(0 font-lock-keyword-face)))
	((eq (car keyword) 'eval)	; Specified (eval . FORM)
	 (font-lock-compile-keyword (eval (cdr keyword))))
	((numberp (cdr keyword))	; Specified (MATCHER . MATCH)
	 (list (car keyword) (list (cdr keyword) 'font-lock-keyword-face)))
	((symbolp (cdr keyword))	; Specified (MATCHER . FACENAME)
	 (list (car keyword) (list 0 (cdr keyword))))
	((nlistp (nth 1 keyword))	; Specified (MATCHER . HIGHLIGHT)
	 (list (car keyword) (cdr keyword)))
	(t				; Hopefully (MATCHER HIGHLIGHT ...)
	 keyword)))

(defun font-lock-choose-keywords (keywords level)
  ;; Return LEVELth element of KEYWORDS.  A LEVEL of nil is equal to a
  ;; LEVEL of 0, a LEVEL of t is equal to (1- (length KEYWORDS)).
  (let ((level (if (not (consp level))
		   level
		 (cdr (or (assq major-mode level) (assq t level))))))
    (cond ((symbolp keywords)
	   keywords)
	  ((numberp level)
	   (or (nth level keywords) (car (reverse keywords))))
	  ((eq level t)
	   (car (reverse keywords)))
	  (t
	   (car keywords)))))


;;; Determining which set of font-lock keywords to use.

(defun font-lock-find-font-lock-defaults (modesym)
  ;; Get the defaults based on the major mode.
  (let (raw-defaults)
    ;; I want a do-while loop!
    (while (progn
	     (setq raw-defaults (get modesym 'font-lock-defaults))
	     (and raw-defaults (symbolp raw-defaults)
		  (setq modesym raw-defaults)))
      )
    raw-defaults))

(defun font-lock-examine-syntax-table ()
  ; Computes the value of font-lock-keywords-only for this buffer.
  (if (eq (syntax-table) (standard-syntax-table))
      ;; Assume that modes which haven't bothered to install their own
      ;; syntax table don't do anything syntactically interesting.
      ;; Really, the standard-syntax-table shouldn't have comments and
      ;; strings in it, but changing that now might break things.
      nil
    ;; else map over the syntax table looking for strings or comments.
    (let (got-one)
      ;; XEmacs 20.0 ...
      (if (fboundp 'map-syntax-table)
	  (setq got-one
		(map-syntax-table
		 #'(lambda (key value)
		     (memq (char-syntax-from-code value)
			   '(?\" ?\< ?\> ?\$)))
		 (syntax-table)))
	;; older Emacsen.
	(let ((i (1- (length (syntax-table)))))
	  (while (>= i 0)
	    (if (memq (char-syntax i) '(?\" ?\< ?\> ?\$))
		(setq got-one t i 0))
	    (setq i (1- i)))))
      (set (make-local-variable 'font-lock-keywords-only) (not got-one)))))

;; font-lock-set-defaults is in fontl-hooks.el.

;;;###autoload
(defun font-lock-set-defaults-1 (&optional explicit-defaults)
  ;; does everything that font-lock-set-defaults does except
  ;; enable font-lock-mode.  This is called by `font-lock-mode'.
  ;; Note that the return value is used!

  (if (and font-lock-defaults-computed (not explicit-defaults))
      ;; nothing to do.
      nil

    (or font-lock-keywords
	(let* ((defaults (or (and (not (eq t explicit-defaults))
				  explicit-defaults)
			     ;; in case modes decide to set
			     ;; `font-lock-defaults' themselves,
			     ;; as in FSF Emacs.
			     font-lock-defaults
			     (font-lock-find-font-lock-defaults major-mode)))
	       (keywords (font-lock-choose-keywords
			  (nth 0 defaults) font-lock-maximum-decoration)))
	  
	  ;; Keywords?
	  (setq font-lock-keywords (if (fboundp keywords)
				       (funcall keywords)
				     (eval keywords)))
	  (or font-lock-keywords
	      ;; older way:
	      ;; try to look for a variable `foo-mode-font-lock-keywords',
	      ;; or similar.
	      (let ((major (symbol-name major-mode))
		    (try #'(lambda (n)
			     (if (stringp n) (setq n (intern-soft n)))
			     (if (and n
				      (boundp n))
				 n
			       nil))))
		(setq font-lock-keywords 
		      (symbol-value
		       (or (funcall try (get major-mode 'font-lock-keywords))
			   (funcall try (concat major "-font-lock-keywords"))
			   (funcall try (and (string-match "-mode\\'" major)
					     (concat (substring 
						      major 0 
						      (match-beginning 0))
						     "-font-lock-keywords")))
			   'font-lock-keywords)))))

	  ;; Case fold?
	  (if (>= (length defaults) 3)
	      (setq font-lock-keywords-case-fold-search (nth 2 defaults))
	    ;; older way:
	    ;; look for a property 'font-lock-keywords-case-fold-search on
	    ;; the major-mode symbol.
	    (let* ((nonexist (make-symbol ""))
		   (value (get major-mode 'font-lock-keywords-case-fold-search
			       nonexist)))
	      (if (not (eq nonexist value))
		  (setq font-lock-keywords-case-fold-search value))))

	  ;; Syntactic?
	  (if (>= (length defaults) 2)
	      (setq font-lock-keywords-only (nth 1 defaults))
	    ;; older way:
	    ;; cleverly examine the syntax table.
	    (font-lock-examine-syntax-table))
	   
	  ;; Syntax table?
	  (if (nth 3 defaults)
	      (let ((slist (nth 3 defaults)))
		(setq font-lock-syntax-table
		      (copy-syntax-table (syntax-table)))
		(while slist
		  (modify-syntax-entry (car (car slist)) (cdr (car slist))
				       font-lock-syntax-table)
		  (setq slist (cdr slist)))))

	  ;; Syntax function?
	  (cond (defaults
		  (setq font-lock-beginning-of-syntax-function
			(nth 4 defaults)))
		(t
		 ;; older way:
		 ;; defaults not specified at all, so use `beginning-of-defun'.
		 (setq font-lock-beginning-of-syntax-function
		       'beginning-of-defun)))))

    (setq font-lock-defaults-computed t)))


;;; Initialization of faces.

(defconst font-lock-face-list
  '(font-lock-comment-face
    font-lock-doc-string-face
    font-lock-string-face
    font-lock-keyword-face
    font-lock-function-name-face
    font-lock-variable-name-face
    font-lock-type-face
    font-lock-reference-face
    font-lock-preprocessor-face))

(defun font-lock-reset-face (face)
  "Reset FACE its default state (from the X resource database).
Returns whether it is indistinguishable from the default face."
  (reset-face face)
  (init-face-from-resources face)
  (face-differs-from-default-p face))

(defun font-lock-reset-all-faces ()
  (mapcar 'font-lock-reset-face font-lock-face-list))

(defun font-lock-add-fonts (tag-list)
  ;; Underling comments looks terrible on tty's
  (if (featurep 'tty)
      (progn
	(set-face-underline-p 'font-lock-comment-face nil 'global
			      (append '(tty) tag-list) 'append)
	(set-face-highlight-p 'font-lock-comment-face t 'global
			      (append '(tty) tag-list)  'append)))
  (set-face-font 'font-lock-comment-face [italic] 'global tag-list 'append)
  (set-face-font 'font-lock-string-face [italic] 'global tag-list 'append)
  (set-face-font 'font-lock-doc-string-face [italic] 'global tag-list 'append)
  (set-face-font 'font-lock-function-name-face [bold] 'global tag-list 'append)
  (set-face-font 'font-lock-variable-name-face [bold] 'global tag-list 'append)
  (set-face-font 'font-lock-keyword-face [bold] 'global tag-list 'append)
  (set-face-font 'font-lock-preprocessor-face [bold-italic] 'global tag-list
		  'append)
  (set-face-font 'font-lock-type-face [italic] 'global tag-list 'append)
  (set-face-font 'font-lock-reference-face [bold] 'global tag-list 'append)
  nil)

(defun font-lock-add-colors (tag-list)
  (set-face-foreground 'font-lock-comment-face "red" 'global tag-list 'append)
  ;(set-face-font 'font-lock-comment-face [italic] 'global tag-list 'append)
  (set-face-foreground 'font-lock-string-face "green4" 'global tag-list
		       'append)
  (set-face-foreground 'font-lock-string-face "green" 'global tag-list
		       'append)
  (set-face-foreground 'font-lock-doc-string-face "green4" 'global tag-list
		       'append)
  (set-face-foreground 'font-lock-doc-string-face "green" 'global tag-list
		       'append)
  (set-face-foreground 'font-lock-function-name-face "blue3" 'global tag-list
		       'append)
  (set-face-foreground 'font-lock-function-name-face "blue" 'global tag-list
		       'append)
  (set-face-foreground 'font-lock-variable-name-face "blue3" 'global tag-list
		       'append)
  (set-face-foreground 'font-lock-variable-name-face "blue" 'global tag-list
		       'append)
  (set-face-foreground 'font-lock-reference-face "red3" 'global
		       tag-list 'append)
  (set-face-foreground 'font-lock-reference-face "red" 'global tag-list
		       'append)
  (set-face-foreground 'font-lock-keyword-face "orange" 'global tag-list
		       'append)
  ;(set-face-font 'font-lock-keyword-face [bold] 'global tag-list 'append)
  (set-face-foreground 'font-lock-preprocessor-face "blue3" 'global tag-list
		       'append)
  (set-face-foreground 'font-lock-preprocessor-face "blue" 'global tag-list
		       'append)
  ;(set-face-font 'font-lock-preprocessor-face [bold] 'global tag-list 'append)
  (set-face-foreground 'font-lock-type-face "#6920ac" 'global tag-list 'append)
  nil)

(defun font-lock-apply-defaults (function tag-list)
  (if (and (listp tag-list)
	   (eq 'or (car tag-list)))
      (mapcar #'(lambda (x)
		  (font-lock-apply-defaults function x))
	      (cdr tag-list))
    (if tag-list
	(if (not (valid-specifier-tag-set-p tag-list))
	    (warn "Invalid tag set found: %s" tag-list)
	  (funcall function tag-list)))))

(defun font-lock-recompute-variables ()
  ;; Is this a Draconian thing to do?
  (mapcar #'(lambda (buffer)
	      (save-excursion
		(set-buffer buffer)
		(font-lock-mode 0)
		(font-lock-set-defaults t)))
	  (buffer-list)))

;; Backwards-compatible crud.

(defun font-lock-use-default-fonts ()
  "Reset the font-lock faces to a default set of fonts."
  (interactive)
  (font-lock-reset-all-faces)
  (font-lock-add-fonts nil))

(defun font-lock-use-default-colors ()
  "Reset the font-lock faces to a default set of colors."
  (interactive)
  (font-lock-reset-all-faces)
  (font-lock-add-colors nil))

(defun font-lock-use-default-minimal-decoration ()
  "Reset the font-lock patterns to a fast, minimal set of decorations."
  (and font-lock-maximum-decoration
       (setq font-lock-maximum-decoration nil)
       (font-lock-recompute-variables)))

(defun font-lock-use-default-maximal-decoration ()
  "Reset the font-lock patterns to a larger set of decorations."
  (and (not (eq t font-lock-maximum-decoration))
       (setq font-lock-maximum-decoration t)
       (font-lock-recompute-variables)))


;;;;;;;;;;;;;;;;;;;;;;         keywords         ;;;;;;;;;;;;;;;;;;;;;;

;;; Various major-mode interfaces.
;;; Probably these should go in with the source of the respective major modes.

;; The defaults and keywords listed here should perhaps be moved into
;; mode-specific files.

;; For C and Lisp modes we use `beginning-of-defun', rather than nil,
;; for SYNTAX-BEGIN.  Thus the calculation of the cache is usually
;; faster but not infallible, so we risk mis-fontification.  --sm.

(put 'c-mode 'font-lock-defaults 
     '((c-font-lock-keywords
	c-font-lock-keywords-1 c-font-lock-keywords-2 c-font-lock-keywords-3)
       nil nil ((?_ . "w")) beginning-of-defun))
(put 'c++-c-mode 'font-lock-defaults 'c-mode)
(put 'elec-c-mode 'font-lock-defaults 'c-mode)

(put 'c++-mode 'font-lock-defaults
     '((c++-font-lock-keywords
	c++-font-lock-keywords-1 c++-font-lock-keywords-2
	c++-font-lock-keywords-3)
       nil nil ((?_ . "w") (?~ . "w")) beginning-of-defun))

(put 'java-mode 'font-lock-defaults 
     '((java-font-lock-keywords
	java-font-lock-keywords-1 java-font-lock-keywords-2
	java-font-lock-keywords-3)
       nil nil ((?_ . "w")) beginning-of-defun
       (font-lock-mark-block-function . mark-defun)))

(put 'lisp-mode 'font-lock-defaults
     '((lisp-font-lock-keywords
	lisp-font-lock-keywords-1 lisp-font-lock-keywords-2)
       nil nil
       ((?: . "w") (?- . "w") (?* . "w") (?+ . "w") (?. . "w") (?< . "w")
	(?> . "w") (?= . "w") (?! . "w") (?? . "w") (?$ . "w") (?% . "w")
	(?_ . "w") (?& . "w") (?~ . "w") (?^ . "w") (?/ . "w"))
       beginning-of-defun))
(put 'emacs-lisp-mode 'font-lock-defaults 'lisp-mode)
(put 'lisp-interaction-mode 'font-lock-defaults 'lisp-mode)

(put 'scheme-mode 'font-lock-defaults
     '(scheme-font-lock-keywords
       nil t
       ((?: . "w") (?- . "w") (?* . "w") (?+ . "w") (?. . "w") (?< . "w")
	(?> . "w") (?= . "w") (?! . "w") (?? . "w") (?$ . "w") (?% . "w")
	(?_ . "w") (?& . "w") (?~ . "w") (?^ . "w") (?/ . "w"))
       beginning-of-defun))
(put 'inferior-scheme-mode 'font-lock-defaults 'scheme-mode)
(put 'scheme-interaction-mode 'font-lock-defaults 'scheme-mode)

(put 'tex-mode 'font-lock-defaults
     ;; For TeX modes we could use `backward-paragraph' for the same reason.
     '(tex-font-lock-keywords nil nil ((?$ . "\""))))
;; the nine billion names of TeX mode...
(put 'bibtex-mode	'font-lock-defaults 'tex-mode)
(put 'plain-tex-mode	'font-lock-defaults 'tex-mode)
(put 'slitex-tex-mode	'font-lock-defaults 'tex-mode)
(put 'SliTeX-mode	'font-lock-defaults 'tex-mode)
(put 'slitex-mode	'font-lock-defaults 'tex-mode)
(put 'latex-tex-mode	'font-lock-defaults 'tex-mode)
(put 'LaTex-tex-mode	'font-lock-defaults 'tex-mode)
(put 'latex-mode        'font-lock-defaults 'tex-mode)
(put 'LaTeX-mode        'font-lock-defaults 'tex-mode)
(put 'japanese-LaTeX-mode 'font-lock-defaults 'tex-mode)
(put 'japanese-SliTeX-mode 'font-lock-defaults 'tex-mode)
(put 'FoilTeX-mode	'font-lock-defaults 'tex-mode)
(put 'LATeX-MoDe	'font-lock-defaults 'tex-mode)
(put 'lATEx-mODe	'font-lock-defaults 'tex-mode)
;; ok, this is getting a bit silly ...
(put 'eDOm-xETAl	'font-lock-defaults 'tex-mode)

;;; Various regexp information shared by several modes.
;;; Information specific to a single mode should go in its load library.

(defconst lisp-font-lock-keywords-1
  (list
   ;; Anything not a variable or type declaration is fontified as a function.
   ;; It would be cleaner to allow preceding whitespace, but it would also be
   ;; about five times slower.
   (list (concat "^(\\(def\\("
		  ;; Variable declarations.
		  "\\(const\\(\\|ant\\)\\|ine-key\\(\\|-after\\)\\|var\\)\\|"
		  ;; Structure declarations.
		  "\\(class\\|struct\\|type\\)\\|"
		  ;; Everything else is a function declaration.
		  "\\([^ \t\n\(\)]+\\)"
		  "\\)\\)\\>"
		  ;; Any whitespace and declared object.
		  "[ \t'\(]*"
		  "\\([^ \t\n\)]+\\)?")
	  '(1 font-lock-keyword-face)
	  '(8 (cond ((match-beginning 3) 'font-lock-variable-name-face)
		    ((match-beginning 6) 'font-lock-type-face)
		    (t 'font-lock-function-name-face))
	      nil t))
   )
 "Subdued level highlighting Lisp modes.")

(defconst lisp-font-lock-keywords-2
  (append lisp-font-lock-keywords-1
   (list
    ;;
    ;; Control structures.  ELisp and CLisp combined.
    ;;
    ;;(regexp-opt
    ;;  '("cond" "if" "while" "let" "let*" "prog" "progn" "prog1"
    ;;    "prog2" "progv" "catch" "throw" "save-restriction"
    ;;    "save-excursion" "save-window-excursion"
    ;;    "save-current-buffer" "with-current-buffer"
    ;;    "with-temp-file" "with-temp-buffer" "with-output-to-string"
    ;;    "with-string-as-buffer-contents"
    ;;    "save-selected-window" "save-match-data" "unwind-protect"
    ;;    "condition-case" "track-mouse" "autoload"
    ;;    "eval-after-load" "eval-and-compile" "eval-when-compile"
    ;;    "when" "unless" "do" "dolist" "dotimes" "flet" "labels"
    ;;    "lambda" "return" "return-from"))
    (cons
     (concat
      "(\\("
      "autoload\\|c\\(atch\\|ond\\(ition-case\\)?\\)\\|do\\(list\\|"
      "times\\)?\\|eval-\\(a\\(fter-load\\|nd-compile\\)\\|when-compile\\)\\|"
      "flet\\|if\\|l\\(a\\(bels\\|mbda\\)\\|et\\*?\\)\\|"
      "prog[nv12\\*]?\\|return\\(-from\\)?\\|save-\\(current-buffer\\|"
      "excursion\\|match-data\\|restriction\\|selected-window\\|"
      "window-excursion\\)\\|t\\(hrow\\|rack-mouse\\)\\|un\\(less\\|"
      "wind-protect\\)\\|w\\(h\\(en\\|ile\\)\\|ith-\\(current-buffer\\|"
      "output-to-string\\|string-as-buffer-contents\\|temp-\\(buffer\\|"
      "file\\)\\)\\)"
      "\\)\\>") 1)
    ;;
    ;; Words inside \\[] tend to be for `substitute-command-keys'.
    '("\\\\\\\\\\[\\(\\sw+\\)]" 1 font-lock-reference-face prepend)
    ;;
    ;; Words inside `' tend to be symbol names.
    '("`\\(\\sw\\sw+\\)'" 1 font-lock-reference-face prepend)
    ;;
    ;; CLisp `:' keywords as references.
    '("\\<:\\sw+\\>" 0 font-lock-reference-face prepend)
    ;;
    ;; ELisp and CLisp `&' keywords as types.
    '("\\<\\&\\(optional\\|rest\\|whole\\)\\>" . font-lock-type-face)
    ))
  "Gaudy level highlighting for Lisp modes.")

(defvar lisp-font-lock-keywords lisp-font-lock-keywords-1
  "Default expressions to highlight in Lisp modes.")

;; The previous version, before replacing it with the FSF version.
;(defconst lisp-font-lock-keywords-1 (purecopy
; '(;;
;   ;; highlight defining forms.  This doesn't work too nicely for
;   ;; (defun (setf foo) ...) but it does work for (defvar foo) which
;   ;; is more important.
;   ("^(def[-a-z]+\\s +\\([^ \t\n\)]+\\)" 1 font-lock-function-name-face)
;   ;;
;   ;; highlight CL keywords (three clauses seems faster than one)
;   ("\\s :\\(\\(\\sw\\|\\s_\\)+\\)\\>" . 1)
;   ("(:\\(\\(\\sw\\|\\s_\\)+\\)\\>" . 1)
;   ("':\\(\\(\\sw\\|\\s_\\)+\\)\\>" . 1)
;   ;;
;   ;; this is highlights things like (def* (setf foo) (bar baz)), but may
;   ;; be slower (I haven't really thought about it)
;;   ("^(def[-a-z]+\\s +\\(\\s(\\S)*\\s)\\|\\S(\\S *\\)"
;;    1 font-lock-function-name-face)
;   ))
; "For consideration as a value of `lisp-font-lock-keywords'.
;This does fairly subdued highlighting.")
;
;(defconst lisp-font-lock-keywords-2 (purecopy
;  (append lisp-font-lock-keywords-1
;   '(;;
;     ;; Highlight control structures
;     ("(\\(cond\\|if\\|when\\|unless\\|[ec]?\\(type\\)?case\\)[ \t\n]" . 1)
;     ("(\\(while\\|do\\|let\\*?\\|flet\\|labels\\|prog[nv12*]?\\)[ \t\n]" . 1)
;     ("(\\(do\\*\\|dotimes\\|dolist\\|loop\\)[ \t\n]" . 1)
;     ("(\\(catch\\|\\throw\\|block\\|return\\|return-from\\)[ \t\n]" . 1)
;     ("(\\(save-restriction\\|save-window-restriction\\)[ \t\n]" . 1)
;     ("(\\(save-excursion\\|unwind-protect\\|condition-case\\)[ \t\n]" . 1)
;     ;;
;     ;; highlight function names in emacs-lisp docstrings (in the syntax
;     ;; that substitute-command-keys understands.)
;     ("\\\\\\\\\\[\\([^]\\\n]+\\)]" 1 font-lock-keyword-face t)
;     ;;
;     ;; highlight words inside `' which tend to be function names
;     ("`\\([-a-zA-Z0-9_][-a-zA-Z0-9_][-a-zA-Z0-9_.]+\\)'"
;      1 font-lock-keyword-face t)
;     )))
; "For consideration as a value of `lisp-font-lock-keywords'.
;
;This does a lot more highlighting.")

(defvar scheme-font-lock-keywords
  (eval-when-compile
    (list
     ;;
     ;; Declarations.  Hannes Haug <hannes.haug@student.uni-tuebingen.de> says
     ;; this works for SOS, STklos, SCOOPS, Meroon and Tiny CLOS.
     (list (concat "(\\(define\\("
		   ;; Function names.
		   "\\(\\|-\\(generic\\(\\|-procedure\\)\\|method\\)\\)\\|"
		   ;; Macro names, as variable names.  A bit dubious, this.
		   "\\(-syntax\\)\\|"
		   ;; Class names.
		   "\\(-class\\)"
		   "\\)\\)\\>"
		   ;; Any whitespace and declared object.
		   "[ \t]*(?"
		   "\\(\\sw+\\)?")
	   '(1 font-lock-keyword-face)
	   '(8 (cond ((match-beginning 3) 'font-lock-function-name-face)
		     ((match-beginning 6) 'font-lock-variable-name-face)
		     (t 'font-lock-type-face))
	       nil t))
     ;;
     ;; Control structures.
;(regexp-opt '("begin" "call-with-current-continuation" "call/cc"
;	       "call-with-input-file" "call-with-output-file" "case" "cond"
;	       "do" "else" "for-each" "if" "lambda"
;	       "let\\*?" "let-syntax" "letrec" "letrec-syntax"
;	       ;; Hannes Haug <hannes.haug@student.uni-tuebingen.de> wants:
;	       "and" "or" "delay"
;	       ;; Stefan Monnier <stefan.monnier@epfl.ch> says don't bother:
;	       ;;"quasiquote" "quote" "unquote" "unquote-splicing"
;	       "map" "syntax" "syntax-rules"))
     (cons
      (concat "(\\("
	      "and\\|begin\\|c\\(a\\(ll\\(-with-\\(current-continuation\\|"
	      "input-file\\|output-file\\)\\|/cc\\)\\|se\\)\\|ond\\)\\|"
	      "d\\(elay\\|o\\)\\|else\\|for-each\\|if\\|"
	      "l\\(ambda\\|et\\(-syntax\\|\\*?\\|rec\\(\\|-syntax\\)\\)\\)\\|"
	      "map\\|or\\|syntax\\(\\|-rules\\)"
	      "\\)\\>") 1)
     ;;
     ;; David Fox <fox@graphics.cs.nyu.edu> for SOS/STklos class specifiers.
     '("\\<<\\sw+>\\>" . font-lock-type-face)
     ;;
     ;; Scheme `:' keywords as references.
     '("\\<:\\sw+\\>" . font-lock-reference-face)
     ))
"Default expressions to highlight in Scheme modes.")

;; The previous version, before replacing it with the FSF version.
;(defconst scheme-font-lock-keywords (purecopy
; '(("(define[ \t]+(?\\([^ \t\n\)]+\\)" 1 font-lock-function-name-face)
;   ("(\\(cond\\|lambda\\|begin\\|if\\|else\\|case\\|do\\)[ \t\n]" . 1)
;   ("(\\(\\|letrec\\|let\\*?\\|set!\\|and\\|or\\)[ \t\n]" . 1)
;   ("(\\(quote\\|unquote\\|quasiquote\\|unquote-splicing\\)[ \t\n]" . 1)
;   ("(\\(syntax\\|syntax-rules\\|define-syntax\\|let-syntax\\|letrec-syntax\\)[ \t\n]" . 1)))
;  "Expressions to highlight in Scheme buffers.")

(defconst c-font-lock-keywords-1 nil
  "Subdued level highlighting for C modes.")

(defconst c-font-lock-keywords-2 nil
  "Medium level highlighting for C modes.")

(defconst c-font-lock-keywords-3 nil
  "Gaudy level highlighting for C modes.")

(defconst c++-font-lock-keywords-1 nil
  "Subdued level highlighting for C++ modes.")

(defconst c++-font-lock-keywords-2 nil
  "Medium level highlighting for C++ modes.")

(defconst c++-font-lock-keywords-3 nil
  "Gaudy level highlighting for C++ modes.")

(defun font-lock-match-c++-style-declaration-item-and-skip-to-next (limit)
  ;; Match, and move over, any declaration/definition item after point.
  ;; The expect syntax of an item is "word" or "word::word", possibly ending
  ;; with optional whitespace and a "(".  Everything following the item (but
  ;; belonging to it) is expected to by skip-able by `forward-sexp', and items
  ;; are expected to be separated with a "," or ";".
  (if (looking-at "[ \t*&]*\\(\\sw+\\)\\(::\\(\\sw+\\)\\)?[ \t]*\\((\\)?")
      (save-match-data
	(condition-case nil
	    (save-restriction
	      ;; Restrict to the end of line, currently guaranteed to be LIMIT.
	      (narrow-to-region (point-min) limit)
	      (goto-char (match-end 1))
	      ;; Move over any item value, etc., to the next item.
	      (while (not (looking-at "[ \t]*\\([,;]\\|$\\)"))
		(goto-char (or (scan-sexps (point) 1) (point-max))))
	      (goto-char (match-end 0)))
	  (error t)))))

(let ((c-keywords
;      ("break" "continue" "do" "else" "for" "if" "return" "switch" "while")
       "break\\|continue\\|do\\|else\\|for\\|if\\|return\\|switch\\|while")
      (c-type-types
;      ("auto" "extern" "register" "static" "typedef" "struct" "union" "enum"
;	"signed" "unsigned" "short" "long" "int" "char" "float" "double"
;	"void" "volatile" "const")
       (concat "auto\\|c\\(har\\|onst\\)\\|double\\|e\\(num\\|xtern\\)\\|"
	       "float\\|int\\|long\\|register\\|"
	       "s\\(hort\\|igned\\|t\\(atic\\|ruct\\)\\)\\|typedef\\|"
	       "un\\(ion\\|signed\\)\\|vo\\(id\\|latile\\)"))	; 6 ()s deep.
      (c++-keywords
;      ("break" "continue" "do" "else" "for" "if" "return" "switch" "while"
;	"asm" "catch" "delete" "new" "operator" "sizeof" "this" "throw" "try"
;       "protected" "private" "public")
       (concat "asm\\|break\\|c\\(atch\\|ontinue\\)\\|d\\(elete\\|o\\)\\|"
	       "else\\|for\\|if\\|new\\|"
	       "p\\(r\\(ivate\\|otected\\)\\|ublic\\)\\|return\\|"
	       "s\\(izeof\\|witch\\)\\|t\\(h\\(is\\|row\\)\\|ry\\)\\|while"))
      (c++-type-types
;      ("auto" "extern" "register" "static" "typedef" "struct" "union" "enum"
;	"signed" "unsigned" "short" "long" "int" "char" "float" "double"
;	"void" "volatile" "const" "class" "inline" "friend" "bool"
;	"virtual" "complex" "template")
       (concat "auto\\|bool\\|c\\(har\\|lass\\|o\\(mplex\\|nst\\)\\)\\|"
	       "double\\|e\\(num\\|xtern\\)\\|f\\(loat\\|riend\\)\\|"
	       "in\\(line\\|t\\)\\|long\\|register\\|"
	       "s\\(hort\\|igned\\|t\\(atic\\|ruct\\)\\)\\|"
	       "t\\(emplate\\|ypedef\\)\\|un\\(ion\\|signed\\)\\|"
	       "v\\(irtual\\|o\\(id\\|latile\\)\\)"))		; 11 ()s deep.
      (ctoken "\\(\\sw\\|\\s_\\|[:~*&]\\)+")
      )
 (setq c-font-lock-keywords-1
  (list
   ;;
   ;; These are all anchored at the beginning of line for speed.
   ;;
   ;; Fontify function name definitions (GNU style; without type on line).
   
   ;; In FSF this has the simpler definition of "\\sw+" for ctoken.
   ;; I'm not sure if ours is more correct.
   ;; This is a subset of the next rule, and is slower when present. --dmoore
   ;; (list (concat "^\\(" ctoken "\\)[ \t]*(") 1 'font-lock-function-name-face)
   ;;
   ;; fontify the names of functions being defined.
   ;; FSF doesn't have this but I think it should be fast for us because
   ;; our regexp routines are more intelligent than FSF's about handling
   ;; anchored-at-newline. (When I added this hack in regex.c, it halved
   ;; the time to do the regexp phase of font-lock for a C file!) Not
   ;; including this discriminates against those who don't follow the
   ;; GNU coding style. --ben
   ;; x?x?x?y?z should always be: (x(xx?)?)?y?z --dmoore
   (list (concat
	  "^\\("
          "\\(" ctoken "[ \t]+\\)"	; type specs; there can be no
	  "\\("
          "\\(" ctoken "[ \t]+\\)"	; more than 3 tokens, right?
          "\\(" ctoken "[ \t]+\\)"
	  "?\\)?\\)?"
          "\\([*&]+[ \t]*\\)?"		; pointer
          "\\(" ctoken "\\)[ \t]*(")	; name
         10 'font-lock-function-name-face)
   ;;
   ;; This is faster but not by much.  I don't see why not.
   ;(list (concat "^\\(" ctoken "\\)[ \t]*(") 1 'font-lock-function-name-face)
   ;;
   ;; Added next two; they're both jolly-good fastmatch candidates so
   ;; should be fast. --ben
   ;;
   ;; Fontify structure names (in structure definition form).
   (list (concat "^\\(typedef[ \t]+struct\\|struct\\|static[ \t]+struct\\)"
      	   "[ \t]+\\(" ctoken "\\)[ \t]*\\(\{\\|$\\)")
         2 'font-lock-function-name-face)
   ;;
   ;; Fontify case clauses.  This is fast because its anchored on the left.
   '("case[ \t]+\\(\\(\\sw\\|\\s_\\)+\\)[ \t]+:". 1)
   ;;
   '("\\<\\(default\\):". 1)
   ;; Fontify filenames in #include <...> preprocessor directives as strings.
   '("^#[ \t]*include[ \t]+\\(<[^>\"\n]+>\\)" 1 font-lock-string-face)
   ;;
   ;; Fontify function macro names.
   '("^#[ \t]*define[ \t]+\\(\\(\\sw+\\)(\\)" 2 font-lock-function-name-face)
   ;;
   ;; Fontify symbol names in #if ... defined preprocessor directives.
   '("^#[ \t]*if\\>"
     ("\\<\\(defined\\)\\>[ \t]*(?\\(\\sw+\\)?" nil nil
      (1 font-lock-preprocessor-face) (2 font-lock-variable-name-face nil t)))
   ;;
   ;; Fontify symbol names in #elif ... defined preprocessor directives.
   '("^#[ \t]*elif\\>"
     ("\\<\\(defined\\)\\>[ \t]*(?\\(\\sw+\\)?" nil nil
      (1 font-lock-preprocessor-face) (2 font-lock-variable-name-face nil t)))
   ;;
   ;; Fontify otherwise as symbol names, and the preprocessor directive names.
   '("^\\(#[ \t]*[a-z]+\\)\\>[ \t]*\\(\\sw+\\)?"
     (1 font-lock-preprocessor-face) (2 font-lock-variable-name-face nil t))
   ))

 (setq c-font-lock-keywords-2
  (append c-font-lock-keywords-1
   (list
    ;;
    ;; Simple regexps for speed.
    ;;
    ;; Fontify all type specifiers.
    (cons (concat "\\<\\(" c-type-types "\\)\\>") 'font-lock-type-face)
    ;;
    ;; Fontify all builtin keywords (except case, default and goto; see below).
    (cons (concat "\\<\\(" c-keywords "\\)\\>") 'font-lock-keyword-face)
    ;;
    ;; Fontify case/goto keywords and targets, and case default/goto tags.
    '("\\<\\(case\\|goto\\)\\>[ \t]*\\([^ \t\n:;]+\\)?"
      (1 font-lock-keyword-face) (2 font-lock-reference-face nil t))
    '("^[ \t]*\\(\\sw+\\)[ \t]*:" 1 font-lock-reference-face)
    )))

 (setq c-font-lock-keywords-3
  (append c-font-lock-keywords-2
   ;;
   ;; More complicated regexps for more complete highlighting for types.
   ;; We still have to fontify type specifiers individually, as C is so hairy.
   (list
    ;;
    ;; Fontify all storage classes and type specifiers, plus their items.
    (list (concat "\\<\\(" c-type-types "\\)\\>"
		  "\\([ \t*&]+\\sw+\\>\\)*")
	  ;; Fontify each declaration item.
	  '(font-lock-match-c++-style-declaration-item-and-skip-to-next
	    ;; Start with point after all type specifiers.
	    (goto-char (or (match-beginning 8) (match-end 1)))
	    ;; Finish with point after first type specifier.
	    (goto-char (match-end 1))
	    ;; Fontify as a variable or function name.
	    (1 (if (match-beginning 4)
		   font-lock-function-name-face
		 font-lock-variable-name-face))))
    ;;
    ;; Fontify structures, or typedef names, plus their items.
    '("\\(}\\)[ \t*]*\\sw"
      (font-lock-match-c++-style-declaration-item-and-skip-to-next
       (goto-char (match-end 1)) nil
       (1 (if (match-beginning 4)
	      font-lock-function-name-face
	    font-lock-variable-name-face))))
    ;;
    ;; Fontify anything at beginning of line as a declaration or definition.
    '("^\\(\\sw+\\)\\>\\([ \t*]+\\sw+\\>\\)*"
      (1 font-lock-type-face)
      (font-lock-match-c++-style-declaration-item-and-skip-to-next
       (goto-char (or (match-beginning 2) (match-end 1))) nil
       (1 (if (match-beginning 4)
	      font-lock-function-name-face
	    font-lock-variable-name-face))))
    )))

 (setq c++-font-lock-keywords-1
  (append
   ;;
   ;; The list `c-font-lock-keywords-1' less that for function names.
   ;; the simple function form regexp has been removed. --dmoore
   ;;(cdr c-font-lock-keywords-1)
   c-font-lock-keywords-1
   ;;
   ;; Fontify function name definitions, possibly incorporating class name.
   (list
    '("^\\(\\sw+\\)\\(::\\(\\sw+\\)\\)?[ \t]*("
      (1 (if (match-beginning 2)
	     font-lock-type-face
	   font-lock-function-name-face))
      (3 (if (match-beginning 2) font-lock-function-name-face) nil t))
    )))

 (setq c++-font-lock-keywords-2
  (append c++-font-lock-keywords-1
   (list
    ;;
    ;; The list `c-font-lock-keywords-2' for C++ plus operator overloading.
    (cons (concat "\\<\\(" c++-type-types "\\)\\>") 'font-lock-type-face)
    ;;
    ;; Fontify operator function name overloading.
    '("\\<\\(operator\\)\\>[ \t]*\\([][)(><!=+-][][)(><!=+-]?\\)?"
      (1 font-lock-keyword-face) (2 font-lock-function-name-face nil t))
    ;;
    ;; Fontify case/goto keywords and targets, and case default/goto tags.
    '("\\<\\(case\\|goto\\)\\>[ \t]*\\([^ \t\n:;]+\\)?"
      (1 font-lock-keyword-face) (2 font-lock-reference-face nil t))
    '("^[ \t]*\\(\\sw+\\)[ \t]*:[^:]" 1 font-lock-reference-face)
    ;;
    ;; Fontify other builtin keywords.
    (cons (concat "\\<\\(" c++-keywords "\\)\\>") 'font-lock-keyword-face)
    )))

 (setq c++-font-lock-keywords-3
  (append c++-font-lock-keywords-2
   ;;
   ;; More complicated regexps for more complete highlighting for types.
   (list
    ;;
    ;; Fontify all storage classes and type specifiers, plus their items.
    (list (concat "\\<\\(" c++-type-types "\\)\\>"
		  "\\([ \t*&]+\\sw+\\>\\)*")
	  ;; Fontify each declaration item.
	  '(font-lock-match-c++-style-declaration-item-and-skip-to-next
	    ;; Start with point after all type specifiers.
	    (goto-char (or (match-beginning 13) (match-end 1)))
	    ;; Finish with point after first type specifier.
	    (goto-char (match-end 1))
	    ;; Fontify as a variable or function name.
	    (1 (cond ((match-beginning 2) 'font-lock-type-face)
		     ((match-beginning 4) 'font-lock-function-name-face)
		     (t 'font-lock-variable-name-face)))
	    (3 (if (match-beginning 4)
		   'font-lock-function-name-face
		 'font-lock-variable-name-face) nil t)))
    ;;
    ;; Fontify structures, or typedef names, plus their items.
    '("\\(}\\)[ \t*]*\\sw"
      (font-lock-match-c++-style-declaration-item-and-skip-to-next
       (goto-char (match-end 1)) nil
       (1 (if (match-beginning 4)
	      font-lock-function-name-face
	    font-lock-variable-name-face))))
    ;;
    ;; Fontify anything at beginning of line as a declaration or definition.
    '("^\\(\\sw+\\)\\>\\([ \t*]+\\sw+\\>\\)*"
      (1 font-lock-type-face)
      (font-lock-match-c++-style-declaration-item-and-skip-to-next
       (goto-char (or (match-beginning 2) (match-end 1))) nil
       (1 (cond ((match-beginning 2) 'font-lock-type-face)
		((match-beginning 4) 'font-lock-function-name-face)
		(t 'font-lock-variable-name-face)))
       (3 (if (match-beginning 4)
	      'font-lock-function-name-face
	    'font-lock-variable-name-face) nil t)))
    )))
 )

(defvar c-font-lock-keywords c-font-lock-keywords-1
  "Default expressions to highlight in C mode.")

(defvar c++-font-lock-keywords c++-font-lock-keywords-1
  "Default expressions to highlight in C++ mode.")

;; The previous version, before replacing it with the FSF version.
;(defconst c-font-lock-keywords-1 nil
; "For consideration as a value of `c-font-lock-keywords'.
;This does fairly subdued highlighting.")
;
;(defconst c-font-lock-keywords-2 nil
; "For consideration as a value of `c-font-lock-keywords'.
;This does a lot more highlighting.")
;
;(let ((storage "auto\\|extern\\|register\\|static\\|volatile")
;      (prefixes "unsigned\\|short\\|long\\|const")
;      (types (concat "int\\|long\\|char\\|float\\|double\\|void\\|struct\\|"
;		      "union\\|enum\\|typedef"))
;      (ctoken "\\(\\sw\\|\\s_\\|[:~*&]\\)+")
;      )
;  (setq c-font-lock-keywords-1 (purecopy
;   (list
;    ;; fontify preprocessor directives.
;    '("^#[ \t]*[a-z]+" . font-lock-preprocessor-face)
;    ;;
;    ;; fontify names being defined.
;    '("^#[ \t]*\\(define\\|undef\\)[ \t]+\\(\\(\\sw\\|\\s_\\)+\\)" 2
;      font-lock-function-name-face)
;    ;;
;    ;; fontify other preprocessor lines.
;    '("^#[ \t]*\\(if\\|ifn?def\\|elif\\)[ \t]+\\([^\n]+\\)"
;      2 font-lock-function-name-face t)
;    ;;
;    ;; fontify the filename in #include <...>
;    ;; don't need to do this for #include "..." because those were
;    ;; already fontified as strings by the syntactic pass.
;    ;; (Changed to not include the <> in the face, since "" aren't.)
;    '("^#[ \t]*include[ \t]+<\\([^>\"\n]+\\)>" 1 font-lock-string-face)
;    ;;
;    ;; fontify the names of functions being defined.
;    ;; I think this should be fast because it's anchored at bol, but it's not.
;    (list (concat
;	    "^\\(" ctoken "[ \t]+\\)?"	; type specs; there can be no
;	    "\\(" ctoken "[ \t]+\\)?"	; more than 3 tokens, right?
;	    "\\(" ctoken "[ \t]+\\)?"
;	    "\\([*&]+[ \t]*\\)?"		; pointer
;	    "\\(" ctoken "\\)[ \t]*(")	; name
;	   8 'font-lock-function-name-face)
;    ;;
;    ;; This is faster but not by much.  I don't see why not.
;;    (list (concat "^\\(" ctoken "\\)[ \t]*(") 1 'font-lock-function-name-face)
;    ;;
;    ;; Fontify structure names (in structure definition form).
;    (list (concat "^\\(typedef[ \t]+struct\\|struct\\|static[ \t]+struct\\)"
;		   "[ \t]+\\(" ctoken "\\)[ \t]*\\(\{\\|$\\)")
;	   2 'font-lock-function-name-face)
;    ;;
;    ;; Fontify case clauses.  This is fast because its anchored on the left.
;    '("case[ \t]+\\(\\(\\sw\\|\\s_\\)+\\):". 1)
;    '("\\<\\(default\\):". 1)
;    )))
;
;  (setq c-font-lock-keywords-2 (purecopy
;   (append c-font-lock-keywords-1
;    (list
;     ;;
;     ;; fontify all storage classes and type specifiers
;     ;; types should be surrounded by non alphanumerics (Raymond Toy)
;     (cons (concat "\\<\\(" storage "\\)\\>") 'font-lock-type-face)
;     (list (concat "\\([^a-zA-Z0-9_]\\|^\\)\\("
;		    types
;		    "\\)\\([^a-zA-Z0-9_]\\|$\\)")
;	    2 'font-lock-type-face)
;     ;; fontify the prefixes now.  The types should have been fontified
;     ;; previously.
;     (list (concat "\\<\\(" prefixes "\\)[ \t]+\\(" types "\\)\\>")
;	    1 'font-lock-type-face)
;     ;;
;     ;; fontify all builtin tokens
;     (cons (concat
;	     "[ \t]\\("
;	     (mapconcat 'identity
;	      '("for" "while" "do" "return" "goto" "case" "break" "switch"
;		"if" "then" "else if" "else" "return" "continue" "default"
;		)
;	      "\\|")
;	     "\\)[ \t\n(){};,]")
;	    1)
;     ;;
;     ;; fontify case targets and goto-tags.  This is slow because the
;     ;; expression is anchored on the right.
;     "\\(\\(\\sw\\|\\s_\\)+\\):"
;     ;;
;     ;; Fontify variables declared with structures, or typedef names.
;     '("}[ \t*]*\\(\\(\\sw\\|\\s_\\)+\\)[ \t]*[,;]"
;	1 font-lock-function-name-face)
;     ;;
;     ;; Fontify global variables without a type.
;;     '("^\\([_a-zA-Z0-9:~*]+\\)[ \t]*[[;={]" 1 font-lock-function-name-face)
;
;     ))))
;  )
;
;
;;; default to the gaudier variety?
;;(defconst c-font-lock-keywords c-font-lock-keywords-2
;;  "Additional expressions to highlight in C mode.")
;(defconst c-font-lock-keywords c-font-lock-keywords-1
;  "Additional expressions to highlight in C mode.")
;
;(defconst c++-font-lock-keywords-1 nil
; "For consideration as a value of `c++-font-lock-keywords'.
;This does fairly subdued highlighting.")
;
;(defconst c++-font-lock-keywords-2 nil
; "For consideration as a value of `c++-font-lock-keywords'.
;This does a lot more highlighting.")
;
;(let ((ctoken "\\(\\sw\\|\\s_\\|[:~*&]\\)+")
;      (c++-types (concat "complex\\|public\\|private\\|protected\\|virtual\\|"
;			  "friend\\|inline"))
;      c++-font-lock-keywords-internal-1
;      c++-font-lock-keywords-internal-2
;      )
;  (setq c++-font-lock-keywords-internal-1 (purecopy
;   (list
;    ;;
;    ;; fontify friend operator functions
;    '("^\\(operator[^(]*\\)(" 1 font-lock-function-name-face)
;    '("^\\(operator[ \\t]*([ \\t]*)[^(]*\\)(" 1 font-lock-function-name-face)
;
;    ;; fontify the class names only in the definition
;    (list (concat "^class[ \t]+" ctoken "[ \t\n{: ;]") 1
;	   'font-lock-function-name-face)
;
;    (list (concat
;	    "^\\(" ctoken "[ \t]+\\)?" ; type specs; there can be no
;	    "\\(" ctoken "[ \t]+\\)?" ; more than 3 tokens, right?
;	    "\\(" ctoken "[ \t]+\\)?"
;	    "\\(\\*+[ \t]*\\)?"	; pointer
;	    "\\(" ctoken "\\(::\\)?~?\\(\\(operator[ \t]*[^ \ta-zA-Z]+\\)\\|"
;	    ctoken "\\)\\)[ \t]*(") ; name
;	   8 'font-lock-function-name-face t)
;    )))
;
;  (setq c++-font-lock-keywords-internal-2 (purecopy
;   (list
;    ;; fontify extra c++ storage classes and type specifiers
;    (cons (concat "\\<\\(" c++-types "\\)\\>") 'font-lock-type-face)
;
;    ;;special check for class
;    '("^\\(\\<\\|template[ \t]+<[ \t]*\\)\\(class\\)[ \t\n]+" 2
;      font-lock-type-face)
;
;    ;; special handling of template
;    "^\\(template\\)\\>"
;    ;; fontify extra c++ builtin tokens
;    (cons (concat
;	    "[ \t]\\("
;	    (mapconcat 'identity
;		       '("asm" "catch" "throw" "try" "delete" "new" "operator"
;			 "sizeof" "this"
;			 )
;		       "\\|")
;	    "\\)[ \t\n(){};,]")
;	   1)
;    )))
;
;  (setq c++-font-lock-keywords-1 (purecopy
;   (append c-font-lock-keywords-1 c++-font-lock-keywords-internal-1)))
;
;  (setq c++-font-lock-keywords-2 (purecopy
;   (append c-font-lock-keywords-2 c++-font-lock-keywords-internal-1
;	    c++-font-lock-keywords-internal-2)))
;  )
;
;(defconst c++-font-lock-keywords c++-font-lock-keywords-1
;  "Additional expressions to highlight in C++ mode.")

;; Java support from Anders Lindgren and Bob Weiner

(defconst java-font-lock-keywords-1 nil
 "For consideration as a value of `java-font-lock-keywords'.
This does fairly subdued highlighting.")

(defconst java-font-lock-keywords-2 nil
 "For consideration as a value of `java-font-lock-keywords'.
This adds highlighting of types and identifier names.")

(defconst java-font-lock-keywords-3 nil
 "For consideration as a value of `java-font-lock-keywords'.
This adds highlighting of Java documentation tags, such as @see.")

(defvar java-font-lock-type-regexp
  (concat "\\<\\(boolean\\|byte\\|char\\|double\\|float\\|int"
         "\\|long\\|short\\|void\\)\\>")
  "Regexp which should match a primitive type.")

(let ((capital-letter "A-Z\300-\326\330-\337")
      (letter "a-zA-Z_$\300-\326\330-\366\370-\377")
      (digit  "0-9"))
(defvar java-font-lock-identifier-regexp
  (concat "\\<\\([" letter "][" letter digit "]*\\)\\>")
  "Regexp which should match all Java identifiers.")

(defvar java-font-lock-class-name-regexp
  (concat "\\<\\([" capital-letter "][" letter digit "]*\\)\\>")
  "Regexp which should match a class or an interface name.
The name is assumed to begin with a capital letter.")
)


(let ((java-modifier-regexp
       (concat "\\<\\(abstract\\|const\\|final\\|native\\|"
	       "private\\|protected\\|public\\|"
	       "static\\|synchronized\\|transient\\|volatile\\)\\>")))

  ;; Basic font-lock support:
  (setq java-font-lock-keywords-1
	(list
	 ;; Keywords:
	 (list        
	  (concat
	   "\\<\\("
	   "break\\|byvalue\\|"
	   "case\\|cast\\|catch\\|class\\|continue\\|"
	   "do\\|else\\|extends\\|"
	   "finally\\|for\\|future\\|"
	   "generic\\|goto\\|"
	   "if\\|implements\\|import\\|"
	   "instanceof\\|interface\\|"
	   "new\\|package\\|return\\|switch\\|"
	   "throws?\\|try\\|while\\)\\>")
	  1 'font-lock-keyword-face)

	 ;; Modifiers:
	 (list java-modifier-regexp 1 font-lock-type-face)

	 ;; Special constants:
	 '("\\<\\(this\\|super\\)\\>" (1 font-lock-reference-face))
	 '("\\<\\(false\\|null\\|true\\)\\>" (1 font-lock-keyword-face))

	 ;; Class names:
	 (list (concat "\\<class\\>\\s *" java-font-lock-identifier-regexp)
	       1 'font-lock-function-name-face)
        
	 ;; Package declarations:
	 (list (concat "\\<\\(package\\|import\\)\\>\\s *"
		       java-font-lock-identifier-regexp)
	       '(2 font-lock-reference-face)
	       (list (concat
		      "\\=\\.\\(" java-font-lock-identifier-regexp "\\)")
		     nil nil '(1 (let ((c (char-after (match-end 0))))
				   (if (and (characterp c)
					    (= c ?.))
				       'font-lock-reference-face
				     'font-lock-type-face)))))

	 ;; Constructors:
	 (list (concat
		"^\\s *\\(" java-modifier-regexp "\\s +\\)*"
		java-font-lock-class-name-regexp "\\s *\(")
	       (list 3
		     '(condition-case nil
			  (save-excursion
			    (goto-char (scan-sexps (- (match-end 0) 1) 1))
			    (parse-partial-sexp (point) (point-max) nil t)
			    (and (looking-at "\\($\\|\\<throws\\>\\|{\\)")
				 'font-lock-function-name-face))
			(error 'font-lock-function-name-face))))

	 ;; Methods:
	 (list (concat "\\(" java-font-lock-type-regexp "\\|"
		       java-font-lock-class-name-regexp "\\)"
		       "\\s *\\(\\[\\s *\\]\\s *\\)*"
		       java-font-lock-identifier-regexp "\\s *\(")
	       5
	       'font-lock-function-name-face)

	 ;; Labels:
	 (list ":"
	       (list
		(concat "^\\s *" java-font-lock-identifier-regexp "\\s *:")
		'(beginning-of-line) '(end-of-line)
		'(1 font-lock-reference-face)))

	 ;; `break' and continue' destination labels:
	 (list (concat "\\<\\(break\\|continue\\)\\>\\s *"
		       java-font-lock-identifier-regexp)
	       2 'font-lock-reference-face)

	 ;; Case statements:
	 ;; In Java, any constant expression is allowed.
	 '("\\<case\\>\\s *\\(.*\\):" 1 font-lock-reference-face)))

  ;; Types and declared variable names:
  (setq java-font-lock-keywords-2
	(append 

	 java-font-lock-keywords-1
	 (list
	  ;; Keywords followed by a type:
	  (list (concat "\\<\\(extends\\|instanceof\\|new\\)\\>\\s *"
			java-font-lock-identifier-regexp)
		'(2 (if (= (char-after (match-end 0)) ?.)
			'font-lock-reference-face 'font-lock-type-face))
		(list (concat "\\=\\." java-font-lock-identifier-regexp)
		      '(goto-char (match-end 0)) nil
		      '(1 (if (= (char-after (match-end 0)) ?.)
			      'font-lock-reference-face 'font-lock-type-face))))

	  ;; Keywords followed by a type list:
	  (list (concat "\\<\\(implements\\|throws\\)\\>\\ s*"
			java-font-lock-identifier-regexp)
		'(2 (if (= (char-after (match-end 0)) ?.)
			font-lock-reference-face font-lock-type-face))
		(list (concat "\\=\\(\\.\\|\\s *\\(,\\)\\s *\\)"
			      java-font-lock-identifier-regexp)
		      '(goto-char (match-end 0)) nil
		      '(3 (if (= (char-after (match-end 0)) ?.)
			      font-lock-reference-face font-lock-type-face))))

	  ;; primitive types, can't be confused with anything else.
	  (list java-font-lock-type-regexp
		'(1 font-lock-type-face)
		'(font-lock-match-java-declarations
		  (goto-char (match-end 0))
		  (goto-char (match-end 0))
		  (0 font-lock-variable-name-face)))

	  ;; Declarations, class types and capitalized variables:
	  ;;
	  ;; Declarations are easy to recognize.  Capitalized words
	  ;; followed by a closing parenthesis are treated as casts if they
	  ;; also are followed by an expression.  Expressions beginning with
	  ;; a unary numerical operator, e.g. +, can't be cast to an object
	  ;; type.
	  ;;
	  ;; The path of a fully qualified type, e.g. java.lang.Foo, is
	  ;; fontified in the reference face.
	  ;;
	  ;; An access to a static field, e.g. System.out.println, is
	  ;; not fontified since it can't be distinguished from the
	  ;; usage of a capitalized variable, e.g. Foo.out.println.

	  (list (concat java-font-lock-class-name-regexp
			"\\s *\\(\\[\\s *\\]\\s *\\)*"
			"\\(\\<\\|$\\|)\\s *\\([\(\"]\\|\\<\\)\\)")
		'(1 (save-match-data
		      (save-excursion
			(goto-char
			 (match-beginning 3))
			(if (not (looking-at "\\<instanceof\\>"))
			    'font-lock-type-face))))
		(list (concat "\\=" java-font-lock-identifier-regexp "\\.")
		      '(progn
			 (goto-char (match-beginning 0))
			 (while (or (= (preceding-char) ?.)
				    (= (char-syntax (preceding-char)) ?w))
			   (backward-char)))
		      '(goto-char (match-end 0))
		      '(1 font-lock-reference-face)
		      '(0 nil))		; Workaround for bug in XEmacs.
		'(font-lock-match-java-declarations
		  (goto-char (match-end 1))
		  (goto-char (match-end 0))
		  (1 font-lock-variable-name-face))))))

  ;; Modifier keywords and Java doc tags
  (setq java-font-lock-keywords-3
	(append

	 '(
	   ;; Feature scoping:
	   ;; These must come first or the Modifiers from keywords-1 will
	   ;; catch them.  We don't want to use override fontification here
	   ;; because then these terms will be fontified within comments.
	   ("\\<private\\>"   0 font-lock-string-face)
	   ("\\<protected\\>" 0 font-lock-preprocessor-face)
	   ("\\<public\\>"    0 font-lock-reference-face))
	 java-font-lock-keywords-2

	 (list

	  ;; Java doc tags
	  '("@\\(author\\|exception\\|param\\|return\\|see\\|version\\)\\s "
	    0 font-lock-keyword-face t)

	  ;; Doc tag - Parameter identifiers
	  (list (concat "@param\\s +" java-font-lock-identifier-regexp)
		1 'font-lock-variable-name-face t)

	  ;; Doc tag - Exception types
	  (list (concat "@exception\\ s*"
			java-font-lock-identifier-regexp)
		'(1 (if (= (char-after (match-end 0)) ?.)
			font-lock-reference-face font-lock-type-face) t)
		(list (concat "\\=\\." java-font-lock-identifier-regexp)
		      '(goto-char (match-end 0)) nil
		      '(1 (if (= (char-after (match-end 0)) ?.)
			      'font-lock-reference-face 'font-lock-type-face) t)))

	  ;; Doc tag - Cross-references, usually to methods 
	  '("@see\\s +\\(\\S *[^][ \t\n\r\f(){},.;:]\\)"
	    1 font-lock-function-name-face t)

	  )))
  )

(defvar java-font-lock-keywords java-font-lock-keywords-1
  "Additional expressions to highlight in Java mode.")

;; Match and move over any declaration/definition item after
;; point.  Does not match items which look like a type declaration
;; (primitive types and class names, i.e. capitalized words.)
;; Should the variable name be followed by a comma, we reposition
;; the cursor to fontify more identifiers.
(defun font-lock-match-java-declarations (limit)
  "Match and skip over variable definitions."
  (if (looking-at "\\s *\\(\\[\\s *\\]\\s *\\)*")
      (goto-char (match-end 0)))
  (and
   (looking-at java-font-lock-identifier-regexp)
   (save-match-data
     (not (string-match java-font-lock-type-regexp
			(buffer-substring (match-beginning 1)
					  (match-end 1)))))
   (save-match-data
     (save-excursion
       (goto-char (match-beginning 1))
       (not (looking-at
	     (concat java-font-lock-class-name-regexp
		     "\\s *\\(\\[\\s *\\]\\s *\\)*\\<")))))
   (save-match-data
     (condition-case nil
	 (save-restriction
	   (narrow-to-region (point-min) limit)
	   (goto-char (match-end 0))
	   ;; Note: Both `scan-sexps' and the second goto-char can
	   ;; generate an error which is caught by the
	   ;; `condition-case' expression.
	   (while (not (looking-at "\\s *\\(\\(,\\)\\|;\\|$\\)"))
	     (goto-char (or (scan-sexps (point) 1) (point-max))))
	   (goto-char (match-end 2)))   ; non-nil
       (error t)))))


(defvar tex-font-lock-keywords
;  ;; Regexps updated with help from Ulrik Dickow <dickow@nbi.dk>.
;  '(("\\\\\\(begin\\|end\\|newcommand\\){\\([a-zA-Z0-9\\*]+\\)}"
;     2 font-lock-function-name-face)
;    ("\\\\\\(cite\\|label\\|pageref\\|ref\\){\\([^} \t\n]+\\)}"
;     2 font-lock-reference-face)
;    ;; It seems a bit dubious to use `bold' and `italic' faces since we might
;    ;; not be able to display those fonts.
;    ("{\\\\bf\\([^}]+\\)}" 1 'bold keep)
;    ("{\\\\\\(em\\|it\\|sl\\)\\([^}]+\\)}" 2 'italic keep)
;    ("\\\\\\([a-zA-Z@]+\\|.\\)" . font-lock-keyword-face)
;    ("^[ \t\n]*\\\\def[\\\\@]\\(\\w+\\)" 1 font-lock-function-name-face keep))
  ;; Rewritten and extended for LaTeX2e by Ulrik Dickow <dickow@nbi.dk>.
  '(("\\\\\\(begin\\|end\\|newcommand\\){\\([a-zA-Z0-9\\*]+\\)}"
     2 font-lock-function-name-face)
    ("\\\\\\(cite\\|label\\|pageref\\|ref\\){\\([^} \t\n]+\\)}"
     2 font-lock-reference-face)
    ("^[ \t]*\\\\def\\\\\\(\\(\\w\\|@\\)+\\)" 1 font-lock-function-name-face)
    "\\\\\\([a-zA-Z@]+\\|.\\)"
    ;; It seems a bit dubious to use `bold' and `italic' faces since we might
    ;; not be able to display those fonts.
    ;; LaTeX2e: \emph{This is emphasized}.
    ("\\\\emph{\\([^}]+\\)}" 1 'italic keep)
    ;; LaTeX2e: \textbf{This is bold}, \textit{...}, \textsl{...}
    ("\\\\text\\(\\(bf\\)\\|it\\|sl\\){\\([^}]+\\)}"
     3 (if (match-beginning 2) 'bold 'italic) keep)
    ;; Old-style bf/em/it/sl. Stop at `\\' and un-escaped `&', for good tables.
    ("\\\\\\(\\(bf\\)\\|em\\|it\\|sl\\)\\>\\(\\([^}&\\]\\|\\\\[^\\]\\)+\\)"
     3 (if (match-beginning 2) 'bold 'italic) keep))
  "Default expressions to highlight in TeX modes.")

;; The previous version, before replacing it with the FSF version.
;(defconst tex-font-lock-keywords (purecopy
;  (list
;   ;; Lionel Mallet: Thu Oct 14 09:41:38 1993
;   ;; I've added an exit condition to the regexp below, and the other
;   ;; regexps for the second part.
;   ;; What would be useful here is something like:
;   ;; ("\\(\\\\\\w+\\)\\({\\(\\w+\\)}\\)+" 1 font-lock-keyword-face t 3
;   ;;  font-lock-function-name-face t)
;   '("\\(\\\\\\w+\\)\\W" 1 font-lock-keyword-face t)
;   '("\\(\\\\\\w+\\){\\([^}\n]+\\)}" 2 font-lock-function-name-face t)
;   '("\\(\\\\\\w+\\){\\(\\w+\\)}{\\(\\w+\\)}" 3
;     font-lock-function-name-face t)
;   '("\\(\\\\\\w+\\){\\(\\w+\\)}{\\(\\w+\\)}{\\(\\w+\\)}" 4
;     font-lock-function-name-face t)
;   '("{\\\\\\(em\\|tt\\)\\([^}]+\\)}" 2 font-lock-comment-face t)
;   '("{\\\\bf\\([^}]+\\)}" 1 font-lock-keyword-face t)
;   '("^[ \t\n]*\\\\def[\\\\@]\\(\\w+\\)\\W" 1 font-lock-function-name-face t)
;   ;; Lionel Mallet: Thu Oct 14 09:40:10 1993
;   ;; the regexp below is useless as it is now covered by the first 2 regexps
;   ;;   '("\\\\\\(begin\\|end\\){\\([a-zA-Z0-9\\*]+\\)}"
;   ;;     2 font-lock-function-name-face t)
;   '("[^\\\\]\\$\\([^$]*\\)\\$" 1 font-lock-string-face t)
;;   '("\\$\\([^$]*\\)\\$" 1 font-lock-string-face t)
;   ))
;  "Additional expressions to highlight in TeX mode.")

(defconst ksh-font-lock-keywords (purecopy
  (list
   '("\\(^\\|[^\$\\\]\\)#.*" . font-lock-comment-face)
   '("\\<\\(if\\|then\\|else\\|elif\\|fi\\|case\\|esac\\|for\\|do\\|done\\|foreach\\|in\\|end\\|select\\|while\\|repeat\\|time\\|function\\|until\\|exec\\|command\\|coproc\\|noglob\\|nohup\\|nocorrect\\|source\\|autoload\\|alias\\|unalias\\|export\\|set\\|echo\\|eval\\|cd\\|log\\|compctl\\)\\>" . font-lock-keyword-face)
   '("\\<\\[\\[.*\\]\\]\\>" . font-lock-type-face)
   '("\$\(.*\)" . font-lock-type-face)
   ))
  "Additional expressions to highlight in ksh-mode.")

(defconst sh-font-lock-keywords (purecopy
  (list
   '("\\(^\\|[^\$\\\]\\)#.*" . font-lock-comment-face)
   '("\\<\\(if\\|then\\|else\\|elif\\|fi\\|case\\|esac\\|for\\|do\\|done\\|in\\|while\\|exec\\|export\\|set\\|echo\\|eval\\|cd\\)\\>" . font-lock-keyword-face)
   '("\\[.*\\]" . font-lock-type-face)
   '("`.*`" . font-lock-type-face)
   ))
  "Additional expressions to highlight in sh-mode.")


;; Install ourselves:

(add-hook 'find-file-hooks 'font-lock-set-defaults t)

(make-face 'font-lock-comment-face "Face to use for comments.")
(make-face 'font-lock-doc-string-face "Face to use for documentation strings.")
(make-face 'font-lock-string-face "Face to use for strings.")
(make-face 'font-lock-keyword-face "Face to use for keywords.")
(make-face 'font-lock-function-name-face "Face to use for function names.")
(make-face 'font-lock-variable-name-face "Face to use for variable names.")
(make-face 'font-lock-type-face "Face to use for type names.")
(make-face 'font-lock-reference-face "Face to use for reference names.")
(make-face 'font-lock-preprocessor-face
	   "Face to use for preprocessor commands.")

;; Backwards compatibility?

(if (eq t font-lock-use-colors)
    (setq font-lock-use-colors '(color)))

(if (eq t font-lock-use-fonts)
    (setq font-lock-use-fonts '(or (mono) (grayscale))))

(font-lock-apply-defaults 'font-lock-add-fonts font-lock-use-fonts)
(font-lock-apply-defaults 'font-lock-add-colors font-lock-use-colors)

;;;###autoload
(add-minor-mode 'font-lock-mode " Font")

;; Provide ourselves:

(provide 'font-lock)

;;; font-lock.el ends here
