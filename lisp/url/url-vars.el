;;; url-vars.el,v --- Variables for Uniform Resource Locator tool
;; Author: wmperry
;; Created: 1996/06/03 15:04:57
;; Version: 1.13
;; Keywords: comm, data, processes, hypermedia

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

(defconst url-version (let ((x "1.13"))
			(if (string-match "Revision: \\([^ \t\n]+\\)" x)
			    (substring x (match-beginning 1) (match-end 1))
			  x))
  "Version # of URL package.")


;;; This is so we can use a consistent method of checking for mule support
;;; Emacs-based mule uses (boundp 'MULE), but XEmacs-based mule uses
;;; (featurep 'mule) - I choose to use the latter.

(if (boundp 'MULE)
    (provide 'mule))

(defvar url-current-can-be-cached t
  "*Whether the current URL can be cached.")

(defvar url-current-object nil
  "A parsed representation of the current url")

(defvar url-current-callback-func nil
  "*The callback function for the current buffer.")

(defvar url-current-callback-data nil
  "*The data to be passed to the callback function.  This should be a list,
each item in the list will be an argument to the url-current-callback-func.")

(mapcar 'make-variable-buffer-local '(
				      url-current-callback-data
				      url-current-callback-func
				      url-current-can-be-cached
				      url-current-content-length
				      url-current-file
				      url-current-isindex
				      url-current-mime-encoding
				      url-current-mime-headers
				      url-current-mime-type
				      url-current-mime-viewer
				      url-current-object
				      url-current-port
				      url-current-referer
				      url-current-type
				      url-current-user
				      ))

(defvar url-default-retrieval-proc 'url-default-callback
  "*The default action to take when an asynchronous retrieval completes.")

(defvar url-honor-refresh-requests t
  "*Whether to do automatic page reloads at the request of the document
author or the server via the `Refresh' header in an HTTP/1.0 response.
If nil, no refresh requests will be honored.
If t, all refresh requests will be honored.
If non-nil and not t, the user will be asked for each refresh request.")

(defvar url-emacs-minor-version
  (if (boundp 'emacs-minor-version)
      (symbol-value 'emacs-minor-version)
    (if (string-match "^[0-9]+\\.\\([0-9]+\\)" emacs-version)
	(string-to-int
	 (substring emacs-version
		    (match-beginning 1) (match-end 1)))
      0))
  "What minor version of emacs we are using.")

(defvar url-inhibit-mime-parsing nil
  "Whether to parse out (and delete) the MIME headers from a message.")

(defvar url-forms-based-ftp nil
  "*If non-nil, local and remote file access of directories will be shown
as an HTML 3.0 form, allowing downloads of multiple files at once.")

(defvar url-automatic-caching nil
  "*If non-nil, all documents will be automatically cached to the local
disk.")

(defvar url-cache-expired
  (function (lambda (t1 t2) (>= (- (car t2) (car t1)) 5)))
  "*A function (`funcall'able) that takes two times as its arguments, and
returns non-nil if the second time is 'too old' when compared to the first
time.")

(defvar url-check-md5s nil
  "*Whether to check md5s of retrieved documents or not.")

(defvar url-expected-md5 nil "What md5 we expect to see.")

(defvar url-broken-resolution nil
  "*Whether to use [ange|efs]-ftp-nslookup-host.")

(defvar url-bug-address "wmperry@spry.com" "Where to send bug reports.")

(defvar url-personal-mail-address nil
  "*Your full email address.  This is what is sent to HTTP/1.0 servers as
the FROM field.  If not set when url-do-setup is run, it defaults to
the value of url-pgp/pem-entity.")

(defvar url-mule-retrieval-coding-system (if (featurep 'mule) *euc-japan*
					  nil)
  "Coding system for retrieval, used before hexified.")

(defvar url-directory-index-file "index.html"
  "*The filename to look for when indexing a directory.  If this file
exists, and is readable, then it will be viewed instead of
automatically creating the directory listing.")

(defvar url-pgp/pem-entity nil
  "*The users PGP/PEM id - usually their email address.")

(defvar url-privacy-level 'none
  "*How private you want your requests to be.
HTTP/1.0 has header fields for various information about the user, including
operating system information, email addresses, the last page you visited, etc.
This variable controls how much of this information is sent.

This should a symbol or a list.
Valid values if a symbol are:
none     -- Send all information
low      -- Don't send the last location
high     -- Don't send the email address or last location
paranoid -- Don't send anything

If a list, this should be a list of symbols of what NOT to send.
Valid symbols are:
email    -- the email address
os       -- the operating system info
lastloc  -- the last location
agent    -- Do not send the User-Agent string

Samples:

(setq url-privacy-level 'high)
(setq url-privacy-level '(email lastloc))    ;; equivalent to 'high
(setq url-privacy-level '(os))

::NOTE::
This variable controls several other variables and is _NOT_ automatically
updated.  Call the function `url-setup-privacy-info' after modifying this
variable.
")

(defvar url-uudecode-program "uudecode" "*The UUdecode executable.")

(defvar url-uuencode-program "uuencode" "*The UUencode executable.")

(defvar url-history-list nil "List of urls visited this session.")

(defvar url-inhibit-uncompression nil "Do not do decompression if non-nil.")

(defvar url-keep-history nil
  "*Controls whether to keep a list of all the URLS being visited.  If
non-nil, url will keep track of all the URLS visited.
If eq to `t', then the list is saved to disk at the end of each emacs
session.")

(defvar url-uncompressor-alist '((".z"  . "x-gzip")
				(".gz" . "x-gzip")
				(".uue" . "x-uuencoded")
				(".hqx" . "x-hqx")
				(".Z"  . "x-compress"))
  "*An assoc list of file extensions and the appropriate uncompression
programs for each.")

(defvar url-xterm-command "xterm -title %s -ut -e %s %s %s"
  "*Command used to start an xterm window.")

(defvar url-tn3270-emulator "tn3270"
  "The client to run in a subprocess to connect to a tn3270 machine.")

(defvar url-use-transparent nil
  "*Whether to use the transparent package by Brian Tompsett instead of
the builtin telnet functions.  Using transparent allows you to have full
vt100 emulation in the telnet and tn3270 links.")

(defvar url-mail-command 'url-mail
  "*This function will be called whenever url needs to send mail.  It should
enter a mail-mode-like buffer in the current window.
The commands mail-to and mail-subject should still work in this
buffer, and it should use mail-header-separator if possible.")

(defvar url-local-exec-path nil
  "*A list of possible locations for x-exec scripts.")

(defvar url-proxy-services nil
  "*An assoc list of access types and servers that gateway them.
Looks like ((\"http\" . \"url://for/proxy/server/\") ....)  This is set up
from the ACCESS_proxy environment variables in url-do-setup.")

(defvar url-global-history-file nil
  "*The global history file used by both Mosaic/X and the url package.
This file contains a list of all the URLs you have visited.  This file
is parsed at startup and used to provide URL completion.")

(defvar url-global-history-save-interval 3600
  "*The number of seconds between automatic saves of the history list.
Default is 1 hour.  Note that if you change this variable after `url-do-setup'
has been run, you need to run the `url-setup-save-timer' function manually.")

(defvar url-global-history-timer nil)

(defvar url-passwd-entry-func nil
  "*This is a symbol indicating which function to call to read in a
password.  It will be set up depending on whether you are running EFS
or ange-ftp at startup if it is nil.  This function should accept the
prompt string as its first argument, and the default value as its
second argument.")

(defvar url-gopher-labels
  '(("0" . "(TXT)")
    ("1" . "(DIR)")
    ("2" . "(CSO)")
    ("3" . "(ERR)")
    ("4" . "(MAC)")
    ("5" . "(PCB)")
    ("6" . "(UUX)")
    ("7" . "(???)")
    ("8" . "(TEL)")
    ("T" . "(TN3)")
    ("9" . "(BIN)")
    ("g" . "(GIF)")
    ("I" . "(IMG)")
    ("h" . "(WWW)")
    ("s" . "(SND)"))
  "*An assoc list of gopher types and how to describe them in the gopher
menus.  These can be any string, but HTML/HTML+ entities should be
used when necessary, or it could disrupt formatting of the document
later on.  It is also a good idea to make sure all the strings are the
same length after entity references are removed, on a strictly
stylistic level.")

(defvar url-gopher-icons
  '(
    ("0" . "&text.document;")
    ("1" . "&folder;")
    ("2" . "&index;")
    ("3" . "&stop;")
    ("4" . "&binhex.document;")
    ("5" . "&binhex.document;")
    ("6" . "&uuencoded.document;")
    ("7" . "&index;")
    ("8" . "&telnet;")
    ("T" . "&tn3270;")
    ("9" . "&binary.document;")
    ("g" . "&image;")
    ("I" . "&image;")
    ("s" . "&audio;"))
  "*An assoc list of gopher types and the graphic entity references to
show when possible.")

(defvar url-standalone-mode nil "*Rely solely on the cache?")
(defvar url-working-buffer " *URL*" "The buffer to do all the processing in.")
(defvar url-current-annotation nil "URL of document we are annotating...")
(defvar url-current-referer nil "Referer of this page.")
(defvar url-current-content-length nil "Current content length.")
(defvar url-current-file nil "Filename of current document.")
(defvar url-current-isindex nil "Is the current document a searchable index?")
(defvar url-current-mime-encoding nil "MIME encoding of current document.")
(defvar url-current-mime-headers nil "An alist of MIME headers.")
(defvar url-current-mime-type nil "MIME type of current document.")
(defvar url-current-mime-viewer nil "How to view the current MIME doc.")
(defvar url-current-nntp-server nil "What nntp server currently opened.")
(defvar url-current-passwd-count 0 "How many times password has failed.")
(defvar url-current-port nil "Port # of the current document.")
(defvar url-current-server nil "Server of the current document.")
(defvar url-current-user nil "Username for ftp login.")
(defvar url-current-type nil "We currently in http or file mode?")
(defvar url-gopher-types "0123456789+gIThws:;<"
  "A string containing character representations of all the gopher types.")
(defvar url-mime-separator-chars (mapcar 'identity
					(concat "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
						"abcdefghijklmnopqrstuvwxyz"
						"0123456789'()+_,-./=?"))
  "Characters allowable in a MIME multipart separator.")

(defvar url-bad-port-list
  '("25" "119" "19")
  "*List of ports to warn the user about connecting to.  Defaults to just
the mail, chargen, and NNTP ports so you cannot be tricked into sending
fake mail or forging messages by a malicious HTML document.")

(defvar url-be-anal-about-file-attributes nil
  "*Whether to use HTTP/1.0 to figure out file attributes
or just guess based on file extension, etc.")

(defvar url-be-asynchronous nil
  "*Controls whether document retrievals over HTTP should be done in
the background.  This allows you to keep working in other windows
while large downloads occur.")
(make-variable-buffer-local 'url-be-asynchronous)

(defvar url-request-data nil "Any data to send with the next request.")

(defvar url-request-extra-headers nil
  "A list of extra headers to send with the next request.  Should be
an assoc list of headers/contents.")

(defvar url-request-method nil "The method to use for the next request.")

(defvar url-mime-encoding-string nil
  "String to send to the server in the Accept-encoding: field in HTTP/1.0
requests.  This is created automatically from mm-content-transfer-encodings.")

(defvar url-mime-language-string "*/*"
  "String to send to the server in the Accept-language: field in
HTTP/1.0 requests.")

(defvar url-mime-accept-string nil
  "String to send to the server in the Accept: field in HTTP/1.0 requests.
This is created automatically from url-mime-viewers, after the mailcap file
has been parsed.")

(defvar url-history-changed-since-last-save nil
  "Whether the history list has changed since the last save operation.")

(defvar url-proxy-basic-authentication nil
  "Internal structure - do not modify!")
  
(defvar url-registered-protocols nil
  "Internal structure - do not modify!  See `url-register-protocol'")

(defvar url-package-version "Unknown" "Version # of package using URL.")

(defvar url-package-name "Unknown" "Version # of package using URL.")

(defvar url-system-type nil "What type of system we are on.")
(defvar url-os-type nil "What OS we are on.")

(defvar url-max-password-attempts 5
  "*Maximum number of times a password will be prompted for when a
protected document is denied by the server.")

(defvar url-wais-to-mime
  '(
    ("WSRC" . "application/x-wais-source") 	; A database description
    ("TEXT" . "text/plain")			; plain text
    )
  "An assoc list of wais doctypes and their corresponding MIME
content-types.")

(defvar url-waisq-prog "waisq"
  "*Name of the waisq executable on this system.  This should be the
waisq program from think.com's wais8-b5.1 distribution.")

(defvar url-wais-gateway-server "www.ncsa.uiuc.edu"
  "*The machine name where the WAIS gateway lives.")

(defvar url-wais-gateway-port "8001"
  "*The port # of the WAIS gateway.")

(defvar url-temporary-directory "/tmp" "*Where temporary files go.")

(defvar url-show-status t
  "*Whether to show a running total of bytes transferred.  Can cause a
large hit if using a remote X display over a slow link, or a terminal
with a slow modem.")

(defvar url-using-proxy nil
  "Either nil or the fully qualified proxy URL in use, e.g.
http://www.domain.com/")

(defvar url-news-server nil
  "*The default news server to get newsgroups/articles from if no server
is specified in the URL.  Defaults to the environment variable NNTPSERVER
or \"news\" if NNTPSERVER is undefined.")

(defvar url-gopher-to-mime
  '((?0 . "text/plain")			; It's a file
    (?1 . "www/gopher")			; Gopher directory
    (?2 . "www/gopher-cso-search")	; CSO search
    (?3 . "text/plain")			; Error
    (?4 . "application/mac-binhex40")	; Binhexed macintosh file
    (?5 . "application/pc-binhex40")	; DOS binary archive of some sort
    (?6 . "archive/x-uuencode")		; Unix uuencoded file
    (?7 . "www/gopher-search")		; Gopher search!
    (?9 . "application/octet-stream")	; Binary file!
    (?g . "image/gif")			; Gif file
    (?I . "image/gif")			; Some sort of image
    (?h . "text/html")			; HTML source
    (?s . "audio/basic")		; Sound file
    )
  "*An assoc list of gopher types and their corresponding MIME types.")

(defvar url-use-hypertext-gopher t
  "*Controls how gopher documents are retrieved.
If non-nil, the gopher pages will be converted into HTML and parsed
just like any other page.  If nil, the requests will be passed off to
the gopher.el package by Scott Snyder.  Using the gopher.el package
will lose the gopher+ support, and inlined searching.")

(defvar url-global-history-hash-table nil
  "Hash table for global history completion.")

(defvar url-nonrelative-link
  "^\\([-a-zA-Z0-9+.]+:\\)"
  "A regular expression that will match an absolute URL.")

(defvar url-configuration-directory nil
  "*Where the URL configuration files can be found.")

(defvar url-confirmation-func 'y-or-n-p
  "*What function to use for asking yes or no functions.  Possible
values are 'yes-or-no-p or 'y-or-n-p, or any function that takes a
single argument (the prompt), and returns t only if a positive answer
is gotten.")

(defvar url-connection-retries 5
  "*# of times to try for a connection before bailing.
If for some reason url-open-stream cannot make a connection to a host
right away, it will sit for 1 second, then try again, up to this many
tries.")

(defvar url-find-this-link nil "Link to go to within a document.")

(defvar url-show-http2-transfer t
  "*Whether to show the total # of bytes, size of file, and percentage
transferred when retrieving a document over HTTP/1.0 and it returns a
valid content-length header.  This can mess up some people behind
gateways.")

(defvar url-gateway-method 'native
  "*The type of gateway support to use.
Should be a symbol specifying how we are to get a connection off of the
local machine.

Currently supported methods:
'program	:: Run a program in a subprocess to connect
                   (examples are itelnet, an expect script, etc)
'native		:: Use the native open-network-stream in emacs
'tcp            :: Use the excellent tcp.el package from gnus.
                   This simply does a (require 'tcp), then sets
                   url-gateway-method to be 'native.")

(defvar url-gateway-shell-is-telnet nil
  "*Whether the login shell of the remote host is telnet.")

(defvar url-gateway-program-interactive nil
  "*Whether url needs to hand-hold the login program on the remote machine.")

(defvar url-gateway-handholding-login-regexp "ogin:"
  "*Regexp for when to send the username to the remote process.")

(defvar url-gateway-handholding-password-regexp "ord:"
  "*Regexp for when to send the password to the remote process.")

(defvar url-gateway-host-prompt-pattern "^[^#$%>;]*[#$%>;] *"
  "*Regexp used to detect when the login is finished on the remote host.")

(defvar url-gateway-telnet-ready-regexp "Escape character is .*"
  "*A regular expression that signifies url-gateway-telnet-program is
ready to accept input.")

(defvar url-local-rlogin-prog "rlogin"
  "*Program for local telnet connections.")

(defvar url-remote-rlogin-prog "rlogin"
  "*Program for remote telnet connections.")

(defvar url-local-telnet-prog "telnet"
  "*Program for local telnet connections.")

(defvar url-remote-telnet-prog "telnet"
  "*Program for remote telnet connections.")  

(defvar url-gateway-telnet-program "itelnet"
  "*Program to run in a subprocess when using gateway-method 'program.")

(defvar url-gateway-local-host-regexp nil
  "*If a host being connected to matches this regexp then the
connection is done natively, otherwise the process is started on
`url-gateway-host' instead.")

(defvar url-use-hypertext-dired t
  "*How to format directory listings.

If value is non-nil, use directory-files to list them out and
transform them into a hypertext document, then pass it through the
parse like any other document.

If value nil, just pass the directory off to dired using find-file.")

(defconst monthabbrev-alist
  '(("Jan" . 1) ("Feb" . 2) ("Mar" . 3) ("Apr" . 4) ("May" . 5) ("Jun" . 6)
    ("Jul" . 7) ("Aug" . 8) ("Sep" . 9) ("Oct" . 10) ("Nov" . 11) ("Dec" . 12)))

(defvar url-default-ports '(("http"   .  "80")
			    ("gopher" .  "70")
			    ("telnet" .  "23")
			    ("news"   . "119")
			    ("https"  . "443")
			    ("shttp"  .  "80"))
  "An assoc list of protocols and default port #s")

(defvar url-setup-done nil "*Has setup configuration been done?")

(defvar url-source nil
  "*Whether to force a sourcing of the next buffer.  This forces local
files to be read into a buffer, no matter what.  Gets around the
optimization that if you are passing it to a viewer, just make a
symbolic link, which looses if you want the source for inlined
images/etc.")

(defconst weekday-alist
  '(("Sunday" . 0) ("Monday" . 1) ("Tuesday" . 2) ("Wednesday" . 3)
    ("Thursday" . 4) ("Friday" . 5) ("Saturday" . 6)
    ("Tues" . 2) ("Thurs" . 4)
    ("Sun" . 0) ("Mon" . 1) ("Tue" . 2) ("Wed" . 3)
    ("Thu" . 4) ("Fri" . 5) ("Sat" . 6)))

(defconst monthabbrev-alist
  '(("Jan" . 1) ("Feb" . 2) ("Mar" . 3) ("Apr" . 4) ("May" . 5) ("Jun" . 6)
    ("Jul" . 7) ("Aug" . 8) ("Sep" . 9) ("Oct" . 10) ("Nov" . 11) ("Dec" . 12))
  )

(defvar url-lazy-message-time 0)

(defvar url-extensions-header "Security/Digest Security/SSL")

(defvar url-mailserver-syntax-table
  (copy-syntax-table emacs-lisp-mode-syntax-table)
  "*A syntax table for parsing the mailserver URL")

(modify-syntax-entry ?' "\"" url-mailserver-syntax-table)
(modify-syntax-entry ?` "\"" url-mailserver-syntax-table)
(modify-syntax-entry ?< "(>" url-mailserver-syntax-table)
(modify-syntax-entry ?> ")<" url-mailserver-syntax-table)
(modify-syntax-entry ?/ " " url-mailserver-syntax-table)

;;; Make OS/2 happy - yeeks
(defvar	tcp-binary-process-input-services nil
  "*Make OS/2 happy with our CRLF pairs...")

(provide 'url-vars)
