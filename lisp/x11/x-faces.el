;;; x-faces.el --- X-specific face frobnication, aka black magic.

;;; Copyright (C) 1992, 1993, 1994 Free Software Foundation, Inc.
;;; Copyright (C) 1995, 1996 Ben Wing.

;; Author: Jamie Zawinski <jwz@lucid.com>
;; Modified by:  Chuck Thompson <cthomp@cs.uiuc.edu>
;; Modified by:  Ben Wing <wing@spg.amdahl.com>

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

;; This file does the magic to parse X font names, and make sure that the
;; default and modeline attributes of new frames are specified enough.
;;
;;  The resource-manager syntax for faces is
;;
;;	 Emacs.bold.attributeFont:		font-name
;;	 Emacs.bold.attributeForeground:	fg
;;	 Emacs.bold.attributeBackground:	bg
;;	 Emacs.bold.attributeBackgroundPixmap:	file
;;	 Emacs.bold.attributeUnderline:		true/false
;;	 Emacs.bold.attributeStrikethru:	true/false
;;
;;  You can specify the properties of a face on a per-frame basis.  For 
;;  example, to have the "isearch" face use a red foreground on frames
;;  named "emacs" (the default) but use a blue foreground on frames that
;;  you create named "debugger", you could do
;;
;;	 Emacs*emacs.isearch.attributeForeground:	red
;;	 Emacs*debugger.isearch.attributeForeground:	blue
;;
;;  Generally things that make faces won't set any of the face attributes if
;;  you have already given them values via the resource database.  You can
;;  also change this stuff from your .emacs file, by using the functions
;;  set-face-foreground, set-face-font, etc.  See the code in this file, and
;;  in faces.el.

(defconst x-font-regexp nil)
(defconst x-font-regexp-head nil)
(defconst x-font-regexp-head-2 nil)
(defconst x-font-regexp-weight nil)
(defconst x-font-regexp-slant nil)
(defconst x-font-regexp-pixel nil)
(defconst x-font-regexp-point nil)
(defconst x-font-regexp-foundry-and-family nil)
(defconst x-font-regexp-registry-and-encoding nil)
(defconst x-font-regexp-spacing nil)

;;; Regexps matching font names in "Host Portable Character Representation."
;;;
(let ((- 		"[-?]")
      (foundry		"[^-]*")
      (family 		"[^-]*")
      (weight		"\\(bold\\|demibold\\|medium\\|black\\)")	; 1
;     (weight\?		"\\(\\*\\|bold\\|demibold\\|medium\\|\\)")	; 1
      (weight\?		"\\([^-]*\\)")					; 1
      (slant		"\\([ior]\\)")					; 2
;     (slant\?		"\\([ior?*]?\\)")				; 2
      (slant\?		"\\([^-]?\\)")					; 2
;     (swidth		"\\(\\*\\|normal\\|semicondensed\\|\\)")	; 3
      (swidth		"\\([^-]*\\)")					; 3
;     (adstyle		"\\(\\*\\|sans\\|\\)")				; 4
      (adstyle		"\\([^-]*\\)")					; 4
      (pixelsize	"\\(\\*\\|[0-9]+\\)")				; 5
      (pointsize	"\\(\\*\\|0\\|[0-9][0-9]+\\)")			; 6
;      (resx		"\\(\\*\\|[0-9][0-9]+\\)")			; 7
;      (resy		"\\(\\*\\|[0-9][0-9]+\\)")			; 8
      (resx		"\\([*0]\\|[0-9][0-9]+\\)")			; 7
      (resy		"\\([*0]\\|[0-9][0-9]+\\)")			; 8
      (spacing		"[cmp?*]")
      (avgwidth		"\\(\\*\\|[0-9]+\\)")				; 9
      (registry		"[^-]*") ; some fonts have omitted registries
;      (encoding	".+")		; note that encoding may contain "-"...
      (encoding	"[^-]+")		; false!
      )
  (setq x-font-regexp
	(purecopy 
	 (concat "\\`\\*?[-?*]"
		 foundry - family - weight\? - slant\? - swidth - adstyle -
		 pixelsize - pointsize - resx - resy - spacing - avgwidth -
		 registry - encoding "\\'"
		 )))
  (setq x-font-regexp-head
	(purecopy
          (concat "\\`[-?*]" foundry - family - weight\? - slant\?
		  "\\([-*?]\\|\\'\\)")))
  (setq x-font-regexp-head-2
	(purecopy
          (concat "\\`[-?*]" foundry - family - weight\? - slant\?
		  - swidth - adstyle - pixelsize - pointsize
		  "\\([-*?]\\|\\'\\)")))
  (setq x-font-regexp-slant (purecopy (concat - slant -)))
  (setq x-font-regexp-weight (purecopy (concat - weight -)))
  ;; if we can't match any of the more specific regexps (unfortunate) then
  ;; look for digits; assume 2+ digits is 10ths of points, and 1-2 digits
  ;; is pixels.  Bogus as hell.
  (setq x-font-regexp-pixel (purecopy "[-?*]\\([0-9][0-9]?\\)[-?*]"))
  (setq x-font-regexp-point (purecopy "[-?*]\\([0-9][0-9]+\\)[-?*]"))
  ;; the following two are used by x-font-menu.el.
  (setq x-font-regexp-foundry-and-family
	(purecopy (concat "\\`[-?*]" foundry - "\\(" family "\\)" -)))
  (setq x-font-regexp-registry-and-encoding
	(purecopy (concat - "\\(" registry "\\)" - "\\(" encoding "\\)\\'")))
  (setq x-font-regexp-spacing
	(purecopy (concat - "\\(" spacing "\\)" - avgwidth
			  - registry - encoding "\\'")))
  )

