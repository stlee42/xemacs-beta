;;; virtex.el - Common code for all TeX formats.

;; $Id: virtex.el,v 1.1 1997/02/20 02:15:44 steve Exp $

;;; Code:

(TeX-add-style-hook "virtex"
 (function
  (lambda ()
    (TeX-add-symbols "/" "above" "abovedisplayshortskip"
		     "abovedisplayskip" "abovewithdelims" "accent"
		     "adjdemerits" "advance" "afterassignment"
		     "aftergroup" "atop" "atopwithdelims" "badness"
		     "baselineskip" "batchmode" "begingroup"
		     "belowdisplayshortskip" "belowdisplayskip"
		     "binoppenalty" "botmark" "box" "boxmaxdepth"
		     "brokenpenalty" "catcode" "char" "chardef"
		     "cleaders" "closein" "closeout" "clubpenalty"
		     "copy" "count" "countdef" "cr" "crcr" "csname"
		     "day" "deadcycles" "def" "defaulthyphenchar"
		     "defaultskewchar" "delcode" "delimiter"
		     "delimiterfactor" "delimitershortfall" "dimen"
		     "dimendef" "discretionary" "displayindent"
		     "displaylimits" "displaystyle"
		     "displaywidowpenalty" "displaywidth" "divide"
		     "doublehyphendemerits" "dp" "dump" "edef" "else"
		     "emergencystretch" "end" "endcsname" "endgroup"
		     "endinput" "endlinechar" "eqno" "errhelp"
		     "errmessage" "errorcontextlines" "errorstopmode"
		     "escapechar" "everycr" "everydisplay"
		     "everyhbox" "everyjob" "everymath" "everypar"
		     "everyvbox" "exhyphenpenalty" "expandafter"
		     "fam" "fi" "finalhyphendemerits" "firstmark"
		     "floatingpenalty" "font" "fontdimen" "fontname"
		     "futurelet" "gdef" "global" "globaldefs"
		     "halign" "hangafter" "hangindent" "hbadness"
		     "hbox" "hfil" "hfill" "hfilneg" "hfuzz"
		     "hoffset" "holdinginserts" "hrule" "hsize"
		     "hskip" "hss" "ht" "hyphenpenation" "hyphenchar"
		     "hyphenpenalty" "if" "ifcase" "ifcat" "ifdim"
		     "ifeof" "iffalse" "ifhbox" "ifinner" "ifhmode"
		     "ifmmode" "ifnum" "ifodd" "iftrue" "ifvbox"
		     "ifvoid" "ifx" "ignorespaces" "immediate"
		     "indent" "input" "inputlineno" "insert"
		     "insertpenalties" "interlinepenalty" "jobname"
		     "kern" "language" "lastbox" "lastkern"
		     "lastpenalty" "lastskip" "lccode" "leaders"
		     "left" "lefthyphenmin" "leftskip" "leqno" "let"
		     "limits" "linepenalty" "lineskip"
		     "lineskiplimit" "long" "looseness" "lower"
		     "lowercase" "mag" "markaccent" "mathbin"
		     "mathchar" "mathchardef" "mathchoise"
		     "mathclose" "mathcode" "mathinner" "mathhop"
		     "mathopen" "mathord" "mathpunct" "mathrel"
		     "mathsurround" "maxdeadcycles" "maxdepth"
		     "meaning" "medmuskip" "message" "mkern" "month"
		     "moveleft" "moveright" "mskip" "multiply"
		     "muskip" "muskipdef" "newlinechar" "noalign"
		     "noboundary" "noexpand" "noindent" "nolimits"
		     "nonscript" "nonstopmode" "nulldelimiterspace"
		     "nullfont" "number" "omit" "openin" "openout"
		     "or" "outer" "output" "outputpenalty"
		     "overfullrule" "parfillskip" "parindent"
		     "parskip" "pausing" "postdisplaypenalty"
		     "predisplaypenalty" "predisplaysize"
		     "pretolerance" "relpenalty" "rightskip"
		     "scriptspace" "showboxbreadth" "showboxdepth"
		     "smallskipamount" "spaceskip" "splitmaxdepth"
		     "splittopskip" "tabskip" "thickmuskip"
		     "thinmuskip" "time" "tolerance" "topskip"
		     "tracingcommands" "tracinglostchars"
		     "tracingmacros" "tracingonline" "tracingoutput"
		     "tracingpages" "tracingparagraphs"
		     "tracingrestores" "tracingstats" "uccode"
		     "uchyph" "underline" "unhbox" "unhcopy" "unkern"
		     "unpenalty" "unskip" "unvbox" "unvcopy"
		     "uppercase" "vadjust" "valign" "vbadness" "vbox"
		     "vcenter" "vfil" "vfill" "vfilneg" "vfuzz"
		     "voffset" "vrule" "vsize" "vskip" "vss" "vtop"
		     "wd" "widowpenalty" "write" "xdef" "xleaders"
		     "xspaceskip" "year"))))

;;; virtex.el ends here
