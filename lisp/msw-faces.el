;;; msw-faces.el --- mswindows-specific face stuff.

;;; Copyright (C) 1992, 1993, 1994 Free Software Foundation, Inc.
;;; Copyright (C) 1995, 1996 Ben Wing.

;; Author: Jamie Zawinski
;; Modified by:  Chuck Thompson
;; Modified by:  Ben Wing
;; Modified by:  Martin Buchholz
;; Rewritten for mswindows by:  Jonathan Harris

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

;; This file does the magic to parse mswindows font names, and make sure that the
;; default and modeline attributes of new frames are specified enough.

(defun mswindows-init-global-faces ()
  )

;;; ensure that the default face has some reasonable fallbacks if nothing
;;; else is specified.
(defun mswindows-init-device-faces (device)
  (or (face-font 'default 'global)
      (set-face-font 'default "Courier New:Regular:10")
      'global)
  (or (face-foreground 'default 'global)
      (set-face-foreground 'default "black" 'global 'mswindows))
  (or (face-background 'default 'global)
      (set-face-background 'default "white" 'global 'mswindows))
  (or (face-background 'modeline 'global)
      (set-face-background 'modeline "grey75" 'global 'mswindows))
  )


(defun mswindows-init-frame-faces (frame)
  )


;;; Fill in missing parts of a font spec. This is primarily intended as a
;;; helper function for the functions below.
;;; mswindows fonts look like:
;;;	fontname[:[weight][ style][:pointsize[:effects[:charset]]]]
;;; A minimal mswindows font spec looks like:
;;;	Courier New
;;; A maximal mswindows font spec looks like:
;;;	Courier New:Bold Italic:10:underline strikeout:ansi
;;; Missing parts of the font spec should be filled in with these values:
;;;	Courier New:Normal:10::ansi
(defun mswindows-font-canicolize-name (font)
  "Given a mswindows font specification, this returns its name in canonical
form."
  (cond ((font-instance-p font)
	 (let ((name (font-instance-name font)))
	   (cond ((string-match
		   "^[a-zA-Z ]+:[a-zA-Z ]*:[0-9]+:[a-zA-Z ]*:[a-zA-Z 0-9]*$"
		   name) name)
		 ((string-match "^[a-zA-Z ]+:[a-zA-Z ]*:[0-9]+:[a-zA-Z ]*$"
				name) (concat name ":ansi"))
		 ((string-match "^[a-zA-Z ]+:[a-zA-Z ]*:[0-9]+$" name)
		  (concat name "::ansi"))
		 ((string-match "^[a-zA-Z ]+:[a-zA-Z ]*$" name)
		  (concat name "10::ansi"))
		 ((string-match "^[a-zA-Z ]+$" name)
		  (concat name ":Normal:10::ansi"))
		 (t "Courier New:Normal:10::ansi"))))
	(t "Courier New:Normal:10::ansi")))

(defun mswindows-make-font-bold (font &optional device)
  "Given a mswindows font specification, this attempts to make a bold font.
If it fails, it returns nil."
  (if (font-instance-p font)
      (let ((name (mswindows-font-canicolize-name font)))
	(string-match "^[a-zA-Z ]+:\\([a-zA-Z ]*\\):" name)
	(make-font-instance (concat
			     (substring name 0 (match-beginning 1))
			     "Bold" (substring name (match-end 1)))
			    device t))))

(defun mswindows-make-font-unbold (font &optional device)
  "Given a mswindows font specification, this attempts to make a non-bold font.
If it fails, it returns nil."
  (if (font-instance-p font)
      (let ((name (mswindows-font-canicolize-name font)))
	(string-match "^[a-zA-Z ]+:\\([a-zA-Z ]*\\):" name)
	(make-font-instance (concat
			     (substring name 0 (match-beginning 1))
			     "Normal" (substring name (match-end 1)))
			    device t))))

(defun mswindows-make-font-italic (font &optional device)
  "Given a mswindows font specification, this attempts to make an `italic'
font. If it fails, it returns nil."
  (if (font-instance-p font)
      (let ((name (mswindows-font-canicolize-name font)))
	(string-match "^[a-zA-Z ]+:\\([a-zA-Z ]*\\):" name)
	(make-font-instance (concat
			     (substring name 0 (match-beginning 1))
			     "Italic" (substring name (match-end 1)))
			    device t))))

(defun mswindows-make-font-unitalic (font &optional device)
  "Given a mswindows font specification, this attempts to make a non-italic
font. If it fails, it returns nil."
  (if (font-instance-p font)
      (let ((name (mswindows-font-canicolize-name font)))
	(string-match "^[a-zA-Z ]+:\\([a-zA-Z ]*\\):" name)
	(make-font-instance (concat
			     (substring name 0 (match-beginning 1))
			     "Normal" (substring name (match-end 1)))
			    device t))))

(defun mswindows-make-font-bold-italic (font &optional device)
  "Given a mswindows font specification, this attempts to make a `bold-italic'
font. If it fails, it returns nil."
  (if (font-instance-p font)
      (let ((name (mswindows-font-canicolize-name font)))
	(string-match "^[a-zA-Z ]+:\\([a-zA-Z ]*\\):" name)
	(make-font-instance (concat
			     (substring name 0 (match-beginning 1))
			     "Bold Italic" (substring name (match-end 1)))
			    device t))))

(defun mswindows-find-smaller-font (font &optional device)
  "Loads a new, version of the given font (or font name).
Returns the font if it succeeds, nil otherwise.
If scalable fonts are available, this returns a font which is 1 point smaller.
Otherwise, it returns the next smaller version of this font that is defined."
  (if (font-instance-p font)
      (let (old-size (name (mswindows-font-canicolize-name font)))
	(string-match "^[a-zA-Z ]+:[a-zA-Z ]*:\\([0-9]+\\):" name)
	(setq old-size (string-to-int
			(substring name (match-beginning 1) (match-end 1))))
	(if (> old-size 0)
	    (make-font-instance (concat
				 (substring name 0 (match-beginning 1))
				 (int-to-string (- old-size 1))
				 (substring name (match-end 1)))
				device t)))))

(defun mswindows-find-larger-font (font &optional device)
  "Loads a new, slightly larger version of the given font (or font name).
Returns the font if it succeeds, nil otherwise.
If scalable fonts are available, this returns a font which is 1 point larger.
Otherwise, it returns the next larger version of this font that is defined."
  (if (font-instance-p font)
      (let (old-size (name (mswindows-font-canicolize-name font)))
	(string-match "^[a-zA-Z ]+:[a-zA-Z ]*:\\([0-9]+\\):" name)
	(setq old-size (string-to-int
			(substring name (match-beginning 1) (match-end 1))))
	(make-font-instance (concat
			     (substring name 0 (match-beginning 1))
			     (int-to-string (+ old-size 1))
			     (substring name (match-end 1)))
			    device t))))