;; A "loser font" is something like "8x13" -> "8x13bold".
;; These are supported only through extreme generosity.
(defconst x-loser-font-regexp (purecopy "\\`[0-9]+x[0-9]+\\'"))

(defun x-frob-font-weight (font which)
  (if (font-instance-p font) (setq font (font-instance-name font)))
  (cond ((null font) nil)
	((or (string-match x-font-regexp font)
	     (string-match x-font-regexp-head font)
	     (string-match x-font-regexp-weight font))
	 (concat (substring font 0 (match-beginning 1)) which
		 (substring font (match-end 1))))
	((string-match x-loser-font-regexp font)
	 (concat font which))
	(t nil)))

(defun x-frob-font-slant (font which)
  (if (font-instance-p font) (setq font (font-instance-name font)))
  (cond ((null font) nil)
	((or (string-match x-font-regexp font)
	     (string-match x-font-regexp-head font))
	 (concat (substring font 0 (match-beginning 2)) which
		 (substring font (match-end 2))))
	((string-match x-font-regexp-slant font)
	 (concat (substring font 0 (match-beginning 1)) which
		 (substring font (match-end 1))))
	((string-match x-loser-font-regexp font)
	 (concat font which))
	(t nil)))

(defun try-font-name (name &optional device)
  ;; yes, name really should be here twice.
  (and name (make-font-instance name device t) name))

(defun x-make-font-bold (font &optional device)
  "Given an X font specification, this attempts to make a `bold' font.
If it fails, it returns nil."
  ;; Certain Type1 fonts know "bold" as "black"...
  (or (try-font-name (x-frob-font-weight font "bold") device)
      (try-font-name (x-frob-font-weight font "black") device)
      (try-font-name (x-frob-font-weight font "demibold") device)))

(defun x-make-font-unbold (font &optional device)
  "Given an X font specification, this attempts to make a non-bold font.
If it fails, it returns nil."
  (try-font-name (x-frob-font-weight font "medium") device))

