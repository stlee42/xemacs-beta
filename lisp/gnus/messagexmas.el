;;; messagexmas.el --- XEmacs extensions to message
;; Copyright (C) 1996 Free Software Foundation, Inc.

;; Author: Lars Magne Ingebrigtsen <larsi@ifi.uio.no>
;; Keywords: mail, news

;; This file is part of GNU Emacs.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;;; Commentary:

;;; Code:

(defvar message-xmas-dont-activate-region nil
  "If t, don't activate region after yanking.")

(defvar message-xmas-glyph-directory nil
  "*Directory where Message logos and icons are located.
If this variable is nil, Message will try to locate the directory
automatically.")

(defvar message-use-toolbar (if (featurep 'toolbar)
				'default-toolbar
			      nil)
  "*If nil, do not use a toolbar.
If it is non-nil, it must be a toolbar.  The five legal values are
`default-toolbar', `top-toolbar', `bottom-toolbar',
`right-toolbar', and `left-toolbar'.")

(defvar message-toolbar 
  '([message-spell ispell-message t "Spell"]
    [message-help (Info-goto-node "(Message)Top") t "Message help"])
  "The message buffer toolbar.")

(defun message-xmas-find-glyph-directory (&optional package)
  (setq package (or package "message"))
  (let ((path load-path)
	(dir (symbol-value
	      (intern-soft (concat package "-xmas-glyph-directory"))))
	result)
    (if (and (stringp dir) (file-directory-p dir))
	dir
      ;; We try to find the dir by looking at the load path,
      ;; stripping away the last component and adding "etc/".
      (while path
	(if (and (car path)
		 (file-exists-p
		  (setq dir (concat
			     (file-name-directory
			      (directory-file-name (car path)))
			     "etc/" (or package "message") "/")))
		 (file-directory-p dir))
	    (setq result dir
		  path nil)
	  (setq path (cdr path))))
      result)))

(defun message-xmas-setup-toolbar (bar &optional force package)
  (let ((dir (message-xmas-find-glyph-directory package))
	(xpm (if (featurep 'xpm) "xpm" "xbm"))
	icon up down disabled name)
    (unless package
      (setq message-xmas-glyph-directory dir))
    (when dir
      (while bar
	(setq icon (aref (car bar) 0)
	      name (symbol-name icon)
	      bar (cdr bar))
	(when (or force
		  (not (boundp icon)))
	  (setq up (concat dir name "-up." xpm))
	  (setq down (concat dir name "-down." xpm))
	  (setq disabled (concat dir name "-disabled." xpm))
	  (if (not (file-exists-p up))
	      (setq bar nil
		    dir nil)
	    (set icon (toolbar-make-button-list
		       up (and (file-exists-p down) down)
		       (and (file-exists-p disabled) disabled)))))))
    dir))

(defun message-setup-toolbar ()
  (and message-use-toolbar
       (message-xmas-setup-toolbar message-toolbar)
       (set-specifier (symbol-value message-use-toolbar)
		      (cons (current-buffer) message-toolbar))))

(defun message-xmas-exchange-point-and-mark ()
  "Exchange point and mark, but allow for XEmacs' optional argument."
  (exchange-point-and-mark message-xmas-dont-activate-region))

(fset 'message-exchange-point-and-mark 'message-xmas-exchange-point-and-mark)

(provide 'messagexmas)

;;; messagexmas.el ends here
