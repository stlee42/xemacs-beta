;;; dutch.el - Setup AUC TeX for editing Dutch text.

;; $Id: dutch.el,v 1.1 1997/08/30 02:45:15 steve Exp $

;;; Code:

(TeX-add-style-hook "dutch"
 (function (lambda ()
   (run-hooks 'TeX-language-nl-hook))))

;;; dutch.el ends here