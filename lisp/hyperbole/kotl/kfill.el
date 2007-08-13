;;!emacs
;;
;; FILE:         kfill.el
;; SUMMARY:      Fill and justify koutline cells (adapted from Kyle Jones' filladapt).
;; USAGE:        GNU Emacs Lisp Library
;; KEYWORDS:     outlines, wp
;;
;; AUTHOR:       Bob Weiner
;; ORIG-DATE:    23-Jan-94
;; LAST-MOD:      4-Nov-95 at 04:53:42 by Bob Weiner
;;; ************************************************************************
;;; Public variables
;;; ************************************************************************

(defvar kfill:function-table
  (progn
    (if (featurep 'filladapt)
	(progn (load "fill")     ;; Save basic fill-paragraph function.
	       (load "simple"))) ;; Save basic do-auto-fill function.
    (list (cons 'fill-paragraph (symbol-function 'fill-paragraph))
	  (cons 'do-auto-fill (symbol-function 'do-auto-fill))))
  "Table containing the old function definitions that kfill overrides.")

(defvar kfill:prefix-table
  '(
    ;; Lists with hanging indents, e.g.
    ;; 1. xxxxx   or   1)  xxxxx   etc.
    ;;    xxxxx            xxx
    ;;
    ;; Be sure pattern does not match to:  (last word in parens starts
    ;; newline)
    (" *(?\\([0-9][0-9a-z.]*\\|[a-z][0-9a-z.]\\)) +" . kfill:hanging-list)
    (" *\\([0-9]+[a-z.]+[0-9a-z.]*\\|[0-9]+\\|[a-z]\\)\\([.>] +\\|  +\\)"
     . kfill:hanging-list)
    ;; Included text in news or mail replies
    ("[ \t]*\\(>+ *\\)+" . kfill:normal-included-text)
    ;; Included text generated by SUPERCITE.  We can't hope to match all
    ;; the possible variations, your mileage may vary.
    ("[ \t]*[A-Za-z0-9][^'`\"< \t\n]*>[ \t]*" . kfill:supercite-included-text)
    ;; Lisp comments
    ("[ \t]*\\(;+[ \t]*\\)+" . kfill:lisp-comment)
    ;; UNIX shell comments
    ("[ \t]*\\(#+[ \t]*\\)+" . kfill:sh-comment)
    ;; Postscript comments
    ("[ \t]*\\(%+[ \t]*\\)+" . kfill:postscript-comment)
    ;; C++ comments
    ("[ \t]*//[/ \t]*" . kfill:c++-comment)
    ("[?!~*+ -]+ " . kfill:hanging-list)
    ;; This keeps normal paragraphs from interacting unpleasantly with
    ;; the types given above.
    ("[^ \t/#%?!~*+-]" . kfill:normal)
    )
"Value is an alist of the form

   ((REGXP . FUNCTION) ...)

When fill-paragraph or do-auto-fill is called, the REGEXP of each alist
element is compared with the beginning of the current line.  If a match
is found the corresponding FUNCTION is called.  FUNCTION is called with
one argument, which is non-nil when invoked on the behalf of
fill-paragraph, nil for do-auto-fill.  It is the job of FUNCTION to set
the values of the paragraph-* variables (or set a clipping region, if
paragraph-start and paragraph-separate cannot be made discerning enough)
so that fill-paragraph and do-auto-fill work correctly in various
contexts.")

;;; ************************************************************************
;;; Public functions
;;; ************************************************************************

