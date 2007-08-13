;;; float-sup.el --- detect absence of floating-point support in XEmacs runtime

;; Copyright (C) 1985, 1986, 1987 Free Software Foundation, Inc.

;; Maintainer: FSF
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
;; Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

;;; Synched up with: FSF 19.30.

;; Provide a meaningful error message if we are running on
;; bare (non-float) emacs.
;; Can't test for 'floatp since that may be defined by float-imitation
;; packages like float.el in this very directory.

(or (featurep 'lisp-float-type)
    (error "Floating point was disabled at compile time"))

;; define pi and e via math-lib calls. (much less prone to killer typos.)
(defconst pi (purecopy (* 4 (atan 1))) "The value of Pi (3.1415926...)")
(defconst e (purecopy (exp 1)) "The value of e (2.7182818...)")

;; Careful when editing this file ... typos here will be hard to spot.
;; (defconst pi       3.14159265358979323846264338327
;;  "The value of Pi (3.14159265358979323846264338327...)")

(defconst degrees-to-radians (purecopy (/ pi 180.0))
  "Degrees to radian conversion constant")
(defconst radians-to-degrees (purecopy (/ 180.0 pi))
  "Radian to degree conversion constant")

;; these expand to a single multiply by a float
;; when byte compiled

(defmacro degrees-to-radians (x)
  "Convert ARG from degrees to radians."
  (list '* (/ pi 180.0) x))
(defmacro radians-to-degrees (x)
  "Convert ARG from radians to degrees."
  (list '* (/ 180.0 pi) x))

;;; float-sup.el ends here
