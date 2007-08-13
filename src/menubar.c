/* Implements an elisp-programmable menubar.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.
   Copyright (C) 1995 Tinker Systems and INS Engineering Corp.

This file is part of XEmacs.

XEmacs is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

XEmacs is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with XEmacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Synched up with: Not in FSF. */

/* #### There ain't much here because menubars have not been
   properly abstracted yet. */

#include <config.h>
#include "lisp.h"

#include "device.h"
#include "frame.h"
#include "menubar.h"
#include "redisplay.h"
#include "window.h"

int menubar_show_keybindings;
Lisp_Object Vmenubar_configuration;

Lisp_Object Qcurrent_menubar;

Lisp_Object Qactivate_menubar_hook, Vactivate_menubar_hook;

Lisp_Object Vmenubar_visible_p;

static Lisp_Object Vcurrent_menubar; /* DO NOT ever reference this.
					Always go through Qcurrent_menubar.
					See below. */
Lisp_Object Vblank_menubar;

int popup_menu_titles;

Lisp_Object Vmenubar_pointer_glyph;

static int
menubar_variable_changed (Lisp_Object sym, Lisp_Object *val,
			  Lisp_Object in_object, int flags)
{
  MARK_MENUBAR_CHANGED;
  return 0;
}

void
update_frame_menubars (struct frame *f)
{
  if (f->menubar_changed || f->windows_changed)
    MAYBE_FRAMEMETH (f, update_frame_menubars, (f));

  f->menubar_changed = 0;
}

void
free_frame_menubars (struct frame *f)
{
  /* If we had directly allocated any memory for the menubars instead
     of using all Lisp_Objects this is where we would now free it. */

  MAYBE_FRAMEMETH (f, free_frame_menubars, (f));
}

static void
menubar_visible_p_changed (Lisp_Object specifier, struct window *w,
			   Lisp_Object oldval)
{
  MARK_MENUBAR_CHANGED;
}

static void
menubar_visible_p_changed_in_frame (Lisp_Object specifier, struct frame *f,
				    Lisp_Object oldval)
{
  update_frame_menubars (f);
}

DEFUN ("popup-menu", Fpopup_menu, Spopup_menu, 1, 2, 0 /*
Pop up the given menu.
A menu description is a list of menu items, strings, and submenus.

The first element of a menu must be a string, which is the name of the menu.
This is the string that will be displayed in the parent menu, if any.  For
toplevel menus, it is ignored.  This string is not displayed in the menu
itself.

If an element of a menu is a string, then that string will be presented in
the menu as unselectable text.

If an element of a menu is a string consisting solely of hyphens, then that
item will be presented as a solid horizontal line.

If an element of a menu is a list, it is treated as a submenu.  The name of
that submenu (the first element in the list) will be used as the name of the
item representing this menu on the parent.

Otherwise, the element must be a vector, which describes a menu item.
A menu item can have any of the following forms:

 [ \"name\" callback <active-p> ]
 [ \"name\" callback <active-p> \"suffix\" ]
 [ \"name\" callback :<keyword> <value>  :<keyword> <value> ... ]

The name is the string to display on the menu; it is filtered through the
resource database, so it is possible for resources to override what string
is actually displayed.

If the `callback' of a menu item is a symbol, then it must name a command.
It will be invoked with `call-interactively'.  If it is a list, then it is
evaluated with `eval'.

The possible keywords are this:

 :active   <form>    Same as <active-p> in the first two forms: the
                     expression is evaluated just before the menu is
                     displayed, and the menu will be selectable only if
                     the result is non-nil.

 :suffix   \"string\"  Same as \"suffix\" in the second form: the suffix is
                     appended to the displayed name, providing a convenient
                     way of adding the name of a command's ``argument'' to
                     the menu, like ``Kill Buffer NAME''.

 :keys     \"string\"  Normally, the keyboard equivalents of commands in
                     menus are displayed when the `callback' is a symbol.
                     This can be used to specify keys for more complex menu
                     items.  It is passed through `substitute-command-keys'
                     first.

 :style    <style>   Specifies what kind of object this menu item is:

                        nil     A normal menu item.
                        toggle  A toggle button.
                        radio   A radio button.

                     The only difference between toggle and radio buttons is
                     how they are displayed.  But for consistency, a toggle
                     button should be used when there is one option whose
                     value can be turned on or off, and radio buttons should
                     be used when there is a set of mutually exclusive
                     options.  When using a group of radio buttons, you
                     should arrange for no more than one to be marked as
                     selected at a time.

 :selected <form>    Meaningful only when STYLE is `toggle' or `radio'.
                     This specifies whether the button will be in the
                     selected or unselected state.

For example:

 [ \"Save As...\"    write-file  t ]
 [ \"Revert Buffer\" revert-buffer (buffer-modified-p) ]
 [ \"Read Only\"     toggle-read-only :style toggle :selected buffer-read-only ]

See menubar.el for many more examples.
*/ )
     (menu_desc, event)
     Lisp_Object menu_desc, event;
{
  struct frame *f = decode_frame(Qnil);
  MAYBE_FRAMEMETH (f, popup_menu, (menu_desc,event));
  return Qnil;
}

