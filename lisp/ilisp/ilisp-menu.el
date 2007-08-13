;;; -*- Mode: Emacs-Lisp -*-

;;; ilisp-menu.el --

;;; This file is part of ILISP.
;;; Version: 5.7
;;;
;;; Copyright (C) 1990, 1991, 1992, 1993 Chris McConnell
;;;               1993, 1994 Ivan Vasquez
;;;               1994, 1995 Marco Antoniotti and Rick Busdiecker
;;;
;;; Other authors' names for which this Copyright notice also holds
;;; may appear later in this file.
;;;
;;; Send mail to 'ilisp-request@lehman.com' to be included in the
;;; ILISP mailing list. 'ilisp@lehman.com' is the general ILISP
;;; mailing list were bugs and improvements are discussed.
;;;
;;; ILISP is freely redistributable under the terms found in the file
;;; COPYING.



(require 'simple-menu)
(setplist 'lisp-command-menu nil)
(def-menu 'lisp-command-menu
    "Lisp"
  "These ILISP commands are available on the menu:"
  '(
    ("Break        Interupt current lisp."  
     (progn (switch-to-lisp t)
	    (interrupt-subjob-ilisp)))
    ("Doc          Menu of commands to get help on variables, etc."
     documentation-lisp-command-menu)
    ("Xpand        macroexpand-lisp."        macroexpand-lisp)
    ("Eval         Eval the surrounding defun." eval-defun-lisp)
    ("1E&G         Eval defun and goto Inferior LISP." eval-defun-and-go-lisp)
    (";            Comment the region."   comment-region-lisp)
    (")            find-unbalanced-lisp parens." find-unbalanced-lisp)
    ("]            close-all-lisp parens that are open." close-all-lisp)
    ("Trace        Traces the previous function symbol." trace-lisp)
    )
  )

(setplist 'documentation-lisp-command-menu nil)
(def-menu 'documentation-lisp-command-menu
    "Lisp help"
  "These commands are available for examining Lisp structures:"
  '(
    ("UDoc         Get user's documentation string." documentation-lisp)
    ("Rglist       Get the arglist for function." arglist-lisp)
    ("Insp         Inspect the current sexp." inspect-lisp)
    ("1Insp        Prompts for something to inspect." (inspect-lisp -4))
    ("Descr        Describe the current sexp." describe-lisp)
    ("1Descr       Prompts for something to describe." (describe-lisp -4))
    )
  )

(provide 'ilisp-menu)
