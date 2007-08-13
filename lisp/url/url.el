;;; url.el,v --- Uniform Resource Locator retrieval tool
;; Author: wmperry
;; Created: 1996/05/30 13:25:47
;; Version: 1.52
;; Keywords: comm, data, processes, hypermedia

;;; LCD Archive Entry:
;;; url|William M. Perry|wmperry@spry.com|
;;; Major mode for manipulating URLs|
;;; 1996/05/30 13:25:47|1.52|Location Undetermined
;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Copyright (c) 1993, 1994, 1995 by William M. Perry (wmperry@spry.com)
;;;
;;; This file is not part of GNU Emacs, but the same permissions apply.
;;;
;;; GNU Emacs is free software; you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation; either version 2, or (at your option)
;;; any later version.
;;;
;;; GNU Emacs is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; You should have received a copy of the GNU General Public License
;;; along with GNU Emacs; see the file COPYING.  If not, write to
;;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Copyright (c) 1993, 1994, 1995 by William M. Perry (wmperry@spry.com)   ;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


(require 'url-vars)
(require 'url-parse)
(require 'urlauth)
(require 'url-cookie)
(require 'mm)
(require 'md5)
(require 'base64)
(require 'url-hash)
(or (featurep 'efs)
    (featurep 'efs-auto)
    (condition-case ()
	(require 'ange-ftp)
      (error nil)))

(load-library "url-sysdp")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Functions that might not exist in old versions of emacs
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun url-save-error (errobj)
  (save-excursion
    (set-buffer (get-buffer-create " *url-error*"))
    (erase-buffer))
  (display-error errobj (get-buffer-create " *url-error*")))

(cond
 ((fboundp 'display-warning)
  (fset 'url-warn 'display-warning))
 ((fboundp 'w3-warn)
  (fset 'url-warn 'w3-warn))
 ((fboundp 'warn)
  (defun url-warn (class message &optional level)
    (warn "(%s/%s) %s" class (or level 'warning) message)))
 (t
  (defun url-warn (class message &optional level)
    (save-excursion
      (set-buffer (get-buffer-create "*W3-WARNINGS*"))
      (goto-char (point-max))
      (save-excursion
	(insert (format "(%s/%s) %s\n" class (or level 'warning) message)))
      (display-buffer (current-buffer))))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Autoload all the URL loaders
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(autoload 'url-file "url-file")
(autoload 'url-ftp "url-file")
(autoload 'url-gopher "url-gopher")
(autoload 'url-irc "url-irc")
(autoload 'url-http "url-http")
(autoload 'url-mailserver "url-mail")
(autoload 'url-mailto "url-mail")
(autoload 'url-info "url-misc")
(autoload 'url-shttp "url-http")
(autoload 'url-https "url-http")
(autoload 'url-finger "url-misc")
(autoload 'url-rlogin "url-misc")
(autoload 'url-telnet "url-misc")
(autoload 'url-tn3270 "url-misc")
(autoload 'url-proxy "url-misc")
(autoload 'url-x-exec "url-misc")
(autoload 'url-news "url-news")
(autoload 'url-nntp "url-news")
(autoload 'url-decode-pgp/pem "url-pgp")
(autoload 'url-wais "url-wais")

(autoload 'url-save-newsrc "url-news")
(autoload 'url-news-generate-reply-form "url-news")
(autoload 'url-parse-newsrc "url-news")
(autoload 'url-mime-response-p "url-http")
(autoload 'url-parse-mime-headers "url-http")
(autoload 'url-handle-refresh-header "url-http")
(autoload 'url-create-mime-request "url-http")
(autoload 'url-create-message-id "url-http")
(autoload 'url-create-multipart-request "url-http")
(autoload 'url-parse-viewer-types "url-http")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; File-name-handler-alist functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun url-setup-file-name-handlers ()
  ;; Setup file-name handlers.
  '(cond
    ((not (boundp 'file-name-handler-alist))
     nil)				; Don't load if no alist
    ((rassq 'url-file-handler file-name-handler-alist)
     nil)				; Don't load twice
    ((and (string-match "XEmacs\\|Lucid" emacs-version)
	  (< url-emacs-minor-version 11)) ; Don't load in lemacs 19.10
     nil)
    (t
     (setq file-name-handler-alist
	   (let ((new-handler (cons
			       (concat "^/*"
				       (substring url-nonrelative-link1 nil))
			       'url-file-handler)))
	     (if file-name-handler-alist
		 (append (list new-handler) file-name-handler-alist)
	       (list new-handler)))))))
  
(defun url-file-handler (operation &rest args)
  ;; Function called from the file-name-handler-alist routines.  OPERATION
  ;; is what needs to be done ('file-exists-p, etc).  args are the arguments
  ;; that would have been passed to OPERATION."
  (let ((fn (get operation 'url-file-handlers))
	(url (car args))
	(myargs (cdr args)))
    (if (= (string-to-char url) ?/)
	(setq url (substring url 1 nil)))
    (if fn (apply fn url myargs)
      (let (file-name-handler-alist)
	(apply operation url myargs)))))

(defun url-file-handler-identity (&rest args)
  (car args))

(defun url-file-handler-null (&rest args)
  nil)

(put 'file-directory-p 'url-file-handlers 'url-file-handler-null)
(put 'substitute-in-file-name 'url-file-handlers 'url-file-handler-identity)
(put 'file-writable-p 'url-file-handlers 'url-file-handler-null)
(put 'file-truename 'url-file-handlers 'url-file-handler-identity)
(put 'insert-file-contents 'url-file-handlers 'url-insert-file-contents)
(put 'expand-file-name 'url-file-handlers 'url-expand-file-name)
(put 'directory-files 'url-file-handlers 'url-directory-files)
(put 'file-directory-p 'url-file-handlers 'url-file-directory-p)
(put 'file-writable-p 'url-file-handlers 'url-file-writable-p)
(put 'file-readable-p 'url-file-handlers 'url-file-exists)
(put 'file-executable-p 'url-file-handlers 'null)
(put 'file-symlink-p 'url-file-handlers 'null)
(put 'file-exists-p 'url-file-handlers 'url-file-exists)
(put 'copy-file 'url-file-handlers 'url-copy-file)
(put 'file-attributes 'url-file-handlers 'url-file-attributes)
(put 'file-name-all-completions 'url-file-handlers
     'url-file-name-all-completions)
(put 'file-name-completion 'url-file-handlers 'url-file-name-completion)
(put 'file-local-copy 'url-file-handlers 'url-file-local-copy)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Utility functions
;;; -----------------
;;; Various functions used around the url code.
;;; Some of these qualify as hacks, but hey, this is elisp.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(if (fboundp 'mm-string-to-tokens)
    (fset 'url-string-to-tokens 'mm-string-to-tokens)
  (defun url-string-to-tokens (str &optional delim)
    "Return a list of words from the string STR"
    (setq delim (or delim ? ))
    (let (results y)
      (mapcar
       (function
	(lambda (x)
	  (cond
	   ((and (= x delim) y) (setq results (cons y results) y nil))
	   ((/= x delim) (setq y (concat y (char-to-string x))))
	   (t nil)))) str)
      (nreverse (cons y results)))))

(defun url-days-between (date1 date2)
  ;; Return the number of days between date1 and date2.
  (- (url-day-number date1) (url-day-number date2)))

(defun url-day-number (date)
  (let ((dat (mapcar (function (lambda (s) (and s (string-to-int s)) ))
		     (timezone-parse-date date))))
    (timezone-absolute-from-gregorian 
     (nth 1 dat) (nth 2 dat) (car dat))))

(defun url-seconds-since-epoch (date)
  ;; Returns a number that says how many seconds have
  ;; lapsed between Jan 1 12:00:00 1970 and DATE."
  (let* ((tdate (mapcar (function (lambda (ti) (and ti (string-to-int ti))))
			(timezone-parse-date date)))
	 (ttime (mapcar (function (lambda (ti) (and ti (string-to-int ti))))
			(timezone-parse-time
			 (aref (timezone-parse-date date) 3))))
	 (edate (mapcar (function (lambda (ti) (and ti (string-to-int ti))))
			(timezone-parse-date "Jan 1 12:00:00 1970")))
	 (tday (- (timezone-absolute-from-gregorian 
		   (nth 1 tdate) (nth 2 tdate) (nth 0 tdate))
		  (timezone-absolute-from-gregorian 
		   (nth 1 edate) (nth 2 edate) (nth 0 edate)))))
    (+ (nth 2 ttime)
       (* (nth 1 ttime) 60)
       (* (nth 0 ttime) 60 60)
       (* tday 60 60 24))))

(defun url-match (s x)
  ;; Return regexp match x in s.
  (substring s (match-beginning x) (match-end x)))

(defun url-split (str del)
  ;; Split the string STR, with DEL (a regular expression) as the delimiter.
  ;; Returns an assoc list that you can use with completing-read."
  (let (x y)
    (while (string-match del str)
      (setq y (substring str 0 (match-beginning 0))
	    str (substring str (match-end 0) nil))
      (if (not (string-match "^[ \t]+$" y))
	  (setq x (cons (list y y) x))))
    (if (not (equal str ""))
	(setq x (cons (list str str) x)))
    x))

(defun url-replace-regexp (regexp to-string)
  (goto-char (point-min))
  (while (re-search-forward regexp nil t)
    (replace-match to-string t nil)))

(defun url-clear-tmp-buffer ()
  (set-buffer (get-buffer-create url-working-buffer))
  (if buffer-read-only (toggle-read-only))
  (erase-buffer))  

(defun url-maybe-relative (url)
  (url-retrieve (url-expand-file-name url)))

(defun url-buffer-is-hypertext (&optional buff)
  "Return t if a buffer contains HTML, as near as we can guess."
  (setq buff (or buff (current-buffer)))
  (save-excursion
    (set-buffer buff)
    (let ((case-fold-search t))
      (goto-char (point-min))
      (re-search-forward
       "<\\(TITLE\\|HEAD\\|BASE\\|H[0-9]\\|ISINDEX\\|P\\)>" nil t))))

(defun nntp-after-change-function (&rest args)
  (save-excursion
    (set-buffer nntp-server-buffer)
    (message "Read %d bytes" (point-max))))

(defun url-percentage (x y)
  (if (fboundp 'float)
      (round (* 100 (/ x (float y))))
    (/ (* x 100) y)))

(defun url-after-change-function (&rest args)
  ;; The nitty gritty details of messaging the HTTP/1.0 status messages
  ;; in the minibuffer."
  (save-excursion
    (set-buffer url-working-buffer)
    (let (status-message)
      (if url-current-content-length
	  nil
	(goto-char (point-min))
	(skip-chars-forward " \t\n")
	(if (not (looking-at "HTTP/[0-9]\.[0-9]"))
	    (setq url-current-content-length 0)
	  (setq url-current-isindex
		(and (re-search-forward "$\r*$" nil t) (point)))
	  (if (re-search-forward
	       "^content-type:[ \t]*\\([^\r\n]+\\)\r*$"
	       url-current-isindex t)
	      (setq url-current-mime-type (downcase
					  (url-eat-trailing-space
					   (buffer-substring
					    (match-beginning 1)
					    (match-end 1))))))
	  (if (re-search-forward "^content-length:\\([^\r\n]+\\)\r*$"
				 url-current-isindex t)
	      (setq url-current-content-length
		    (string-to-int (buffer-substring (match-beginning 1)
						     (match-end 1))))
	    (setq url-current-content-length nil))))
      (goto-char (point-min))
      (if (re-search-forward "^status:\\([^\r]*\\)" url-current-isindex t)
	  (progn
	    (setq status-message (buffer-substring (match-beginning 1)
						   (match-end 1)))
	    (replace-match (concat "btatus:" status-message))))
      (goto-char (point-max))
      (cond
       (status-message (url-lazy-message "%s" status-message))
       ((and url-current-content-length (> url-current-content-length 1)
	     url-current-mime-type)
	(url-lazy-message "Read %d of %d bytes (%d%%) [%s]"
			 (point-max) url-current-content-length
			 (url-percentage (point-max) url-current-content-length)
			 url-current-mime-type))
       ((and url-current-content-length (> url-current-content-length 1))
	(url-lazy-message "Read %d of %d bytes (%d%%)"
			 (point-max) url-current-content-length
			 (url-percentage (point-max)
					 url-current-content-length)))
       ((and (/= 1 (point-max)) url-current-mime-type)
	(url-lazy-message "Read %d bytes. [%s]" (point-max)
			 url-current-mime-type))
       ((/= 1 (point-max))
	(url-lazy-message "Read %d bytes." (point-max)))
       (t (url-lazy-message "Waiting for response."))))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Information information
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defvar url-process-lookup-table nil)

(defun url-setup-process-get ()
  (let ((x nil)
	(nativep t))
    (condition-case ()
	(progn
	  (setq x (start-process "Test" nil "/bin/sh"))
	  (get x 'command))
      (error (setq nativep nil)))
    (cond
     ((fboundp 'process-get)		; Emacs 19.31 w/my hacks
      (defun url-process-get (proc prop &optional default)
	(or (process-get proc prop) default)))
     (nativep				; XEmacs 19.14 w/my hacks
      (fset 'url-process-get 'get))
     (t
      (defun url-process-get (proc prop &optional default)
	(or (plist-get (cdr-safe (assq proc url-process-lookup-table)) prop)
	    default))))
    (cond
     ((fboundp 'process-put)		; Emacs 19.31 w/my hacks
      (fset 'url-process-put 'process-put))
     (nativep
      (fset 'url-process-put 'put))
     (t
      (defun url-process-put (proc prop val)
	(let ((node (assq proc url-process-lookup-table)))
	  (if (not node)
	      (setq url-process-lookup-table (cons (cons proc (list prop val))
						   url-process-lookup-table))
	    (setcdr node (plist-put (cdr node) prop val)))))))
    (and (processp x) (delete-process x))))

(defun url-gc-process-lookup-table ()
  (let (new)
    (while url-process-lookup-table
      (if (not (memq (process-status (caar url-process-lookup-table))
		     '(stop closed nil)))
	  (setq new (cons (car url-process-lookup-table) new)))
      (setq url-process-lookup-table (cdr url-process-lookup-table)))
    (setq url-process-lookup-table new)))

(defun url-list-processes ()
  (interactive)
  (url-gc-process-lookup-table)
  (let ((processes (process-list))
	proc len type)
    (set-buffer (get-buffer-create "URL Status Display"))
    (display-buffer (current-buffer))
    (erase-buffer)
    (insert
     (eval-when-compile (format "%-40s%-10s%-25s" "URL" "Size" "Type")) "\n"
     (eval-when-compile (make-string 75 ?-)) "\n")
    (while processes
      (setq proc (car processes)
	    processes (cdr processes))
      (if (url-process-get proc 'url)
	  (progn
	    (save-excursion
	      (set-buffer (process-buffer proc))
	      (setq len url-current-content-length
		    type url-current-mime-type))
	    (insert
	     (format "%-40s%-10d%-25s" (url-process-get proc 'url)
		     len type)))))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; file-name-handler stuff calls this
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun url-have-visited-url (url &rest args)
  "Return non-nil iff the user has visited URL before.
The return value is a cons of the url and the date last accessed as a string"
  (url-gethash url url-global-history-hash-table))

(defun url-directory-files (url &rest args)
  "Return a list of files on a server."
  nil)

(defun url-file-writable-p (url &rest args)
  "Return t iff a url is writable by this user"
  nil)

(defun url-copy-file (url &rest args)
  "Copy a url to the specified filename."
  nil)

(defun url-file-directly-accessible-p (url)
  "Returns t iff the specified URL is directly accessible
on your filesystem.  (nfs, local file, etc)."
  (let* ((urlobj (if (vectorp url) url (url-generic-parse-url url)))
	 (type (url-type urlobj)))
    (and (member type '("file" "ftp"))
	 (not (url-host urlobj)))))

;;;###autoload
(defun url-file-attributes (url &rest args)
  "Return a list of attributes of URL.
Value is nil if specified file cannot be opened.
Otherwise, list elements are:
 0. t for directory, string (name linked to) for symbolic link, or nil.
 1. Number of links to file.
 2. File uid.
 3. File gid.
 4. Last access time, as a list of two integers.
  First integer has high-order 16 bits of time, second has low 16 bits.
 5. Last modification time, likewise.
 6. Last status change time, likewise.
 7. Size in bytes. (-1, if number is out of range).
 8. File modes, as a string of ten letters or dashes as in ls -l.
    If URL is on an http server, this will return the content-type if possible.
 9. t iff file's gid would change if file were deleted and recreated.
10. inode number.
11. Device number.

If file does not exist, returns nil."
  (and url
       (let* ((urlobj (url-generic-parse-url url))
	      (type (url-type urlobj))
	      (url-automatic-caching nil)
	      (data nil)
	      (exists nil))
	 (cond
	  ((equal type "http")
	   (cond
	    ((not url-be-anal-about-file-attributes)
	     (setq data (list
			 (url-file-directory-p url) ; Directory
			 1		; number of links to it
			 0		; UID
			 0		; GID
			 (cons 0 0)	; Last access time
			 (cons 0 0)	; Last mod. time
			 (cons 0 0)	; Last status time
			 -1		; file size
			 (mm-extension-to-mime
			  (url-file-extension (url-filename urlobj)))
			 nil		; gid would change
			 0		; inode number
			 0		; device number
			 )))
	    (t				; HTTP/1.0, use HEAD
	     (let ((url-request-method "HEAD")
		   (url-request-data nil)
		   (url-working-buffer " *url-temp*"))
	       (save-excursion
		 (condition-case ()
		     (progn
		       (url-retrieve url)
		       (setq data (and
				   (setq exists
					 (cdr
					  (assoc "status"
						 url-current-mime-headers)))
				   (>= exists 200)
				   (< exists 300)
				   (list
				    (url-file-directory-p url) ; Directory
				    1	; links to
				    0	; UID
				    0	; GID
				    (cons 0 0) ; Last access time
				    (cons 0 0) ; Last mod. time
				    (cons 0 0) ; Last status time
				    (or ; Size in bytes
				     (cdr (assoc "content-length"
						 url-current-mime-headers))
				     -1)
				    (or
				     (cdr (assoc "content-type"
						 url-current-mime-headers))
				     (mm-extension-to-mime
				      (url-file-extension
				       (url-filename urlobj)))) ; content-type
				    nil ; gid would change
				    0	; inode number
				    0	; device number
				    ))))
		   (error nil))
		 (and (not data)
		      (setq data (list (url-file-directory-p url)
				       1 0 0 (cons 0 0) (cons 0 0) (cons 0 0)
				       -1 (mm-extension-to-mime
					   (url-file-extension
					    url-current-file))
				       nil 0 0)))
		 (kill-buffer " *url-temp*"))))))
	  ((member type '("ftp" "file"))
	   (let ((fname (if (url-host urlobj)
			    (concat "/"
				    (if (url-user urlobj)
					(concat (url-user urlobj) "@")
				      "")
				    (url-host urlobj) ":"
				    (url-filename urlobj))
			  (url-filename urlobj))))
	     (setq data (or (file-attributes fname) (make-list 12 nil)))
	     (setcar (cdr (cdr (cdr (cdr (cdr (cdr (cdr (cdr data))))))))
		     (mm-extension-to-mime (url-file-extension fname)))))
	  (t nil))
	 data)))

(defun url-file-name-all-completions (file dirname &rest args)
  "Return a list of all completions of file name FILE in directory DIR.
These are all file names in directory DIR which begin with FILE."
  ;; need to rewrite
  )

(defun url-file-name-completion (file dirname &rest args)
  "Complete file name FILE in directory DIR.
Returns the longest string
common to all filenames in DIR that start with FILE.
If there is only one and FILE matches it exactly, returns t.
Returns nil if DIR contains no name starting with FILE."
  (apply 'url-file-name-all-completions file dirname args))

(defun url-file-local-copy (file &rest args)
  "Copy the file FILE into a temporary file on this machine.
Returns the name of the local copy, or nil, if FILE is directly
accessible."
  nil)

(defun url-insert-file-contents (url &rest args)
  "Insert the contents of the URL in this buffer."
  (save-excursion
    (let ((old-asynch url-be-asynchronous))
      (setq-default url-be-asynchronous nil)
      (url-retrieve url)
      (setq-default url-be-asynchronous old-asynch)))
  (insert-buffer url-working-buffer)
  (setq buffer-file-name url)
  (kill-buffer url-working-buffer))

(defun url-file-directory-p (url &rest args)
  "Return t iff a url points to a directory"
  (equal (substring url -1 nil) "/"))

(defun url-file-exists (url &rest args)
  "Return t iff a file exists."
  (let* ((urlobj (url-generic-parse-url url))
	 (type (url-type urlobj))
	 (exists nil))
    (cond
     ((equal type "http")		; use head
      (let ((url-request-method "HEAD")
	    (url-request-data nil)
	    (url-working-buffer " *url-temp*"))
	(save-excursion
	  (url-retrieve url)
	  (setq exists (or (cdr
			    (assoc "status" url-current-mime-headers)) 500))
	  (kill-buffer " *url-temp*")
	  (setq exists (and (>= exists 200) (< exists 300))))))
     ((member type '("ftp" "file"))	; file-attributes
      (let ((fname (if (url-host urlobj)
		       (concat "/"
			       (if (url-user urlobj)
				   (concat (url-user urlobj) "@")
				 "")
			       (url-host urlobj) ":"
			       (url-filename urlobj))
		     (url-filename urlobj))))
	(setq exists (file-exists-p fname))))
     (t nil))
    exists))

;;;###autoload
(defun url-normalize-url (url)
  "Return a 'normalized' version of URL.  This strips out default port
numbers, etc."
  (let (type data grok retval)
    (setq data (url-generic-parse-url url)
	  type (url-type data))
    (if (member type '("www" "about" "mailto" "mailserver" "info"))
	(setq retval url)
      (setq retval (url-recreate-url data)))
    retval))

;;;###autoload
(defun url-buffer-visiting (url)
  "Return the name of a buffer (if any) that is visiting URL."
  (setq url (url-normalize-url url))
  (let ((bufs (buffer-list))
	(found nil))
    (if (condition-case ()
	    (string-match "\\(.*\\)#" url)
	  (error nil))
	(setq url (url-match url 1)))
    (while (and bufs (not found))
      (save-excursion
	(set-buffer (car bufs))
	(setq found (if (and
			 (not (equal (buffer-name (car bufs))
				     url-working-buffer))
			 (memq major-mode '(url-mode w3-mode))
			 (equal (url-view-url t) url)) (car bufs) nil)
	      bufs (cdr bufs))))
    found))

(defun url-file-size (url &rest args)
  "Return the size of a file in bytes, or -1 if can't be determined."
  (let* ((urlobj (url-generic-parse-url url))
	 (type (url-type urlobj))
	 (size -1)
	 (data nil))
    (cond
     ((equal type "http")		; use head
      (let ((url-request-method "HEAD")
	    (url-request-data nil)
	    (url-working-buffer " *url-temp*"))
	(save-excursion
	  (url-retrieve url)
	  (setq size (or (cdr
			  (assoc "content-length" url-current-mime-headers))
			 -1))
	  (kill-buffer " *url-temp*"))))
     ((member type '("ftp" "file"))	; file-attributes
      (let ((fname (if (url-host urlobj)
		       (concat "/"
			       (if (url-user urlobj)
				   (concat (url-user urlobj) "@")
				 "")
			       (url-host urlobj) ":"
			       (url-filename urlobj))
		     (url-filename urlobj))))
	(setq data (file-attributes fname)
	      size (nth 7 data))))
     (t nil))
    (cond
     ((stringp size) (string-to-int size))
     ((integerp size) size)
     ((null size) -1)
     (t -1))))

(defun url-generate-new-buffer-name (start)
  "Create a new buffer name based on START."
  (let ((x 1)
	name)
    (if (not (get-buffer start))
	start
      (progn
	(setq name (format "%s<%d>" start x))
	(while (get-buffer name)
	  (setq x (1+ x)
		name (format "%s<%d>" start x)))
	name))))

(defun url-generate-unique-filename (&optional fmt)
  "Generate a unique filename in url-temporary-directory"
  (if (not fmt)
      (let ((base (format "url-tmp.%d" (user-real-uid)))
	    (fname "")
	    (x 0))
	(setq fname (format "%s%d" base x))
	(while (file-exists-p (expand-file-name fname url-temporary-directory))
	  (setq x (1+ x)
		fname (concat base (int-to-string x))))
	(expand-file-name fname url-temporary-directory))
    (let ((base (concat "url" (int-to-string (user-real-uid))))
	  (fname "")
	  (x 0))
      (setq fname (format fmt (concat base (int-to-string x))))
      (while (file-exists-p (expand-file-name fname url-temporary-directory))
	(setq x (1+ x)
	      fname (format fmt (concat base (int-to-string x)))))
      (expand-file-name fname url-temporary-directory))))

(defun url-lazy-message (&rest args)
  "Just like `message', but is a no-op if called more than once a second.
Will not do anything if url-show-status is nil."
  (if (or (null url-show-status)
	  (= url-lazy-message-time
	     (setq url-lazy-message-time (nth 1 (current-time)))))
      nil
    (apply 'message args)))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Gateway Support
;;; ---------------
;;; Fairly good/complete gateway support
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun url-kill-process (proc)
  "Kill the process PROC - knows about all the various gateway types,
and acts accordingly."
  (cond
   ((eq url-gateway-method 'native) (delete-process proc))
   ((eq url-gateway-method 'program) (kill-process proc))
   (t (error "Unknown url-gateway-method %s" url-gateway-method))))

(defun url-accept-process-output (proc)
  "Allow any pending output from subprocesses to be read by Emacs.
It is read into the process' buffers or given to their filter functions.
Where possible, this will not exit until some output is received from PROC,
or 1 second has elapsed."
  (accept-process-output proc 1))

(defun url-process-status (proc)
  "Return the process status of a url buffer"
  (cond
   ((memq url-gateway-method '(native ssl program)) (process-status proc))
   (t (error "Unkown url-gateway-method %s" url-gateway-method))))  

(defun url-open-stream (name buffer host service)
  "Open a stream to a host"
  (let ((tmp-gateway-method (if (and url-gateway-local-host-regexp
				     (not (eq 'ssl url-gateway-method))
				     (string-match
				      url-gateway-local-host-regexp
				      host))
				'native
			      url-gateway-method))
	(tcp-binary-process-output-services (if (stringp service)
						(list service)
					      (list service
						    (int-to-string service)))))
    (and (eq url-gateway-method 'tcp)
	 (require 'tcp)
	 (setq url-gateway-method 'native
	       tmp-gateway-method 'native))
    (cond
     ((eq tmp-gateway-method 'ssl)
      (open-ssl-stream name buffer host service))
     ((eq tmp-gateway-method 'native)
      (if url-broken-resolution
	  (setq host
		(cond
		 ((featurep 'ange-ftp) (ange-ftp-nslookup-host host))
		 ((featurep 'efs) (efs-nslookup-host host))
		 ((featurep 'efs-auto) (efs-nslookup-host host))
		 (t host))))
      (let ((max-retries url-connection-retries)
	    (cur-retries 0)
	    (retry t)
	    (errobj nil)
	    (conn nil))
	(while (and (not conn) retry)
	  (condition-case errobj
	      (setq conn (open-network-stream name buffer host service))
	    (error
	     (url-save-error errobj)
	     (save-window-excursion
	       (save-excursion
		 (switch-to-buffer-other-window " *url-error*")
		 (shrink-window-if-larger-than-buffer)
		 (goto-char (point-min))
		 (if (and (re-search-forward "in use" nil t)
			  (< cur-retries max-retries))
		     (progn
		       (setq retry t
			     cur-retries (1+ cur-retries))
		       (sleep-for 0.5))
		   (setq cur-retries 0
			 retry (funcall url-confirmation-func
					(concat "Connection to " host
						" failed, retry? "))))
		 (kill-buffer (current-buffer)))))))
	(if conn
 	    (progn
 	      (if (featurep 'mule)
		  (save-excursion
		    (set-buffer (get-buffer-create buffer))
		    (setq mc-flag nil)
		    (set-process-coding-system conn *noconv* *noconv*)))
 	      conn)
	  (error "Unable to connect to %s:%s" host service))))
     ((eq tmp-gateway-method 'program)
      (let ((proc (start-process name buffer url-gateway-telnet-program host
				 (int-to-string service)))
	    (tmp nil))
	(save-excursion
	  (set-buffer buffer)
	  (setq tmp (point))
	  (while (not (progn
			(goto-char (point-min))
			(re-search-forward 
			 url-gateway-telnet-ready-regexp nil t)))
	    (url-accept-process-output proc))
	  (delete-region tmp (point))
	  (goto-char (point-min))
	  (if (re-search-forward "connect:" nil t)
	      (progn
		(condition-case ()
		    (delete-process proc)
		  (error nil))
		(url-replace-regexp ".*connect:.*" "")
		nil)
	    proc))))
     (t (error "Unknown url-gateway-method %s" url-gateway-method)))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Miscellaneous functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun url-setup-privacy-info ()
  (interactive)
  (setq url-system-type
	(cond
	 ((or (eq url-privacy-level 'paranoid)
	      (and (listp url-privacy-level)
		   (memq 'os url-privacy-level)))
	  nil)
	 ((eq system-type 'Apple-Macintosh) "Macintosh")
	 ((eq system-type 'next-mach) "NeXT")
	 ((eq system-type 'windows-nt) "Windows-NT; 32bit")
	 ((eq system-type 'ms-windows) "Windows; 16bit")
	 ((eq system-type 'ms-dos) "MS-DOS; 32bit")
	 ((and (eq system-type 'vax-vms) (device-type))
	  "VMS; X11")
	 ((eq system-type 'vax-vms) "VMS; TTY")
	 ((eq (device-type) 'x) "X11")
	 ((eq (device-type) 'ns) "NeXTStep")
	 ((eq (device-type) 'pm) "OS/2")
	 ((eq (device-type) 'win32) "Windows; 32bit")
	 ((eq (device-type) 'tty) "(Unix?); TTY")
	 (t "UnkownPlatform")))

  ;; Set up the entity definition for PGP and PEM authentication
  (setq url-pgp/pem-entity (or url-pgp/pem-entity
			       user-mail-address
			       (format "%s@%s"  (user-real-login-name)
				       (system-name))))
  
  (setq url-personal-mail-address (or url-personal-mail-address
				      url-pgp/pem-entity
				      user-mail-address))

  (if (or (memq url-privacy-level '(paranoid high))
	  (and (listp url-privacy-level)
	       (memq 'email url-privacy-level)))
      (setq url-personal-mail-address nil))

  (if (or (eq url-privacy-level 'paranoid)
	  (and (listp url-privacy-level)
	       (memq 'os url-privacy-level)))
      (setq url-os-type nil)
    (let ((vers (emacs-version)))
      (if (string-match "(\\([^, )]+\\))$" vers)
	  (setq url-os-type (url-match vers 1))
	(setq url-os-type (symbol-name system-type))))))

(defun url-handle-no-scheme (url)
  (let ((temp url-registered-protocols)
	(found nil))
    (while (and temp (not found))
      (if (and (not (member (car (car temp)) '("auto" "www")))
	       (string-match (concat "^" (car (car temp)) "\\.")
					url))
	  (setq found t)
	(setq temp (cdr temp))))
    (cond
     (found				; Found something like ftp.spry.com
      (url-retrieve (concat (car (car temp)) "://" url)))
     ((string-match "^www\\." url)
      (url-retrieve (concat "http://" url)))
     ((string-match "\\(\\.[^\\.]+\\)\\(\\.[^\\.]+\\)" url)
      ;; Ok, we have at least two dots in the filename, just stick http on it
      (url-retrieve (concat "http://" url)))
     (t
      (url-retrieve (concat "http://www." url ".com"))))))

(defun url-setup-save-timer ()
  "Reset the history list timer."
  (interactive)
  (cond
   ((featurep 'itimer)
    (if (get-itimer "url-history-saver")
	(delete-itimer (get-itimer "url-history-saver")))
    (start-itimer "url-history-saver" 'url-write-global-history
		  url-global-history-save-interval
		  url-global-history-save-interval))
   ((fboundp 'run-at-time)
    (run-at-time url-global-history-save-interval
		 url-global-history-save-interval
		 'url-write-global-history))
   (t nil)))

(defvar url-download-minor-mode nil)

(defun url-download-minor-mode (on)
  (setq url-download-minor-mode (if on
				   (1+ (or url-download-minor-mode 0))
				 (1- (or url-download-minor-mode 1))))
  (if (<= url-download-minor-mode 0)
      (setq url-download-minor-mode nil)))

(defun url-do-setup ()
  "Do setup - this is to avoid conflict with user settings when URL is
dumped with emacs."
  (if url-setup-done
      nil

    (add-minor-mode 'url-download-minor-mode " Webbing" nil)
    ;; Decide what type of process-get to use
    ;(url-setup-process-get)
    
    ;; Make OS/2 happy
    (setq tcp-binary-process-input-services
	  (append '("http" "80")
		  tcp-binary-process-input-services))
    
    ;; Register all the protocols we can handle
    (url-register-protocol 'file)
    (url-register-protocol 'ftp        nil nil "21")
    (url-register-protocol 'gopher     nil nil "70")
    (url-register-protocol 'http       nil nil "80")
    (url-register-protocol 'https      nil nil "443")
    (url-register-protocol 'nfs        nil nil "2049")
    (url-register-protocol 'info       nil 'url-identity-expander)
    (url-register-protocol 'mailserver nil 'url-identity-expander)
    (url-register-protocol 'finger     nil 'url-identity-expander "79")
    (url-register-protocol 'mailto     nil 'url-identity-expander)
    (url-register-protocol 'news       nil 'url-identity-expander "119")
    (url-register-protocol 'nntp       nil 'url-identity-expander "119")
    (url-register-protocol 'irc        nil 'url-identity-expander "6667")
    (url-register-protocol 'rlogin)
    (url-register-protocol 'shttp      nil nil "80")
    (url-register-protocol 'telnet)
    (url-register-protocol 'tn3270)
    (url-register-protocol 'wais)
    (url-register-protocol 'x-exec)
    (url-register-protocol 'proxy)
    (url-register-protocol 'auto 'url-handle-no-scheme)

    ;; Register all the authentication schemes we can handle
    (url-register-auth-scheme "basic" nil 4)
    (url-register-auth-scheme "digest" nil 7)

    ;; Filename handler stuff for emacsen that support it
    (url-setup-file-name-handlers)

    (setq url-cookie-file
	  (or url-cookie-file
	      (expand-file-name "~/.w3cookies")))
    
    (setq url-global-history-file
	  (or url-global-history-file
	      (and (memq system-type '(ms-dos ms-windows))
		   (expand-file-name "~/mosaic.hst"))
	      (and (memq system-type '(axp-vms vax-vms))
		   (expand-file-name "~/mosaic.global-history"))
	      (condition-case ()
		  (expand-file-name "~/.mosaic-global-history")
		(error nil))))
  
    ;; Parse the global history file if it exists, so that it can be used
    ;; for URL completion, etc.
    (if (and url-global-history-file
	     (file-exists-p url-global-history-file))
	(url-parse-global-history))

    ;; Setup save timer
    (and url-global-history-save-interval (url-setup-save-timer))

    (if (and url-cookie-file
	     (file-exists-p url-cookie-file))
	(url-cookie-parse-file url-cookie-file))
    
    ;; Read in proxy gateways
    (let ((noproxy (and (not (assoc "no_proxy" url-proxy-services))
			(or (getenv "NO_PROXY")
			    (getenv "no_PROXY")
			    (getenv "no_proxy")))))
      (if noproxy
	  (setq url-proxy-services
		(cons (cons "no_proxy"
			    (concat "\\("
				    (mapconcat
				     (function
				      (lambda (x)
					(cond
					 ((= x ?,) "\\|")
					 ((= x ? ) "")
					 ((= x ?.) (regexp-quote "."))
					 ((= x ?*) ".*")
					 ((= x ??) ".")
					 (t (char-to-string x)))))
				     noproxy "") "\\)"))
		      url-proxy-services))))

    ;; Set the url-use-transparent with decent defaults
    (if (not (eq (device-type) 'tty))
	(setq url-use-transparent nil))
    (and url-use-transparent (require 'transparent))
  
    ;; Set the password entry funtion based on user defaults or guess
    ;; based on which remote-file-access package they are using.
    (cond
     (url-passwd-entry-func nil)	; Already been set
     ((boundp 'read-passwd)		; Use secure password if available
      (setq url-passwd-entry-func 'read-passwd))
     ((or (featurep 'efs)		; Using EFS
	  (featurep 'efs-auto))		; or autoloading efs
      (if (not (fboundp 'read-passwd))
	  (autoload 'read-passwd "passwd" "Read in a password" nil))
      (setq url-passwd-entry-func 'read-passwd))
     ((or (featurep 'ange-ftp)		; Using ange-ftp
	  (and (boundp 'file-name-handler-alist)
	       (not (string-match "Lucid" (emacs-version)))))
      (setq url-passwd-entry-func 'ange-ftp-read-passwd))
     (t
      (url-warn 'security
		"Can't determine how to read passwords, winging it.")))
  
    ;; Set up the news service if they haven't done so
    (setq url-news-server
	  (cond
	   (url-news-server url-news-server)
	   ((and (boundp 'gnus-default-nntp-server)
		 (not (equal "" gnus-default-nntp-server)))
	    gnus-default-nntp-server)
	   ((and (boundp 'gnus-nntp-server)
		 (not (null gnus-nntp-server))
		 (not (equal "" gnus-nntp-server)))
	    gnus-nntp-server)
	   ((and (boundp 'nntp-server-name)
		 (not (null nntp-server-name))
		 (not (equal "" nntp-server-name)))
	    nntp-server-name)
	   ((getenv "NNTPSERVER") (getenv "NNTPSERVER"))
	   (t "news")))
  
    ;; Set up the MIME accept string if they haven't got it hardcoded yet
    (or url-mime-accept-string
	(setq url-mime-accept-string (url-parse-viewer-types)))
    (or url-mime-encoding-string
	(setq url-mime-encoding-string
	      (mapconcat 'car
			 mm-content-transfer-encodings
			 ", ")))
  
    (url-setup-privacy-info)
    (run-hooks 'url-load-hook)
    (setq url-setup-done t)))

(defun url-cache-file-writable-p (file)
  "Follows the documentation of file-writable-p, unlike file-writable-p."
  (and (file-writable-p file)
       (if (file-exists-p file)
           (not (file-directory-p file))
         (file-directory-p (file-name-directory file)))))
                
(defun url-prepare-cache-for-file (file)
  "Makes it possible to cache data in FILE.
Creates any necessary parent directories, deleting any non-directory files
that would stop this.  Returns nil if parent directories can not be
created.  If FILE already exists as a non-directory, it changes
permissions of FILE or deletes FILE to make it possible to write a new
version of FILE.  Returns nil if this can not be done.  Returns nil if
FILE already exists as a directory.  Otherwise, returns t, indicating that
FILE can be created or overwritten."

  ;; COMMENT: We don't delete directories because that requires
  ;; recursively deleting the directories's contents, which might
  ;; eliminate a substantial portion of the cache.

  (cond
   ((url-cache-file-writable-p file)
    t)
   ((file-directory-p file)
    nil)
   (t
    (catch 'upcff-tag
      (let ((dir (file-name-directory file))
            dir-parent dir-last-component)
        (if (string-equal dir file)
            ;; *** Should I have a warning here?
            ;; FILE must match a pattern like /foo/bar/, indicating it is a
            ;; name only suitable for a directory.  So presume we won't be
            ;; able to overwrite FILE and return nil.
            (throw 'upcff-tag nil))
        
        ;; Make sure the containing directory exists, or throw a failure
        ;; if we can't create it.
        (if (file-directory-p dir)
            nil
          (or (fboundp 'make-directory)
              (throw 'upcff-tag nil))
          (make-directory dir t)
          ;; make-directory silently fails if there is an obstacle, so
          ;; we must verify its results.
          (if (file-directory-p dir)
              nil
            ;; Look at prefixes of the path to find the obstacle that is
            ;; stopping us from making the directory.  Unfortunately, there
            ;; is no portable function in Emacs to find the parent directory
            ;; of a *directory*.  So this code may not work on VMS.
            (while (progn
                     (if (eq ?/ (aref dir (1- (length dir))))
                         (setq dir (substring dir 0 -1))
                       ;; Maybe we're on VMS where the syntax is different.
                       (throw 'upcff-tag nil))
                     (setq dir-parent (file-name-directory dir))
                     (not (file-directory-p dir-parent)))
              (setq dir dir-parent))
            ;; We have found the longest path prefix that exists as a
            ;; directory.  Deal with any obstacles in this directory.
            (if (file-exists-p dir)
                (condition-case nil
                    (delete-file dir)
                  (error (throw 'upcff-tag nil))))
            (if (file-exists-p dir)
                (throw 'upcff-tag nil))
            ;; Try making the directory again.
            (setq dir (file-name-directory file))
            (make-directory dir t)
            (or (file-directory-p dir)
                (throw 'upcff-tag nil))))

        ;; The containing directory exists.  Let's see if there is
        ;; something in the way in this directory.
        (if (url-cache-file-writable-p file)
            (throw 'upcff-tag t)
          (condition-case nil
              (delete-file file)
            (error (throw 'upcff-tag nil))))

        ;; The return value, if we get this far.
        (url-cache-file-writable-p file))))))
       
(defun url-store-in-cache (&optional buff)
  "Store buffer BUFF in the cache"
  (if (or (not (get-buffer buff))
	  (member url-current-type '("www" "about" "https" "shttp"
					 "news" "mailto"))
	  (and (member url-current-type '("file" "ftp" nil))
	       (not url-current-server))
	  )
      nil
    (save-excursion
      (and buff (set-buffer buff))
      (let* ((fname (url-create-cached-filename (url-view-url t)))
             (fname-hdr (concat (if (memq system-type '(ms-windows ms-dos os2))
                                    (url-file-extension fname t)
                                  fname) ".hdr"))
	     (info (mapcar (function (lambda (var)
				       (cons (symbol-name var)
					     (symbol-value var))))
			   '( url-current-content-length
			      url-current-file
			      url-current-isindex
			      url-current-mime-encoding
			      url-current-mime-headers
			      url-current-mime-type
			      url-current-port
			      url-current-server
			      url-current-type
			      url-current-user
			      ))))
	(cond ((and (url-prepare-cache-for-file fname)
		    (url-prepare-cache-for-file fname-hdr))
	       (write-region (point-min) (point-max) fname nil 5)
	       (set-buffer (get-buffer-create " *cache-tmp*"))
	       (erase-buffer)
	       (insert "(setq ")
	       (mapcar
		(function
		 (lambda (x)
		   (insert (car x) " "
			   (cond ((null (setq x (cdr x))) "nil")
				 ((stringp x) (prin1-to-string x))
				 ((listp x) (concat "'" (prin1-to-string x)))
				 ((numberp x) (int-to-string x))
				 (t "'???")) "\n")))
		info)
	       (insert ")\n")
	       (write-region (point-min) (point-max) fname-hdr nil 5)))))))
	
	     
(defun url-is-cached (url)
  "Return non-nil if the URL is cached."
  (let* ((fname (url-create-cached-filename url))
	 (attribs (file-attributes fname)))
    (and fname				; got a filename
	 (file-exists-p fname)		; file exists
	 (not (eq (nth 0 attribs) t))	; Its not a directory
	 (nth 5 attribs))))		; Can get last mod-time
    
(defun url-create-cached-filename-using-md5 (url)
  (if url
      (expand-file-name (md5 url)
			(concat url-temporary-directory "/"
				(user-real-login-name)))))

(defun url-create-cached-filename (url)
  "Return a filename in the local cache for URL"
  (if url
      (let* ((url url)
	     (urlobj (if (vectorp url)
			 url
		       (url-generic-parse-url url)))
	     (protocol (url-type urlobj))
	     (hostname (url-host urlobj))
	     (host-components
	      (cons
	       (user-real-login-name)
	       (cons (or protocol "file")
		     (nreverse
		      (delq nil
			    (mm-string-to-tokens
			     (or hostname "localhost") ?.))))))
	     (fname    (url-filename urlobj)))
	(if (and fname (/= (length fname) 0) (= (aref fname 0) ?/))
	    (setq fname (substring fname 1 nil)))
	(if fname
	    (let ((slash nil))
	      (setq fname
		    (mapconcat
		     (function
		      (lambda (x)
			(cond
			 ((and (= ?/ x) slash)
			  (setq slash nil)
			  "%2F")
			 ((= ?/ x)
			  (setq slash t)
			  "/")
			 (t
			  (setq slash nil)
			  (char-to-string x))))) fname ""))))

	(if (and fname (memq system-type '(ms-windows ms-dos windows-nt))
		 (string-match "\\([A-Za-z]\\):[/\\]" fname))
	    (setq fname (concat (url-match fname 1) "/"
				(substring fname (match-end 0)))))
	
	(setq fname (and fname
			 (mapconcat
			  (function (lambda (x)
				      (if (= x ?~) "" (char-to-string x))))
			  fname ""))
	      fname (cond
		     ((null fname) nil)
		     ((or (string= "" fname) (string= "/" fname))
		      url-directory-index-file)
		     ((= (string-to-char fname) ?/)
		      (if (string= (substring fname -1 nil) "/")
			  (concat fname url-directory-index-file)
			(substring fname 1 nil)))
		     (t
		      (if (string= (substring fname -1 nil) "/")
			  (concat fname url-directory-index-file)
			fname))))

	;; Honor hideous 8.3 filename limitations on dos and windows
	;; we don't have to worry about this in Windows NT/95 (or OS/2?)
	(if (and fname (memq system-type '(ms-windows ms-dos)))
	    (let ((base (url-file-extension fname t))
		  (ext  (url-file-extension fname nil)))
	      (setq fname (concat (substring base 0 (min 8 (length base)))
				  (substring ext  0 (min 4 (length ext)))))
	      (setq host-components
		    (mapcar
		     (function
		      (lambda (x)
			(if (> (length x) 8)
			    (concat 
			     (substring x 0 8) "."
			     (substring x 8 (min (length x) 11)))
			  x)))
		     host-components))))

	(and fname
	     (expand-file-name fname
			       (expand-file-name
				(mapconcat 'identity host-components "/")
				url-temporary-directory))))))

(defun url-extract-from-cache (fnam)
  "Extract FNAM from the local disk cache"
  (set-buffer (get-buffer-create url-working-buffer))
  (erase-buffer)
  (setq url-current-mime-viewer nil)
  (insert-file-contents-literally fnam)
  (load (concat (if (memq system-type '(ms-windows ms-dos os2))
		    (url-file-extension fnam t)
		  fnam) ".hdr") t t)) 

;;;###autoload
(defun url-get-url-at-point (&optional pt)
  "Get the URL closest to point, but don't change your
position. Has a preference for looking backward when not
directly on a symbol."
  ;; Not at all perfect - point must be right in the name.
  (save-excursion
    (if pt (goto-char pt))
    (let ((filename-chars "%.?@a-zA-Z0-9---()_/:~=&") start url)
      (save-excursion
	;; first see if you're just past a filename
	(if (not (eobp))
	    (if (looking-at "[] \t\n[{}()]") ; whitespace or some parens
		(progn
		  (skip-chars-backward " \n\t\r({[]})")
		  (if (not (bobp))
		      (backward-char 1)))))
	(if (string-match (concat "[" filename-chars "]")
			  (char-to-string (following-char)))
	    (progn
	      (skip-chars-backward filename-chars)
	      (setq start (point))
	      (skip-chars-forward filename-chars))
	  (setq start (point)))
	(setq url (if (fboundp 'buffer-substring-no-properties)
		      (buffer-substring-no-properties start (point))
		    (buffer-substring start (point)))))
      (if (string-match "^URL:" url)
	  (setq url (substring url 4 nil)))
      (if (string-match "\\.$" url)
	  (setq url (substring url 0 -1)))
      (if (not (string-match url-nonrelative-link url))
	  (setq url nil))
      url)))

(defun url-eat-trailing-space (x)
  ;; Remove spaces/tabs at the end of a string
  (let ((y (1- (length x)))
	(skip-chars (list ?  ?\t ?\n)))
    (while (and (>= y 0) (memq (aref x y) skip-chars))
      (setq y (1- y)))
    (substring x 0 (1+ y))))

(defun url-strip-leading-spaces (x)
  ;; Remove spaces at the front of a string
  (let ((y (1- (length x)))
	(z 0)
	(skip-chars (list ?  ?\t ?\n)))
    (while (and (<= z y) (memq (aref x z) skip-chars))
      (setq z (1+ z)))
    (substring x z nil)))

(defun url-convert-newlines-to-spaces (x)
  "Convert newlines and carriage returns embedded in a string into spaces,
and swallow following whitespace.
The argument is not side-effected, but may be returned by this function."
  (if (string-match "[\n\r]+\\s-*" x)   ; [\\n\\r\\t ]
      (concat (substring x 0 (match-beginning 0)) " "
	      (url-convert-newlines-to-spaces
	       (substring x (match-end 0))))
    x))

;; Test cases
;; (url-convert-newlines-to-spaces "foo    bar")  ; nothing happens
;; (url-convert-newlines-to-spaces "foo\n  \t  bar") ; whitespace converted
;;
;; This implementation doesn't mangle the match-data, is fast, and doesn't
;; create garbage, but it leaves whitespace.
;; (defun url-convert-newlines-to-spaces (x)
;;   "Convert newlines and carriage returns embedded in a string into spaces.
;; The string is side-effected, then returned."
;;   (let ((i 0)
;;      (limit (length x)))
;;     (while (< i limit)
;;       (if (or (= ?\n (aref x i))
;;            (= ?\r (aref x i)))
;;        (aset x i ? ))
;;       (setq i (1+ i)))
;;     x))

(defun url-expand-file-name (url &optional default)
  "Convert URL to a fully specified URL, and canonicalize it.
Second arg DEFAULT is a URL to start with if URL is relative.
If DEFAULT is nil or missing, the current buffer's URL is used.
Path components that are `.' are removed, and 
path components followed by `..' are removed, along with the `..' itself."
  (if url
      (setq url (mapconcat (function (lambda (x)
				       (if (= x ?\n) "" (char-to-string x))))
			   (url-strip-leading-spaces
			    (url-eat-trailing-space url)) "")))
  (cond
   ((null url) nil)			; Something hosed!  Be graceful
   ((string-match "^#" url)		; Offset link, use it raw
    url)
   (t
    (let* ((urlobj (url-generic-parse-url url))
	   (inhibit-file-name-handlers t)
	   (defobj (cond
		    ((vectorp default) default)
		    (default (url-generic-parse-url default))
		    ((and (null default) url-current-object)
		     url-current-object)
		    (t (url-generic-parse-url (url-view-url t)))))
	   (expander (cdr-safe
		      (cdr-safe
		       (assoc (or (url-type urlobj)
				  (url-type defobj))
			      url-registered-protocols)))))
      (if (string-match "^//" url)
	  (setq urlobj (url-generic-parse-url (concat (url-type defobj) ":"
						      url))))
      (if (fboundp expander)
	  (funcall expander urlobj defobj)
	(message "Unknown URL scheme: %s" (or (url-type urlobj)
					     (url-type defobj)))
	(url-identity-expander urlobj defobj))
      (url-recreate-url urlobj)))))

(defun url-default-expander (urlobj defobj)
  ;; The default expansion routine - urlobj is modified by side effect!
  (url-set-type urlobj (or (url-type urlobj) (url-type defobj)))
  (url-set-port urlobj (or (url-port urlobj)
			   (and (string= (url-type urlobj)
					 (url-type defobj))
				(url-port defobj))))
  (if (not (string= "file" (url-type urlobj)))
      (url-set-host urlobj (or (url-host urlobj) (url-host defobj))))
  (if (string= "ftp"  (url-type urlobj))
      (url-set-user urlobj (or (url-user urlobj) (url-user defobj))))
  (if (string= (url-filename urlobj) "")
      (url-set-filename urlobj "/"))
  (if (string-match "^/" (url-filename urlobj))
      nil
    (url-set-filename urlobj
		      (url-remove-relative-links
		       (concat (url-basepath (url-filename defobj))
			       (url-filename urlobj))))))

(defun url-identity-expander (urlobj defobj)
  (url-set-type urlobj (or (url-type urlobj) (url-type defobj))))

(defun url-hexify-string (str)
  "Escape characters in a string"
  (if (and (featurep 'mule) str)
      (setq str (code-convert-string 
 		 str *internal* url-mule-retrieval-coding-system)))
  (setq str (mapconcat
	     (function
	      (lambda (char)
		(if (or (> char ?z)
			(< char ?-)
			(and (< char ?a)
			     (> char ?Z))
			(and (< char ?@)
			     (> char ?:)))
		    (if (< char 16)
			(upcase (format "%%0%x" char))
		      (upcase (format "%%%x" char)))
		  (char-to-string char)))) str "")))

(defun url-make-sequence (start end)
  "Make a sequence (list) of numbers from START to END"
  (cond
   ((= start end) '())
   ((> start end) '())
   (t
    (let ((sqnc '()))
      (while (<= start end)
	(setq sqnc (cons end sqnc)
	      end (1- end)))
      sqnc))))
 
(defun url-file-extension (fname &optional x)
  "Return the filename extension of FNAME.  If optional variable X is t,
then return the basename of the file with the extension stripped off."
  (if (and fname (string-match "\\.[^./]+$" fname))
      (if x (substring fname 0 (match-beginning 0))
	(substring fname (match-beginning 0) nil))
    ;;
    ;; If fname has no extension, and x then return fname itself instead of 
    ;; nothing. When caching it allows the correct .hdr file to be produced
    ;; for filenames without extension.
    ;;
    (if x
 	fname
      "")))

(defun url-basepath (file &optional x)
  "Return the base pathname of FILE, or the actual filename if X is true"
  (cond
   ((null file) "")
   (x (file-name-nondirectory file))
   (t (file-name-directory file))))

(defun url-unhex (x)
  (if (> x ?9)
      (if (>= x ?a)
	  (+ 10 (- x ?a))
	(+ 10 (- x ?A)))
    (- x ?0)))

(defun url-unhex-string (str)
  "Remove %XXX embedded spaces, etc in a url"
  (setq str (or str ""))
  (let ((tmp ""))
    (while (string-match "%[0-9a-f][0-9a-f]" str)
      (let* ((start (match-beginning 0))
	     (ch1 (url-unhex (elt str (+ start 1))))
	     (code (+ (* 16 ch1)
		      (url-unhex (elt str (+ start 2))))))
	(setq tmp
	      (concat 
	       tmp (substring str 0 start)
	       (if (or (= code ?\n) (= code ?\r)) " " (char-to-string code)))
	      str (substring str (match-end 0)))))
    (setq tmp (concat tmp str))
    tmp))

(defun url-clean-text ()
  "Clean up a buffer, removing any excess garbage from a gateway mechanism,
and decoding any MIME content-transfer-encoding used."
  (set-buffer url-working-buffer)
  (goto-char (point-min))
  (url-replace-regexp "Connection closed by.*" "")
  (goto-char (point-min))
  (url-replace-regexp "Process WWW.*" ""))

(defun url-uncompress ()
  "Do any necessary uncompression on `url-working-buffer'"
  (set-buffer url-working-buffer)
  (if (not url-inhibit-uncompression)
      (let* ((extn (url-file-extension url-current-file))
	     (decoder nil)
	     (code-1 (cdr-safe
		      (assoc "content-transfer-encoding"
			     url-current-mime-headers)))
	     (code-2 (cdr-safe
		      (assoc "content-encoding" url-current-mime-headers)))
	     (code-3 (and (not code-1) (not code-2)
			  (cdr-safe (assoc extn url-uncompressor-alist))))
	     (done nil)
	     (default-process-coding-system
	       (if (featurep 'mule) (cons *noconv* *noconv*))))
	(mapcar
	 (function
	  (lambda (code)
	    (setq decoder (and (not (member code done))
			       (cdr-safe
				(assoc code mm-content-transfer-encodings)))
		  done (cons code done))
	    (cond
	     ((null decoder) nil)
	     ((stringp decoder)
	      (message "Decoding...")
	      (call-process-region (point-min) (point-max) decoder t t nil)
	      (message "Decoding... done."))
	     ((listp decoder)
	      (apply 'call-process-region (point-min) (point-max)
		     (car decoder) t t nil (cdr decoder)))
	     ((and (symbolp decoder) (fboundp decoder))
	      (message "Decoding...")
	      (funcall decoder (point-min) (point-max))
	      (message "Decoding... done."))
	     (t
	      (error "Bad entry for %s in `mm-content-transfer-encodings'"
		     code)))))
	 (list code-1 code-2 code-3))))
  (set-buffer-modified-p nil))

(defun url-filter (proc string)
  (save-excursion
    (set-buffer url-working-buffer)
    (insert string)
    (if (string-match "\nConnection closed by" string)
	(progn (set-process-filter proc nil)
	       (url-sentinel proc string))))
  string)

(defun url-default-callback (buf)
  (url-download-minor-mode nil)
  (cond
   ((save-excursion (set-buffer buf)
		    (and url-current-callback-func
			 (fboundp url-current-callback-func)))
    (save-excursion
      (save-window-excursion
	(set-buffer buf)
	(cond
	 ((listp url-current-callback-data)
	  (apply url-current-callback-func
		 url-current-callback-data))
	 (url-current-callback-data
	  (funcall url-current-callback-func
		   url-current-callback-data))
	 (t
	  (funcall url-current-callback-func))))))
   ((fboundp 'w3-sentinel)
    (set-variable 'w3-working-buffer buf)
    (w3-sentinel))
   (t
    (message "Retrieval for %s complete." buf))))

(defun url-sentinel (proc string)
  (if (buffer-name (process-buffer proc))
      (save-excursion
	(set-buffer (get-buffer (process-buffer proc)))
	(remove-hook 'after-change-functions 'url-after-change-function)
	(let ((status nil)
	      (url-working-buffer (current-buffer)))
	  (if url-be-asynchronous
	      (progn
		(widen)
		(url-clean-text)
		(cond
		 ((and (null proc) (not (get-buffer url-working-buffer))) nil)
		 ((url-mime-response-p)
		  (setq status (url-parse-mime-headers))))
		(if (not url-current-mime-type)
		    (setq url-current-mime-type (mm-extension-to-mime
						 (url-file-extension
						  url-current-file))))))
	  (if (member status '(401 301 302 303 204))
	      nil
	    (funcall url-default-retrieval-proc (buffer-name)))))
    (url-warn 'url (format "Process %s completed with no buffer!" proc))))

(defun url-remove-relative-links (name)
  ;; Strip . and .. from pathnames
  (let ((new (if (not (string-match "^/" name))
		 (concat "/" name)
	       name)))
    (while (string-match "/\\([^/]*/\\.\\./\\)" new)
      (setq new (concat (substring new 0 (match-beginning 1))
			(substring new (match-end 1)))))
    (while (string-match "/\\(\\./\\)" new)
      (setq new (concat (substring new 0 (match-beginning 1))
			(substring new (match-end 1)))))
    (while (string-match "^/\\.\\.\\(/\\)" new)
      (setq new (substring new (match-beginning 1) nil)))
    new))

(defun url-truncate-url-for-viewing (url &optional width)
  "Return a shortened version of URL that is WIDTH characters or less wide.
WIDTH defaults to the current frame width."
  (let* ((fr-width (or width (frame-width)))
	 (str-width (length url))
	 (tail (file-name-nondirectory url))
	 (fname nil)
	 (modified 0)
	 (urlobj nil))
    ;; The first thing that can go are the search strings
    (if (and (>= str-width fr-width)
	     (string-match "?" url))
	(setq url (concat (substring url 0 (match-beginning 0)) "?...")
	      str-width (length url)
	      tail (file-name-nondirectory url)))
    (if (< str-width fr-width)
	nil				; Hey, we are done!
      (setq urlobj (url-generic-parse-url url)
	    fname (url-filename urlobj)
	    fr-width (- fr-width 4))
      (while (and (>= str-width fr-width)
		  (string-match "/" fname))
	(setq fname (substring fname (match-end 0) nil)
	      modified (1+ modified))
	(url-set-filename urlobj fname)
	(setq url (url-recreate-url urlobj)
	      str-width (length url)))
      (if (> modified 1)
	  (setq fname (concat "/.../" fname))
	(setq fname (concat "/" fname)))
      (url-set-filename urlobj fname)
      (setq url (url-recreate-url urlobj)))
    url))

(defun url-view-url (&optional no-show)
  "View the current document's URL.  Optional argument NO-SHOW means
just return the URL, don't show it in the minibuffer."
  (interactive)
  (let ((url ""))
    (cond
     ((equal url-current-type "gopher")
      (setq url (format "%s://%s%s/%s"
			url-current-type url-current-server
			(if (or (null url-current-port)
				(string= "70" url-current-port)) ""
			  (concat ":" url-current-port))
			url-current-file)))
     ((equal url-current-type "news")
      (setq url (concat "news:"
			(if (not (equal url-current-server
					url-news-server))
			    (concat "//" url-current-server
				    (if (or (null url-current-port)
					    (string= "119" url-current-port))
					""
				      (concat ":" url-current-port)) "/"))
			url-current-file)))
     ((equal url-current-type "about")
      (setq url (concat "about:" url-current-file)))
     ((member url-current-type '("http" "shttp" "https"))
      (setq url (format  "%s://%s%s/%s" url-current-type url-current-server
			 (if (or (null url-current-port)
				 (string= "80" url-current-port))
			     ""
			   (concat ":" url-current-port))
			 (if (and url-current-file
				  (= ?/ (string-to-char url-current-file)))
			     (substring url-current-file 1 nil)
			   url-current-file))))
     ((equal url-current-type "ftp")
      (setq url (format "%s://%s%s/%s" url-current-type
			(if (and url-current-user
				 (not (string= "anonymous" url-current-user)))
			    (concat url-current-user "@") "")
			url-current-server
			(if (and url-current-file
				 (= ?/ (string-to-char url-current-file)))
			    (substring url-current-file 1 nil)
			  url-current-file))))
     ((and (member url-current-type '("file" nil)) url-current-file)
      (setq url (format "file:%s" url-current-file)))
     ((equal url-current-type "www")
      (setq url (format "www:/%s/%s" url-current-server url-current-file)))
     (t
      (setq url nil)))
    (if (not no-show) (message "%s" url) url)))

(defun url-parse-Netscape-history (fname)
  ;; Parse a Netscape/X style global history list.
  (let (pos				; Position holder
	url				; The URL
	time)				; Last time accessed
    (goto-char (point-min))
    (skip-chars-forward "^\n")
    (skip-chars-forward "\n \t")	; Skip past the tag line
    (setq url-global-history-hash-table (url-make-hashtable 131))
    ;; Here we will go to the end of the line and
    ;; skip back over a token, since we might run
    ;; into spaces in URLs, depending on how much
    ;; smarter netscape is than the old XMosaic :)
    (while (not (eobp))
      (setq pos (point))
      (end-of-line)
      (skip-chars-backward "^ \t")
      (skip-chars-backward " \t")
      (setq url (buffer-substring pos (point))
	    pos (1+ (point)))
      (skip-chars-forward "^\n")
      (setq time (buffer-substring pos (point)))
      (skip-chars-forward "\n")
      (setq url-history-changed-since-last-save t)
      (url-puthash url time url-global-history-hash-table))))

(defun url-parse-Mosaic-history-v1 (fname)
  ;; Parse an NCSA Mosaic/X style global history list
  (goto-char (point-min))
  (skip-chars-forward "^\n")
  (skip-chars-forward "\n \t")	; Skip past the tag line
  (skip-chars-forward "^\n")
  (skip-chars-forward "\n \t")	; Skip past the second tag line
  (setq url-global-history-hash-table (url-make-hashtable 131))
  (let (pos				; Temporary position holder
	bol				; Beginning-of-line
	url				; URL
	time				; Time
	last-end			; Last ending point
	)
    (while (not (eobp))
      (setq bol (point))
      (end-of-line)
      (setq pos (point)
	    last-end (point))
      (skip-chars-backward "^ \t" bol)	; Skip over year
      (skip-chars-backward " \t" bol)
      (skip-chars-backward "^ \t" bol)	; Skip over time
      (skip-chars-backward " \t" bol)
      (skip-chars-backward "^ \t" bol)	; Skip over day #
      (skip-chars-backward " \t" bol)
      (skip-chars-backward "^ \t" bol)	; Skip over month
      (skip-chars-backward " \t" bol)
      (skip-chars-backward "^ \t" bol)	; Skip over day abbrev.
      (if (bolp)
	  nil				; Malformed entry!!! Ack! Bailout!
	(setq time (buffer-substring pos (point)))
	(skip-chars-backward " \t")
	(setq pos (point)))
      (beginning-of-line)
      (setq url (buffer-substring (point) pos))
      (goto-char (min (1+ last-end) (point-max))) ; Goto next line
      (if (/= (length url) 0)
	  (progn
	    (setq url-history-changed-since-last-save t)
	    (url-puthash url time url-global-history-hash-table))))))

(defun url-parse-Mosaic-history-v2 (fname)
  ;; Parse an NCSA Mosaic/X style global history list (version 2)
  (goto-char (point-min))
  (skip-chars-forward "^\n")
  (skip-chars-forward "\n \t")	; Skip past the tag line
  (skip-chars-forward "^\n")
  (skip-chars-forward "\n \t")	; Skip past the second tag line
  (setq url-global-history-hash-table (url-make-hashtable 131))
  (let (pos				; Temporary position holder
	bol				; Beginning-of-line
	url				; URL
	time				; Time
	last-end			; Last ending point
	)
    (while (not (eobp))
      (setq bol (point))
      (end-of-line)
      (setq pos (point)
	    last-end (point))
      (skip-chars-backward "^ \t" bol)	; Skip over time
      (if (bolp)
	  nil				; Malformed entry!!! Ack! Bailout!
	(setq time (buffer-substring pos (point)))
	(skip-chars-backward " \t")
	(setq pos (point)))
      (beginning-of-line)
      (setq url (buffer-substring (point) pos))
      (goto-char (min (1+ last-end) (point-max))) ; Goto next line
      (if (/= (length url) 0)
	  (progn
	    (setq url-history-changed-since-last-save t)
	    (url-puthash url time url-global-history-hash-table))))))

(defun url-parse-Emacs-history (&optional fname)
  ;; Parse out the Emacs-w3 global history file for completion, etc.
  (or fname (setq fname (expand-file-name url-global-history-file)))
  (cond
   ((not (file-exists-p fname))
    (message "%s does not exist." fname))
   ((not (file-readable-p fname))
    (message "%s is unreadable." fname))
   (t
    (condition-case ()
	(load fname nil t)
      (error (message "Could not load %s" fname)))
    (if (boundp 'url-global-history-completion-list)
	;; Hey!  Automatic conversion of old format!
	(progn
	  (setq url-global-history-hash-table (url-make-hashtable 131)
		url-history-changed-since-last-save t)
	  (mapcar (function
		   (lambda (x)
		     (url-puthash (car x) (cdr x)
				 url-global-history-hash-table)))
		  (symbol-value 'url-global-history-completion-list)))))))

(defun url-parse-global-history (&optional fname)
  ;; Parse out the mosaic global history file for completions, etc.
  (or fname (setq fname (expand-file-name url-global-history-file)))
  (cond
   ((not (file-exists-p fname))
    (message "%s does not exist." fname))
   ((not (file-readable-p fname))
    (message "%s is unreadable." fname))
   (t
    (save-excursion
      (set-buffer (get-buffer-create " *url-tmp*"))
      (erase-buffer)
      (insert-file-contents-literally fname)
      (goto-char (point-min))
      (cond
       ((looking-at "(setq") (url-parse-Emacs-history fname))
       ((looking-at "ncsa-mosaic-.*-1$") (url-parse-Mosaic-history-v1 fname))
       ((looking-at "ncsa-mosaic-.*-2$") (url-parse-Mosaic-history-v2 fname))
       ((or (looking-at "MCOM-") (looking-at "netscape"))
	(url-parse-Netscape-history fname))
       (t
	(url-warn 'url (format "Cannot deduce type of history file: %s"
			       fname))))))))

(defun url-write-Emacs-history (fname)
  ;; Write an Emacs-w3 style global history list into FNAME
  (erase-buffer)
  (let ((count 0))
    (url-maphash (function
		  (lambda (key value)
		    (setq count (1+ count))
		    (insert "(url-puthash "
			    (if (stringp key)
				(prin1-to-string key)
			      (concat "\"" (symbol-name key) "\""))
			    (if (not (stringp value)) " '" "")
			    (prin1-to-string value)
			    " url-global-history-hash-table)\n")))
		 url-global-history-hash-table)
    (goto-char (point-min))
    (insert (format
	     "(setq url-global-history-hash-table (url-make-hashtable %d))\n"
	     (/ count 4)))
    (goto-char (point-max))
    (insert "\n")
    (write-file fname)))

(defun url-write-Netscape-history (fname)
  ;; Write a Netscape-style global history list into FNAME
  (erase-buffer)
  (let ((last-valid-time "785305714"))	; Picked out of thin air,
					; in case first in assoc list
					; doesn't have a valid time
    (goto-char (point-min))
    (insert "MCOM-Global-history-file-1\n")
    (url-maphash (function
		  (lambda (url time)
		    (if (or (not (stringp time)) (string-match " \t" time))
			(setq time last-valid-time)
		      (setq last-valid-time time))
		    (insert (concat (if (stringp url)
					url
				      (symbol-name url))
				    " " time "\n"))))
		 url-global-history-hash-table)
    (write-file fname)))

(defun url-write-Mosaic-history-v1 (fname)
  ;; Write a Mosaic/X-style global history list into FNAME
  (erase-buffer)
  (goto-char (point-min))
  (insert "ncsa-mosaic-history-format-1\nGlobal\n")
  (url-maphash (function
		(lambda (url time)
		  (if (listp time)
		      (setq time (current-time-string time)))
		  (if (or (not (stringp time))
			  (not (string-match " " time)))
		      (setq time (current-time-string)))
		  (insert (concat (if (stringp url)
				      url
				    (symbol-name url))
				  " " time "\n"))))
	       url-global-history-hash-table)
  (write-file fname))

(defun url-write-Mosaic-history-v2 (fname)
  ;; Write a Mosaic/X-style global history list into FNAME
  (let ((last-valid-time "827250806"))
    (erase-buffer)
    (goto-char (point-min))
    (insert "ncsa-mosaic-history-format-2\nGlobal\n")
    (url-maphash (function
		  (lambda (url time)
		    (if (listp time)
			(setq time last-valid-time)
		      (setq last-valid-time time))
		    (if (not (stringp time))
			(setq time last-valid-time))
		    (insert (concat (if (stringp url)
					url
				      (symbol-name url))
				    " " time "\n"))))
		 url-global-history-hash-table)
    (write-file fname)))

(defun url-write-global-history (&optional fname)
  "Write the global history file into `url-global-history-file'.
The type of data written is determined by what is in the file to begin
with.  If the type of storage cannot be determined, then prompt the
user for what type to save as."
  (interactive)
  (or fname (setq fname (expand-file-name url-global-history-file)))
  (cond
   ((not url-history-changed-since-last-save) nil)
   ((not (file-writable-p fname))
    (message "%s is unwritable." fname))
   (t
    (let ((make-backup-files nil)
	  (version-control nil)
	  (require-final-newline t))
      (save-excursion
	(set-buffer (get-buffer-create " *url-tmp*"))
	(erase-buffer)
	(condition-case ()
	    (insert-file-contents-literally fname)
	  (error nil))
	(goto-char (point-min))
	(cond
	 ((looking-at "ncsa-mosaic-.*-1$") (url-write-Mosaic-history-v1 fname))
	 ((looking-at "ncsa-mosaic-.*-2$") (url-write-Mosaic-history-v2 fname))
	 ((looking-at "MCOM-") (url-write-Netscape-history fname))
	 ((looking-at "netscape") (url-write-Netscape-history fname))
	 ((looking-at "(setq") (url-write-Emacs-history fname))
	 (t (url-write-Emacs-history fname)))
	(kill-buffer (current-buffer))))))
  (setq url-history-changed-since-last-save nil))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; The main URL fetching interface
;;; -------------------------------
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;###autoload
(defun url-popup-info (url)
  "Retrieve the HTTP/1.0 headers and display them in a temp buffer."
  (let* ((urlobj (url-generic-parse-url url))
	 (type (url-type urlobj))
	 data)
    (cond
     ((string= type "http")
      (let ((url-request-method "HEAD")
	    (url-automatic-caching nil)
	    (url-inhibit-mime-parsing t)
	    (url-working-buffer " *popup*"))
	(save-excursion
	  (set-buffer (get-buffer-create url-working-buffer))
	  (erase-buffer)
	  (setq url-be-asynchronous nil)
	  (url-retrieve url)
	  (subst-char-in-region (point-min) (point-max) ?\r ? )
	  (buffer-string))))
     ((or (string= type "file") (string= type "ftp"))
      (setq data (url-file-attributes url))
      (set-buffer (get-buffer-create
		   (url-generate-new-buffer-name "*Header Info*")))
      (erase-buffer)
      (if data
	  (concat (if (stringp (nth 0 data))
		      (concat "    Linked to: " (nth 0 data))
		    (concat "    Directory: " (if (nth 0 data) "Yes" "No")))
		  "\n        Links: " (int-to-string (nth 1 data))
		  "\n     File UID: " (int-to-string (nth 2 data))
		  "\n     File GID: " (int-to-string (nth 3 data))
		  "\n  Last Access: " (current-time-string (nth 4 data))
		  "\nLast Modified: " (current-time-string (nth 5 data))
		  "\n Last Changed: " (current-time-string (nth 6 data))
		  "\n Size (bytes): " (int-to-string (nth 7 data))
		  "\n    File Type: " (or (nth 8 data) "text/plain"))
	(concat "No info found for " url)))
     ((and (string= type "news") (string-match "@" url))
      (let ((art (url-filename urlobj)))
	(if (not (string= (substring art -1 nil) ">"))
	    (setq art (concat "<" art ">")))
	(url-get-headers-from-article-id art)))
     (t (concat "Don't know how to find information on " url)))))

(defun url-decode-text ()
  ;; Decode text transmitted by NNTP.
  ;; 0. Delete status line.
  ;; 1. Delete `^M' at end of line.
  ;; 2. Delete `.' at end of buffer (end of text mark).
  ;; 3. Delete `.' at beginning of line."
  (save-excursion
    (set-buffer nntp-server-buffer)
    ;; Insert newline at end of buffer.
    (goto-char (point-max))
    (if (not (bolp))
	(insert "\n"))
    ;; Delete status line.
    (goto-char (point-min))
    (delete-region (point) (progn (forward-line 1) (point)))
    ;; Delete `^M' at end of line.
    ;; (replace-regexp "\r$" "")
    (while (not (eobp))
      (end-of-line)
      (if (= (preceding-char) ?\r)
	  (delete-char -1))
      (forward-line 1)
      )
    ;; Delete `.' at end of buffer (end of text mark).
    (goto-char (point-max))
    (forward-line -1)			;(beginning-of-line)
    (if (looking-at "^\\.$")
	(delete-region (point) (progn (forward-line 1) (point))))
    ;; Replace `..' at beginning of line with `.'.
    (goto-char (point-min))
    ;; (replace-regexp "^\\.\\." ".")
    (while (search-forward "\n.." nil t)
      (delete-char -1))
    ))

(defun url-get-headers-from-article-id (art)
  ;; Return the HEAD of ART (a usenet news article)
  (cond
   ((string-match "flee" nntp-version)
    (nntp/command "HEAD" art)
    (save-excursion
      (set-buffer nntp-server-buffer)
      (while (progn (goto-char (point-min))
		    (not (re-search-forward "^.\r*$" nil t)))
	(url-accept-process-output nntp/connection))))
   (t
    (nntp-send-command "^\\.\r$" "HEAD" art)
    (url-decode-text)))
  (save-excursion
    (set-buffer nntp-server-buffer)
    (buffer-string)))

(defvar url-external-retrieval-program "www"
  "*Name of the external executable to run to retrieve URLs.")

(defvar url-external-retrieval-args '("-source")
  "*A list of arguments to pass to `url-external-retrieval-program' to
retrieve a URL by its HTML source.")

(defun url-retrieve-externally (url &optional no-cache)
  (if (get-buffer url-working-buffer)
      (save-excursion
	(set-buffer url-working-buffer)
	(set-buffer-modified-p nil)
	(kill-buffer url-working-buffer)))
  (set-buffer (get-buffer-create url-working-buffer))
  (let* ((args (append url-external-retrieval-args (list url)))
	 (urlobj (url-generic-parse-url url))
	 (type (url-type urlobj)))
    (if (or (member type '("www" "about" "mailto" "mailserver"))
	    (url-file-directly-accessible-p urlobj))
	(url-retrieve-internally url)
      (url-lazy-message "Retrieving %s..." url)
      (apply 'call-process url-external-retrieval-program
	     nil t nil args)
      (url-lazy-message "Retrieving %s... done" url)
      (if (and type urlobj)
	  (setq url-current-server (url-host urlobj)
		url-current-type (url-type urlobj)
		url-current-port (url-port urlobj)
		url-current-file (url-filename urlobj)))
      (if (member url-current-file '("/" ""))
	  (setq url-current-mime-type "text/html")))))

(defun url-get-normalized-date (&optional specified-time)
  ;; Return a 'real' date string that most HTTP servers can understand.
  (require 'timezone)
  (let* ((raw (if specified-time (current-time-string specified-time)
		(current-time-string)))
	 (gmt (timezone-make-date-arpa-standard raw
						(nth 1 (current-time-zone))
						"GMT"))
	 (parsed (timezone-parse-date gmt))
	 (day (cdr-safe (assoc (substring raw 0 3) weekday-alist)))
	 (year nil)
	 (month (car
		 (rassoc
		  (string-to-int (aref parsed 1)) monthabbrev-alist)))
	 )
    (setq day (or (car-safe (rassoc day weekday-alist))
		  (substring raw 0 3))
	  year (aref parsed 0))
    ;; This is needed for plexus servers, or the server will hang trying to
    ;; parse the if-modified-since header.  Hopefully, I can take this out
    ;; soon.
    (if (and year (> (length year) 2))
	(setq year (substring year -2 nil)))

    (concat day ", " (aref parsed 2) "-" month "-" year " "
	    (aref parsed 3) " " (or (aref parsed 4)
				    (concat "[" (nth 1 (current-time-zone))
					    "]")))))

;;;###autoload
(defun url-cache-expired (url mod)
  "Return t iff a cached file has expired."
  (if (not (string-match url-nonrelative-link url))
      t
    (let* ((urlobj (url-generic-parse-url url))
	   (type (url-type urlobj)))
      (cond
       (url-standalone-mode
	(not (file-exists-p (url-create-cached-filename urlobj))))
       ((string= type "http")
	(if (not url-standalone-mode) t
	  (not (file-exists-p (url-create-cached-filename urlobj)))))
       ((not (fboundp 'current-time))
	t)
       ((member type '("file" "ftp"))
	(if (or (equal mod '(0 0)) (not mod))
	      (return t)
	    (or (> (nth 0 mod) (nth 0 (current-time)))
		(> (nth 1 mod) (nth 1 (current-time))))))
       (t nil)))))

(defun url-retrieve-internally (url &optional no-cache)
  (if (get-buffer url-working-buffer)
      (save-excursion
	(set-buffer url-working-buffer)
	(erase-buffer)
	(setq url-current-can-be-cached (not no-cache))
	(set-buffer-modified-p nil)))
  (let* ((urlobj (url-generic-parse-url url))
	 (type (url-type urlobj))
	 (url-using-proxy (and
			   (if (assoc "no_proxy" url-proxy-services)
			       (not (string-match
				     (cdr
				      (assoc "no_proxy" url-proxy-services))
				     url))
			     t)
			   (not
			    (and
			     (string-match "file:" url)
			     (not (string-match "file://" url))))
			   (cdr (assoc type url-proxy-services))))
	 (handler nil)
	 (original-url url)
	 (cached nil)
	 (tmp url-current-file))
    (if url-using-proxy (setq type "proxy"))
    (setq cached (url-is-cached url)
	  cached (and cached (not (url-cache-expired url cached)))
	  handler (if cached 'url-extract-from-cache
		    (car-safe
		     (cdr-safe (assoc (or type "auto")
				      url-registered-protocols))))
	  url (if cached (url-create-cached-filename url) url))
    (save-excursion
      (set-buffer (get-buffer-create url-working-buffer))
      (setq url-current-can-be-cached (not no-cache)))
    (if url-be-asynchronous
	(url-download-minor-mode t))
    (if (and handler (fboundp handler))
	(funcall handler url)
      (set-buffer (get-buffer-create url-working-buffer))
      (setq url-current-file tmp)
      (erase-buffer)
      (insert "<title> Link Error! </title>\n"
	      "<h1> An error has occurred... </h1>\n"
	      (format "The link type `<code>%s</code>'" type)
	      " is unrecognized or unsupported at this time.<p>\n"
	      "If you feel this is an error, please "
	      "<a href=\"mailto://" url-bug-address "\">send me mail.</a>"
	      "<p><address>William Perry</address><br>"
	      "<address>" url-bug-address "</address>")
      (setq url-current-file "error.html"))
    (if (and
	 (not url-be-asynchronous)
	 (get-buffer url-working-buffer))
	(progn
	  (set-buffer url-working-buffer)
	  (if (not url-current-object)
	      (setq url-current-object urlobj))
	  (url-clean-text)))
    (cond
     ((equal type "wais") nil)
     ((and url-be-asynchronous (not cached) (member type '("http" "proxy")))
      nil)
     (url-be-asynchronous
      (funcall url-default-retrieval-proc (buffer-name)))
     ((not (get-buffer url-working-buffer)) nil)
     ((and (not url-inhibit-mime-parsing)
	   (or cached (url-mime-response-p t)))
      (or cached (url-parse-mime-headers nil t))))
    (if (and (or (not url-be-asynchronous)
		 (not (equal type "http")))
	     (not url-current-mime-type))
	(if (url-buffer-is-hypertext)
	    (setq url-current-mime-type "text/html")
	  (setq url-current-mime-type (mm-extension-to-mime
				      (url-file-extension
				       url-current-file)))))
    (if (and url-automatic-caching url-current-can-be-cached
	     (not url-be-asynchronous))
	(save-excursion
	  (url-store-in-cache url-working-buffer)))
    (if (not (url-hashtablep url-global-history-hash-table))
	(setq url-global-history-hash-table (url-make-hashtable 131)))
    (if (not (string-match "^about:" original-url))
	(progn
	  (setq url-history-changed-since-last-save t)
	  (url-puthash original-url (current-time)
		       url-global-history-hash-table)))
    cached))

;;;###autoload
(defun url-retrieve (url &optional no-cache expected-md5)
  "Retrieve a document over the World Wide Web.
The document should be specified by its fully specified
Uniform Resource Locator.  No parsing is done, just return the
document as the server sent it.  The document is left in the
buffer specified by url-working-buffer.  url-working-buffer is killed
immediately before starting the transfer, so that no buffer-local
variables interfere with the retrieval.  HTTP/1.0 redirection will
be honored before this function exits."
  (url-do-setup)
  (if (and (fboundp 'set-text-properties)
	   (subrp (symbol-function 'set-text-properties)))
      (set-text-properties 0 (length url) nil url))
  (if (and url (string-match "^url:" url))
      (setq url (substring url (match-end 0) nil)))
  (let ((status (url-retrieve-internally url no-cache)))
    (if (and expected-md5 url-check-md5s)
	(let ((cur-md5 (md5 (current-buffer))))
	  (if (not (string= cur-md5 expected-md5))
	      (and (not (funcall url-confirmation-func
				 "MD5s do not match, use anyway? "))
		   (error "MD5 error.")))))
    status))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; How to register a protocol
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun url-register-protocol (protocol &optional retrieve expander defport)
  "Register a protocol with the URL retrieval package.
PROTOCOL is the type of protocol being registers (http, nntp, etc),
         and is the first chunk of the URL.  ie: http:// URLs will be
         handled by the protocol registered as 'http'.  PROTOCOL can
         be either a symbol or a string - it is converted to a string,
         and lowercased before being registered.
RETRIEVE (optional) is the function to be called with a url as its
         only argument.  If this argument is omitted, then this looks
         for a function called 'url-PROTOCOL'.  A warning is shown if
         the function is undefined, but the protocol is still
         registered.
EXPANDER (optional) is the function to call to expand a relative link
         of type PROTOCOL.  If omitted, this defaults to
         `url-default-expander'

Any proxy information is read in from environment variables at this
time, so this function should only be called after dumping emacs."
  (let* ((protocol (cond
		    ((stringp protocol) (downcase protocol))
		    ((symbolp protocol) (downcase (symbol-name protocol)))
		    (t nil)))
		     
	 (retrieve (or retrieve (intern (concat "url-" protocol))))
	 (expander (or expander 'url-default-expander))
	 (cur-protocol (assoc protocol url-registered-protocols))
	 (urlobj nil)
	 (cur-proxy (assoc protocol url-proxy-services))
	 (env-proxy (or (getenv (concat protocol "_proxy"))
			(getenv (concat protocol "_PROXY"))
			(getenv (upcase (concat protocol "_PROXY"))))))

    (if (not protocol)
	(error "Invalid data to url-register-protocol."))
    
    (if (not (fboundp retrieve))
	(message "Warning: %s registered, but no function found." protocol))

    ;; Store the default port, if none previously specified and
    ;; defport given
    (if (and defport (not (assoc protocol url-default-ports)))
	(setq url-default-ports (cons (cons protocol defport)
				      url-default-ports)))
    
    ;; Store the appropriate information for later
    (if cur-protocol
	(setcdr cur-protocol (cons retrieve expander))
      (setq url-registered-protocols (cons (cons protocol
						 (cons retrieve expander))
					   url-registered-protocols)))

    ;; Store any proxying information - this will not overwrite an old
    ;; entry, so that people can still set this information in their
    ;; .emacs file
    (cond
     (cur-proxy nil)			; Keep their old settings
     ((null env-proxy) nil)		; No proxy setup
     ;; First check if its something like hostname:port
     ((string-match "^\\([^:]+\\):\\([0-9]+\\)$" env-proxy)
      (setq urlobj (url-generic-parse-url nil)) ; Get a blank object
      (url-set-type urlobj "http")
      (url-set-host urlobj (url-match env-proxy 1))
      (url-set-port urlobj (url-match env-proxy 2)))
     ;; Then check if its a fully specified URL
     ((string-match url-nonrelative-link env-proxy)
      (setq urlobj (url-generic-parse-url env-proxy))
      (url-set-type urlobj "http")
      (url-set-target urlobj nil))
     ;; Finally, fall back on the assumption that its just a hostname
     (t
      (setq urlobj (url-generic-parse-url nil)) ; Get a blank object
      (url-set-type urlobj "http")
      (url-set-host urlobj env-proxy)))

     (if (and (not cur-proxy) urlobj)
	 (progn
	   (setq url-proxy-services
		 (cons (cons protocol (url-recreate-url urlobj))
		       url-proxy-services))
	   (message "Using a proxy for %s..." protocol)))))

(provide 'url)
