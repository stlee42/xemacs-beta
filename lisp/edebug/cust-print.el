;;; cust-print.el --- handles print-level and print-circle.

;; Copyright (C) 1992 Free Software Foundation, Inc.

;; Author: Daniel LaLiberte <liberte@cs.uiuc.edu>
;; Adapted-By: ESR
;; Keywords: extensions

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
;; along with XEmacs; see the file COPYING.  If not, write to the Free
;; Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
;; 02111-1307, USA.

;;; Synched up with: Not in FSF

;; LCD Archive Entry:
;; cust-print|Daniel LaLiberte|liberte@cs.uiuc.edu
;; |Handle print-level, print-circle and more.
;; |$Date: 1996/12/18 03:54:29 $|$Revision: 1.1.1.2 $|

;; ===============================
;; $Header: /afs/informatik.uni-tuebingen.de/local/web/xemacs/xemacs-cvs/XEmacs/xemacs-19/lisp/edebug/cust-print.el,v 1.1.1.2 1996/12/18 03:54:29 steve Exp $
;; $Log: cust-print.el,v $
;; Revision 1.1.1.2  1996/12/18 03:54:29  steve
;; XEmacs 19.15-b3
;;
;; Revision 1.4  1994/03/23  20:34:29  liberte
;; * Change "emacs" to "original" - I just can't decide. 
;;
;; Revision 1.3  1994/02/21  21:25:36  liberte
;; * Make custom-prin1-to-string more robust when errors occur.
;; * Change "internal" to "emacs".
;;
;; Revision 1.2  1993/11/22  22:36:36  liberte
;; * Simplified and generalized printer customization.
;;     custom-printers is an alist of (PREDICATE . PRINTER) pairs
;;     for any data types.  The PRINTER function should print to
;;     `standard-output'  add-custom-printer and delete-custom-printer
;;     change custom-printers.
;;
;; * Installation function now called install-custom-print.  The
;;     old name is still around for now.
;;
;; * New macro with-custom-print (added earlier) - executes like
;;     progn but with custom-print activated temporarily.
;;
;; * Cleaned up comments for replacements of standardard printers.
;;
;; * Changed custom-prin1-to-string to use a temporary buffer.
;;
;; * Internal symbols are prefixed with CP::.
;;
;; * Option custom-print-vectors (added earlier) - controls whether
;;     vectors should be printed according to print-length and
;;     print-length.  Emacs doesnt do this, but cust-print would
;;     otherwise do it only if custom printing is required.
;;
;; * Uninterned symbols are treated as non-read-equivalent.
;;


;;; Commentary:

;; This package provides a general print handler for prin1 and princ
;; that supports print-level and print-circle, and by the way,
;; print-length since the standard routines are being replaced.  Also,
;; to print custom types constructed from lists and vectors, use
;; custom-print-list and custom-print-vector.  See the documentation
;; strings of these variables for more details.  

;; If the results of your expressions contain circular references to
;; other parts of the same structure, the standard Emacs print
;; subroutines may fail to print with an untrappable error,
;; "Apparently circular structure being printed".  If you only use cdr
;; circular lists (where cdrs of lists point back; what is the right
;; term here?), you can limit the length of printing with
;; print-length.  But car circular lists and circular vectors generate
;; the above mentioned error in Emacs version 18.  Version
;; 19 supports print-level, but it is often useful to get a better
;; print representation of circular and shared structures; the print-circle
;; option may be used to print more concise representations.

;; There are three main ways to use this package.  First, you may
;; replace prin1, princ, and some subroutines that use them by calling
;; install-custom-print so that any use of these functions in
;; Lisp code will be affected; you can later reset with
;; uninstall-custom-print.  Second, you may temporarily install
;; these functions with the macro with-custom-print.  Third, you
;; could call the custom routines directly, thus only affecting the
;; printing that requires them.

;; Note that subroutines which call print subroutines directly will
;; not use the custom print functions.  In particular, the evaluation
;; functions like eval-region call the print subroutines directly.
;; Therefore, if you evaluate (aref circ-list 0), where circ-list is a
;; circular list rather than an array, aref calls error directly which
;; will jump to the top level instead of printing the circular list.

;; Uninterned symbols are recognized when print-circle is non-nil,
;; but they are not printed specially here.  Use the cl-packages package
;; to print according to print-gensym.

;; Obviously the right way to implement this custom-print facility is
;; in C or with hooks into the standard printer.  Please volunteer
;; since I don't have the time or need.  More CL-like printing
;; capabilities could be added in the future.

;; Implementation design: we want to use the same list and vector
;; processing algorithm for all versions of prin1 and princ, since how
;; the processing is done depends on print-length, print-level, and
;; print-circle.  For circle printing, a preprocessing step is
;; required before the final printing.  Thanks to Jamie Zawinski
;; for motivation and algorithms.


;;; Code:
;;=========================================================

;; If using cl-packages:

'(defpackage "cust-print"
   (:nicknames "CP" "custom-print")
   (:use "el")
   (:export
    print-level
    print-circle

    install-custom-print
    uninstall-custom-print
    custom-print-installed-p
    with-custom-print

    custom-prin1
    custom-princ
    custom-prin1-to-string
    custom-print
    custom-format
    custom-message
    custom-error

    custom-printers
    add-custom-printer
    ))

'(in-package cust-print)

(require 'backquote)

;; Emacs 18 doesnt have defalias.
;; Provide def for byte compiler.
(defun defalias (symbol func) (fset symbol func))
;; Better def when loaded.
(or (fboundp 'defalias) (fset 'defalias 'fset))


;; Variables:
;;=========================================================

;;(defvar print-length nil
;;  "*Controls how many elements of a list, at each level, are printed.
;;This is defined by emacs.")

(defvar print-level nil
  "*Controls how many levels deep a nested data object will print.  

If nil, printing proceeds recursively and may lead to
max-lisp-eval-depth being exceeded or an error may occur:
`Apparently circular structure being printed.'
Also see `print-length' and `print-circle'.

