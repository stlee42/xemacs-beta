;; gm-lingo.el 
;; Translate to ISO from/to net/TeX conventions  ...
;; Copyright 1993 Michael Gschwind (mike@vlsivie.tuwien.ac.at)

;; Keywords: tex, iso, latin, 8bit

;;  From: mike@vlsivie.tuwien.ac.at (Michael Gschwind)
;;  Newsgroups: gnu.emacs.sources
;;  Subject: tex sequence to ISO latin conversions (and back)
;;  Date: 13 Oct 1993 12:12:35 GMT
;;  
;;  The enclosed elisp file installs hooks which automatically translate
;;  TeX sequences to ISO latin1 upon loading of a TeX file in emacs. This
;;  allows editing of TeX documents without having to type escape
;;  sequences.  Upon saving a file, ISO latin1 characters are converted
;;  back to TeX sequences. (If you have a tex style which can handle 8 bit
;;  characters, this part is not necessary, but the loading half is still
;;  neat to convert old files to 8 bit - also, 8 bit are less portable
;;  than 7...) 
;;  
;;  It also contains a function 'german which translates net conventions
;;  for typing german characters into the real thing - if you install this
;;  in news-reader/mail/whatever hooks, you'll never again be bothered
;;  with having to read characters like "s or \3 or "a etc.
;;  
;;  mike
;;  

;; This file works with GNU Emacs19 or higher, but is not part of GNU Emacs.

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program; if not, write to the Free Software
;; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

; it's the author's first lisp program in a long time, so don't judge 
; him by it :( 

; to do: translate buffer when displaying from GNUS, 
; use function 'german which does the Right Thing
; upon saving, the buffer reverts to TeX format...

; Description:
; calling 'german will turn the net convention f. umlauts ("a etc.) 
; into ISO latin umlaute for easy reading.
; hooks change TeX files to latin1 for editing and back to TeX sequences 
; for calling TeX. An alternative is a TeX style that handles 
; 8 bit ISO files (available on ftp.vlsivie.tuwien.ac.at in /pub/8bit) 
; - but these files are difficult to transmit ... so while the net is  
; still @ 7 bit this may be useful
;
; fixed bug that causes uppercase umlauts to become lower case by 
; conversion -- msz 960429

(defvar spanish-trans-tab '(
			    ("~n" "�")
			    ("\([a-zA-Z]\)#" "\\1�")
			    ("~N" "�")
			    ( "\\([-a-zA-Z\"`]\\)\"u" "\\1�")
			    ( "\\([-a-zA-Z\"`]\\)\"U" "\\1�")
			    ( "\\([-a-zA-Z]\\)'o" "\\1�")
			    ( "\\([-a-zA-Z]\\)'O" "\\�")
			    ( "\\([-a-zA-Z]\\)'e" "\\1�")
			    ( "\\([-a-zA-Z]\\)'E" "\\1�")
			    ( "\\([-a-zA-Z]\\)'a" "\\1�")
			    ( "\\([-a-zA-Z]\\)'A" "\\1A")
			    ( "\\([-a-zA-Z]\\)'i" "\\1�")
			    ( "\\([-a-zA-Z]\\)'I" "\\1�")
			   )
  "Spanish")

(defun translate-conventions (trans-tab)
  (interactive)
  (save-excursion
    (widen)
    (goto-char (point-min))
    (setq save-case-fold-search case-fold-search) ;; msz 960429
    (setq case-fold-search nil)                   ;; 
    (let ((work-tab trans-tab)
	  (buffer-read-only nil))
      (while work-tab
	(save-excursion
	  (let ((trans-this (car work-tab)))
	    (while (re-search-forward (car trans-this) nil t)
	      (replace-match (car (cdr trans-this)) nil nil)))
	  (setq work-tab (cdr work-tab)))))
    (setq case-fold-serch save-case-fold-search)))

(defun spanish ()
  "Translate net conventions for Spanish to ISO"
  (interactive)
  (translate-conventions spanish-trans-tab))

(defvar aggressive-german-trans-tab '(
		   ( "\"a" "�")
		   ( "\"A" "�")
		   ( "\"o" "�")
		   ( "\"O" "�")
		   ( "\"u" "�")
		   ( "\"U" "�")
		   ( "\"s" "�")
		   ( "\\\\3" "�")
		   )
  "German - may do too much")