(defun do-auto-fill ()
  (save-restriction
    (if (null fill-prefix)
	(let ((paragraph-ignore-fill-prefix nil)
	      ;; Need this or Emacs 19 ignores fill-prefix when
	      ;; inside a comment.
	      (comment-multi-line t)
	      fill-prefix)
	  (kfill:adapt nil)
	  (kfill:funcall 'do-auto-fill))
      (kfill:funcall 'do-auto-fill))))

(defun fill-paragraph (arg &optional skip-prefix-remove)
  "Fill paragraph at or after point.  Prefix ARG means justify as well."
  (interactive "*P")
  ;; Emacs 19 expects a specific symbol here.
  (if (and arg (not (symbolp arg))) (setq arg 'full))
  (or skip-prefix-remove (kfill:remove-paragraph-prefix))
  (save-restriction
    (catch 'done
      (if (null fill-prefix)
	(let ((paragraph-ignore-fill-prefix nil)
	      ;; Need this or Emacs 19 ignores fill-prefix when
	      ;; inside a comment.
	      (comment-multi-line t)
	      (paragraph-start paragraph-start)
	      (paragraph-separate paragraph-separate)
	      fill-prefix)
	    (if (kfill:adapt t)
		(throw 'done (kfill:funcall 'fill-paragraph arg)))))
      ;; Kfill:adapt failed or fill-prefix is set, so do a basic
      ;; paragraph fill as adapted from par-align.el.
      (kfill:fill-paragraph arg skip-prefix-remove))))

;;;
;;; Redefine this function so that it sets 'fill-prefix-prev' also.
;;;
(defun set-fill-prefix (&optional turn-off)
  "Set the fill-prefix to the current line up to point.
Also sets fill-prefix-prev to previous value of fill-prefix.
Filling expects lines to start with the fill prefix and reinserts the fill
prefix in each resulting line."
  (interactive)
  (setq fill-prefix-prev fill-prefix
	fill-prefix (if turn-off
			nil
		      (buffer-substring
		       (save-excursion (beginning-of-line) (point))
		       (point))))
  (if (equal fill-prefix-prev "")
      (setq fill-prefix-prev nil))
  (if (equal fill-prefix "")
      (setq fill-prefix nil))
  (if fill-prefix
      (message "fill-prefix: \"%s\"" fill-prefix)
    (message "fill-prefix cancelled")))

;;; ************************************************************************
;;; Private functions
;;; ************************************************************************

(defun kfill:adapt (paragraph)
  (let ((table kfill:prefix-table)
	case-fold-search
	success )
    (save-excursion
      (beginning-of-line)
      (while table
	(if (not (looking-at (car (car table))))
	    (setq table (cdr table))
	  (funcall (cdr (car table)) paragraph)
	  (setq success t table nil))))
    success ))

(defun kfill:c++-comment (paragraph)
  (setq fill-prefix (buffer-substring (match-beginning 0) (match-end 0)))
  (if paragraph
      (setq paragraph-separate "^[^ \t/]")))

(defun kfill:fill-paragraph (justify-flag &optional leave-prefix)
  (save-excursion
    (end-of-line)
    ;; Backward to para begin
    (re-search-backward (concat "\\`\\|" paragraph-separate))
    (forward-line 1)
    (let ((region-start (point)))
      (forward-line -1)
      (let ((from (point)))
	(forward-paragraph)
	;; Forward to real paragraph end
	(re-search-forward (concat "\\'\\|" paragraph-separate))
	(or (= (point) (point-max)) (beginning-of-line))
	(or leave-prefix
	    (kfill:replace-string
	      (or fill-prefix fill-prefix-prev)
	      "" nil region-start (point)))
	(fill-region-as-paragraph from (point) justify-flag)))))

(defun kfill:funcall (function &rest args)
  (apply (cdr (assq function kfill:function-table)) args))

(defun kfill:hanging-list (paragraph)
  (let (prefix match beg end)
    (setq prefix (make-string (- (match-end 0) (match-beginning 0)) ?\ ))
    (if paragraph
	(progn
	  (setq match (buffer-substring (match-beginning 0) (match-end 0)))
	  (if (string-match "^ +$" match)
	      (save-excursion
		(while (and (not (bobp)) (looking-at prefix))
		  (forward-line -1))

		(cond ((kfill:hanging-p)
		       (setq beg (point)))
		      (t (setq beg (progn (forward-line 1) (point))))))
	    (setq beg (point)))
	  (save-excursion
	    (forward-line)
	    (while (and (looking-at prefix)
			(not (equal (char-after (match-end 0)) ?\ )))
	      (forward-line))
	    (setq end (point)))
	  (narrow-to-region beg end)))
    (setq fill-prefix prefix)))

(defun kfill:hanging-p ()
  "Return non-nil iff point is in front of a hanging list."
  (eval kfill:hanging-expression))

(defun kfill:lisp-comment (paragraph)
  (setq fill-prefix (buffer-substring (match-beginning 0) (match-end 0)))
  (if paragraph
      (setq paragraph-separate
	    (concat "^" fill-prefix " *;\\|^"
		    (kfill:negate-string fill-prefix)))))

(defun kfill:negate-string (string)
  (let ((len (length string))
	(i 0) string-list)
    (setq string-list (cons "\\(" nil))
    (while (< i len)
      (setq string-list
	    (cons (if (= i (1- len)) "" "\\|")
		  (cons "]"
			(cons (substring string i (1+ i))
			      (cons "[^"
				    (cons (regexp-quote (substring string 0 i))
					  string-list)))))
	    i (1+ i)))
    (setq string-list (cons "\\)" string-list))
    (apply 'concat (nreverse string-list))))

(defun kfill:normal (paragraph)
  (if paragraph
      (setq paragraph-separate
	    (concat paragraph-separate "\\|^[ \t/#%?!~*+-]"))))

(defun kfill:normal-included-text (paragraph)
  (setq fill-prefix (buffer-substring (match-beginning 0) (match-end 0)))
  (if paragraph
      (setq paragraph-separate
	    (concat "^" fill-prefix " *>\\|^"
		    (kfill:negate-string fill-prefix)))))

(defun kfill:postscript-comment (paragraph)
  (setq fill-prefix (buffer-substring (match-beginning 0) (match-end 0)))
  (if paragraph
      (setq paragraph-separate
	    (concat "^" fill-prefix " *%\\|^"
		    (kfill:negate-string fill-prefix)))))

(defun kfill:remove-paragraph-prefix (&optional indent-str)
  "Remove fill prefix from current paragraph."
  (save-excursion
    (end-of-line)
    ;; Backward to para begin
    (re-search-backward (concat "\\`\\|" paragraph-separate))
    (forward-line 1)
    (let ((region-start (point)))
      (forward-line -1)
      (forward-paragraph)
      ;; Forward to real paragraph end
      (re-search-forward (concat "\\'\\|" paragraph-separate))
      (or (= (point) (point-max)) (beginning-of-line))
      (kfill:replace-string (or fill-prefix fill-prefix-prev)
				(if (eq major-mode 'kotl-mode)
				    (or indent-str
					(make-string (kcell-view:indent) ?  ))
				  "")
				nil region-start (point)))))

(defun kfill:replace-string (fill-str-prev fill-str &optional suffix start end)
  "Replace whitespace separated FILL-STR-PREV with FILL-STR.
Optional SUFFIX non-nil means replace at ends of lines, default is beginnings.
Optional arguments START and END specify the replace region, default is the
current region."
  (if fill-str-prev
      (progn (if start
		 (let ((s (min start end)))
		   (setq end (max start end)
			 start s))
	       (setq start (region-beginning)
		     end (region-end)))
	     (if (not fill-str) (setq fill-str ""))
	     (save-excursion
	       (save-restriction
		 (narrow-to-region start end)
		 (goto-char (point-min))
		 (let ((prefix
			(concat
			 (if suffix nil "^")
			 "[ \t]*"
			 (regexp-quote
			  ;; Get non-whitespace separated fill-str-prev
			  (substring
			   fill-str-prev
			   (or (string-match "[^ \t]" fill-str-prev) 0)
			   (if (string-match
				"[ \t]*\\(.*[^ \t]\\)[ \t]*$"
				fill-str-prev)
			       (match-end 1))))
			 "[ \t]*"
			 (if suffix "$"))))
		   (while (re-search-forward prefix nil t)
		     (replace-match fill-str nil t))))))))

(defun kfill:sh-comment (paragraph)
  (setq fill-prefix (buffer-substring (match-beginning 0) (match-end 0)))
  (if paragraph
      (setq paragraph-separate
	    (concat "^" fill-prefix " *#\\|^"
		    (kfill:negate-string fill-prefix)))))

(defun kfill:supercite-included-text (paragraph)
  (setq fill-prefix (buffer-substring (match-beginning 0) (match-end 0)))
  (if paragraph
      (setq paragraph-separate
	    (concat "^" (kfill:negate-string fill-prefix)))))

;;; ************************************************************************
;;; Private variables
;;; ************************************************************************

(defconst kfill:hanging-expression
  (cons 'or
	(delq nil (mapcar (function
			    (lambda (pattern-type)
			      (if (eq (cdr pattern-type) 'kfill:hanging-list)
				  (list 'looking-at (car pattern-type)))))
			  kfill:prefix-table)))
  "Conditional expression used to test for hanging indented lists.")

(defvar fill-prefix-prev nil
  "Prior string inserted at front of new line during filling, or nil for none.
Setting this variable automatically makes it local to the current buffer.")
(make-variable-buffer-local 'fill-prefix-prev)


(provide 'kfill)
