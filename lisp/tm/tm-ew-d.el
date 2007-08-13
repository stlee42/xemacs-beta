;;; tm-ew-d.el --- RFC 2047 based encoded-word decoder for GNU Emacs

;; Copyright (C) 1995,1996 Free Software Foundation, Inc.

;; Author: ENAMI Tsugutomo <enami@sys.ptg.sony.co.jp>
;;         MORIOKA Tomohiko <morioka@jaist.ac.jp>
;; Maintainer: MORIOKA Tomohiko <morioka@jaist.ac.jp>
;; Created: 1995/10/03
;; Original: 1992/07/20 ENAMI Tsugutomo's `mime.el'.
;;	Renamed: 1993/06/03 to tiny-mime.el.
;;	Renamed: 1995/10/03 from tiny-mime.el. (split off encoder)
;; Version: $Revision: 1.1.1.1 $
;; Keywords: encoded-word, MIME, multilingual, header, mail, news

;; This file is part of tm (Tools for MIME).

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

(require 'emu)
(require 'std11)
(require 'mel)
(require 'tm-def)


;;; @ version
;;;

(defconst tm-ew-d/RCS-ID
  "$Id: tm-ew-d.el,v 1.1.1.1 1996/12/18 03:55:31 steve Exp $")
(defconst mime/eword-decoder-version (get-version-string tm-ew-d/RCS-ID))


;;; @ MIME encoded-word definition
;;;

(defconst mime/encoded-text-regexp "[!->@-~]+")
(defconst mime/encoded-word-regexp (concat (regexp-quote "=?")
					   "\\("
					   mime/charset-regexp
					   "\\)"
					   (regexp-quote "?")
					   "\\(B\\|Q\\)"
					   (regexp-quote "?")
					   "\\("
					   mime/encoded-text-regexp
					   "\\)"
					   (regexp-quote "?=")))


;;; @ for string
;;;

(defun mime-eword/decode-string (string &optional must-unfold)
  "Decode MIME encoded-words in STRING.

STRING is unfolded before decoding.

If an encoded-word is broken or your emacs implementation can not
decode the charset included in it, it is not decoded.

If MUST-UNFOLD is non-nil, it unfolds and eliminates line-breaks even
if there are in decoded encoded-words (generated by bad manner MUA
such as a version of Net$cape). [tm-ew-d.el]"
  (setq string (std11-unfold-string string))
  (let ((dest "")(ew nil)
	beg end)
    (while (and (string-match mime/encoded-word-regexp string)
		(setq beg (match-beginning 0)
		      end (match-end 0))
		)
      (if (> beg 0)
	  (if (not
	       (and (eq ew t)
		    (string-match "^[ \t]+$" (substring string 0 beg))
		    ))
	      (setq dest (concat dest (substring string 0 beg)))
	    )
	)
      (setq dest
	    (concat dest
		    (mime/decode-encoded-word
		     (substring string beg end) must-unfold)
		    ))
      (setq string (substring string end))
      (setq ew t)
      )
    (concat dest string)
    ))


;;; @ for region
;;;

(defun mime-eword/decode-region (start end &optional unfolding must-unfold)
  "Decode MIME encoded-words in region between START and END.

If UNFOLDING is not nil, it unfolds before decoding.

If MUST-UNFOLD is non-nil, it unfolds and eliminates line-breaks even
if there are in decoded encoded-words (generated by bad manner MUA
such as a version of Net$cape). [tm-ew-d.el]"
  (interactive "*r")
  (save-excursion
    (save-restriction
      (narrow-to-region start end)
      (if unfolding
	  (mime/unfolding)
	)
      (goto-char (point-min))
      (while (re-search-forward "\\?=\\(\n*\\s +\\)+=\\?" nil t)
	(replace-match "?==?")
	)
      (goto-char (point-min))
      (let (charset encoding text)
	(while (re-search-forward mime/encoded-word-regexp nil t)
	  (insert (mime/decode-encoded-word
		   (prog1
		       (buffer-substring (match-beginning 0) (match-end 0))
		     (delete-region (match-beginning 0) (match-end 0))
		     ) must-unfold))
	  ))
      )))


;;; @ for message header
;;;

(defun mime/decode-message-header ()
  "Decode MIME encoded-words in message header. [tm-ew-d.el]"
  (interactive "*")
  (save-excursion
    (save-restriction
      (narrow-to-region (goto-char (point-min))
			(progn (re-search-forward "^$" nil t) (point)))
      (mime-eword/decode-region (point-min) (point-max) t)
      )))

(defun mime/unfolding ()
  (goto-char (point-min))
  (let (field beg end)
    (while (re-search-forward std11-field-head-regexp nil t)
      (setq beg (match-beginning 0)
            end (std11-field-end))
      (setq field (buffer-substring beg end))
      (if (string-match mime/encoded-word-regexp field)
          (save-restriction
            (narrow-to-region (goto-char beg) end)
            (while (re-search-forward "\n\\([ \t]\\)" nil t)
              (replace-match
               (match-string 1))
              )
	    (goto-char (point-max))
	    ))
      )))


;;; @ encoded-word decoder
;;;

(defun mime/decode-encoded-word (word &optional must-unfold)
  "Decode WORD if it is an encoded-word.

If your emacs implementation can not decode the charset of WORD, it
returns WORD.  Similarly the encoded-word is broken, it returns WORD.

If MUST-UNFOLD is non-nil, it unfolds and eliminates line-breaks even
if there are in decoded encoded-word (generated by bad manner MUA such
as a version of Net$cape). [tm-ew-d.el]"
  (or (if (string-match mime/encoded-word-regexp word)
	  (let ((charset
		 (substring word (match-beginning 1) (match-end 1))
		 )
		(encoding
		 (upcase
		  (substring word (match-beginning 2) (match-end 2))
		  ))
		(text
		 (substring word (match-beginning 3) (match-end 3))
		 ))
            (condition-case err
                (mime/decode-encoded-text charset encoding text must-unfold)
              (error nil))
	    ))
      word))


;;; @ encoded-text decoder
;;;

(defun mime/decode-encoded-text (charset encoding string &optional must-unfold)
  "Decode STRING as an encoded-text.

If your emacs implementation can not decode CHARSET, it returns nil.

If ENCODING is not \"B\" or \"Q\", it occurs error.
So you should write error-handling code if you don't want break by errors.

If MUST-UNFOLD is non-nil, it unfolds and eliminates line-breaks even
if there are in decoded encoded-text (generated by bad manner MUA such
as a version of Net$cape). [tm-ew-d.el]"
  (let ((cs (mime-charset-to-coding-system charset)))
    (if cs
	(let ((dest
               (cond ((and (string-equal "B" encoding)
                           (string-match mime/B-encoded-text-regexp string))
                      (base64-decode-string string))
                     ((and (string-equal "Q" encoding)
                           (string-match mime/Q-encoded-text-regexp string))
                      (q-encoding-decode-string string))
		     (t (message "Invalid encoded-word %s" encoding)
			nil))))
	  (if dest
	      (progn
		(setq dest (decode-coding-string dest cs))
		(if must-unfold
		    (mapconcat (function
				(lambda (chr)
				  (if (eq chr ?\n)
				      ""
				    (char-to-string chr)
				    )
				  ))
			       (std11-unfold-string dest)
			       "")
		  dest)
		))))))


;;; @ end
;;;

(provide 'tm-ew-d)

;;; tm-ew-d.el ends here