(defvar *try-oblique-before-italic-fonts* nil
  "*If NIL, italic fonts are searched before oblique fonts.  If
non-NIL, oblique fonts are tried before italic fonts.  This is mostly
applicable to adobe-courier fonts")

(defun x-make-font-italic (font &optional device)
  "Given an X font specification, this attempts to make an `italic' font.
If it fails, it returns nil."
  (if *try-oblique-before-italic-fonts*
      (or (try-font-name (x-frob-font-slant font "o") device)
	  (try-font-name (x-frob-font-slant font "i") device))
    (or (try-font-name (x-frob-font-slant font "i") device)
	(try-font-name (x-frob-font-slant font "o") device))))

(defun x-make-font-unitalic (font &optional device)
  "Given an X font specification, this attempts to make a non-italic font.
If it fails, it returns nil."
  (try-font-name (x-frob-font-slant font "r") device))

(defun x-make-font-bold-italic (font &optional device)
  "Given an X font specification, this attempts to make a `bold-italic' font.
If it fails, it returns nil."
  ;; This is haired up to avoid loading the "intermediate" fonts.
  (or (try-font-name
       (x-frob-font-slant (x-frob-font-weight font "bold") "i") device)
      (try-font-name
       (x-frob-font-slant (x-frob-font-weight font "bold") "o") device)
      (try-font-name
       (x-frob-font-slant (x-frob-font-weight font "black") "i") device)
      (try-font-name
       (x-frob-font-slant (x-frob-font-weight font "black") "o") device)
      (try-font-name
       (x-frob-font-slant (x-frob-font-weight font "demibold") "i") device)
      (try-font-name
       (x-frob-font-slant (x-frob-font-weight font "demibold") "o") device)))

(defun x-font-size (font)
  "Return the nominal size of the given font.
This is done by parsing its name, so it's likely to lose.
X fonts can be specified (by the user) in either pixels or 10ths of points,
 and this returns the first one it finds, so you have to decide which units
 the returned value is measured in yourself..."
  (if (font-instance-p font) (setq font (font-instance-name font)))
  (cond ((or (string-match x-font-regexp font)
	     (string-match x-font-regexp-head-2 font))
	 (string-to-int (substring font (match-beginning 6) (match-end 6))))
	((or (string-match x-font-regexp-pixel font)
	     (string-match x-font-regexp-point font))
	 (string-to-int (substring font (match-beginning 1) (match-end 1))))
	(t nil)))

;; Given a font name, this function returns a list describing all fonts
;; of all sizes that otherwise match the given font spec.  Each element
;; in the list is a list of three items: the pixel size of the font,
;; the point size (in 1/10ths of a point) of the font, and the fully-
;; qualified font name.  The first two values may be zero; this
;; refers to a scalable font.

(defun x-available-font-sizes (font device)
  (if (font-instance-p font) (setq font (font-instance-name font)))
  (cond ((string-match x-font-regexp font)
	 ;; turn pixelsize, pointsize, and avgwidth into wildcards
	 (setq font
	       (concat (substring font 0 (match-beginning 5)) "*"
		       (substring font (match-end 5) (match-beginning 6)) "*"
		       (substring font (match-end 6) (match-beginning 9)) "*"
		       (substring font (match-end 9) (match-end 0)))))
	((string-match x-font-regexp-head-2 font)
	 ;; turn pixelsize and pointsize into wildcards
	 (setq font
	       (concat (substring font 0 (match-beginning 5)) "*"
		       (substring font (match-end 5) (match-beginning 6)) "*"
		       (substring font (match-end 6) (match-end 0)))))
	((string-match  "[-?*]\\([0-9]+\\)[-?*]" font)
	 ;; Turn the first integer we match into a wildcard.
	 ;; This is pretty dubious...
	 (setq font
	       (concat (substring font 0 (match-beginning 1)) "*"
		       (substring font (match-end 1) (match-end 0))))))
  (sort
   (delq nil
	 (mapcar (function
		  (lambda (name)
		    (and (string-match x-font-regexp name)
			 (list
			  (string-to-int (substring name (match-beginning 5)
						    (match-end 5)))
			  (string-to-int (substring name (match-beginning 6)
						    (match-end 6)))
			  name))))
		 (list-fonts font device)))
   (function (lambda (x y) (if (= (nth 1 x) (nth 1 y))
			       (< (nth 0 x) (nth 0 y))
			       (< (nth 1 x) (nth 1 y)))))))

;; Given a font name, this attempts to construct a valid font name for
;; DEVICE whose size is the next smaller (if UP-P is nil) or larger
;; (if UP-P is t) size and whose other characteristics are the same
;; as the given font.

(defun x-frob-font-size (font up-p device)
  (if (stringp font) (setq font (make-font-instance font device)))
  (if (font-instance-p font) (setq font (font-instance-truename font)))
  (let ((available (and font
			(x-available-font-sizes font device))))
    (cond
     ((null available) nil)
     ((or (= 0 (nth 0 (car available)))
	  (= 0 (nth 1 (car available))))
      ;; R5 scalable fonts: change size by 1 point.
      ;; If they're scalable the first font will have pixel or point = 0.
      ;; Sometimes one is 0 and the other isn't (if it's a bitmap font that
      ;; can be scaled), sometimes both are (if it's a true outline font).
      (let ((name (nth 2 (car available)))
	    old-size)
	(or (string-match x-font-regexp font) (error "can't parse %S" font))
	(setq old-size (string-to-int
			(substring font (match-beginning 6) (match-end 6))))
	(or (> old-size 0) (error "font truename has 0 pointsize?"))
	(or (string-match x-font-regexp name) (error "can't parse %S" name))
	;; turn pixelsize into a wildcard, and make pointsize be +/- 10,
	;; which is +/- 1 point.  All other fields stay the same as they
	;; were in the "template" font returned by x-available-font-sizes.
	;;
	;; #### But this might return the same font: for example, if the
	;;      truename of "-*-courier-medium-r-normal--*-230-75-75-m-0-*"
	;;      is "...-240-..." (instead of 230) then this loses, because
	;;      the 230 that was passed in as an arg got turned into 240
	;;      by the call to font-instance-truename; then we decrement that
	;;	by 10 and return the result which is the same.  I think the
	;;	way to fix this is to make this be a loop that keeps trying
	;;      progressively larger pointsize deltas until it finds one
	;;      whose truename differs.  Have to be careful to avoid infinite
	;;      loops at the upper end...
	;;
	(concat (substring name 0 (match-beginning 5)) "*"
		(substring name (match-end 5) (match-beginning 6))
		(int-to-string (+ old-size (if up-p 10 -10)))
		(substring name (match-end 6) (match-end 0)))))
     (t
      ;; non-scalable fonts: take the next available size.
      (let ((rest available)
	    (last nil)
	    result)
	(setq font (downcase font))
	(while rest
	  (cond ((and (not up-p) (equal font (downcase (nth 2 (car rest)))))
		 (setq result last
		       rest nil))
		((and up-p (equal font (and last (downcase (nth 2 last)))))
		 (setq result (car rest)
		       rest nil)))
	  (setq last (car rest))
	  (setq rest (cdr rest)))
	(nth 2 result))))))

(defun x-find-smaller-font (font &optional device)
  "Loads a new, slightly smaller version of the given font (or font name).
Returns the font if it succeeds, nil otherwise.
If scalable fonts are available, this returns a font which is 1 point smaller.
Otherwise, it returns the next smaller version of this font that is defined."
  (x-frob-font-size font nil device))

(defun x-find-larger-font (font &optional device)
  "Loads a new, slightly larger version of the given font (or font name).
Returns the font if it succeeds, nil otherwise.
If scalable fonts are available, this returns a font which is 1 point larger.
Otherwise, it returns the next larger version of this font that is defined."
  (x-frob-font-size font t device))

(defalias 'x-make-face-bold 'make-face-bold)
(defalias 'x-make-face-italic 'make-face-italic)
(defalias 'x-make-face-bold-italic 'make-face-bold-italic)
(defalias 'x-make-face-unbold 'make-face-unbold)
(defalias 'x-make-face-unitalic 'make-face-unitalic)

(make-obsolete 'x-make-face-bold 'make-face-bold)
(make-obsolete 'x-make-face-italic 'make-face-italic)
(make-obsolete 'x-make-face-bold-italic 'make-face-bold-italic)
(make-obsolete 'x-make-face-unbold 'make-face-unbold)
(make-obsolete 'x-make-face-unitalic 'make-face-unitalic)


;; Define some logical color names to be used when reading the pixmap files.
(if (featurep 'xpm)
    (setq xpm-color-symbols
	  (list
	   (purecopy '("foreground" (face-foreground 'default)))
	   (purecopy '("background" (face-background 'default)))
	   (purecopy '("backgroundToolBarColor"
		       (x-get-resource "backgroundToolBarColor"
				       "BackgroundToolBarColor" 'string)))
	   )))