(defvar conservative-german-trans-tab '(
		   ( "\\([-a-zA-Z\"`]\\)\"a" "\\1�")
		   ( "\\([-a-zA-Z\"`]\\)\"A" "\\1�")
		   ( "\\([-a-zA-Z\"`]\\)\"o" "\\1�")
		   ( "\\([-a-zA-Z\"`]\\)\"O" "\\1�")
		   ( "\\([-a-zA-Z\"`]\\)\"u" "\\1�")
		   ( "\\([-a-zA-Z\"`]\\)\"U" "\\1�")
		   ( "\\([-a-zA-Z\"`]\\)\"s" "\\1�")
		   ( "\\([-a-zA-Z\"`]\\)\\\\3" "\\1�")
		   )
  "conservative German - may do too little")


(defvar german-trans-tab aggressive-german-trans-tab "used for char translation")

(defun german ()
 "Translate net conventions for German to ISO"
 (interactive)
 (translate-conventions german-trans-tab))
 
(defvar iso2tex-trans-tab '(
			    ("�" "{\\\\\"a}")
			    ("�" "{\\\\`a}")
			    ("�" "{\\\\'a}")
			    ("�" "{\\\\~a}")
			    ("�" "{\\\\^a}")
			    ("�" "{\\\\\"e}")
			    ("�" "{\\\\`e}")
			    ("�" "{\\\\'e}")
			    ("�" "{\\\\^e}")
			    ("�" "{\\\\\"\\\\i}")
			    ("�" "{\\\\`\\\\i}")
			    ("�" "{\\\\'\\\\i}")
			    ("�" "{\\\\^\\\\i}")
			    ("�" "{\\\\\"o}")
			    ("�" "{\\\\`o}")
			    ("�" "{\\\\'o}")
			    ("�" "{\\\\~o}")
			    ("�" "{\\\\^o}")
			    ("�" "{\\\\\"u}")
			    ("�" "{\\\\`u}")
			    ("�" "{\\\\'u}")
			    ("�" "{\\\\^u}")
			    ("�" "{\\\\\"A}")
			    ("�" "{\\\\`A}")
			    ("�" "{\\\\'A}")
			    ("�" "{\\\\~A}")
			    ("�" "{\\\\^A}")
			    ("�" "{\\\\\"E}")
			    ("�" "{\\\\`E}")
			    ("�" "{\\\\'E}")
			    ("�" "{\\\\^E}")
			    ("�" "{\\\\\"I}")
			    ("�" "{\\\\`I}")
			    ("�" "{\\\\'I}")
			    ("�" "{\\\\^I}")
			    ("�" "{\\\\\"O}")
			    ("�" "{\\\\`O}")
			    ("�" "{\\\\'O}")
			    ("�" "{\\\\~O}")
			    ("�" "{\\\\^O}")
			    ("�" "{\\\\\"U}")
			    ("�" "{\\\\`U}")
			    ("�" "{\\\\'U}")
			    ("�" "{\\\\^U}")
			    ("�" "{\\\\~n}")
			    ("�" "{\\\\~N}")
			    ("�" "{\\\\c c}")
			    ("�" "{\\\\c C}")
			    ("�" "{\\\\ss}")
			    ("�" "{?`}")
			    ("�" "{!`}")
			    )
  )




(defun iso2tex ()
 "Translate ISO to TeX"
 (interactive)
 (translate-conventions iso2tex-trans-tab))