If non-nil, components at levels equal to or greater than `print-level'
are printed simply as `#'.  The object to be printed is at level 0,
and if the object is a list or vector, its top-level components are at
level 1.")


(defvar print-circle nil
  "*Controls the printing of recursive structures.  

If nil, printing proceeds recursively and may lead to
`max-lisp-eval-depth' being exceeded or an error may occur:
\"Apparently circular structure being printed.\"  Also see
`print-length' and `print-level'.

If non-nil, shared substructures anywhere in the structure are printed
with `#N=' before the first occurrence (in the order of the print
representation) and `#N#' in place of each subsequent occurrence,
where N is a positive decimal integer.

There is no way to read this representation in standard Emacs,
but if you need to do so, try the cl-read.el package.")


(defvar custom-print-vectors nil
  "*Non-nil if printing of vectors should obey print-level and print-length.

For Emacs 18, setting print-level, or adding custom print list or
vector handling will make this happen anyway.  Emacs 19 obeys
print-level, but not for vectors.")


;; Custom printers
;;==========================================================

(defconst custom-printers nil
  ;; e.g. '((symbolp . pkg::print-symbol))
  "An alist for custom printing of any type.
Pairs are of the form (PREDICATE . PRINTER).  If PREDICATE is true
for an object, then PRINTER is called with the object.
PRINTER should print to `standard-output' using CP::original-princ
if the standard printer is sufficient, or CP::prin for complex things.
The PRINTER should return the object being printed.

Don't modify this variable directly.  Use `add-custom-printer' and
`delete-custom-printer'")
;; Should CP::original-princ and CP::prin be exported symbols?
;; Or should the standard printers functions be replaced by
;; CP ones in elisp so that CP internal functions need not be called?

(defun add-custom-printer (pred printer)
  "Add a pair of PREDICATE and PRINTER to `custom-printers'.
Any pair that has the same PREDICATE is first removed."
  (setq custom-printers (cons (cons pred printer) 
			      (delq (assq pred custom-printers)
				    custom-printers)))
  ;; Rather than updating here, we could wait until CP::top-level is called.
  (CP::update-custom-printers))

(defun delete-custom-printer (pred)
  "Delete the custom printer associated with PREDICATE."
  (setq custom-printers (delq (assq pred custom-printers)
			      custom-printers))
  (CP::update-custom-printers))


(defun CP::use-custom-printer (object)
  ;; Default function returns nil.
  nil)

(defun CP::update-custom-printers ()
  ;; Modify the definition of CP::use-custom-printer
  (defalias 'CP::use-custom-printer
    ;; We dont really want to require the byte-compiler.
    ;; (byte-compile
     (` (lambda (object)
	  (cond
	   (,@ (mapcar (function 
			(lambda (pair)
			  (` (((, (car pair)) object) 
			      ((, (cdr pair)) object)))))
		       custom-printers))
	   ;; Otherwise return nil.
	   (t nil)
	   )))
     ;; )
  ))


