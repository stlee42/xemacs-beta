;;; code-files.el --- File I/O functions for XEmacs.

;; Copyright (C) 1992,93,94,95 Free Software Foundation, Inc.
;; Copyright (C) 1995 Amdahl Corporation.
;; Copyright (C) 1995 Sun Microsystems.
;; Copyright (C) 2001, 2002 Ben Wing.

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

;;; Synched up with: Not synched.

;;; Commentary:

;; Derived from mule.el in the original Mule but heavily modified
;; by Ben Wing.

;; 1997/3/11 modified by MORIOKA Tomohiko to sync with Emacs 20 API.

;; This file was derived from the former mule-files.el which has been removed
;; as of XEmacs 21.2.15.

;;; Code:

(put 'buffer-file-coding-system 'permanent-local t)

(defvar buffer-file-coding-system-when-loaded nil
  "Coding system used when current buffer's file was read in.

Automatically buffer-local when set in any fashion.  This is set
automatically when a file is loaded and is used when the file needs to be
reloaded (e.g. `revert-buffer').  Normally this will have the same value as
`buffer-file-coding-system', but the latter may be changed because it's
also used to specify the encoding when the file is written out.")
(make-variable-buffer-local 'buffer-file-coding-system-when-loaded)
(put 'buffer-file-coding-system-when-loaded 'permanent-local t)

(define-obsolete-variable-alias
  'file-coding-system
  'buffer-file-coding-system)

(define-obsolete-variable-alias
  'overriding-file-coding-system
  'coding-system-for-read)

;; NOTE: The real default value is set in code-init.el.
(defvar buffer-file-coding-system-for-read nil
  "Default coding system used when reading a file.
This provides coarse-grained control; for finer-grained control, use
`file-coding-system-alist'.  From a Lisp program, if you wish to
unilaterally specify the coding system used for one particular
operation, you should bind the variable `coding-system-for-read'
rather than setting this variable, which is intended to be used for
global environment specification.

See `insert-file-contents' for a full description of how a file's
coding system is determined when it is read in.")

(define-obsolete-variable-alias
  'file-coding-system-for-read
  'buffer-file-coding-system-for-read)

(defvar file-coding-system-alist
  `(
;; This must not be necessary, slb suggests -kkm
;;  ("loaddefs.el$" . (binary . binary))
    ,@(mapcar
       #'(lambda (regexp) (cons regexp 'binary)) binary-file-regexps)
    ("TUTORIAL\\.\\(?:hr\\|pl\\|ro\\)\\'" . iso-8859-2)
    ;; ("\\.\\(el\\|emacs\\|info\\(-[0-9]+\\)?\\|texi\\)$" . iso-2022-8)
    ;; ("\\(ChangeLog\\|CHANGES-beta\\)$" . iso-2022-8)

    ;; This idea is totally broken, and the code didn't work anyway.
    ;; Mailboxes should be decoded by mail clients, who actually know
    ;; how to deal with them.  Otherwise, their contents should be
    ;; treated as `binary'.
    ;("/spool/mail/.*$" . convert-mbox-coding-system)
    )
  "Alist to decide a coding system to use for a file I/O operation.
The format is ((PATTERN . VAL) ...),
where PATTERN is a regular expression matching a file name,
VAL is a coding system, a cons of coding systems, or a function symbol.
If VAL is a coding system, it is used for both decoding and encoding
the file contents.
If VAL is a cons of coding systems, the car part is used for decoding,
and the cdr part is used for encoding.
If VAL is a function symbol, the function must return a coding system
or a cons of coding systems which are used as above.

This overrides the more general specification in
`buffer-file-coding-system-for-read', but is overridden by
`coding-system-for-read'.")

(defun set-buffer-file-coding-system (coding-system &optional force)
  "Set buffer-file-coding-system of the current buffer to CODING-SYSTEM.
If optional argument FORCE (interactively, the prefix argument) is not
given, attempt to match the EOL type of the new coding system to
the current value of `buffer-file-coding-system'."
  (interactive "zFile coding system: \nP")
  (get-coding-system coding-system) ;; correctness check
  (if (not force)
      (setq coding-system
	    (subsidiary-coding-system
	     coding-system
	     (coding-system-eol-type buffer-file-coding-system))))
  (setq buffer-file-coding-system coding-system)
  (redraw-modeline t))

(defun toggle-buffer-file-coding-system ()
  "Set EOL type of buffer-file-coding-system of the current buffer to
something other than what it is at the moment."
  (interactive)
  (let ((eol-type
	 (coding-system-eol-type buffer-file-coding-system)))
    (setq buffer-file-coding-system
	  (subsidiary-coding-system
	   (coding-system-base buffer-file-coding-system)
	   (cond ((eq eol-type 'lf) 'crlf)
		 ((eq eol-type 'crlf) 'lf)
		 ((eq eol-type 'cr) 'lf))))
    (set-buffer-modified-p t)))

(define-obsolete-function-alias
  'set-file-coding-system
  'set-buffer-file-coding-system)

(defun set-buffer-file-coding-system-for-read (coding-system)
  "Set the coding system used when reading in a file.
This is equivalent to setting the variable
`buffer-file-coding-system-for-read'.  You can also use
`file-coding-system-alist' to specify the coding system for
particular files."
  (interactive "zFile coding system for read: ")
  (get-coding-system coding-system) ;; correctness check
  (setq buffer-file-coding-system-for-read coding-system))

(define-obsolete-function-alias
  'set-file-coding-system-for-read
  'set-buffer-file-coding-system-for-read)

(defun set-default-buffer-file-coding-system (coding-system)
  "Set the default value of `buffer-file-coding-system' to CODING-SYSTEM.
The default value is used both for buffers without associated files
and for files with no apparent coding system (i.e. primarily ASCII).
See `buffer-file-coding-system' for more information."
  (interactive "zDefault file coding system: ")
  (setq-default buffer-file-coding-system coding-system)
  (redraw-modeline t))

(define-obsolete-function-alias
  'set-default-file-coding-system
  'set-default-buffer-file-coding-system)

(defun find-file-coding-system-for-read-from-filename (filename)
  "Look up coding system to read a file in `file-coding-system-alist'.
The return value will be nil (no applicable entry) or a coding system
object (the entry specified a coding system)."
  (let ((alist file-coding-system-alist)
	(found nil)
	(codesys nil))
    (let ((case-fold-search nil))
      (setq filename (file-name-sans-versions filename))
      (while (and (not found) alist)
	(if (string-match (car (car alist)) filename)
	    (setq codesys (cdr (car alist))
		  found t))
	(setq alist (cdr alist))))
    (when codesys
      (if (functionp codesys)
	  (setq codesys (funcall codesys 'insert-file-contents filename))
	)
      (cond ((consp codesys) (find-coding-system (car codesys)))
	    ((find-coding-system codesys))
	    ))))

(define-obsolete-function-alias
  'find-file-coding-system-from-filename
  'find-file-coding-system-for-read-from-filename)

(defun find-file-coding-system-for-write-from-filename (filename)
  "Look up coding system to write a file in `file-coding-system-alist'.
The return value will be nil (no applicable entry) or a coding system
object (the entry specified a coding system)."
  (let ((alist file-coding-system-alist)
	(found nil)
	(codesys nil))
    (let ((case-fold-search nil))
      (setq filename (file-name-sans-versions filename))
      (while (and (not found) alist)
	(if (string-match (car (car alist)) filename)
	    (setq codesys (cdr (car alist))
		  found t))
	(setq alist (cdr alist))))
    (when codesys
      (if (functionp codesys)
	  (setq codesys (funcall codesys 'write-region filename))
	)
      (cond ((consp codesys) (find-coding-system (cdr codesys)))
	    ((find-coding-system codesys))
	    ))))

;; This was completely broken, not only in implementation (does not
;; understand MIME), but in concept -- such high-level decoding should
;; be done by mail readers, not by IO code!  Removed 2000-04-18.

;(defun convert-mbox-coding-system (filename visit start end) ...)

(defun load (file &optional noerror nomessage nosuffix)
  "Execute a file of Lisp code named FILE, or load a binary module.
First tries to find a Lisp FILE with .elc appended, then with .el, then with
 FILE unmodified.  If unsuccessful, tries to find a binary module FILE with
 the elements of `module-extensions' appended, one at a time.
Searches directories in load-path for Lisp files, and in `module-load-path'
 for binary modules.
If optional second arg NOERROR is non-nil,
 report no error if FILE doesn't exist.
Print messages at start and end of loading unless
 optional third arg NOMESSAGE is non-nil.
If optional fourth arg NOSUFFIX is non-nil, don't try adding suffixes
 .elc, .el, or elements of `module-extensions' to the specified name FILE.
Return t if file exists."
  (declare (special load-modules-quietly))
  (let* ((filename (substitute-in-file-name file))
	 (handler (find-file-name-handler filename 'load))
	 (path nil))
    (if handler
	(funcall handler 'load filename noerror nomessage nosuffix)
      ;; First try to load a Lisp file
      (if (and (> (length filename) 0)
	       (setq path (locate-file filename load-path
				       (and (not nosuffix)
					    '(".elc" ".el" "")))))
	  ;; now use the internal load to actually load the file.
	  (load-internal
	   file noerror nomessage nosuffix
	   (let ((elc ; use string= instead of string-match to keep match-data.
		(equalp ".elc" (substring path -4))))
	     (or (and (not elc) coding-system-for-read) ;prefer for source file
		 ;; find magic-cookie
		 (let ((codesys
			(find-coding-system-magic-cookie-in-file path)))
		   (when codesys
		     (setq codesys (intern codesys))
		     (if (find-coding-system codesys) codesys)))
		 (if elc
		     ;; if reading a byte-compiled file and we didn't find
		     ;; a coding-system magic cookie, then use `binary'.
		     ;; We need to guarantee that we never do autodetection
		     ;; on byte-compiled files because confusion here would
		     ;; be a very bad thing.  Pre-existing byte-compiled
		     ;; files are always in the `binary' coding system.
		     ;; Also, byte-compiled files always use `lf' to terminate
		     ;; a line; don't risk confusion here either.
		     'binary
		   (or (find-file-coding-system-for-read-from-filename path)
		       ;; looking up in `file-coding-system-alist'.
		       ;; otherwise use `buffer-file-coding-system-for-read',
		       ;; as normal
		       buffer-file-coding-system-for-read)
		   ))))
	;; The file name is invalid, or we want to load a binary module
	(if (and (> (length filename) 0)
		 (locate-file filename module-load-path
			      (and (not nosuffix) module-extensions)))
	    (if (featurep 'modules)
		(let ((load-modules-quietly nomessage))
		  (declare-fboundp (load-module filename)))
	      (signal 'file-error '("This XEmacs does not support modules")))
	  (and (null noerror)
	       (signal 'file-error (list "Cannot open load file" filename))))
	))))

(defvar insert-file-contents-access-hook nil
  "A hook to make a file accessible before reading it.
`insert-file-contents' calls this hook before doing anything else.
Called with two arguments: FILENAME and VISIT, the same as the
corresponding arguments in the call to `insert-file-contents'.")

(defvar insert-file-contents-pre-hook nil
  "A special hook to decide the coding system used for reading in a file.

Before reading a file, `insert-file-contents' calls the functions on
this hook with arguments FILENAME and VISIT, the same as the
corresponding arguments in the call to `insert-file-contents'.  In
these functions, you may refer to the global variable
`buffer-file-coding-system-for-read'.

The return value of the functions should be either

-- nil
-- A coding system or a symbol denoting it, indicating the coding system
   to be used for reading the file
-- A list of two elements (absolute pathname and length of data inserted),
   which is used as the return value to `insert-file-contents'.  In this
   case, `insert-file-contents' assumes that the function has inserted
   the file for itself and suppresses further reading.

If any function returns non-nil, the remaining functions are not called.")

(defvar insert-file-contents-error-hook nil
  "A hook to set `buffer-file-coding-system' when a read error has occurred.

When a file error (e.g. nonexistent file) occurs while read a file,
`insert-file-contents' calls the functions on this hook with three
arguments: FILENAME and VISIT (the same as the corresponding arguments
in the call to `insert-file-contents') and a cons (SIGNALED-CONDITIONS
. SIGNAL-DATA).

After calling this hook, the error is signalled for real and
propagates to the caller of `insert-file-contents'.")

(defvar insert-file-contents-post-hook nil
  "A hook to set `buffer-file-coding-system' for the current buffer.

After successful reading, `insert-file-contents' calls the functions
on this hook with four arguments: FILENAME and VISIT (the same as the
corresponding arguments in the call to `insert-file-contents'),
CODING-SYSTEM (the actual coding system used to decode the file), and
a cons of absolute pathname and length of data inserted (the same
thing as will be returned from `insert-file-contents').")

(defun insert-file-contents (filename &optional visit start end replace)
  "Insert contents of file FILENAME after point.
Returns list of absolute file name and length of data inserted.
If second argument VISIT is non-nil, the buffer's visited filename
and last save file modtime are set, and it is marked unmodified.
If visiting and the file does not exist, visiting is completed
before the error is signaled.

The optional third and fourth arguments START and END
specify what portion of the file to insert.
If VISIT is non-nil, START and END must be nil.
If optional fifth argument REPLACE is non-nil,
it means replace the current buffer contents (in the accessible portion)
with the file contents.  This is better than simply deleting and inserting
the whole thing because (1) it preserves some marker positions
and (2) it puts less data in the undo list.

The coding system used for decoding the file is determined as follows:

1. `coding-system-for-read', if non-nil. (Intended as a temporary overriding
      mechanism for use by Lisp code.)
2. The result of `insert-file-contents-pre-hook', if non-nil. (Intended for
      handling tricky cases where the coding system of the file cannot be
      determined just by looking at the filename's extension and the standard
      auto-detection mechanism isn't suitable, so more clever code is required.
      In general, this hook should rarely be used.)
3. The matching value for this filename from `file-coding-system-alist',
      if any. (Intended as the standard way of determining encoding from
      the name, or esp. the extension, of the file.  Akin to the way
      file-name extensions are used under MS Windows to determine how to
      handle the file, but more flexible.)
4. `buffer-file-coding-system-for-read', if non-nil. (Intended to be where
      the global default coding system is set.  Usually, you want to use
      the value `undecided', to let the system auto-detect according to the
      priorities set up by `set-coding-priority-list'.  This is usually
      initialized from the `coding-system' property of the current language
      environment.)
5. The coding system 'raw-text.

If a local value for `buffer-file-coding-system' in the current buffer
does not exist, it is set to the coding system which was actually used
for reading.

#### This should explain in more detail the exact workings of the
coding-system determination procedure.

See also `insert-file-contents-access-hook',
`insert-file-contents-pre-hook', `insert-file-contents-error-hook',
and `insert-file-contents-post-hook'."
  (let (return-val coding-system used-codesys)
    ;; OK, first load the file.
    (condition-case err
	(progn
	  (run-hook-with-args 'insert-file-contents-access-hook
			      filename visit)
	  ;; determine the coding system to use, as described above.
	  (setq coding-system
		(or
		 ;; #1.
		 coding-system-for-read
		 ;; #2.
		 (run-hook-with-args-until-success
		  'insert-file-contents-pre-hook
		  filename visit)
		 ;; #3.
		 (find-file-coding-system-for-read-from-filename filename)
		 ;; #4.
		 buffer-file-coding-system-for-read
		 ;; #5.
		 'raw-text))
	  (if (consp coding-system)
	      (setq return-val coding-system)
	    (if (null (find-coding-system coding-system))
		(progn
		  (lwarn 'coding-system 'notice
		   "Invalid coding-system (%s), using 'undecided"
		   coding-system)
		  (setq coding-system 'undecided)))
	    (setq return-val
		  (insert-file-contents-internal filename visit start end
						 replace coding-system
						 ;; store here!
						 'used-codesys))
	    ))
      (file-error
       (run-hook-with-args 'insert-file-contents-error-hook
			   filename visit err)
       (signal (car err) (cdr err))))
    (setq coding-system used-codesys)
    ;; call any `post-read-conversion' for the coding system that
    ;; was used ...
    (let ((func
	   (coding-system-property coding-system 'post-read-conversion))
	  (endmark (make-marker)))
      (set-marker endmark (+ (point) (nth 1 return-val)))
      (if func
	  (unwind-protect
	      (save-excursion
		(let (buffer-read-only)
		  (if (>= (function-max-args func) 2)
		      ;; #### fuckme!  Someone at FSF changed the calling
		      ;; convention of post-read-conversion.  We try to
		      ;; support the old way.  #### Should we kill this?
		      (funcall func (point) (marker-position endmark))
		    (funcall func (- (marker-position endmark) (point))))))
	    (if visit
		(progn
		  (set-buffer-auto-saved)
		  (set-buffer-modified-p nil)))))
      (setcar (cdr return-val) (- (marker-position endmark) (point))))
    ;; now finally set the buffer's `buffer-file-coding-system' ...
    (if (run-hook-with-args-until-success 'insert-file-contents-post-hook
					  filename visit return-val)
	nil
      (if (local-variable-p 'buffer-file-coding-system (current-buffer))
	  ;; if buffer-file-coding-system is already local, just
	  ;; set its eol type to what was found, if it wasn't
	  ;; set already.
	  (set-buffer-file-coding-system
	   (subsidiary-coding-system buffer-file-coding-system
				     (coding-system-eol-type coding-system)) t)
	;; otherwise actually set buffer-file-coding-system.
	(set-buffer-file-coding-system coding-system t)))
    ;; ... and `buffer-file-coding-system-when-loaded'.  the machinations
    ;; of set-buffer-file-coding-system cause the actual coding system
    ;; object to be stored, so do that here, too.
    (setq buffer-file-coding-system-when-loaded 
	  (get-coding-system coding-system))
    return-val))

(defvar write-region-pre-hook nil
  "A special hook to decide the coding system used for writing out a file.

Before writing a file, `write-region' calls the functions on this hook with
arguments START, END, FILENAME, APPEND, VISIT, LOCKNAME and CODING-SYSTEM,
the same as the corresponding arguments in the call to `write-region'.

The return value of each function should be one of

-- nil
-- A coding system or a symbol denoting it, indicating the coding system
   to be used for writing the file
-- A list of two elements (absolute pathname and length of data written),
   which is used as the return value to `write-region'.  In this case,
   `write-region' assumes that the function has written the file and
   returns.

If any function returns non-nil, the remaining functions are not called.")

(defvar write-region-post-hook nil
  "A hook called by `write-region' after a file has been written out.

The functions on this hook are called with arguments START, END,
FILENAME, APPEND, VISIT, LOCKNAME, and CODING-SYSTEM, the same as the
corresponding arguments in the call to `write-region'.")

(defun write-region (start end filename
		     &optional append visit lockname coding-system)
  "Write current region into specified file.
By default the file's existing contents are replaced by the specified region.
Called interactively, prompts for a file name.  With a prefix arg, prompts
for a coding system as well.

When called from a program, takes three required arguments:
START, END and FILENAME.  START and END are buffer positions.
Optional fourth argument APPEND if non-nil means
  append to existing file contents (if any).
Optional fifth argument VISIT if t means
  set last-save-file-modtime of buffer to this file's modtime
  and mark buffer not modified.
If VISIT is a string, it is a second file name;
  the output goes to FILENAME, but the buffer is marked as visiting VISIT.
  VISIT is also the file name to lock and unlock for clash detection.
If VISIT is neither t nor nil nor a string,
  that means do not print the \"Wrote file\" message.
The optional sixth arg LOCKNAME, if non-nil, specifies the name to
  use for locking and unlocking, overriding FILENAME and VISIT.
Kludgy feature: if START is a string, then that string is written
to the file, instead of any buffer contents, and END is ignored.
Optional seventh argument CODING-SYSTEM specifies the coding system
  used to encode the text when it is written out, and defaults to
  the value of `buffer-file-coding-system' in the current buffer.
See also `write-region-pre-hook' and `write-region-post-hook'."
  (interactive "r\nFWrite region to file: \ni\ni\ni\nZCoding-system: ")
  (setq coding-system
	(or coding-system-for-write
	    (run-hook-with-args-until-success
	     'write-region-pre-hook
	     start end filename append visit lockname coding-system)
	    coding-system
	    buffer-file-coding-system
	    (find-file-coding-system-for-write-from-filename filename)
	    ))
  (if (consp coding-system)
      ;; One of the `write-region-pre-hook' functions wrote the file
      coding-system
    (let ((func
	   (coding-system-property coding-system 'pre-write-conversion)))
      (if func
	  (let ((curbuf (current-buffer))
		(tempbuf (generate-new-buffer " *temp-write-buffer*"))
		(modif (buffer-modified-p)))
	    (unwind-protect
		(save-excursion
		  (set-buffer tempbuf)
		  (erase-buffer)
		  (insert-buffer-substring curbuf start end)
		  (funcall func (point-min) (point-max))
		  (write-region-internal (point-min) (point-max) filename
					 append
					 (if (eq visit t) nil visit)
					 lockname
					 coding-system))
	      ;; leaving a buffer associated with file will cause problems
	      ;; when next visiting.
	      (kill-buffer tempbuf)
	      (if (or visit (null modif))
		  (progn
		    (set-buffer-auto-saved)
		    (set-buffer-modified-p nil)
		    (if (buffer-file-name) (set-visited-file-modtime))))))
	(write-region-internal start end filename append visit lockname
			       coding-system)))
    (run-hook-with-args 'write-region-post-hook
			start end filename append visit lockname
			coding-system)))

;;; code-files.el ends here
