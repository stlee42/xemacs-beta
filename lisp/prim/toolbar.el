;; Toolbar support.
;; Copyright (C) 1995 Board of Trustees, University of Illinois

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
;; Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

;;; Synched up with: Not in FSF.

(defvar toolbar-help-enabled t
  "If non-nil help is echoed for toolbar buttons.")

(defvar toolbar-icon-directory nil
  "Location of standard toolbar icon bitmaps.")

(defun toolbar-make-button-list (up &optional down disabled cap-up cap-down cap-disabled)
  "Calls make-glyph on each arg and returns a list of the results."
  (if (featurep 'x)
      (let ((up-glyph (make-glyph up))
	    (down-glyph (and down (make-glyph down)))
	    (disabled-glyph (and disabled (make-glyph disabled)))
	    (cap-up-glyph (and cap-up (make-glyph cap-up)))
	    (cap-down-glyph (and cap-down (make-glyph cap-down)))
	    (cap-disabled-glyph (and cap-disabled (make-glyph cap-disabled))))
	(if cap-disabled
	    (list up-glyph down-glyph disabled-glyph
		  cap-up-glyph cap-down-glyph cap-disabled-glyph)
	  (if cap-down
	    (list up-glyph down-glyph disabled-glyph
		  cap-up-glyph cap-down-glyph)
	    (if cap-up
		(list up-glyph down-glyph disabled-glyph cap-up-glyph)
	      (if disabled-glyph
		  (list up-glyph down-glyph disabled-glyph)
		(if down-glyph
		    (list up-glyph down-glyph)
		  (list up-glyph)))))))
    nil))

(defun init-toolbar-location ()
  (if (not toolbar-icon-directory)
      (setq toolbar-icon-directory
	    (file-name-as-directory
	     (expand-file-name "toolbar" data-directory)))))

(defun init-toolbar-from-resources (locale)
  (if (and (featurep 'x)
	   (or (eq locale 'global)
	       (eq 'x (device-or-frame-type locale)))
	   (x-init-toolbar-from-resources locale))))


;; #### Is this actually needed or will the code in
;; default-mouse-motion-handler suffice?
(define-key global-map 'button1up 'release-toolbar-button)

(defvar toolbar-map (let ((m (make-sparse-keymap)))
		      (set-keymap-name m 'toolbar-map)
		      m)
  "Keymap consulted for mouse-clicks over a toolbar.")

(define-key toolbar-map 'button1 'press-toolbar-button)
(define-key toolbar-map 'button1up 'release-and-activate-toolbar-button)
(defvar last-pressed-toolbar-button nil)
(defvar toolbar-active nil)

;;
;; It really sucks that we also have to tie onto
;; default-mouse-motion-handler to make sliding buttons work right.
;;
(defun press-toolbar-button (event)
  "Press a toolbar button.  This only changes its appearance."
  (interactive "_e")
  (setq this-command last-command)
  (let ((button (event-toolbar-button event)))
    ;; We silently ignore non-buttons.  This most likely means we are
    ;; over a blank part of the toolbar.
    (setq toolbar-active t)
    (if (toolbar-button-p button)
	(progn
	  (set-toolbar-button-down-flag button t)
	  (setq last-pressed-toolbar-button button)))))

(defun release-and-activate-toolbar-button (event)
  "Release a toolbar button and activate its callback."
  (interactive "_e")
  (or (button-release-event-p event)
      (error "%s must be invoked by a mouse-release" this-command))
  (release-toolbar-button event)
  (let ((button (event-toolbar-button event)))
    (if (and (toolbar-button-p button)
	     (toolbar-button-enabled-p button)
	     (toolbar-button-callback button))
	(let ((callback (toolbar-button-callback button)))
	  (setq this-command callback)
	  (if (symbolp callback)
	      (call-interactively callback)
	    (eval callback))))))

;; If current is not t, then only release the toolbar button stored in
;; last-pressed-toolbar-button
(defun release-toolbar-button-internal (event current)
  (let ((button (event-toolbar-button event)))
    (setq zmacs-region-stays t)
    (if (and last-pressed-toolbar-button
	     (not (eq last-pressed-toolbar-button button))
	     (toolbar-button-p last-pressed-toolbar-button))
	(progn
	  (set-toolbar-button-down-flag last-pressed-toolbar-button nil)
	  (setq last-pressed-toolbar-button nil)))
    (if (and current (toolbar-button-p button))
	(set-toolbar-button-down-flag button nil))))

(defun release-toolbar-button (event)
  "Release all pressed toolbar buttons."
  (interactive "_e")
  (or (button-release-event-p event)
      (error "%s must be invoked by a mouse-release" this-command))
  (release-toolbar-button-internal event t)
  ;; Don't set this-command if we're being called
  ;; from release-and-activate-toolbar-button.
  (if (interactive-p)
      (setq this-command last-command))
  (setq toolbar-active nil))

(defun release-previous-toolbar-button (event)
  (setq zmacs-region-stays t)
  (release-toolbar-button-internal event nil))

