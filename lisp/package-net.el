;;; package-net.el --- Installation and Maintenance of XEmacs packages

;; Copyright (C) 2000 Andy Piper.

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
;; along with XEmacs; see the file COPYING.  If not, write to the Free
;; Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
;; 02111-1307, USA.

;;; Synched up with: Not in FSF

;;; Commentary:

;; Manipulate packages for the netinstall setup utility

(require 'package-admin)
(require 'package-get)

;; What path should we use from the myriad available?
;; For netinstall we just want something simple, and anyway this is only to 
;; bootstrap the process. This will be:
;; <root>/setup/ for native windows
;; <root>/lib/xemacs/setup for cygwin.
(defun package-net-setup-directory ()
  (file-truename (concat data-directory "../../" (if (eq system-type 'cygwin32)
						     "xemacs/setup/" "setup/"))))

(defun package-net-convert-index-to-ini (&optional destdir remote version)
  "Convert the package index to ini file format in DESTDIR.
DESTDIR defaults to the value of `data-directory'."
  (package-get-require-base remote)

  (setq destdir (file-name-as-directory (or destdir data-directory)))
  (let ((buf (get-buffer-create "*setup.ini*")))
    (unwind-protect
        (save-excursion
          (set-buffer buf)
          (erase-buffer buf)
          (goto-char (point-min))
          (let ((entries package-get-base) entry plist)
	    (insert "# This file is automatically generated.  If you edit it, your\n")
	    (insert "# edits will be discarded next time the file is generated.\n")
	    (insert "#\n\n")
	    (insert (format "setup-timestamp: %d\n" 
			    (+ (* (car (current-time)) 65536) (car (cdr (current-time))))))
	    (insert (format "setup-version: %s\n\n" (or version "1.0")))
	    ;; Native version
	    ;; We give the package a capitalised name so that it appears at the top
	    (insert (format "@ %s\n" "xemacs-i586-pc-win32"))
	    (insert (format "version: %s\n" emacs-program-version))
	    (insert "type: native\n")
	    (insert (format "install: binaries/win32/%s %d\n\n"
			    (concat emacs-program-name
				    "-i586-pc-win32-"
				    emacs-program-version ".tar.gz") 0))
	    ;; Cygwin version
	    ;; We give the package a capitalised name so that it appears at the top
	    (insert (format "@ %s\n" "xemacs-i686-pc-cygwin32"))
	    (insert (format "version: %s\n" emacs-program-version))
	    (insert "type: cygwin\n")
	    (insert (format "install: binaries/cygwin32/%s %d\n\n"
			    (concat emacs-program-name
				    "-i686-pc-cygwin32-"
				    emacs-program-version ".tar.gz") 6779200))
	    ;; Standard packages
	    (while entries
	      (setq entry (car entries))
	      (setq plist (car (cdr entry)))
	      (insert (format "@ %s\n" (symbol-name (car entry))))
	      (insert (format "version: %s\n" (plist-get plist 'version)))
	      (insert (format "install: packages/%s %s\n" (plist-get plist 'filename)
			      (plist-get plist 'size)))
	      ;; These are not supported as yet
	      ;;
	      ;; (insert (format "source: %s\n" (plist-get plist 'source)))
	      ;; (insert "[prev]\n")
	      ;; (insert (format "version: %s\n" (plist-get plist 'version)))
	      ;; (insert (format "install: %s\n" (plist-get plist 'filename)))
	      ;; (insert (format "source: %s\n" (plist-get plist 'source)))
	      (insert "\n")
	      (setq entries (cdr entries))))
	  (insert "# setup.ini file ends here\n")
	  (write-region (point-min) (point-max) (concat destdir "setup.ini")))
      (kill-buffer buf))))

(defun package-net-update-installed-db (&optional destdir)
  "Write out the installed package index in a net install suitable format.
If DESTDIR is non-nil then use that as the destination directory. 
DESTDIR defaults to the value of `package-net-setup-directory'."
  ;; Need the local version
  (package-get-require-base)

  (setq destdir (file-name-as-directory 
		 (or destdir (package-net-setup-directory))))
  (let ((buf (get-buffer-create "*installed.db*")))
    (unwind-protect
        (save-excursion
          (set-buffer buf)
          (erase-buffer buf)
          (goto-char (point-min))
          (let ((entries package-get-base) entry plist)
	    (while entries
	      (setq entry (car entries))
	      (setq plist (car (cdr entry)))
	      (insert (format "%s %s %s\n" (symbol-name (car entry))
			      (plist-get plist 'filename)
			      (plist-get plist 'size)))
	      (setq entries (cdr entries))))
	  (make-directory-path destdir)
	  (write-region (point-min) (point-max) (concat destdir "installed.db")))
      (kill-buffer buf))))

(defun package-net-convert-download-sites-to-mirrors (&optional destdir)
  "Write out the download site list in a net install suitable format.
If DESTDIR is non-nil then use that as the destination directory. 
DESTDIR defaults to the value of `data-directory'."

  (setq destdir (file-name-as-directory (or destdir data-directory)))
  (let ((buf (get-buffer-create "*mirrors.lst*")))
    (unwind-protect
        (save-excursion
          (set-buffer buf)
          (erase-buffer buf)
          (goto-char (point-min))
          (let ((entries package-get-download-sites) entry)
	    (while entries
	      (setq entry (car entries))
	      (insert (format "ftp://%s/%s;%s;%s\n"
			      (nth 1 entry) (substring (nth 2 entry) 0 -9)
			      (nth 0 entry) (nth 0 entry)))
	      (setq entries (cdr entries))))
	  (write-region (point-min) (point-max) (concat destdir "mirrors.lst")))
      (kill-buffer buf))))