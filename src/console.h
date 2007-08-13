/* Define console object for XEmacs.
   Copyright (C) 1996 Ben Wing

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

/* Written by Ben Wing. */

#ifndef _XEMACS_CONSOLE_H_
#define _XEMACS_CONSOLE_H_

/* Devices and consoles are similar entities.  The idea is that
   a console represents a physical keyboard/mouse/other-input-source
   while a device represents a display where frames appear on.
   In the X world, a console is a "Display" while a device is a
   "Screen".  Implementationally, it can sometimes get confusing:
   under X, multiple devices on a single console are different
   "Display" connections to what is in reality the same Display on
   the same server.  Because of this, input comes from the device
   and not from the console.  This is OK because events are basically
   always tagged to a particular X window (i.e. frame),
   which exists on only one screen; therefore the event won't be
   reported multiple times even if there are multiple devices on
   the same physical display.  This is an implementational detail
   specific to X consoles (e.g. under NeXTstep or Windows, this
   could be different, and input would come directly from the console).
*/

struct console_methods
{
  CONST char *name;	/* Used by print_console, print_device, print_frame */
  Lisp_Object symbol;
  Lisp_Object predicate_symbol;

  /* console methods */
  void (*init_console_method) (struct console *, Lisp_Object props);
  void (*mark_console_method) (struct console *, void (*)(Lisp_Object));
  int (*initially_selected_for_input_method) (struct console *);
  void (*delete_console_method) (struct console *);
  Lisp_Object (*semi_canonicalize_console_connection_method)
    (Lisp_Object connection, Error_behavior errb);
  Lisp_Object (*semi_canonicalize_device_connection_method)
    (Lisp_Object connection, Error_behavior errb);
  Lisp_Object (*canonicalize_console_connection_method)
    (Lisp_Object connection, Error_behavior errb);
  Lisp_Object (*canonicalize_device_connection_method)
    (Lisp_Object connection, Error_behavior errb);
  Lisp_Object (*device_to_console_connection_method)
    (Lisp_Object connection, Error_behavior errb);

  /* device methods */
  void (*init_device_method) (struct device *, Lisp_Object props);
  void (*finish_init_device_method) (struct device *, Lisp_Object props);
  void (*delete_device_method) (struct device *);
  void (*mark_device_method) (struct device *, void (*)(Lisp_Object));
  void (*asynch_device_change_method) (void);
  int (*device_pixel_width_method) (struct device *);
  int (*device_pixel_height_method) (struct device *);
  int (*device_mm_width_method) (struct device *);
  int (*device_mm_height_method) (struct device *);
  int (*device_bitplanes_method) (struct device *);
  int (*device_color_cells_method) (struct device *);

  /* frame methods */
  Lisp_Object *device_specific_frame_props;
  void (*init_frame_1_method) (struct frame *, Lisp_Object properties);
  void (*init_frame_2_method) (struct frame *, Lisp_Object properties);
  void (*init_frame_3_method) (struct frame *);
  void (*after_init_frame_method) (struct frame *, int first_on_device,
				   int first_on_console);
  void (*mark_frame_method) (struct frame *, void (*)(Lisp_Object));
  void (*delete_frame_method) (struct frame *);
  void (*focus_on_frame_method) (struct frame *);
  void (*raise_frame_method) (struct frame *);
  void (*lower_frame_method) (struct frame *);
  int (*get_mouse_position_method) (struct device *d, Lisp_Object *frame,
				    int *x, int *y);
  void (*set_mouse_position_method) (struct window *w, int x, int y);
  void (*make_frame_visible_method) (struct frame *f);
  void (*make_frame_invisible_method) (struct frame *f);
  void (*iconify_frame_method) (struct frame *f);
  Lisp_Object (*frame_property_method) (struct frame *f, Lisp_Object prop);
  int (*internal_frame_property_p_method) (struct frame *f,
					   Lisp_Object prop);
  Lisp_Object (*frame_properties_method) (struct frame *f);
  void (*set_frame_properties_method) (struct frame *f, Lisp_Object plist);
  void (*set_frame_size_method) (struct frame *f, int width, int height);
  void (*set_frame_position_method) (struct frame *f, int xoff, int yoff);
  int (*frame_visible_p_method) (struct frame *f);
  int (*frame_totally_visible_p_method) (struct frame *f);
  int (*frame_iconified_p_method) (struct frame *f);
  void (*set_title_from_char_method) (struct frame *f, char *title);
  void (*set_icon_name_from_char_method) (struct frame *f, char *title);
  void (*set_frame_pointer_method) (struct frame *f);
  void (*set_frame_icon_method) (struct frame *f);
  void (*popup_menu_method) (Lisp_Object menu, Lisp_Object event);
  Lisp_Object (*get_frame_parent_method) (struct frame *f);

