/* Various initialization function prototypes.
   Copyright (C) 1995 Board of Trustees, University of Illinois.

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

#ifndef _XEMACS_SYMSINIT_H_
#define _XEMACS_SYMSINIT_H_

/* Earliest environment initializations (dump-time and run-time). */

void init_data_very_early (void);
void init_floatfns_very_early (void);
void init_free_hook (void);
void init_intl_very_early (void);
void init_process_times_very_early (void);
void init_ralloc (void);
void init_signals_very_early (void);

/* Early Lisp-engine initialization (dump-time only). */

void init_alloc_once_early (void);
void init_symbols_once_early (void);
void init_errors_once_early (void);
void init_opaque_once_early (void);

/* Declare the built-in symbols and primitives (dump-time only). */

void syms_of_abbrev (void);
void syms_of_alloc (void);
void syms_of_balloon_x (void);
void syms_of_buffer (void);
void syms_of_bytecode (void);
void syms_of_callint (void);
void syms_of_callproc (void);
void syms_of_casefiddle (void);
void syms_of_casetab (void);
void syms_of_chartab (void);
void syms_of_cmdloop (void);
void syms_of_cmds (void);
void syms_of_console_tty (void);
void syms_of_console (void);
void syms_of_data (void);
void syms_of_dbm (void);
void syms_of_debug (void);
void syms_of_device_tty (void);
void syms_of_device_x (void);
void syms_of_device (void);
void syms_of_dialog_x (void);
void syms_of_dialog (void);
void syms_of_dired (void);
void syms_of_doc (void);
void syms_of_editfns (void);
void syms_of_elhash (void);
void syms_of_emacs (void);
void syms_of_energize (void);
void syms_of_eval (void);
void syms_of_event_stream (void);
void syms_of_event_Xt (void);
void syms_of_events (void);
void syms_of_extents (void);
void syms_of_faces (void);
void syms_of_fileio (void);
void syms_of_filelock (void);
void syms_of_floatfns (void);
void syms_of_fns (void);
void syms_of_font_lock (void);
void syms_of_frame_x (void);
void syms_of_frame (void);
void syms_of_free_hook (void);
void syms_of_general (void);
void syms_of_glyphs_x (void);
void syms_of_glyphs (void);
void syms_of_gui_x (void);
void syms_of_gui (void);
void syms_of_indent (void);
void syms_of_intl (void);
void syms_of_keymap (void);
void syms_of_lread (void);
void syms_of_macros (void);
void syms_of_marker (void);
void syms_of_md5 (void);
void syms_of_menubar_x (void);
void syms_of_menubar (void);
void syms_of_minibuf (void);
void syms_of_mule (void);
void syms_of_mule_canna (void);
void syms_of_mule_ccl (void);
void syms_of_mule_charset (void);
void syms_of_mule_coding (void);
void syms_of_mule_wnn (void);
void syms_of_objects_tty (void);
void syms_of_objects_x (void);
void syms_of_objects (void);
void syms_of_print (void);
void syms_of_process (void);
void syms_of_profile (void);
void syms_of_ralloc (void);
void syms_of_rangetab (void);
void syms_of_redisplay (void);
void syms_of_scrollbar (void);
void syms_of_search (void);
void syms_of_signal (void);
void syms_of_sound (void);
void syms_of_specifier (void);
void syms_of_sunpro (void);
void syms_of_symbols (void);
void syms_of_syntax (void);
void syms_of_toolbar (void);
void syms_of_tooltalk (void);
void syms_of_undo (void);
void syms_of_window (void);
void syms_of_xselect (void);

/* Initialize the console types (dump-time only). */

void console_type_create (void);
void console_type_create_stream (void);
void console_type_create_tty (void);
void console_type_create_device_tty (void);
void console_type_create_frame_tty (void);
void console_type_create_objects_tty (void);
void console_type_create_redisplay_tty (void);
void console_type_create_x (void);
void console_type_create_device_x (void);
void console_type_create_frame_x (void);
void console_type_create_glyphs_x (void);
void console_type_create_menubar_x (void);
void console_type_create_objects_x (void);
void console_type_create_redisplay_x (void);
void console_type_create_scrollbar_x (void);
void console_type_create_toolbar_x (void);

/* Initialize the specifier types (dump-time only). */

void specifier_type_create (void);
void specifier_type_create_image (void);
void specifier_type_create_objects (void);
void specifier_type_create_toolbar (void);

/* Initialize the structure types (dump-time only). */

void structure_type_create (void);
void structure_type_create_chartab (void);
void structure_type_create_faces (void);
void structure_type_create_rangetab (void);

/* Initialize the image instantiator types (dump-time only). */

void image_instantiator_format_create (void);
void image_instantiator_format_create_glyphs_x (void);

/* Initialize the lstream types (dump-time only). */

void lstream_type_create (void);
void lstream_type_create_mule_coding (void);
void lstream_type_create_print (void);

/* Allow for Fprovide() (dump-time only). */

