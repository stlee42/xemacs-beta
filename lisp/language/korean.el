;;; korean.el --- Support for Korean

;; Copyright (C) 1995 Free Software Foundation, Inc.
;; Copyright (C) 1995 Electrotechnical Laboratory, JAPAN.
;; Copyright (C) 1997 MORIOKA Tomohiko

;; Keywords: multilingual, Korean

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

;;; Commentary:

;; For Korean, the character set KSC5601 is supported.

;;; Code:

;; Syntax of Korean characters.
(loop for row from 33 to  34 do
      (modify-syntax-entry `[korean-ksc5601 ,row] "."))
(loop for row from 35 to  37 do
      (modify-syntax-entry `[korean-ksc5601 ,row] "w"))
(loop for row from 38 to  41 do
      (modify-syntax-entry `[korean-ksc5601 ,row] "."))
(loop for row from 42 to 126 do
      (modify-syntax-entry `[korean-ksc5601 ,row] "w"))

;; Setting for coding-system and quail were moved to
;; language/korean.el.

(make-coding-system
 'iso-2022-int-1 'iso2022
 "ISO-2022-INT-1"
 '(charset-g0 ascii
   charset-g1 korean-ksc5601
   short t
   seven t
   lock-shift t
   mnemonic "INT-1"))

;; EGG specific setup
(define-egg-environment 'korean
  "Korean settings for egg"
  (lambda ()
    (when (not (featurep 'egg-kor))
      (load "its-hangul")
      (setq its:*standard-modes*
	    (cons (its:get-mode-map "hangul") its:*standard-modes*))
      (provide 'egg-kor))
    (setq wnn-server-type 'kserver)
    (setq egg-default-startup-file "eggrc-wnn")
    (setq-default its:*current-map* (its:get-mode-map "hangul"))))

;; (make-coding-system
;;  'euc-kr 2 ?K
;;  "Coding-system of Korean EUC (Extended Unix Code)."
;;  '((ascii t) korean-ksc5601 nil nil
;;    nil ascii-eol ascii-cntl))

(make-coding-system
 'euc-kr 'iso2022
 "Coding-system of Korean EUC (Extended Unix Code)."
 '(charset-g0 ascii
   charset-g1 korean-ksc5601
   mnemonic "ko/EUC"
   eol-type lf))

;;(define-coding-system-alias 'euc-kr 'euc-korea)

(copy-coding-system 'euc-kr 'korean-euc)

;; (make-coding-system
;;  'iso-2022-kr 2 ?k
;;  "MIME ISO-2022-KR"
;;  '(ascii (nil korean-ksc5601) nil nil
;;          nil ascii-eol ascii-cntl seven locking-shift nil nil nil nil nil
;;          designation-bol))

(make-coding-system
 'iso-2022-kr 'iso2022
 "Coding-System used for communication with mail in Korea."
 '(charset-g0 ascii
   charset-g1 korean-ksc5601
   force-g1-on-output t
   seven t
   lock-shift t
   mnemonic "Ko/7bit"
   eol-type lf))

(register-input-method
 "Korean" '("quail-hangul" quail-use-package "quail/hangul"))
(register-input-method
 "Korean" '("quail-hangul3" quail-use-package "quail/hangul3"))
(register-input-method
 "Korean" '("quail-hanja" quail-use-package "quail/hanja"))
(register-input-method
 "Korean" '("quail-symbol-ksc" quail-use-package "quail/symbol-ksc"))
(register-input-method
 "Korean" '("quail-hanja-jis" quail-use-package "quail/hanja-jis"))

(defun setup-korean-environment ()
  "Setup multilingual environment (MULE) for Korean."
  (interactive)
  (setup-english-environment)
  (setq coding-category-iso-8-2 'euc-kr)
  
  (set-coding-priority
   '(coding-category-iso-7
     coding-category-iso-8-2
     coding-category-iso-8-1))
  
  (setq-default buffer-file-coding-system 'euc-kr)
  
  (setq default-input-method '("Korean" . "quail-hangul"))
  )

(defun describe-korean-support ()
  "Describe How Emacs supports Korean."
  (interactive)
  (describe-language-support-internal "Korean"))

(set-language-info-alist
 "Korean" '((setup-function . setup-korean-environment)
	    (describe-function . describe-korean-support)
	    (tutorial . "TUTORIAL.kr")
	    (charset . (korean-ksc5601))
	    (coding-system . (euc-kr iso-2022-kr))
	    (sample-text . "Hangul ($(CGQ1[(B)       $(C>H3gGO<<?d(B, $(C>H3gGO=J4O1n(B")
	    (documentation . nil)))

;;; for XEmacs (will be obsoleted)

(define-language-environment 'korean
  "Korean"
  (lambda ()
    (set-coding-category-system 'iso-8-2 'euc-kr)
    (set-coding-priority-list '(iso-8-2 iso-7 iso-8-designate))
    (set-pathname-coding-system 'euc-kr)
    (add-hook 'comint-exec-hook
	      (lambda ()
		(let ((proc (get-buffer-process (current-buffer))))
		  (set-process-input-coding-system  proc 'euc-kr)
		  (set-process-output-coding-system proc 'euc-kr))))
    (set-buffer-file-coding-system-for-read 'automatic-conversion)
    (set-default-buffer-file-coding-system  'euc-kr)
    (setq keyboard-coding-system            'euc-kr)
    (setq terminal-coding-system            'euc-kr)
    (when (eq 'x (device-type (selected-device)))
      (x-use-halfwidth-roman-font 'korean-ksc5601 "ksc5636"))
    
    ;; EGG specific setup 97.02.05 jhod
    (when (featurep 'egg)
      (when (not (featurep 'egg-kor))
	(provide 'egg-kor)
	(load "its/its-hangul")
	(setq its:*standard-modes*
	      (cons (its:get-mode-map "hangul") its:*standard-modes*)))
      (setq-default its:*current-map* (its:get-mode-map "hangul")))
    
;;     ;; (setq-default quail-current-package
;;     ;;               (assoc "hangul" quail-package-alist))
    ))

;;; korean.el ends here