  /* redisplay methods */
  int (*left_margin_width_method) (struct window *);
  int (*right_margin_width_method) (struct window *);
  int (*text_width_method) (struct face_cachel *cachel,
			    CONST Emchar *str, Charcount len);
  void (*output_display_block_method) (struct window *, struct display_line *,
				       int, int, int, int, int, int, int);
  int (*divider_width_method) (void);
  int (*divider_height_method) (void);
  int (*eol_cursor_width_method) (void);
  void (*output_vertical_divider_method) (struct window *, int);
  void (*clear_to_window_end_method) (struct window *, int, int);
  void (*clear_region_method) (Lisp_Object, face_index, int, int, int, int);
  void (*clear_frame_method) (struct frame *);
  void (*output_begin_method) (struct device *);
  void (*output_end_method) (struct device *);
  int (*flash_method) (struct device *);
  void (*ring_bell_method) (struct device *, int volume, int pitch,
			    int duration);
  void (*frame_redraw_cursor_method) (struct frame *f);

  /* color methods */
  int (*initialize_color_instance_method) (struct Lisp_Color_Instance *,
					   Lisp_Object name,
					   Lisp_Object device,
					   Error_behavior errb);
  void (*mark_color_instance_method) (struct Lisp_Color_Instance *,
				      void (*)(Lisp_Object));
  void (*print_color_instance_method) (struct Lisp_Color_Instance *,
				       Lisp_Object printcharfun,
				       int escapeflag);
  void (*finalize_color_instance_method) (struct Lisp_Color_Instance *);
  int (*color_instance_equal_method) (struct Lisp_Color_Instance *,
				      struct Lisp_Color_Instance *,
				      int depth);
  unsigned long (*color_instance_hash_method) (struct Lisp_Color_Instance *,
					       int depth);
  Lisp_Object (*color_instance_rgb_components_method)
    (struct Lisp_Color_Instance *);
  int (*valid_color_name_p_method) (struct device *, Lisp_Object color);

  /* font methods */
  int (*initialize_font_instance_method) (struct Lisp_Font_Instance *,
					  Lisp_Object name,
					  Lisp_Object device,
					  Error_behavior errb);
  void (*mark_font_instance_method) (struct Lisp_Font_Instance *,
				     void (*)(Lisp_Object));
  void (*print_font_instance_method) (struct Lisp_Font_Instance *,
				      Lisp_Object printcharfun,
				      int escapeflag);
  void (*finalize_font_instance_method) (struct Lisp_Font_Instance *);
  Lisp_Object (*font_instance_truename_method) (struct Lisp_Font_Instance *,
						Error_behavior errb);
  Lisp_Object (*font_instance_properties_method) (struct Lisp_Font_Instance *);
  Lisp_Object (*list_fonts_method) (Lisp_Object pattern,
				    Lisp_Object device);
  Lisp_Object (*find_charset_font_method) (Lisp_Object device,
					   Lisp_Object font,
					   Lisp_Object charset);
  int (*font_spec_matches_charset_method) (struct device *d,
					   Lisp_Object charset,
					   CONST Bufbyte *nonreloc,
					   Lisp_Object reloc,
					   Bytecount offset,
					   Bytecount length);