(defvar tex2iso-trans-tab '(
			    ( "{\\\\\"a}" "�")
			    ( "{\\\\`a}" "�")
			    ( "{\\\\'a}" "�")
			    ( "{\\\\~a}" "�")
			    ( "{\\\\^a}" "�")
			    ( "{\\\\\"e}" "�")
			    ( "{\\\\`e}" "�")
			    ( "{\\\\'e}" "�")
			    ( "{\\\\^e}" "�")
			    ( "{\\\\\"\\\\i}" "�")
			    ( "{\\\\`\\\\i}" "�")
			    ( "{\\\\'\\\\i}" "�")
			    ( "{\\\\^\\\\i}" "�")
			    ( "{\\\\\"i}" "�")
			    ( "{\\\\`i}" "�")
			    ( "{\\\\'i}" "�")
			    ( "{\\\\^i}" "�")
			    ( "{\\\\\"o}" "�")
			    ( "{\\\\`o}" "�")
			    ( "{\\\\'o}" "�")
			    ( "{\\\\~o}" "�")
			    ( "{\\\\^o}" "�")
			    ( "{\\\\\"u}" "�")
			    ( "{\\\\`u}" "�")
			    ( "{\\\\'u}" "�")
			    ( "{\\\\^u}" "�")
			    ( "{\\\\\"A}" "�")
			    ( "{\\\\`A}" "�")
			    ( "{\\\\'A}" "�")
			    ( "{\\\\~A}" "�")
			    ( "{\\\\^A}" "�")
			    ( "{\\\\\"E}" "�")
			    ( "{\\\\`E}" "�")
			    ( "{\\\\'E}" "�")
			    ( "{\\\\^E}" "�")
			    ( "{\\\\\"I}" "�")
			    ( "{\\\\`I}" "�")
			    ( "{\\\\'I}" "�")
			    ( "{\\\\^I}" "�")
			    ( "{\\\\\"O}" "�")
			    ( "{\\\\`O}" "�")
			    ( "{\\\\'O}" "�")
			    ( "{\\\\~O}" "�")
			    ( "{\\\\^O}" "�")
			    ( "{\\\\\"U}" "�")
			    ( "{\\\\`U}" "�")
			    ( "{\\\\'U}" "�")
			    ( "{\\\\^U}" "�")
			    ( "{\\\\~n}" "�")
			    ( "{\\\\~N}" "�")
			    ( "{\\\\c c}" "�")
			    ( "{\\\\c C}" "�")
			    ( "\\\\\"{a}" "�")
			    ( "\\\\`{a}" "�")
			    ( "\\\\'{a}" "�")
			    ( "\\\\~{a}" "�")
			    ( "\\\\^{a}" "�")
			    ( "\\\\\"{e}" "�")
			    ( "\\\\`{e}" "�")
			    ( "\\\\'{e}" "�")
			    ( "\\\\^{e}" "�")
			    ( "\\\\\"{\\\\i}" "�")
			    ( "\\\\`{\\\\i}" "�")
			    ( "\\\\'{\\\\i}" "�")
			    ( "\\\\^{\\\\i}" "�")
			    ( "\\\\\"{i}" "�")
			    ( "\\\\`{i}" "�")
			    ( "\\\\'{i}" "�")
			    ( "\\\\^{i}" "�")
			    ( "\\\\\"{o}" "�")
			    ( "\\\\`{o}" "�")
			    ( "\\\\'{o}" "�")
			    ( "\\\\~{o}" "�")
			    ( "\\\\^{o}" "�")
			    ( "\\\\\"{u}" "�")
			    ( "\\\\`{u}" "�")
			    ( "\\\\'{u}" "�")
			    ( "\\\\^{u}" "�")
			    ( "\\\\\"{A}" "�")
			    ( "\\\\`{A}" "�")
			    ( "\\\\'{A}" "�")
			    ( "\\\\~{A}" "�")
			    ( "\\\\^{A}" "�")
			    ( "\\\\\"{E}" "�")
			    ( "\\\\`{E}" "�")
			    ( "\\\\'{E}" "�")
			    ( "\\\\^{E}" "�")
			    ( "\\\\\"{I}" "�")
			    ( "\\\\`{I}" "�")
			    ( "\\\\'{I}" "�")
			    ( "\\\\^{I}" "�")
			    ( "\\\\\"{O}" "�")
			    ( "\\\\`{O}" "�")
			    ( "\\\\'{O}" "�")
			    ( "\\\\~{O}" "�")
			    ( "\\\\^{O}" "�")
			    ( "\\\\\"{U}" "�")
			    ( "\\\\`{U}" "�")
			    ( "\\\\'{U}" "�")
			    ( "\\\\^{U}" "�")
			    ( "\\\\~{n}" "�")
			    ( "\\\\~{N}" "�")
			    ( "\\\\c{c}" "�")
			    ( "\\\\c{C}" "�")
			    ( "{\\\\ss}" "�")
			    ( "{?`}" "�")
			    ( "{!`}" "�")
			    )
  )

(defun tex2iso ()
 "Translate TeX to ISO"
 (interactive)
 (translate-conventions tex2iso-trans-tab))