;;; internal routines

;;; x-init-face-from-resources is responsible for initializing a
;;; newly-created face from the resource database.
;;;
;;; When a new frame is created, it is called from `x-init-frame-faces'
;;; called from `init-frame-faces' called from init_frame_faces()
;;; from Fmake_frame().  In this case it is called once for each existing
;;; face, with the newly-created frame as the argument.  It then initializes
;;; the newly-created faces on that frame.
;;;
;;; It's also called from `init-device-faces' and
;;; `init-global-faces'.
;;;
;;; This had better not signal an error.  The frame is in an intermediate
;;; state where signalling an error or entering the debugger would likely
;;; result in a crash.

(defun x-init-face-from-resources (face &optional locale set-anyway)

  ;;
  ;; These are things like "attributeForeground" instead of simply
  ;; "foreground" because people tend to do things like "*foreground",
  ;; which would cause all faces to be fully qualified, making faces
  ;; inherit attributes in a non-useful way.  So we've made them slightly
  ;; less obvious to specify in order to make them work correctly in
  ;; more random environments.
  ;;
  ;; I think these should be called "face.faceForeground" instead of
  ;; "face.attributeForeground", but they're the way they are for
  ;; hysterical reasons. (jwz)

  (let* ((append (if set-anyway nil 'append))
	 (face-sym (face-name face))
	 (name (symbol-name face-sym))
	 (fn (x-get-resource-and-maybe-bogosity-check
	      (concat name ".attributeFont")
	      "Face.AttributeFont"
	      'string locale))
	 (fg (x-get-resource-and-maybe-bogosity-check
	      (concat name ".attributeForeground")
	      "Face.AttributeForeground"
	      'string locale))
	 (bg (x-get-resource-and-maybe-bogosity-check
	      (concat name ".attributeBackground")
	      "Face.AttributeBackground"
	      'string locale))
	 (bgp (x-get-resource-and-maybe-bogosity-check
	       (concat name ".attributeBackgroundPixmap")
	       "Face.AttributeBackgroundPixmap"
	       'string locale))
	 (ulp (x-get-resource-and-maybe-bogosity-check
	       (concat name ".attributeUnderline")
	       "Face.AttributeUnderline"
	       'boolean locale))
	 (stp (x-get-resource-and-maybe-bogosity-check
	       (concat name ".attributeStrikethru")
	       "Face.AttributeStrikethru"
	       'boolean locale))
	 ;; we still resource for these TTY-only resources so that
	 ;; you can specify resources for TTY frames/devices.  This is
	 ;; useful when you start up your XEmacs on an X display and later
	 ;; open some TTY frames.
	 (hp (x-get-resource-and-maybe-bogosity-check
	      (concat name ".attributeHighlight")
	      "Face.AttributeHighlight"
	      'boolean locale))
	 (dp (x-get-resource-and-maybe-bogosity-check
	      (concat name ".attributeDim")
	      "Face.AttributeDim"
	      'boolean locale))
	 (bp (x-get-resource-and-maybe-bogosity-check
	      (concat name ".attributeBlinking")
	      "Face.AttributeBlinking"
	      'boolean locale))
	 (rp (x-get-resource-and-maybe-bogosity-check
	      (concat name ".attributeReverse")
	      "Face.AttributeReverse"
	      'boolean locale))
	 )

    ;;
    ;; If this is the default face, then any unspecified properties should
    ;; be defaulted from the global properties.  Can't do this for
    ;; frames or devices because then, common resource specs like
    ;; "*Foreground: black" will have unwanted effects.
    ;;
    (if (and (eq (face-name face) 'default)
	     (or (null locale) (eq locale 'global)))
	(progn
	  (or fn (setq fn (x-get-resource
			   "font" "Font" 'string locale)))
	  (or fg (setq fg (x-get-resource
			   "foreground" "Foreground" 'string locale)))
	  (or bg (setq bg (x-get-resource
			   "background" "Background" 'string locale)))))
    ;;
    ;; "*cursorColor: foo" is equivalent to setting the background of the
    ;; text-cursor face.
    ;;
    (if (and (eq (face-name face) 'text-cursor)
	     (or (null locale) (eq locale 'global)))
	(setq bg (or (x-get-resource
		      "cursorColor" "CursorColor" 'string locale) bg)))
    ;; #### should issue warnings?  I think this should be
    ;; done when the instancing actually happens, but I'm not
    ;; sure how it should actually be dealt with.
    (if fn
	(set-face-font face fn locale nil append))
    ;; Kludge-o-rooni.  Set the foreground and background resources for
    ;; X devices only -- otherwise things tend to get all messed up
    ;; if you start up an X frame and then later create a TTY frame.
    (if fg
	(set-face-foreground face fg locale 'x append))
    (if bg
	(set-face-background face bg locale 'x append))
    (if bgp
	(set-face-background-pixmap face bgp locale nil append))
    (if ulp
	(set-face-underline-p face ulp locale nil append))
    (if stp
	(set-face-strikethru-p face stp locale nil append))
    (if hp
	(set-face-highlight-p face hp locale nil append))
    (if dp
	(set-face-dim-p face dp locale nil append))
    (if bp
	(set-face-blinking-p face bp locale nil append))
    (if rp
	(set-face-reverse-p face rp locale nil append))
    ))

;; GNU Emacs compatibility. (move to obsolete.el?)
(defalias 'make-face-x-resource-internal 'x-init-face-from-resources)

;;; x-init-global-faces is responsible for ensuring that the
;;; default face has some reasonable fallbacks if nothing else is
;;; specified.
;;;
(defun x-init-global-faces ()
  (or (face-font 'default 'global)
      (set-face-font 'default
		     "-*-courier-medium-r-*-*-*-120-*-*-*-*-iso8859-*")
      'global)
  (or (face-foreground 'default 'global)
      (set-face-foreground 'default "black" 'global 'x))
  (or (face-background 'default 'global)
      (set-face-background 'default "gray80" 'global 'x)))

;;; x-init-device-faces is responsible for initializing default
;;; values for faces on a newly created device.
;;;
(defun x-init-device-faces (device)
  ;;
  ;; If the "default" face didn't have a font specified, try to pick one.
  ;;
  (or
   (face-font-instance 'default device)
   ;;
   ;; No font specified in the resource database; try to cope.
   ;;
   ;; At first I wanted to do this by just putting a font-spec in the
   ;; fallback resources passed to XtAppInitialize(), but that fails
   ;; if there is an Emacs app-defaults file which doesn't specify a
   ;; font: apparently the fallback resources are not consulted when
   ;; there is an app-defaults file, which seems pretty bogus to me.
   ;;
   ;; We should also probably try "*xtDefaultFont", but I think that it
   ;; might be legal to specify that as "xtDefaultFont:", that is, at
   ;; top level, instead of "*xtDefaultFont:", that is, applicable to
   ;; every application.  `x-get-resource' can't handle that right now.
   ;; Anyway, xtDefaultFont is probably variable-width.
   ;;
   ;; Some who have LucidaTypewriter think it's a better font than Courier,
   ;; but it has the bug that there are no italic and bold italic versions.
   ;; We could hair this code up to try and mix-and-match fonts to get a
   ;; full complement, but really, why bother.  It's just a default.
   ;;
   (let (new-x-font)
     (setq new-x-font (or
      ;;
      ;; We default to looking for iso8859 fonts.  Using a wildcard for the
      ;; encoding would be bad, because that can cause English speakers to get
      ;; Kanji fonts by default.  It is safe to assume that people using a
      ;; language other than English have both set $LANG, and have specified
      ;; their `font' and `fontList' resources.  In any event, it's better to
      ;; err on the side of the English speaker in this case because they are
      ;; much less likely to have encountered this problem, and are thus less
      ;; likely to know what to do about it.

      ;; Try for Courier.  Almost everyone has that.  (Does anyone not?)
      (make-font-instance
       "-*-courier-medium-r-*-*-*-120-*-*-*-*-iso8859-*" device t)
      (make-font-instance
       "-*-courier-*-r-*-*-*-120-*-*-*-*-iso8859-*" device t)
      ;; Next try for any "medium" charcell or monospaced iso8859 font.
      (make-font-instance "-*-*-medium-r-*-*-*-120-*-*-m-*-iso8859-*" device t)
      (make-font-instance "-*-*-medium-r-*-*-*-120-*-*-c-*-iso8859-*" device t)
      ;; Next try for any charcell or monospaced iso8859 font.
      (make-font-instance "-*-*-*-r-*-*-*-120-*-*-m-*-iso8859-*" device t)
      (make-font-instance "-*-*-*-r-*-*-*-120-*-*-c-*-iso8859-*" device t)
      ;; Ok, let's at least try to stay in 8859...
      (make-font-instance "-*-*-*-r-*-*-*-120-*-*-*-*-iso8859-*" device t)
      ;; Boy, we sure are losing now.  Try the above, but in any encoding.
      (make-font-instance "-*-*-medium-r-*-*-*-120-*-*-m-*-*-*" device t)
      (make-font-instance "-*-*-medium-r-*-*-*-120-*-*-c-*-*-*" device t)
      (make-font-instance "-*-*-*-r-*-*-*-120-*-*-m-*-*-*" device t)
      (make-font-instance "-*-*-*-r-*-*-*-120-*-*-c-*-*-*" device t)
      (make-font-instance "-*-*-*-r-*-*-*-120-*-*-*-*-*-*" device t)
      ;; Hello?  Please?
      (make-font-instance "-*-*-*-*-*-*-*-120-*-*-*-*-*-*" device t)
      (make-font-instance "*" device t)
      ;; if we get to here we're screwed, and faces.c will fatal()...
      ))
     (if (not (face-font 'default 'global))
	 (set-face-font 'default new-x-font)
       (set-face-font 'default new-x-font device))))
  ;;
  ;; If the "default" face didn't have both colors specified, then pick
  ;; some, taking into account whether one of the colors was specified.  
  ;;
  (let ((fg (face-foreground-instance 'default device))
	(bg (face-background-instance 'default device)))
    (if (not (and fg bg))
	(if (or (and fg (equal (downcase (color-instance-name fg)) "white"))
		(and bg (equal (downcase (color-instance-name bg)) "black")))
	    (progn
	      (or fg (set-face-foreground 'default "white" device))
	      (or bg (set-face-background 'default "black" device)))
	  (or fg (set-face-foreground 'default "white" device))
	  (or bg (set-face-background 'default "black" device)))))

  ;; Don't look at reverseVideo now or initialize the modeline.  This
  ;; is done on a per-frame basis at the appropriate time.

  ;;
  ;; Now let's try to pick some reasonable defaults for a few other faces.
  ;; This kind of stuff should normally go on the create-frame-hook, but
  ;; this way we won't be in danger of the user screwing things up by not
  ;; adding hooks in a safe way.
  ;;
  (x-init-pointer-shape device)  ; from x-mouse.el
    )

;;; This is called from `init-frame-faces', which is called from 
;;; init_frame_faces() which is called from Fmake_frame(), to perform
;;; any device-specific initialization.
;;;
(defun x-init-frame-faces (frame)
  ;;
  ;; The faces already got initialized (by init-frame-faces) from
  ;; the resource database or global, non-frame faces.  The default,
  ;; bold, bold-italic, and italic faces (plus various other random faces)
  ;; got set up then.  But modeline didn't so that reverseVideo can be
  ;; frame-specific.
  ;;

  ;;
  ;; If reverseVideo was specified, swap the foreground and background
  ;; of the default and modeline faces.
  ;;
  (cond ((car (x-get-resource "reverseVideo" "ReverseVideo" 'boolean frame))
	 ;; First make sure the modeline has fg and bg, inherited from the
	 ;; current default face - for the case where only one is specified,
	 ;; so that invert-face doesn't do something weird.
	 (or (face-foreground 'modeline frame)
	     (set-face-foreground 'modeline
				  (face-foreground-instance 'default frame)
				  frame))
	 (or (face-background 'modeline frame)
	     (set-face-background 'modeline
				  (face-background-instance 'default frame)
				  frame))
	 ;; Now invert both of them.  If they end up looking the same,
	 ;; make-frame-initial-faces will invert the modeline again later.
	 (invert-face 'default frame)
	 (invert-face 'modeline frame)
	 )))