  /* image methods */
  void (*mark_image_instance_method) (struct Lisp_Image_Instance *,
				      void (*)(Lisp_Object));
  void (*print_image_instance_method) (struct Lisp_Image_Instance *,
				       Lisp_Object printcharfun,
				       int escapeflag);
  void (*finalize_image_instance_method) (struct Lisp_Image_Instance *);
  int (*image_instance_equal_method) (struct Lisp_Image_Instance *,
				      struct Lisp_Image_Instance *,
				      int depth);
  unsigned long (*image_instance_hash_method) (struct Lisp_Image_Instance *,
					       int depth);
  int (*colorize_image_instance_method) (Lisp_Object image_instance,
					 Lisp_Object fg, Lisp_Object bg);
  Lisp_Object image_conversion_list;

#ifdef HAVE_TOOLBARS
  /* toolbar methods */
  void (*toolbar_size_changed_in_frame_method) (struct frame *f,
						enum toolbar_pos pos,
						Lisp_Object oldval);
  void (*toolbar_visible_p_changed_in_frame_method) (struct frame *f,
						     enum toolbar_pos pos,
						     Lisp_Object oldval);
  void (*output_frame_toolbars_method) (struct frame *);
  void (*initialize_frame_toolbars_method) (struct frame *);
  void (*free_frame_toolbars_method) (struct frame *);
  void (*output_toolbar_button_method) (struct frame *, Lisp_Object);
  void (*redraw_frame_toolbars_method) (struct frame *);
  void (*redraw_exposed_toolbars_method) (struct frame *f, int x, int y,
					  int width, int height);
#endif

#ifdef HAVE_SCROLLBARS
  /* scrollbar methods */
  int (*inhibit_scrollbar_thumb_size_change_method) (void);
  void (*free_scrollbar_instance_method) (struct scrollbar_instance *);
  void (*release_scrollbar_instance_method) (struct scrollbar_instance *);
  void (*create_scrollbar_instance_method) (struct frame *, int,
					    struct scrollbar_instance *);
  void (*scrollbar_width_changed_in_frame_method) (Lisp_Object, struct frame *,
						   Lisp_Object);
  void (*scrollbar_height_changed_in_frame_method) (Lisp_Object,
						    struct frame *,
						   Lisp_Object);
  void (*update_scrollbar_instance_values_method) (struct window *,
						   struct scrollbar_instance *,
						   int, int, int, int, int,
						   int, int, int, int, int);
  void (*update_scrollbar_instance_status_method) (struct window *, int, int,
						   struct
						   scrollbar_instance *);
  void (*scrollbar_pointer_changed_in_window_method) (struct window *w);
#ifdef MEMORY_USAGE_STATS
  int (*compute_scrollbar_instance_usage_method) (struct device *,
						  struct scrollbar_instance *,
						  struct overhead_stats *);
#endif
#endif /* HAVE_SCROLLBARS */

#ifdef HAVE_MENUBARS
  /* menubar methods */
  void (*update_frame_menubars_method) (struct frame *);
  void (*free_frame_menubars_method) (struct frame *);
#endif

#ifdef HAVE_DIALOGS
  /* dialog methods */
#endif
};

#define CONSOLE_TYPE_NAME(c) ((c)->conmeths->name)
#define CONSOLE_TYPE(c) ((c)->conmeths->symbol)
#define CONMETH_TYPE(meths) ((meths)->symbol)

/******** Accessing / calling a console method *********/

#define HAS_CONTYPE_METH_P(meth, m) ((meth)->m##_method)
#define CONTYPE_METH(meth, m, args) (((meth)->m##_method) args)

/* Call a void-returning console method, if it exists */
#define MAYBE_CONTYPE_METH(meth, m, args)			\
do {								\
  struct console_methods *_maybe_contype_meth_meth = (meth);	\
  if (HAS_CONTYPE_METH_P (_maybe_contype_meth_meth, m))		\
    CONTYPE_METH (_maybe_contype_meth_meth, m, args);		\
} while (0)

MAC_DECLARE_EXTERN (struct console_methods *, MTcontype_meth_or_given)

/* Call a console method, if it exists; otherwise return
   the specified value */
#define CONTYPE_METH_OR_GIVEN(meth, m, args, given)		\
MAC_BEGIN							\
  MAC_DECLARE (struct console_methods *,			\
	       MTcontype_meth_or_given, meth)			\
  HAS_CONTYPE_METH_P (MTcontype_meth_or_given, m) ?		\
    CONTYPE_METH (MTcontype_meth_or_given, m, args) : (given)	\
MAC_END

/* Call an int-returning console method, if it exists; otherwise
   return 0 */
#define MAYBE_INT_CONTYPE_METH(meth, m, args) \
  CONTYPE_METH_OR_GIVEN (meth, m, args, 0)