(defvar gtex2iso-trans-tab '(
			    ( "\"a" "�")
			    ( "\"A" "�")
			    ( "\"o" "�")
			    ( "\"O" "�")
			    ( "\"u" "�")
			    ( "\"U" "�")
			    ( "\"s" "�")
			    ( "\\\\3" "�")
			    ( "{\\\\\"a}" "�")
			    ( "{\\\\`a}" "�")
			    ( "{\\\\'a}" "�")
			    ( "{\\\\~a}" "�")
			    ( "{\\\\^a}" "�")
			    ( "{\\\\\"e}" "�")
			    ( "{\\\\`e}" "�")
			    ( "{\\\\'e}" "�")
			    ( "{\\\\^e}" "�")
			    ( "{\\\\\"\\\\i}" "�")
			    ( "{\\\\`\\\\i}" "�")
			    ( "{\\\\'\\\\i}" "�")
			    ( "{\\\\^\\\\i}" "�")
			    ( "{\\\\\"i}" "�")
			    ( "{\\\\`i}" "�")
			    ( "{\\\\'i}" "�")
			    ( "{\\\\^i}" "�")
			    ( "{\\\\\"o}" "�")
			    ( "{\\\\`o}" "�")
			    ( "{\\\\'o}" "�")
			    ( "{\\\\~o}" "�")
			    ( "{\\\\^o}" "�")
			    ( "{\\\\\"u}" "�")
			    ( "{\\\\`u}" "�")
			    ( "{\\\\'u}" "�")
			    ( "{\\\\^u}" "�")
			    ( "{\\\\\"A}" "�")
			    ( "{\\\\`A}" "�")
			    ( "{\\\\'A}" "�")
			    ( "{\\\\~A}" "�")
			    ( "{\\\\^A}" "�")
			    ( "{\\\\\"E}" "�")
			    ( "{\\\\`E}" "�")
			    ( "{\\\\'E}" "�")
			    ( "{\\\\^E}" "�")
			    ( "{\\\\\"I}" "�")
			    ( "{\\\\`I}" "�")
			    ( "{\\\\'I}" "�")
			    ( "{\\\\^I}" "�")
			    ( "{\\\\\"O}" "�")
			    ( "{\\\\`O}" "�")
			    ( "{\\\\'O}" "�")
			    ( "{\\\\~O}" "�")
			    ( "{\\\\^O}" "�")
			    ( "{\\\\\"U}" "�")
			    ( "{\\\\`U}" "�")
			    ( "{\\\\'U}" "�")
			    ( "{\\\\^U}" "�")
			    ( "{\\\\~n}" "�")
			    ( "{\\\\~N}" "�")
			    ( "{\\\\c c}" "�")
			    ( "{\\\\c C}" "�")
			    ( "\\\\\"{a}" "�")
			    ( "\\\\`{a}" "�")
			    ( "\\\\'{a}" "�")
			    ( "\\\\~{a}" "�")
			    ( "\\\\^{a}" "�")
			    ( "\\\\\"{e}" "�")
			    ( "\\\\`{e}" "�")
			    ( "\\\\'{e}" "�")
			    ( "\\\\^{e}" "�")
			    ( "\\\\\"{\\\\i}" "�")
			    ( "\\\\`{\\\\i}" "�")
			    ( "\\\\'{\\\\i}" "�")
			    ( "\\\\^{\\\\i}" "�")
			    ( "\\\\\"{i}" "�")
			    ( "\\\\`{i}" "�")
			    ( "\\\\'{i}" "�")
			    ( "\\\\^{i}" "�")
			    ( "\\\\\"{o}" "�")
			    ( "\\\\`{o}" "�")
			    ( "\\\\'{o}" "�")
			    ( "\\\\~{o}" "�")
			    ( "\\\\^{o}" "�")
			    ( "\\\\\"{u}" "�")
			    ( "\\\\`{u}" "�")
			    ( "\\\\'{u}" "�")
			    ( "\\\\^{u}" "�")
			    ( "\\\\\"{A}" "�")
			    ( "\\\\`{A}" "�")
			    ( "\\\\'{A}" "�")
			    ( "\\\\~{A}" "�")
			    ( "\\\\^{A}" "�")
			    ( "\\\\\"{E}" "�")
			    ( "\\\\`{E}" "�")
			    ( "\\\\'{E}" "�")
			    ( "\\\\^{E}" "�")
			    ( "\\\\\"{I}" "�")
			    ( "\\\\`{I}" "�")
			    ( "\\\\'{I}" "�")
			    ( "\\\\^{I}" "�")
			    ( "\\\\\"{O}" "�")
			    ( "\\\\`{O}" "�")
			    ( "\\\\'{O}" "�")
			    ( "\\\\~{O}" "�")
			    ( "\\\\^{O}" "�")
			    ( "\\\\\"{U}" "�")
			    ( "\\\\`{U}" "�")
			    ( "\\\\'{U}" "�")
			    ( "\\\\^{U}" "�")
			    ( "\\\\~{n}" "�")
			    ( "\\\\~{N}" "�")
			    ( "\\\\c{c}" "�")
			    ( "\\\\c{C}" "�")
			    ( "{\\\\ss}" "�")
			    ( "{?`}" "�")
			    ( "{!`}" "�")
			    )
  )

