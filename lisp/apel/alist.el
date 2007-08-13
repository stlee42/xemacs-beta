;;; alist.el --- utility functions about assoc-list

;; Copyright (C) 1993,1994,1995,1996 Free Software Foundation, Inc.

;; Author: MORIOKA Tomohiko <morioka@jaist.ac.jp>
;; Version:
;;	$Id: alist.el,v 1.1 1997/06/03 04:18:34 steve Exp $
;; Keywords: alist

;; This file is part of SEMI (SEMI is Emacs MIME Interfaces).

;; This program is free software; you can redistribute it and/or
;; modify it under the terms of the GNU General Public License as
;; published by the Free Software Foundation; either version 2, or (at
;; your option) any later version.

;; This program is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;;; Code:

(defun put-alist (item value alist)
  "Modify ALIST to set VALUE to ITEM.
If there is a pair whose car is ITEM, replace its cdr by VALUE.
If there is not such pair, create new pair (ITEM . VALUE) and
return new alist whose car is the new pair and cdr is ALIST.
\[tomo's ELIS like function]"
  (let ((pair (assoc item alist)))
    (if pair
	(progn
	  (setcdr pair value)
	  alist)
      (cons (cons item value) alist)
      )))

(defun del-alist (item alist)
  "If there is a pair whose key is ITEM, delete it from ALIST.
\[tomo's ELIS emulating function]"
  (if (equal item (car (car alist)))
      (cdr alist)
    (let ((pr alist)
	  (r (cdr alist))
	  )
      (catch 'tag
	(while (not (null r))
	  (if (equal item (car (car r)))
	      (progn
		(rplacd pr (cdr r))
		(throw 'tag alist)))
	  (setq pr r)
	  (setq r (cdr r))
	  )
	alist))))

(defun set-alist (symbol item value)
  "Modify a alist indicated by SYMBOL to set VALUE to ITEM."
  (or (boundp symbol)
      (set symbol nil)
      )
  (set symbol (put-alist item value (symbol-value symbol)))
  )

(defun remove-alist (symbol item)
  "Remove ITEM from the alist indicated by SYMBOL."
  (and (boundp symbol)
       (set symbol (del-alist item (symbol-value symbol)))
       ))

(defun modify-alist (modifier default)
  "Modify alist DEFAULT into alist MODIFIER."
  (mapcar (function
	   (lambda (as)
	     (setq default (put-alist (car as)(cdr as) default))
	     ))
	  modifier)
  default)

(defun set-modified-alist (sym modifier)
  "Modify a value of a symbol SYM into alist MODIFIER.
The symbol SYM should be alist. If it is not bound,
its value regard as nil."
  (if (not (boundp sym))
      (set sym nil)
    )
  (set sym (modify-alist modifier (eval sym)))
  )


;;; @ end
;;;

(provide 'alist)

;;; alist.el ends here