/* Call an Lisp-Object-returning console method, if it exists;
   otherwise return Qnil */
#define MAYBE_LISP_CONTYPE_METH(meth, m, args) \
  CONTYPE_METH_OR_GIVEN (meth, m, args, Qnil)

/******** Same functions, operating on a console instead of a
          struct console_methods ********/

#define HAS_CONMETH_P(c, m) HAS_CONTYPE_METH_P ((c)->conmeths, m)
#define CONMETH(c, m, args) CONTYPE_METH ((c)->conmeths, m, args)
#define MAYBE_CONMETH(c, m, args) MAYBE_CONTYPE_METH ((c)->conmeths, m, args)
#define CONMETH_OR_GIVEN(c, m, args, given) \
  CONTYPE_METH_OR_GIVEN((c)->conmeths, m, args, given)
#define MAYBE_INT_CONMETH(c, m, args) \
  MAYBE_INT_CONTYPE_METH ((c)->conmeths, m, args)
#define MAYBE_LISP_CONMETH(c, m, args) \
  MAYBE_LISP_CONTYPE_METH ((c)->conmeths, m, args)

/******** Defining new console types ********/

struct console_type_entry
{
  Lisp_Object symbol;
  struct console_methods *meths;
};

#define DECLARE_CONSOLE_TYPE(type)				\
extern struct console_methods * type##_console_methods

#define DEFINE_CONSOLE_TYPE(type)				\
struct console_methods * type##_console_methods

