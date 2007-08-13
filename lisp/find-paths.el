;;; find-paths.el --- setup various XEmacs paths

;; Copyright (C) 1985-1986, 1990, 1992-1997 Free Software Foundation, Inc.
;; Copyright (c) 1993, 1994 Sun Microsystems, Inc.
;; Copyright (C) 1995 Board of Trustees, University of Illinois

;; Author: Mike Sperber <sperber@informatik.uni-tuebingen.de>
;; Maintainer: XEmacs Development Team
;; Keywords: internal, dumped

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

;; This file is dumped with XEmacs.

;; This file contains the library functionality to find paths into the
;; XEmacs hierarchy.

;;; Code:

(defvar paths-version-control-bases '("RCS" "CVS" "SCCS")
  "File bases associated with version control.")

(defun paths-find-recursive-path (directories &optional max-depth exclude)
  "Return a list of the directory hierarchy underneath DIRECTORIES.
The returned list is sorted by pre-order and lexicographically.
MAX-DEPTH limits the depth of the search to MAX-DEPTH level,
if it is a number.  If MAX-DEPTH is NIL, the search depth is unlimited.
EXCLUDE is a list of directory names to exclude from the search."
  (let ((path '()))
    (while directories
      (let ((directory (file-name-as-directory
			(expand-file-name
			 (car directories)))))
	(if (file-directory-p directory)
	    (let ((raw-dirs
		   (if (equal 0 max-depth)
		       '()
		     (directory-files directory nil "^[^-.]" nil 'dirs-only)))
		  (reverse-dirs '()))

	      (while raw-dirs
		(if (null (member (car raw-dirs) exclude))
		    (setq reverse-dirs
			  (cons (expand-file-name (car raw-dirs) directory)
				reverse-dirs)))
		(setq raw-dirs (cdr raw-dirs)))

	      (let ((sub-path
		     (paths-find-recursive-path (reverse reverse-dirs)
						(if (numberp max-depth)
						    (- max-depth 1)
						  max-depth)
						exclude)))
		(setq path (nconc path
				  (list directory)
				  sub-path))))))
      (setq directories (cdr directories)))
    path))

(defun paths-find-recursive-load-path (directories &optional max-depth)
  "Construct a recursive load path underneath DIRECTORIES."
  (paths-find-recursive-path directories
			     max-depth  paths-version-control-bases))

(defun paths-emacs-root-p (directory)
  "Check if DIRECTORY is a plausible installation root for XEmacs."
  (or
   ;; installed
   (file-directory-p (paths-construct-path (list directory "lib" "xemacs")))
   ;; in-place
   (and 
    (file-directory-p (paths-construct-path (list directory "lib-src")))
    (file-directory-p (paths-construct-path (list directory "lisp")))
    (file-directory-p (paths-construct-path (list directory "src"))))))

(defun paths-chase-symlink (file-name)
  "Chase a symlink until the bitter end."
      (let ((maybe-symlink (file-symlink-p file-name)))
	(if maybe-symlink
	    (let* ((directory (file-name-directory file-name))
		   (destination (expand-file-name maybe-symlink directory)))
	      (paths-chase-symlink destination))
	  file-name)))

(defun paths-find-emacs-root
  (invocation-directory invocation-name)
  "Find the run-time root of XEmacs."
  (let* ((executable-file-name (paths-chase-symlink
				(concat invocation-directory
					invocation-name)))
	 (executable-directory (file-name-directory executable-file-name))
	 (maybe-root-1 (file-name-as-directory
			(paths-construct-path '("..") executable-directory)))
	 (maybe-root-2 (file-name-as-directory
			(paths-construct-path '(".." "..") executable-directory))))
    (or (and (paths-emacs-root-p maybe-root-1)
	     maybe-root-1)
	(and (paths-emacs-root-p maybe-root-2)
	     maybe-root-2))))

(defun paths-construct-path (components &optional expand-directory)
  "Convert list of path components COMPONENTS into a path.
If EXPAND-DIRECTORY is non-NIL, use it as a directory to feed
to EXPAND-FILE-NAME."
  (let* ((reverse-components (reverse components))
	 (last-component (car reverse-components))
	 (first-components (reverse (cdr reverse-components)))
	 (path
	  (apply #'concat
		 (append (mapcar #'file-name-as-directory first-components)
			 (list last-component)))))
    (if expand-directory
	(expand-file-name path expand-directory)
      path)))

(defun paths-construct-emacs-directory (root suffix base)
  "Construct a directory name within the XEmacs hierarchy."
  (file-name-as-directory
   (expand-file-name 
    (concat
     (file-name-as-directory root)
     suffix
     base))))

(defun paths-find-emacs-directory (roots suffix base &optional envvar default)
  "Find a directory in the XEmacs hierarchy.
ROOTS must be a list of installation roots.
SUFFIX is the subdirectory from there.
BASE is the base to look for.
ENVVAR is the name of the environment variable that might also
specify the directory.
DEFAULT is the preferred value."
  (let ((preferred-value (or (and envvar (getenv envvar))
			     default)))
    (if (and preferred-value
	     (file-directory-p preferred-value))
	(file-name-as-directory preferred-value)
      (catch 'gotcha
	(while roots
	  (let* ((root (car roots))
		 (path (paths-construct-emacs-directory root suffix base)))
	    ;; installed
	    (if (file-directory-p path)
		(throw 'gotcha path)
	      (let ((path (paths-construct-emacs-directory root "" base)))
		;; in-place
		(if (file-directory-p path)
		    (throw 'gotcha path)))))
	  (setq roots (cdr roots)))
	nil))))

(defun paths-find-site-directory (roots base &optional envvar default)
  "Find a site-specific directory in the XEmacs hierarchy."
  (paths-find-emacs-directory roots
			      (file-name-as-directory
			       (paths-construct-path '("lib" "xemacs")))
			      base
			      envvar default))

(defun paths-find-version-directory (roots base &optional envvar default)
  "Find a version-specific directory in the XEmacs hierarchy."
  (paths-find-emacs-directory roots
			      (file-name-as-directory
			       (paths-construct-path
				(list "lib"
				      (concat "xemacs-"
					      (construct-emacs-version)))))
			      base
			      envvar default))

(defun paths-find-architecture-directory (roots base &optional envvar default)
  "Find an architecture-specific directory in the XEmacs hierarchy."
  (or
   ;; from more to less specific
   (paths-find-version-directory roots
				 (concat base system-configuration)
				 envvar)
   (paths-find-version-directory roots
				 base
				 envvar)
   (paths-find-version-directory roots
				 system-configuration
				 envvar default)))
  
(defvar paths-path-emacs-version nil
  "Emacs version as it appears in paths.")

(defun construct-emacs-version ()
  "Construct the raw version number of XEmacs in the form XX.XX."
  ;; emacs-version isn't available early, but we really don't care then
  (if (null (boundp 'emacs-version))
      "XX.XX"
  (or paths-path-emacs-version		; cache
      (progn
	(string-match "\\`[^0-9]*\\([0-9]+\\.[0-9]+\\)" emacs-version)
	(let ((version (substring emacs-version
				  (match-beginning 1) (match-end 1))))
	  (if (string-match "(beta *\\([0-9]+\\))" emacs-version)
	      (setq version (concat version
				    "-b"
				    (substring emacs-version
					       (match-beginning 1) (match-end 1)))))
	  (setq paths-path-emacs-version version)
	  version)))))

(defun paths-directories-which-exist (directories)
  "Return the directories among DIRECTORIES."
  (let ((reverse-directories '()))
    (while directories
      (if (file-directory-p (car directories))
	  (setq reverse-directories 
		(cons (car directories)
		      reverse-directories)))
      (setq directories (cdr directories)))
    (reverse reverse-directories)))

(defun paths-uniq-append (list-1 list-2)
  "Append LIST-1 and LIST-2, omitting duplicates."
  (let ((reverse-survivors '()))
    (while list-2
      (if (null (member (car list-2) list-1))
	  (setq reverse-survivors (cons (car list-2) reverse-survivors)))
      (setq list-2 (cdr list-2)))
    (append list-1
	    (reverse reverse-survivors))))

(defun paths-delete (predicate list)
  "Delete all matches of PREDICATE from LIST."
  (let ((reverse-result '()))
    (while list
      (if (not (funcall predicate (car list)))
	  (setq reverse-result (cons (car list) reverse-result)))
      (setq list (cdr list)))
    (nreverse reverse-result)))

(defun paths-decode-directory-path (string &optional drop-empties)
  "Split STRING at path separators into a directory list.
Non-\"\" comonents are converted into directory form.
If DROP-EMPTIES is non-NIL, \"\" components are dropped from the output.
Otherwise, they are left alone."
  (let* ((components (decode-path-internal string))
	 (directories
	  (mapcar #'(lambda (component)
		      (if (string-equal "" component)
			  component
			(file-name-as-directory component)))
		  components)))
    (if drop-empties
	(paths-delete #'(lambda (component)
			  (string-equal "" component))
		      directories)
      directories)))

(defun paths-find-emacs-roots (invocation-directory
			       invocation-name)
  "Find all plausible installation roots for XEmacs."
  (let ((invocation-root
	 (paths-find-emacs-root invocation-directory invocation-name))
	(installation-root
	 (and configure-prefix-directory
	      (file-directory-p configure-prefix-directory)
	      (file-name-as-directory configure-prefix-directory))))
    (append (and invocation-root
		 (list invocation-root))
	    (and installation-root
		 (paths-emacs-root-p installation-root)
		 (list installation-root)))))

;;; find-paths.el ends here