void init_provide_once (void);

/* Initialize most variables (dump-time only). */

void vars_of_abbrev (void);
void vars_of_alloc (void);
void vars_of_balloon_x (void);
void vars_of_buffer (void);
void vars_of_bytecode (void);
void vars_of_callint (void);
void vars_of_callproc (void);
void vars_of_cmdloop (void);
void vars_of_cmds (void);
void vars_of_console (void);
void vars_of_console_stream (void);
void vars_of_console_tty (void);
void vars_of_data (void);
void vars_of_dbm (void);
void vars_of_debug (void);
void vars_of_device_x (void);
void vars_of_device (void);
void vars_of_dialog_x (void);
void vars_of_dialog (void);
void vars_of_dired (void);
void vars_of_doc (void);
void vars_of_editfns (void);
void vars_of_elhash (void);
void vars_of_emacs (void);
void vars_of_energize (void);
void vars_of_eval (void);
void vars_of_event_stream (void);
void vars_of_event_tty (void);
void vars_of_event_Xt (void);
void vars_of_events (void);
void vars_of_extents (void);
void vars_of_faces (void);
void vars_of_fileio (void);
void vars_of_filelock (void);
void vars_of_floatfns (void);
void vars_of_font_lock (void);
void vars_of_frame_tty (void);
void vars_of_frame_x (void);
void vars_of_frame (void);
void vars_of_glyphs_x (void);
void vars_of_glyphs (void);
void vars_of_gui_x (void);
void vars_of_gui (void);
void vars_of_input_method_motif (void);
void vars_of_input_method_xlib (void);
void vars_of_indent (void);
void vars_of_insdel (void);
void vars_of_intl (void);
void vars_of_keymap (void);
void vars_of_lread (void);
void vars_of_lstream (void);
void vars_of_macros (void);
void vars_of_md5 (void);
void vars_of_menubar_x (void);
void vars_of_menubar (void);
void vars_of_minibuf (void);
void vars_of_mule (void);
void vars_of_mule_canna (void);
void vars_of_mule_charset (void);
void vars_of_mule_coding (void);
void vars_of_mule_wnn (void);
void vars_of_objects (void);
void vars_of_objects_tty (void);
void vars_of_objects_x (void);
void vars_of_print (void);
void vars_of_process (void);
void vars_of_profile (void);
void vars_of_ralloc (void);
void vars_of_redisplay (void);
void vars_of_scrollbar_x (void);
void vars_of_scrollbar (void);
void vars_of_search (void);
void vars_of_sound (void);
void vars_of_specifier (void);
void vars_of_sunpro (void);
void vars_of_symbols (void);
void vars_of_syntax (void);
void vars_of_toolbar (void);
void vars_of_tooltalk (void);
void vars_of_undo (void);
void vars_of_window (void);
void vars_of_xselect (void);

/* Initialize specifier variables (dump-time only). */

void specifier_vars_of_glyphs (void);
void specifier_vars_of_menubar (void);
void specifier_vars_of_redisplay (void);
void specifier_vars_of_scrollbar (void);
void specifier_vars_of_toolbar (void);
void specifier_vars_of_window (void);

/* Initialize variables with complex dependencies
   on other variables (dump-time only). */

void complex_vars_of_regex (void);
void complex_vars_of_search (void);
void complex_vars_of_event_stream (void);
void complex_vars_of_extents (void);
void complex_vars_of_faces (void);
void complex_vars_of_mule_charset (void);
void complex_vars_of_mule_coding (void);
void complex_vars_of_glyphs (void);
void complex_vars_of_glyphs_x (void);
void complex_vars_of_alloc (void);
void complex_vars_of_menubar (void);
void complex_vars_of_scrollbar (void);
void complex_vars_of_frame (void);
void complex_vars_of_energize (void);
void complex_vars_of_casetab (void);
void complex_vars_of_syntax (void);
void complex_vars_of_chartab (void);
void complex_vars_of_buffer (void);
void complex_vars_of_console (void);
void complex_vars_of_minibuf (void);
void complex_vars_of_callproc (void);
void complex_vars_of_filelock (void);
void complex_vars_of_keymap (void);

/* Reset the Lisp engine (run-time only). */

void reinit_alloc (void);
void reinit_eval (void);

/* Late initialization -- stuff pertaining only to interactive usage,
   I/O, or Lisp reading. (Dump-time and run-time.) */

void init_buffer (void);
void init_callproc (void);
void init_console_stream (void);
void init_device_tty (void);
void init_dosfns (void);
void init_editfns (void);
void init_environment (void);
void init_event_Xt_late (void);
void init_event_stream (void);
void init_event_tty_late (void);
void init_event_unixoid (void);
void init_gif_err (void);
void init_lread (void);
void init_macros (void);
/* Not named init_process in order to avoid conflict with NS 3.3 */
void init_xemacs_process (void);
void init_redisplay (void);
void init_sunpro (void);

#endif /* _XEMACS_SYMSINIT_H_ */