void
syms_of_menubar (void)
{
  defsymbol (&Qcurrent_menubar, "current-menubar");
  defsubr (&Spopup_menu);
}

void
vars_of_menubar (void)
{
  {
    /* put in Vblank_menubar a menubar value which has no visible
     * items.  This is a bit tricky due to various quirks.  We
     * could use '(["" nil nil]), but this is apparently equivalent
     * to '(nil), and a new frame created with this menubar will
     * get a vertically-squished menubar.  If we use " " as the
     * button title instead of "", we get an etched button border.
     * So we use
     *  '(("No active menubar" ["" nil nil]))
     * which creates a menu whose title is "No active menubar",
     * and this works fine.
     */

    Lisp_Object menu_item[3];
    static CONST char *blank_msg = "No active menubar";

    menu_item[0] = build_string ("");
    menu_item[1] = Qnil;
    menu_item[2] = Qnil;
    Vblank_menubar = Fcons (Fcons (build_string (blank_msg), 
				   Fcons (Fvector (3, &menu_item[0]), 
					  Qnil)),
			    Qnil);
    Vblank_menubar = Fpurecopy (Vblank_menubar);
    staticpro (&Vblank_menubar);
  }

  DEFVAR_BOOL ("popup-menu-titles", &popup_menu_titles /*
If true, popup menus will have title bars at the top.
*/ );
  popup_menu_titles = 1;

  /* #### Replace current menubar with a specifier. */

  /* All C code must access the menubar via Qcurrent_menubar
     because it can be buffer-local.  Note that Vcurrent_menubar
     doesn't need to exist at all, except for the magic function. */

  DEFVAR_LISP_MAGIC ("current-menubar", &Vcurrent_menubar /*
The current menubar.  This may be buffer-local.

When the menubar is changed, the function `set-menubar-dirty-flag' has to
be called for the menubar to be updated on the frame.  See `set-menubar'
and `set-buffer-menubar'.

A menubar is a list of menus and menu-items.
A menu is a list of menu items, keyword-value pairs, strings, and submenus.

The first element of a menu must be a string, which is the name of the menu.
This is the string that will be displayed in the parent menu, if any.  For
toplevel menus, it is ignored.  This string is not displayed in the menu
itself.

Immediately following the name string of the menu, any of three
optional keyword-value pairs is permitted.

If an element of a menu (or menubar) is a string, then that string will be
presented as unselectable text.

If an element of a menu is a string consisting solely of hyphens, then that
item will be presented as a solid horizontal line.

If an element of a menu is a list, it is treated as a submenu.  The name of
that submenu (the first element in the list) will be used as the name of the
item representing this menu on the parent.

If an element of a menubar is `nil', then it is used to represent the
division between the set of menubar-items which are flushleft and those
which are flushright.

Otherwise, the element must be a vector, which describes a menu item.
A menu item can have any of the following forms:

 [ \"name\" callback <active-p> ]
 [ \"name\" callback <active-p> \"suffix\" ]
 [ \"name\" callback :<keyword> <value>  :<keyword> <value> ... ]

The name is the string to display on the menu; it is filtered through the
resource database, so it is possible for resources to override what string
is actually displayed.

If the `callback' of a menu item is a symbol, then it must name a command.
It will be invoked with `call-interactively'.  If it is a list, then it is
evaluated with `eval'.

The possible keywords are this:

 :active   <form>    Same as <active-p> in the first two forms: the
                     expression is evaluated just before the menu is
                     displayed, and the menu will be selectable only if
                     the result is non-nil.

 :suffix   \"string\"  Same as \"suffix\" in the second form: the suffix is
                     appended to the displayed name, providing a convenient
                     way of adding the name of a command's ``argument'' to
                     the menu, like ``Kill Buffer NAME''.

 :keys     \"string\"  Normally, the keyboard equivalents of commands in
                     menus are displayed when the `callback' is a symbol.
                     This can be used to specify keys for more complex menu
                     items.  It is passed through `substitute-command-keys'
                     first.

 :style    <style>   Specifies what kind of object this menu item is:

                        nil     A normal menu item.
                        toggle  A toggle button.
                        radio   A radio button.
                        button  A menubar button.

                     The only difference between toggle and radio buttons is
                     how they are displayed.  But for consistency, a toggle
                     button should be used when there is one option whose
                     value can be turned on or off, and radio buttons should
                     be used when there is a set of mutually exclusive
                     options.  When using a group of radio buttons, you
                     should arrange for no more than one to be marked as
                     selected at a time.

 :selected <form>    Meaningful only when STYLE is `toggle', `radio' or
                     `button'.  This specifies whether the button will be in
		     the selected or unselected state.

 :included <form>    This can be used to control the visibility of a menu or
		     menu item.  The form is evaluated and the menu or menu
		     item is only displayed if the result is non-nil.

 :config  <symbol>   This is an efficient shorthand for
		         :included (memq symbol menubar-configuration)
	             See the variable `menubar-configuration'.

 :filter <function>  A menu filter can only be used in a menu item list.
		     (i.e.:  not in a menu item itself).  It is used to
		     sensitize or incrementally create a submenu only when
		     it is selected by the user and not every time the
		     menubar is activated.  The filter function is passed
		     the list of menu items in the submenu and must return a
		     list of menu items to be used for the menu.  It is
		     called only when the menu is about to be displayed, so
		     other menus may already be displayed.  Vile and
		     terrible things will happen if a menu filter function
		     changes the current buffer, window, or frame.  It
		     also should not raise, lower, or iconify any frames.
		     Basically, the filter function should have no
		     side-effects.

For example:

 (\"File\"
  :filter file-menu-filter	; file-menu-filter is a function that takes
				; one argument (a list of menu items) and
				; returns a list of menu items
  [ \"Save As...\"    write-file  t ]
  [ \"Revert Buffer\" revert-buffer (buffer-modified-p) ]
  [ \"Read Only\"     toggle-read-only :style toggle
		      :selected buffer-read-only ]
  )

See x-menubar.el for many more examples.

After the menubar is clicked upon, but before any menus are popped up,
the functions on the `activate-menubar-hook' are invoked to make top-level
changes to the menus and menubar.  Note, however, that the use of menu
filters (using the :filter keyword) is usually a more efficient way to
dynamically alter or sensitize menus.
*/, menubar_variable_changed);

  Vcurrent_menubar = Qnil;

  DEFVAR_LISP ("activate-menubar-hook", &Vactivate_menubar_hook /*
Function or functions called before a menubar menu is pulled down.
These functions are called with no arguments, and should interrogate and
modify the value of `current-menubar' as desired.

The functions on this hook are invoked after the mouse goes down, but before
the menu is mapped, and may be used to activate, deactivate, add, or delete
items from the menus.  However, it is probably the case that using a :filter
keyword in a submenu would be a more efficient way of updating menus.  See
the documentation of `current-menubar'.

These functions may return the symbol `t' to assert that they have made
no changes to the menubar.  If any other value is returned, the menubar is
recomputed.  If `t' is returned but the menubar has been changed, then the
changes may not show up right away.  Returning `nil' when the menubar has
not changed is not so bad; more computation will be done, but redisplay of
the menubar will still be performed optimally.
*/ );
  Vactivate_menubar_hook = Qnil;
  defsymbol (&Qactivate_menubar_hook, "activate-menubar-hook");

  DEFVAR_BOOL ("menubar-show-keybindings", &menubar_show_keybindings /*
If true, the menubar will display keyboard equivalents.
If false, only the command names will be displayed.
*/ );
  menubar_show_keybindings = 1;

  DEFVAR_LISP_MAGIC ("menubar-configuration", &Vmenubar_configuration /*
A list of symbols, against which the value of the :config tag for each
menubar item will be compared.  If a menubar item has a :config tag, then
it is omitted from the menubar if that tag is not a member of the
`menubar-configuration' list.
*/ , menubar_variable_changed);
  Vmenubar_configuration = Qnil;

  DEFVAR_LISP ("menubar-pointer-glyph", &Vmenubar_pointer_glyph /*
*The shape of the mouse-pointer when over the menubar.
This is a glyph; use `set-glyph-image' to change it.
If unspecified in a particular domain, the window-system-provided
default pointer is used.
*/ );

  Fprovide (intern ("menubar"));
}

void
specifier_vars_of_menubar (void)
{
  DEFVAR_SPECIFIER ("menubar-visible-p", &Vmenubar_visible_p /*
*Whether the menubar is visible.
This is a specifier; use `set-specifier' to change it.
*/ );
  Vmenubar_visible_p = Fmake_specifier (Qboolean);

  set_specifier_fallback (Vmenubar_visible_p, list1 (Fcons (Qnil, Qt)));
  set_specifier_caching (Vmenubar_visible_p,
			 slot_offset (struct window,
				      menubar_visible_p),
			 menubar_visible_p_changed,
			 slot_offset (struct frame,
				      menubar_visible_p),
			 menubar_visible_p_changed_in_frame);
}

void
complex_vars_of_menubar (void)
{
  Vmenubar_pointer_glyph = Fmake_glyph_internal (Qpointer);
}