;; Saving and restoring emacs printing routines.
;;====================================================

(defun CP::set-function-cell (symbol-pair)
  (defalias (car symbol-pair) 
    (symbol-function (car (cdr symbol-pair)))))

(defun CP::original-princ (object &optional stream)) ; dummy def

;; Save emacs routines.
(if (not (fboundp 'CP::original-prin1))
    (mapcar 'CP::set-function-cell
	    '((CP::original-prin1 prin1)
	      (CP::original-princ princ)
	      (CP::original-print print)
	      (CP::original-prin1-to-string prin1-to-string)
	      (CP::original-format format)
	      (CP::original-message message)
	      (CP::original-error error))))


(defalias 'install-custom-print-funcs 'install-custom-print)
(defun install-custom-print ()
  "Replace print functions with general, customizable, Lisp versions.
The emacs subroutines are saved away, and you can reinstall them
by running `uninstall-custom-print'."
  (interactive)
  (mapcar 'CP::set-function-cell
	  '((prin1 custom-prin1)
	    (princ custom-princ)
	    (print custom-print)
	    (prin1-to-string custom-prin1-to-string)
	    (format custom-format)
	    (message custom-message)
	    (error custom-error)
	    ))
  t)
  
(defalias 'uninstall-custom-print-funcs 'uninstall-custom-print)
(defun uninstall-custom-print ()
  "Reset print functions to their emacs subroutines."
  (interactive)
  (mapcar 'CP::set-function-cell
	  '((prin1 CP::original-prin1)
	    (princ CP::original-princ)
	    (print CP::original-print)
	    (prin1-to-string CP::original-prin1-to-string)
	    (format CP::original-format)
	    (message CP::original-message)
	    (error CP::original-error)
	    ))
  t)

(defalias 'custom-print-funcs-installed-p 'custom-print-installed-p)
(defun custom-print-installed-p ()
  "Return t if custom-print is currently installed, nil otherwise."
  (eq (symbol-function 'custom-prin1) (symbol-function 'prin1)))

(put 'with-custom-print-funcs 'edebug-form-spec '(body))
(put 'with-custom-print 'edebug-form-spec '(body))

(defalias 'with-custom-print-funcs 'with-custom-print)
(defmacro with-custom-print (&rest body)
  "Temporarily install the custom print package while executing BODY."
  (` (unwind-protect
	 (progn
	   (install-custom-print)
	   (,@ body))
       (uninstall-custom-print))))


;; Lisp replacements for prin1 and princ, and for some subrs that use them
;;===============================================================
;; - so far only the printing and formatting subrs.

