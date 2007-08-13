;;; update-elc.el --- Bytecompile out-of-date dumped files

;; Copyright (C) 1997 Free Software Foundation, Inc.
;; Copyright (C) 1996 Unknown

;; Maintainer: XEmacs Development Team
;; Keywords: internal

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
;; Free Software Foundation, 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;;; Synched up with: Not in FSF.

;;; Commentary:

;; Byte compile the .EL files necessary to dump out xemacs.
;; Use this file like this:
;;
;; temacs -batch -l ../lisp/prim/update-elc.el $lisp
;;
;; where $lisp comes from the Makefile.  .elc files listed in $lisp will
;; cause the corresponding .el file to be compiled.  .el files listed in
;; $lisp will be ignored.
;;
;; (the idea here is that you can bootstrap if your .ELC files
;; are missing or badly out-of-date)

;; Currently this code gets the list of files to check passed to it from
;; src/Makefile.  This must be fixed.  -slb

;;; Code:

(setq update-elc-files-to-compile
      (delq nil
	    (mapcar (function
		     (lambda (x)
		       (if (string-match "\.elc$" x)
			   (let ((src (substring x 0 -1)))
			     (if (file-newer-than-file-p src x)
				 (progn
				   (and (file-exists-p x)
					(null (file-writable-p x))
					(set-file-modes x (logior (file-modes x) 128)))
				   src))))))
		    ;; -batch gets filtered out.
		    (nthcdr 3 command-line-args))))

(if update-elc-files-to-compile
    (progn
      (setq command-line-args
	    (cons (car command-line-args)
		  (append
		   '("-l" "loadup-el.el" "run-temacs"
		     "-batch" "-q" "-no-site-file"
		     "-l" "bytecomp" "-f" "batch-byte-compile")
		   update-elc-files-to-compile)))
      (load "loadup-el.el")))

(kill-emacs)

;;; update-elc.el ends here
