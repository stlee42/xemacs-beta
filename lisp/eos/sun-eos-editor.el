;;; sun-eos-editor.el --- Implements the XEmacs/SPARCworks editor protocol

;; Copyright (C) 1995 Sun Microsystems, Inc.

;; Maintainer:	Eduardo Pelegri-Llopart <eduardo.pelegri-llopart@Eng.Sun.COM>
;; Author:      Eduardo Pelegri-Llopart <eduardo.pelegri-llopart@Eng.Sun.COM>

;; Keywords:	SPARCworks EOS Era on SPARCworks editor

;;; Commentary:

;; Please send feedback to eduardo.pelegri-llopart@eng.sun.com

;;; Code:

(require 'eos-common "sun-eos-common")

;; ===============
;; Editor protocol
;;
;; message is
;; SPRO_Visit_File CONTEXT_UID filename lineno center==0

(defvar eos::visit-file-pattern)
(defvar eos::get-src-line-pattern)

(defun eos::editor-startup ()
  ;; Actions to do at startup time for eos-editor
  (setq eos::visit-file-pattern
	(eos::create-visit-file-pattern))
  (setq eos::get-src-line-pattern
	(eos::create-get-src-line-pattern))
  (eos::register-get-src-line-pattern)
  )

(defun eos::visit-file-callback (msg pat)
  ;; A callback for a SPRO_Visit_File message
  ;; really should be discarded in the pattern
  (let* ((filename
	  (get-tooltalk-message-attribute msg 'arg_val 1))
	 (lineno-dot
	  (read
	   (get-tooltalk-message-attribute msg 'arg_ival 2)))
	 )
    (if (null (eos::find-line filename lineno-dot 'debugger-visit))
	(message "No frame to select"))
    (return-tooltalk-message msg)
    ))

(defun eos::create-visit-file-pattern ()
  ;; Create Visit File pattern
  (let* ((pattern-desc '(category TT_HANDLE
			    scope TT_SESSION
			    class TT_REQUEST
			    op "SPRO_Visit_File"
			    callback eos::visit-file-callback))
	 (pattern (make-tooltalk-pattern pattern-desc))
	 )
    pattern
    ))

(defun eos::register-visit-file-pattern ()
  ;; Register Visit File pattern
  (register-tooltalk-pattern eos::visit-file-pattern))

(defun eos::unregister-visit-file-pattern ()
  ;; Unregister Visit File pattern
  (unregister-tooltalk-pattern eos::visit-file-pattern))

;;
;; ====================
;;
;; Auxiliary TT message to get source and lineno.
;;
;; message is
;; SPRO_Get_Src_Line CONTEXT_UID (INOUT filename) (INOUT lineno)

;;

(defun eos::get-src-line-callback (msg pat)
  ;; A callback for a SPRO_Get_Src_Line message
  ;; really should be discarded in the pattern
  (let* ((filename
	  (buffer-file-name))
	 (lineno
	  (format "%d" (eos::line-at (point)))))
    (set-tooltalk-message-attribute filename msg 'arg_val 1)
    (set-tooltalk-message-attribute lineno msg 'arg_val 2)
    (return-tooltalk-message msg)
    ))

(defun eos::create-get-src-line-pattern ()
  ;; Create a pattern to get filename and lineno
  (let* ((pattern-desc '(category TT_HANDLE
			    scope TT_SESSION
			    class TT_REQUEST
			    op "SPRO_Get_Src_Line"
			    callback eos::get-src-line-callback))
	 (pattern (make-tooltalk-pattern pattern-desc))
	 )
    pattern
    ))

(defun eos::register-get-src-line-pattern ()
  ;; Register Get Src Line pattern
  (register-tooltalk-pattern eos::get-src-line-pattern))
  
(defun eos::unregister-get-src-line-pattern ()
  ;; Unregister Get Src Line pattern
  (unregister-tooltalk-pattern eos::get-src-line-pattern))
  
(provide 'eos-editor)

;;; sun-eos-debugger.el ends here