(defun custom-prin1 (object &optional stream)
  "Output the printed representation of OBJECT, any Lisp object.
Quoting characters are printed when needed to make output that `read'
can handle, whenever this is possible.
Output stream is STREAM, or value of `standard-output' (which see).

This is the custom-print replacement for the standard `prin1'.  It
uses the appropriate printer depending on the values of `print-level'
and `print-circle' (which see)."
  (CP::top-level object stream 'CP::original-prin1))


(defun custom-princ (object &optional stream)
  "Output the printed representation of OBJECT, any Lisp object.
No quoting characters are used; no delimiters are printed around
the contents of strings.
Output stream is STREAM, or value of `standard-output' (which see).

This is the custom-print replacement for the standard `princ'."
  (CP::top-level object stream 'CP::original-princ))


(defun custom-prin1-to-string (object)
  "Return a string containing the printed representation of OBJECT,
any Lisp object.  Quoting characters are used when needed to make output
that `read' can handle, whenever this is possible.

This is the custom-print replacement for the standard `prin1-to-string'."
  (let ((buf (get-buffer-create " *custom-print-temp*")))
    ;; We must erase the buffer before printing in case an error 
    ;; occured during the last prin1-to-string and we are in debugger.
    (save-excursion
      (set-buffer buf)
      (erase-buffer))
    ;; We must be in the current-buffer when the print occurs.
    (custom-prin1 object buf)
    (save-excursion
      (set-buffer buf)
      (buffer-string)
      ;; We could erase the buffer again, but why bother?
      )))


(defun custom-print (object &optional stream)
  "Output the printed representation of OBJECT, with newlines around it.
Quoting characters are printed when needed to make output that `read'
can handle, whenever this is possible.
Output stream is STREAM, or value of `standard-output' (which see).

This is the custom-print replacement for the standard `print'."
  (CP::original-princ "\n" stream)
  (custom-prin1 object stream)
  (CP::original-princ "\n" stream))


(defun custom-format (fmt &rest args)
  "Format a string out of a control-string and arguments.  
The first argument is a control string.  It, and subsequent arguments
substituted into it, become the value, which is a string.
It may contain %s or %d or %c to substitute successive following arguments.
%s means print an argument as a string, %d means print as number in decimal,
%c means print a number as a single character.
The argument used by %s must be a string or a symbol;
the argument used by %d, %b, %o, %x or %c must be a number.

This is the custom-print replacement for the standard `format'.  It
calls the emacs `format' after first making strings for list,
vector, or symbol args.  The format specification for such args should
be `%s' in any case, so a string argument will also work.  The string
is generated with `custom-prin1-to-string', which quotes quotable
characters."
  (apply 'CP::original-format fmt
	 (mapcar (function (lambda (arg)
			     (if (or (listp arg) (vectorp arg) (symbolp arg))
				 (custom-prin1-to-string arg)
			       arg)))
		 args)))
	    
  
(defun custom-message (fmt &rest args)
  "Print a one-line message at the bottom of the screen.
The first argument is a control string.
It may contain %s or %d or %c to print successive following arguments.
%s means print an argument as a string, %d means print as number in decimal,
%c means print a number as a single character.
The argument used by %s must be a string or a symbol;
the argument used by %d or %c must be a number.

This is the custom-print replacement for the standard `message'.
See `custom-format' for the details."
  ;; It doesn't work to princ the result of custom-format as in:
  ;; (CP::original-princ (apply 'custom-format fmt args))
  ;; because the echo area requires special handling
  ;; to avoid duplicating the output.  
  ;; CP::original-message does it right.
  (apply 'CP::original-message  fmt
	 (mapcar (function (lambda (arg)
			     (if (or (listp arg) (vectorp arg) (symbolp arg))
				 (custom-prin1-to-string arg)
			       arg)))
		 args)))
	    

(defun custom-error (fmt &rest args)
  "Signal an error, making error message by passing all args to `format'.

This is the custom-print replacement for the standard `error'.
See `custom-format' for the details."
  (signal 'error (list (apply 'custom-format fmt args))))



;; Support for custom prin1 and princ
;;=========================================

;; Defs to quiet byte-compiler.
(defvar circle-table)
(defvar CP::current-level)

(defun CP::original-printer (object))  ; One of the standard printers.
(defun CP::low-level-prin (object))    ; Used internally.
(defun CP::prin (object))              ; Call this to print recursively.