#define INITIALIZE_CONSOLE_TYPE(type, obj_name, pred_sym)	\
  do {								\
    type##_console_methods =					\
      malloc_type_and_zero (struct console_methods);		\
    type##_console_methods->name = obj_name;			\
    type##_console_methods->symbol = Q##type;			\
    defsymbol (&type##_console_methods->predicate_symbol,	\
	       pred_sym);					\
    add_entry_to_console_type_list (Q##type,			\
				   type##_console_methods);	\
    type##_console_methods->image_conversion_list = Qnil;	\
    staticpro (&type##_console_methods->image_conversion_list);	\
  } while (0)

/* Declare that console-type TYPE has method M; used in
   initialization routines */
#define CONSOLE_HAS_METHOD(type, m) \
  (type##_console_methods->m##_method = type##_##m)

struct console
{
  struct lcrecord_header header;

  /* Description of this console's methods.  */
  struct console_methods *conmeths;

  /* A structure of auxiliary data specific to the console type.
     struct x_console is used for X window frames; defined in console-x.h
     struct tty_console is used to TTY's; defined in console-tty.h */
  void *console_data;

  /* Character that causes a quit.  Normally C-g.
     #### Should be possible for this not to be ASCII. */
  int quit_char;

  /* ----- begin partially-completed console localization of
           event loop ---- */

  int local_var_flags;

#define MARKED_SLOT(x) Lisp_Object x
#include "conslots.h"
#undef MARKED_SLOT

  /* Where to store the next keystroke of the macro.
     Index into con->kbd_macro_builder. */
  int kbd_macro_ptr;

  /* The finalized section of the macro starts at kbd_macro_buffer and
     ends before this.  This is not the same as kbd_macro_pointer, because
     we advance this to kbd_macro_pointer when a key's command is complete.
     This way, the keystrokes for "end-kbd-macro" are not included in the
     macro.  */
  int kbd_macro_end;

  /* ----- end partially-completed console localization of event loop ---- */

  unsigned int input_enabled :1;
};

DECLARE_LRECORD (console, struct console);
#define XCONSOLE(x) XRECORD (x, console, struct console)
#define XSETCONSOLE(x, p) XSETRECORD (x, p, console)
#define CONSOLEP(x) RECORDP (x, console)
#define GC_CONSOLEP(x) GC_RECORDP (x, console)
#define CHECK_CONSOLE(x) CHECK_RECORD (x, console)
#define CONCHECK_CONSOLE(x) CONCHECK_RECORD (x, console)

#define CHECK_LIVE_CONSOLE(x)						\
  do { CHECK_CONSOLE (x);						\
       if (! CONSOLEP (x)						\
	   || ! CONSOLE_LIVE_P (XCONSOLE (x)))				\
         dead_wrong_type_argument (Qconsole_live_p, (x)); } while (0)
#define CONCHECK_LIVE_CONSOLE(x)					\
  do { CONCHECK_CONSOLE (x);						\
       if (! CONSOLEP (x)						\
	   || ! CONSOLE_LIVE_P (XCONSOLE (x)))				\
         x = wrong_type_argument (Qconsole_live_p, (x)); } while (0)

#define CONSOLE_TYPE_P(con, type) EQ (CONSOLE_TYPE (con), Q##type)

#ifdef ERROR_CHECK_TYPECHECK
MAC_DECLARE_EXTERN (struct console *, MTconsole_data)
# define CONSOLE_TYPE_DATA(con, type)				\
MAC_BEGIN							\
  MAC_DECLARE (struct console *, MTconsole_data, con)		\
  assert (CONSOLE_TYPE_P (MTconsole_data, type))		\
  MAC_SEP							\
  (struct type##_console *) MTconsole_data->console_data	\
MAC_END
#else
# define CONSOLE_TYPE_DATA(con, type)				\
  ((struct type##_console *) (con)->console_data)
#endif

#define CHECK_CONSOLE_TYPE(x, type)				\
  do {								\
    CHECK_CONSOLE (x);						\
    if (!(CONSOLEP (x) && CONSOLE_TYPE_P (XCONSOLE (x),		\
					 type)))		\
      dead_wrong_type_argument					\
	(type##_console_methods->predicate_symbol, x);		\
  } while (0)
#define CONCHECK_CONSOLE_TYPE(x, type)				\
  do {								\
    CONCHECK_CONSOLE (x);					\
    if (!(CONSOLEP (x) && CONSOLE_TYPE_P (XCONSOLE (x),		\
					 type)))		\
      x = wrong_type_argument					\
	(type##_console_methods->predicate_symbol, x);		\
  } while (0)

/* #### These should be in the console-*.h files but there are
   too many places where the abstraction is broken.  Need to
   fix. */

#ifdef HAVE_X_WINDOWS
#define CONSOLE_TYPESYM_X_P(typesym) EQ (typesym, Qx)
#else
#define CONSOLE_TYPESYM_X_P(typesym) 0
#endif
#ifdef HAVE_NEXTSTEP
#define CONSOLE_TYPESYM_NS_P(typesym) EQ (typesym, Qns)
#else
#define CONSOLE_TYPESYM_NS_P(typesym) 0
#endif
#ifdef HAVE_TTY
#define CONSOLE_TYPESYM_TTY_P(typesym) EQ (typesym, Qtty)
#else
#define CONSOLE_TYPESYM_TTY_P(typesym) 0
#endif
#define CONSOLE_TYPESYM_STREAM_P(typesym) EQ (typesym, Qstream)

#define CONSOLE_TYPESYM_WIN_P(typesym) \
  (CONSOLE_TYPESYM_X_P (typesym) || CONSOLE_TYPESYM_NS_P (typesym))

#define CONSOLE_X_P(con) CONSOLE_TYPESYM_X_P (CONSOLE_TYPE (con))
#define CHECK_X_CONSOLE(z) CHECK_CONSOLE_TYPE (z, x)
#define CONCHECK_X_CONSOLE(z) CONCHECK_CONSOLE_TYPE (z, x)

#define CONSOLE_NS_P(con) CONSOLE_TYPESYM_NS_P (CONSOLE_TYPE (con))
#define CHECK_NS_CONSOLE(z) CHECK_CONSOLE_TYPE (z, ns)
#define CONCHECK_NS_CONSOLE(z) CONCHECK_CONSOLE_TYPE (z, ns)

#define CONSOLE_TTY_P(con) CONSOLE_TYPESYM_TTY_P (CONSOLE_TYPE (con))
#define CHECK_TTY_CONSOLE(z) CHECK_CONSOLE_TYPE (z, tty)
#define CONCHECK_TTY_CONSOLE(z) CONCHECK_CONSOLE_TYPE (z, tty)

#define CONSOLE_STREAM_P(con) CONSOLE_TYPESYM_STREAM_P (CONSOLE_TYPE (con))
#define CHECK_STREAM_CONSOLE(z) CHECK_CONSOLE_TYPE (z, stream)
#define CONCHECK_STREAM_CONSOLE(z) CONCHECK_CONSOLE_TYPE (z, stream)

#define CONSOLE_WIN_P(con) CONSOLE_TYPESYM_WIN_P (CONSOLE_TYPE (con))

extern Lisp_Object Vconsole_list, Vselected_console, Vdefault_console;
extern Lisp_Object Qconsole_live_p;

/* This structure holds the default values of the console-local
   variables defined with DEFVAR_CONSOLE_LOCAL, that have special
   slots in each console.  The default value occupies the same slot
   in this structure as an individual console's value occupies in
   that console.  Setting the default value also goes through the
   list of consoles and stores into each console that does not say
   it has a local value.  */

extern Lisp_Object Vconsole_defaults;

/* This structure marks which slots in a console have corresponding
   default values in console_defaults.
   Each such slot has a nonzero value in this structure.
   The value has only one nonzero bit.

   When a console has its own local value for a slot,
   the bit for that slot (found in the same slot in this structure)
   is turned on in the console's local_var_flags slot.

   If a slot in this structure is zero, then even though there may
   be a DEFVAR_CONSOLE_LOCAL for the slot, there is no default value for it;
   and the corresponding slot in console_defaults is not used.  */

extern struct console console_local_flags;

extern Lisp_Object Vconsole_type_list;

extern Lisp_Object Qtty, Qstream, Qdead;
#ifdef HAVE_X_WINDOWS
extern Lisp_Object Qx;
#endif /* HAVE_X_WINDOWS */
#ifdef HAVE_NEXTSTEP
extern Lisp_Object Qns;
#endif /* HAVE_NEXTSTEP */

int valid_console_type_p (Lisp_Object type);

#define CONSOLE_LIVE_P(con) (!EQ (CONSOLE_TYPE (con), Qdead))

#define CONSOLE_NAME(con) ((con)->name)
#define CONSOLE_CONNECTION(con) ((con)->connection)
#define CONSOLE_CANON_CONNECTION(con) ((con)->canon_connection)
#define CONSOLE_FUNCTION_KEY_MAP(con) ((con)->function_key_map)
#define CONSOLE_DEVICE_LIST(con) ((con)->device_list)
#define CONSOLE_SELECTED_DEVICE(con) ((con)->selected_device)
#define CONSOLE_SELECTED_FRAME(con) \
  DEVICE_SELECTED_FRAME (XDEVICE ((con)->selected_device))
#define CONSOLE_LAST_NONMINIBUF_FRAME(con) NON_LVALUE ((con)->_last_nonminibuf_frame)
#define CONSOLE_QUIT_CHAR(con) ((con)->quit_char)

#define CDFW_CONSOLE(obj)			\
  (WINDOWP (obj)				\
   ? WINDOW_CONSOLE (XWINDOW (obj))		\
   : (FRAMEP (obj)				\
      ? FRAME_CONSOLE (XFRAME (obj))		\
      : (DEVICEP (obj)				\
	 ? DEVICE_CONSOLE (XDEVICE (obj))    	\
	 : (CONSOLEP (obj)			\
	    ? obj				\
	    : Qnil))))

#define CONSOLE_LOOP(concons) LIST_LOOP (concons, Vconsole_list)
#define CONSOLE_DEVICE_LOOP(devcons, con) \
  LIST_LOOP (devcons, CONSOLE_DEVICE_LIST (con))

DECLARE_CONSOLE_TYPE (dead);
extern console_type_entry_dynarr *the_console_type_entry_dynarr;

Lisp_Object create_console (Lisp_Object name, Lisp_Object type,
			    Lisp_Object connection, Lisp_Object props);
void select_console_1 (Lisp_Object);
struct console *decode_console (Lisp_Object);
Lisp_Object make_console (struct console *c);
void add_entry_to_console_type_list (Lisp_Object symbol,
				     struct console_methods *type);
struct console_methods *decode_console_type (Lisp_Object type,
					     Error_behavior errb);
void delete_console_internal (struct console *con, int force,
			      int from_kill_emacs, int from_io_error);
void io_error_delete_console (Lisp_Object console);
void set_console_last_nonminibuf_frame (struct console *con,
					Lisp_Object frame);

#endif /* _XEMACS_CONSOLE_H_ */
