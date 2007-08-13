;;; url-news.el,v --- News Uniform Resource Locator retrieval code
;; Author: wmperry
;; Created: 1996/05/29 15:48:29
;; Version: 1.9
;; Keywords: comm, data, processes

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
(require 'url-vars)
(require 'url-parse)

(defun url-format-news ()
  (url-clear-tmp-buffer)
  (insert "HTTP/1.0 200 Retrieval OK\r\n"
 	  (save-excursion
 	    (set-buffer nntp-server-buffer)
 	    (buffer-string)))
  (url-parse-mime-headers)
  (let ((from  (cdr (assoc "from" url-current-mime-headers)))
 	(subj  (cdr (assoc "subject" url-current-mime-headers)))
 	(org   (cdr (assoc "organization" url-current-mime-headers)))
 	(typ   (or (cdr (assoc "content-type" url-current-mime-headers))
 		   "text/plain"))
 	(grps  (mapcar 'car
 		       (url-split
 			(or (cdr (assoc "newsgroups" url-current-mime-headers))
 			    "")
 			"[ \t\n,]+")))
 	(refs  (mapcar 'car
 		       (url-split
 			(or (cdr (assoc "references" url-current-mime-headers))
 			    "")
 			"[ \t,\n<>]+")))
 	(date  (cdr (assoc "date" url-current-mime-headers))))
    (setq url-current-file ""
 	  url-current-type "")
    (if (or (not (string-match "text/" typ))
 	    (string-match "text/html" typ))
 	nil				; Let natural content-type take over
      (insert "<html>\n"
 	      " <head>\n"
 	      "  <title>" subj "</title>\n"
 	      "  <link rev=\"made\" href=\"mailto:" from "\">\n"
 	      " </head>\n"
 	      " <body>\n"
 	      "  <div>\n"
 	      "   <h1 align=center>" subj "</h1>\n"
 	      "   <p role=\"headers\">\n"
 	      "    <b>From</b>: <address> " from "</address><br>\n"
 	      "    <b>Newsgroups</b>: "
 	      (mapconcat
 	       (function
 		(lambda (grp)
 		  (concat "<a href=\"" grp "\"> " grp "</a>"))) grps ", ")
 	      "<br>\n"
 	      (if org
 		  (concat
 		   "    <b>Organization</b>: <i> " org "</i> <br>\n")
 		"")
 	      "    <b>Date</b>: <date> " date "</date> <br>\n"
 	      "   </p> <hr>\n"
 	      (if (null refs)
 		  ""
 		(concat
 		 "   <p>References\n"
 		 "    <ol>\n"
 		 (mapconcat
 		  (function
 		   (lambda (ref)
 		     (concat "     <li> <a href=\"" ref "\"> " 
 			     ref "</a></li>\n")))
 		  refs "")
 		 "    </ol>\n"
		 "   </p>\n"
 		 "   <hr>\n"))
 	      "   <ul plain>\n"
 	      "    <li><a href=\"newspost:disfunctional\"> "
 	      "Post to this group </a></li>\n"
 	      "    <li><a href=\"mailto:" from "\"> Reply to " from
 	      "</a></li>\n"
 	      "   </ul>\n"
 	      "   <hr>"
 	      "   <xmp>\n")
      (goto-char (point-max))
      (setq url-current-mime-type "text/html"
 	    url-current-mime-viewer (mm-mime-info url-current-mime-type nil 5))
      (let ((x (assoc "content-type" url-current-mime-headers)))
 	(if x
 	    (setcdr x "text/html")
 	  (setq url-current-mime-headers (cons (cons "content-type"
 						     "text/html")
 					       url-current-mime-headers))))
      (insert "\n"
 	      "   </xmp>\n"
 	      "  </div>\n"
 	      " </body>\n"
 	      "</html>\n"
 	      "<!-- Automatically generated by URL/" url-version
 	      "-->"))))

(defun url-check-gnus-version ()
  (require 'nntp)
  (condition-case ()
      (require 'gnus)
    (error (setq gnus-version "GNUS not found")))
  (if (or (not (boundp 'gnus-version))
	  (string-match "v5.[.0-9]+$" gnus-version)
	  (string-match "September" gnus-version))
      nil
    (url-warn 'url (concat
		    "The version of GNUS found on this system is too old and does\n"
		    "not support the necessary functionality for the URL package.\n"
		    "Please upgrade to version 5.x of GNUS.  This is bundled by\n"
		    "default with Emacs 19.30 and XEmacs 19.14 and later.\n\n"
		    "This version of GNUS is: " gnus-version "\n"))
    (fset 'url-news 'url-news-version-too-old))
  (fset 'url-check-gnus-version 'ignore))

(defun url-news-version-too-old (article)
  (set-buffer (get-buffer-create url-working-buffer))
  (setq url-current-mime-headers '(("content-type" . "text/html"))
	url-current-mime-type "text/html")
  (insert "<html>\n"
	  " <head>\n"
	  "  <title>News Error</title>\n"
	  " </head>\n"
	  " <body>\n"
	  "  <h1>News Error - too old</h1>\n"
	  "  <p>\n"
	  "   The version of GNUS found on this system is too old and does\n"
	  "   not support the necessary functionality for the URL package.\n"
	  "   Please upgrade to version 5.x of GNUS.  This is bundled by\n"
	  "   default with Emacs 19.30 and XEmacs 19.14 and later.\n\n"
	  "   This version of GNUS is: " gnus-version "\n"
	  "  </p>\n"
	  " </body>\n"
	  "</html>\n"))

(defun url-news-open-host (host port user pass)
  (nntp-open-server host (list (string-to-int port)))
  (if (and user pass)
      (progn
	(nntp-send-command "^.*\r?\n" "AUTHINFO USER" user)
	(nntp-send-command "^.*\r?\n" "AUTHINFO PASS" pass)
	(if (not (nntp-server-opened host))
	    (url-warn 'url (format "NNTP authentication to `%s' as `%s' failed"
				   host user))))))

(defun url-news-fetch-article-number (newsgroup article)
  (nntp-request-group newsgroup)
  (nntp-request-article article))

(defun url-news-fetch-message-id (host port message-id)
  (if (eq ?> (aref article (1- (length article))))
      nil
    (setq message-id (concat "<" message-id ">")))
  (if (nntp-request-article message-id)
      (url-format-news)
    (set-buffer (get-buffer-create url-working-buffer))
    (setq url-current-can-be-cached nil)
    (insert "<html>\n"
	    " <head>\n"
	    "  <title>Error</title>\n"
	    " </head>\n"
	    " <body>\n"
	    "  <div>\n"
	    "   <h1>Error requesting article...</h1>\n"
	    "   <p>\n"
	    "    The status message returned by the NNTP server was:"
	    "<br><hr>\n"
	    "    <xmp>\n"
	    (nntp-status-message)
	    "    </xmp>\n"
	    "   </p>\n"
	    "   <p>\n"
	    "    If you If you feel this is an error, <a href=\""
	    "mailto:" url-bug-address "\">send me mail</a>\n"
	    "   </p>\n"
	    "  </div>\n"
	    " </body>\n"
	    "</html>\n"
	    "<!-- Automatically generated by URL v" url-version " -->\n"
	    )))

(defun url-news-fetch-newsgroup (newsgroup)
  (if (string-match "^/+" newsgroup)
      (setq newsgroup (substring newsgroup (match-end 0))))
  (if (string-match "/+$" newsgroup)
      (setq newsgroup (substring newsgroup 0 (match-beginning 0))))

  ;; This saves a bogus 'Untitled' buffer by Emacs-W3
  (kill-buffer url-working-buffer)
  
  ;; This saves us from checking new news if GNUS is already running
  (if (or (not (get-buffer gnus-group-buffer))
	  (save-excursion
	    (set-buffer gnus-group-buffer)
	    (not (eq major-mode 'gnus-group-mode))))
      (gnus))
  (set-buffer gnus-group-buffer)
  (goto-char (point-min))
  (gnus-group-read-ephemeral-group newsgroup (list 'nntp host)
				   nil
				   (cons (current-buffer) 'browse)))
  
(defun url-news (article)
  ;; Find a news reference
  (url-check-gnus-version)
  (let* ((urlobj (url-generic-parse-url article))
	 (host (or (url-host urlobj) url-news-server))
	 (port (or (url-port urlobj)
		   (cdr-safe (assoc "news" url-default-ports))))
	 (article-brackets nil)
	 (article (url-filename urlobj)))
    (url-news-open-host host port (url-user urlobj) (url-password urlobj))
    (cond
     ((string-match "@" article)	; Its a specific article
      (url-news-fetch-message-id article))
     ((string= article "")		; List all newsgroups
      (gnus)
      (kill-buffer url-working-buffer))
     (t					; Whole newsgroup
      (url-news-fetch-newsgroup article)))
    (setq url-current-type "news"
	  url-current-server host
	  url-current-user (url-user urlobj)
	  url-current-port port
	  url-current-file article)))

(defun url-nntp (url)
  ;; Find a news reference
  (url-check-gnus-version)
  (let* ((urlobj (url-generic-parse-url url))
	 (host (or (url-host urlobj) url-news-server))
	 (port (or (url-port urlobj)
		   (cdr-safe (assoc "nntp" url-default-ports))))
	 (article-brackets nil)
	 (article (url-filename urlobj)))
    (url-news-open-host host port (url-user urlobj) (url-password urlobj))
    (cond
     ((string-match "@" article)	; Its a specific article
      (url-news-fetch-message-id article))
     ((string-match "/\\([0-9]+\\)$" article)
      (url-news-fetch-article-number (substring article 0
						(match-beginning 0))
				     (match-string 1 article)))
						
     ((string= article "")		; List all newsgroups
      (gnus)
      (kill-buffer url-working-buffer))
     (t					; Whole newsgroup
      (url-news-fetch-newsgroup article)))
    (setq url-current-type "news"
	  url-current-server host
	  url-current-user (url-user urlobj)
	  url-current-port port
	  url-current-file article)))

(provide 'url-news)