(defun CP::top-level (object stream emacs-printer)
  ;; Set up for printing.
  (let ((standard-output (or stream standard-output))
	;; circle-table will be non-nil if anything is circular.
	(circle-table (and print-circle 
			   (CP::preprocess-circle-tree object)))
	(CP::current-level (or print-level -1)))

    (defalias 'CP::original-printer emacs-printer)
    (defalias 'CP::low-level-prin 
      (cond
       ((or custom-printers
	    circle-table
	    print-level			; comment out for version 19
	    ;; Emacs doesn't use print-level or print-length
	    ;; for vectors, but custom-print can.
	    (if custom-print-vectors
		(or print-level print-length)))
	'CP::print-object)
       (t 'CP::original-printer)))
    (defalias 'CP::prin 
      (if circle-table 'CP::print-circular 'CP::low-level-prin))

    (CP::prin object)
    object))


(defun CP::print-object (object)
  ;; Test object type and print accordingly.
  ;; Could be called as either CP::low-level-prin or CP::prin.
  (cond 
   ((null object) (CP::original-printer object))
   ((CP::use-custom-printer object) object)
   ((consp object) (CP::list object))
   ((vectorp object) (CP::vector object))
   ;; All other types, just print.
   (t (CP::original-printer object))))


(defun CP::print-circular (object)
  ;; Printer for `prin1' and `princ' that handles circular structures.
  ;; If OBJECT appears multiply, and has not yet been printed,
  ;; prefix with label; if it has been printed, use `#N#' instead.
  ;; Otherwise, print normally.
  (let ((tag (assq object circle-table)))
    (if tag
	(let ((id (cdr tag)))
	  (if (> id 0)
	      (progn
		;; Already printed, so just print id.
		(CP::original-princ "#")
		(CP::original-princ id)
		(CP::original-princ "#"))
	    ;; Not printed yet, so label with id and print object.
	    (setcdr tag (- id)) ; mark it as printed
	    (CP::original-princ "#")
	    (CP::original-princ (- id))
	    (CP::original-princ "=")
	    (CP::low-level-prin object)
	    ))
      ;; Not repeated in structure.
      (CP::low-level-prin object))))


;;================================================
;; List and vector processing for print functions.

(defun CP::list (list)
  ;; Print a list using print-length, print-level, and print-circle.
  (if (= CP::current-level 0)
      (CP::original-princ "#")
    (let ((CP::current-level (1- CP::current-level)))
      (CP::original-princ "(")
      (let ((length (or print-length 0)))

	;; Print the first element always (even if length = 0).
	(CP::prin (car list))
	(setq list (cdr list))
	(if list (CP::original-princ " "))
	(setq length (1- length))

	;; Print the rest of the elements.
	(while (and list (/= 0 length))
	  (if (and (listp list)
		   (not (assq list circle-table)))
	      (progn
		(CP::prin (car list))
		(setq list (cdr list)))

	    ;; cdr is not a list, or it is in circle-table.
	    (CP::original-princ ". ")
	    (CP::prin list)
	    (setq list nil))

	  (setq length (1- length))
	  (if list (CP::original-princ " ")))

	(if (and list (= length 0)) (CP::original-princ "..."))
	(CP::original-princ ")"))))
  list)


(defun CP::vector (vector)
  ;; Print a vector according to print-length, print-level, and print-circle.
  (if (= CP::current-level 0)
      (CP::original-princ "#")
    (let ((CP::current-level (1- CP::current-level))
	  (i 0)
	  (len (length vector)))
      (CP::original-princ "[")

      (if print-length
	  (setq len (min print-length len)))
      ;; Print the elements
      (while (< i len)
	(CP::prin (aref vector i))
	(setq i (1+ i))
	(if (< i (length vector)) (CP::original-princ " ")))

      (if (< i (length vector)) (CP::original-princ "..."))
      (CP::original-princ "]")
      ))
  vector)



;; Circular structure preprocessing
;;==================================

(defun CP::preprocess-circle-tree (object)
  ;; Fill up the table.  
  (let (;; Table of tags for each object in an object to be printed.
	;; A tag is of the form:
	;; ( <object> <nil-t-or-id-number> )
	;; The id-number is generated after the entire table has been computed.
	;; During walk through, the real circle-table lives in the cdr so we
	;; can use setcdr to add new elements instead of having to setq the
	;; variable sometimes (poor man's locf).
	(circle-table (list nil)))
    (CP::walk-circle-tree object)

    ;; Reverse table so it is in the order that the objects will be printed.
    ;; This pass could be avoided if we always added to the end of the
    ;; table with setcdr in walk-circle-tree.
    (setcdr circle-table (nreverse (cdr circle-table)))

    ;; Walk through the table, assigning id-numbers to those
    ;; objects which will be printed using #N= syntax.  Delete those
    ;; objects which will be printed only once (to speed up assq later).
    (let ((rest circle-table)
	  (id -1))
      (while (cdr rest)
	(let ((tag (car (cdr rest))))
	  (cond ((cdr tag)
		 (setcdr tag id)
		 (setq id (1- id))
		 (setq rest (cdr rest)))
		;; Else delete this object.
		(t (setcdr rest (cdr (cdr rest))))))
	))
    ;; Drop the car.
    (cdr circle-table)
    ))



(defun CP::walk-circle-tree (object)
  (let (read-equivalent-p tag)
    (while object
      (setq read-equivalent-p 
	    (or (numberp object) 
		(and (symbolp object)
		     ;; Check if it is uninterned.
		     (eq object (intern-soft (symbol-name object)))))
	    tag (and (not read-equivalent-p)
		     (assq object (cdr circle-table))))
      (cond (tag
	     ;; Seen this object already, so note that.
	     (setcdr tag t))

	    ((not read-equivalent-p)
	     ;; Add a tag for this object.
	     (setcdr circle-table
		     (cons (list object)
			   (cdr circle-table)))))
      (setq object
	    (cond 
	     (tag ;; No need to descend since we have already.
	      nil)

	     ((consp object)
	      ;; Walk the car of the list recursively.
	      (CP::walk-circle-tree (car object))
	      ;; But walk the cdr with the above while loop
	      ;; to avoid problems with max-lisp-eval-depth.
	      ;; And it should be faster than recursion.
	      (cdr object))

	     ((vectorp object)
	      ;; Walk the vector.
	      (let ((i (length object))
		    (j 0))
		(while (< j i)
		  (CP::walk-circle-tree (aref object j))
		  (setq j (1+ j))))))))))


;; Example.
;;=======================================

'(progn
   (progn
     ;; Create some circular structures.
     (setq circ-sym (let ((x (make-symbol "FOO"))) (list x x)))
     (setq circ-list (list 'a 'b (vector 1 2 3 4) 'd 'e 'f))
     (setcar (nthcdr 3 circ-list) circ-list)
     (aset (nth 2 circ-list) 2 circ-list)
     (setq dotted-circ-list (list 'a 'b 'c))
     (setcdr (cdr (cdr dotted-circ-list)) dotted-circ-list)
     (setq circ-vector (vector 1 2 3 4 (list 'a 'b 'c 'd) 6 7))
     (aset circ-vector 5 (make-symbol "-gensym-"))
     (setcar (cdr (aref circ-vector 4)) (aref circ-vector 5))
     nil)

   (install-custom-print)
   ;; (setq print-circle t)

   (let ((print-circle t))
     (or (equal (prin1-to-string circ-list) "#1=(a b [1 2 #1# 4] #1# e f)")
	 (error "circular object with array printing")))

   (let ((print-circle t))
     (or (equal (prin1-to-string dotted-circ-list) "#1=(a b c . #1#)")
	 (error "circular object with array printing")))

   (let* ((print-circle t)
	  (x (list 'p 'q))
	  (y (list (list 'a 'b) x 'foo x)))
     (setcdr (cdr (cdr (cdr y))) (cdr y))
     (or (equal (prin1-to-string y) "((a b) . #1=(#2=(p q) foo #2# . #1#))"
		)
	 (error "circular list example from CL manual")))

   (let ((print-circle nil))
     ;; cl-packages.el is required to print uninterned symbols like #:FOO.
     ;; (require 'cl-packages)
     (or (equal (prin1-to-string circ-sym) "(#:FOO #:FOO)")
	 (error "uninterned symbols in list")))
   (let ((print-circle t))
     (or (equal (prin1-to-string circ-sym) "(#1=FOO #1#)")
	 (error "circular uninterned symbols in list")))

   (uninstall-custom-print)
   )

(provide 'cust-print)

;;; cust-print.el ends here
