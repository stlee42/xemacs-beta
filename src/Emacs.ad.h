/*	Do not edit this file!
  	Automatically generated from /m/xemacs-19.14/etc/Emacs.ad
 */
"Emacs.modeline*attributeForeground:	Black",
"Emacs.modeline*attributeBackground:	Gray75",
"Emacs.text-cursor*attributeBackground:	Red3",
"*menubar*Foreground:			Gray30",
"*menubar*Background:			Gray75",
"*menubar*buttonForeground:		Blue",
"*XlwMenu*selectColor:			ForestGreen",
"*XmToggleButton*selectColor:		ForestGreen",
"*popup*Foreground:			Black",
"*popup*Background:			Gray75",
"*dialog*Foreground:			Black",
"*dialog*Background:			#A5C0C1",
"*dialog*XmTextField*Background:		WhiteSmoke",
"*dialog*XmText*Background:		WhiteSmoke",
"*dialog*XmList*Background:		WhiteSmoke",
"*dialog*Command*Background:		WhiteSmoke",
"*XlwScrollBar*Foreground:		Gray30",
"*XlwScrollBar*Background:		Gray75",
"*XmScrollBar*Foreground:		Gray30",
"*XmScrollBar*Background:		Gray75",
"*topToolBarShadowColor:			Gray90",
"*bottomToolBarShadowColor:		Gray40",
"*backgroundToolBarColor:		Gray75",
"*toolBarShadowThickness:		2",
"*menubar*Font: 			-*-helvetica-bold-r-*-*-*-120-*-*-*-*-iso8859-*",
"*popup*Font:			-*-helvetica-bold-r-*-*-*-120-*-*-*-*-iso8859-*",
"*XmDialogShell*FontList:	-*-helvetica-bold-r-*-*-*-120-*-*-*-*-iso8859-*",
"*XmTextField*FontList:		-*-courier-medium-r-*-*-*-120-*-*-*-*-iso8859-*",
"*XmText*FontList:		-*-courier-medium-r-*-*-*-120-*-*-*-*-iso8859-*",
"*XmList*FontList:		-*-courier-medium-r-*-*-*-120-*-*-*-*-iso8859-*",
"*Dialog*Font:			-*-helvetica-bold-r-*-*-*-140-*-*-*-*-iso8859-*",
"*dialog*button1.accelerators:#override\
<KeyPress>Return: ArmAndActivate()\\n\
<KeyPress>KP_Enter: ArmAndActivate()\\n\
Ctrl<KeyPress>m: ArmAndActivate()\\n",
"*XmTextField*translations: #override\\n\
	!<Key>osfBackSpace:	delete-previous-character()\\n\
	!<Key>osfDelete:	delete-previous-character()\\n\
	!Ctrl<Key>h: 		delete-previous-character()\\n\
	!Ctrl<Key>d: 		delete-next-character()\\n\
	!Meta<Key>osfDelete:	delete-previous-word()\\n\
	!Meta<Key>osfBackSpace:	delete-previous-word()\\n\
	!Meta<Key>d:		delete-next-word()\\n\
	!Ctrl<Key>k:		delete-to-end-of-line()\\n\
	!Ctrl<Key>g:		process-cancel()\\n\
	!Ctrl<Key>b:		backward-character()\\n\
	!<Key>osfLeft:		backward-character()\\n\
	!Ctrl<Key>f:		forward-character()\\n\
	!<Key>osfRight:		forward-character()\\n\
	!Meta<Key>b:		backward-word()\\n\
	!Meta<Key>osfLeft:	backward-word()\\n\
	!Meta<Key>f:		forward-word()\\n\
	!Meta<Key>osfRight:	forward-word()\\n\
	!Ctrl<Key>e:		end-of-line()\\n\
	!Ctrl<Key>a:		beginning-of-line()\\n\
	!Ctrl<Key>w:		cut-clipboard()\\n\
	!Meta<Key>w:		copy-clipboard()\\n\
	<Btn2Up>:		copy-primary()\\n",
"*dialog*XmPushButton*translations:#override\\n\
    <Btn1Down>:         Arm()\\n\
    <Btn1Down>,<Btn1Up>: Activate()\
			Disarm()\\n\
    <Btn1Down>(2+):     MultiArm()\\n\
    <Btn1Up>(2+):       MultiActivate()\\n\
    <Btn1Up>:           Activate()\
		        Disarm()\\n\
    <Key>osfSelect:  	ArmAndActivate()\\n\
    <Key>osfActivate:   ArmAndActivate()\\n\
    <Key>osfHelp:	Help()\\n\
    ~Shift ~Meta ~Alt <Key>Return:	ArmAndActivate()\\n\
    <EnterWindow>:      Enter()\\n\
    <LeaveWindow>:      Leave()\\n",