(defvar iso2gtex-trans-tab '(
			    ("�" "\"a")
			    ("�" "{\\\\`a}")
			    ("�" "{\\\\'a}")
			    ("�" "{\\\\~a}")
			    ("�" "{\\\\^a}")
			    ("�" "{\\\\\"e}")
			    ("�" "{\\\\`e}")
			    ("�" "{\\\\'e}")
			    ("�" "{\\\\^e}")
			    ("�" "{\\\\\"\\\\i}")
			    ("�" "{\\\\`\\\\i}")
			    ("�" "{\\\\'\\\\i}")
			    ("�" "{\\\\^\\\\i}")
			    ("�" "\"o")
			    ("�" "{\\\\`o}")
			    ("�" "{\\\\'o}")
			    ("�" "{\\\\~o}")
			    ("�" "{\\\\^o}")
			    ("�" "\"u")
			    ("�" "{\\\\`u}")
			    ("�" "{\\\\'u}")
			    ("�" "{\\\\^u}")
			    ("�" "\"A")
			    ("�" "{\\\\`A}")
			    ("�" "{\\\\'A}")
			    ("�" "{\\\\~A}")
			    ("�" "{\\\\^A}")
			    ("�" "{\\\\\"E}")
			    ("�" "{\\\\`E}")
			    ("�" "{\\\\'E}")
			    ("�" "{\\\\^E}")
			    ("�" "{\\\\\"I}")
			    ("�" "{\\\\`I}")
			    ("�" "{\\\\'I}")
			    ("�" "{\\\\^I}")
			    ("�" "\"O")
			    ("�" "{\\\\`O}")
			    ("�" "{\\\\'O}")
			    ("�" "{\\\\~O}")
			    ("�" "{\\\\^O}")
			    ("�" "\"U")
			    ("�" "{\\\\`U}")
			    ("�" "{\\\\'U}")
			    ("�" "{\\\\^U}")
			    ("�" "{\\\\~n}")
			    ("�" "{\\\\~N}")
			    ("�" "{\\\\c c}")
			    ("�" "{\\\\c C}")
			    ("�" "\\\\3")
			    ("�" "{?`}")
			    ("�" "{!`}")
			    )
  )



(defun gtex2iso ()
 "Translate german TeX to ISO"
 (interactive)
 (translate-conventions gtex2iso-trans-tab))


(defun iso2gtex ()
 "Translate ISO to german TeX"
 (interactive)
 (translate-conventions iso2gtex-trans-tab))


(defun german-texP ()
 "Check if tex buffer is german LaTeX"
 (save-excursion
   (widen)
   (goto-char (point-min))
   (re-search-forward "\\\\documentstyle\\[.*german.*\\]" nil t)))


(defun fix-iso2tex ()
  "Turn ISO latin1 into TeX sequences"
  (if (equal major-mode 'latex-mode)
      (if (german-texP)
	  (iso2gtex)
	(iso2tex)))
  (if (equal major-mode 'tex-mode)
      (iso2tex)))

(defun fix-tex2iso ()
  "Turn TeX sequences into ISO latin1"
  (if (equal major-mode 'latex-mode)
      (if (german-texP)
	  (gtex2iso)
	(tex2iso)))
  (if (equal major-mode 'tex-mode)
      (tex2iso)))

(add-hook 'find-file-hooks 'fix-tex2iso)
(add-hook 'write-file-hooks 'fix-iso2tex)
