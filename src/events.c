/* Events: printing them, converting them to and from characters.
   Copyright (C) 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
   Copyright (C) 1994, 1995 Board of Trustees, University of Illinois.

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

/* This file has been Mule-ized. */

#include <config.h>
#include "lisp.h"
#include "buffer.h"
#include "console.h"
#include "console-tty.h" /* for stuff in character_to_event */
#include "device.h"
#include "console-x.h"	/* for x_event_name prototype */
#include "extents.h"	/* Just for the EXTENTP abort check... */
#include "events.h"
#include "frame.h"
#include "glyphs.h"
#include "keymap.h" /* for key_desc_list_to_event() */
#include "redisplay.h"
#include "window.h"

#ifdef WINDOWSNT
/* Hmm, under unix we want X modifiers, under NT we want X modifiers if
   we are running X and Windows modifiers otherwise.
   gak. This is a kludge until we support multiple native GUIs!
*/
#undef MOD_ALT
#undef MOD_CONTROL
#undef MOD_SHIFT
#endif

#include "events-mod.h"

/* Where old events go when they are explicitly deallocated.
   The event chain here is cut loose before GC, so these will be freed
   eventually.
 */
static Lisp_Object Vevent_resource;

Lisp_Object Qeventp;
Lisp_Object Qevent_live_p;
Lisp_Object Qkey_press_event_p;
Lisp_Object Qbutton_event_p;
Lisp_Object Qmouse_event_p;
Lisp_Object Qprocess_event_p;

Lisp_Object Qkey_press, Qbutton_press, Qbutton_release, Qmisc_user;
Lisp_Object Qascii_character;

#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
Lisp_Object Qdnd_drop_event_p;
Lisp_Object Qdnd_drop;
#endif

/* #### Ad-hoc hack.  Should be part of define_lrecord_implementation */
void
clear_event_resource (void)
{
  Vevent_resource = Qnil;
}

/* Make sure we lose quickly if we try to use this event */
static void
deinitialize_event (Lisp_Object ev)
{
  int i;
  struct Lisp_Event *event = XEVENT (ev);

  for (i = 0; i < ((sizeof (struct Lisp_Event)) / sizeof (int)); i++)
    ((int *) event) [i] = 0xdeadbeef;
  event->event_type = dead_event;
  event->channel = Qnil;
  set_lheader_implementation (&(event->lheader), lrecord_event);
  XSET_EVENT_NEXT (ev, Qnil);
}

/* Set everything to zero or nil so that it's predictable. */
void
zero_event (struct Lisp_Event *e)
{
  memset (e, 0, sizeof (*e));
  set_lheader_implementation (&(e->lheader), lrecord_event);
  e->event_type = empty_event;
  e->next = Qnil;
  e->channel = Qnil;
}

static Lisp_Object
mark_event (Lisp_Object obj, void (*markobj) (Lisp_Object))
{
  struct Lisp_Event *event = XEVENT (obj);

  switch (event->event_type)
    {
    case key_press_event:
      ((markobj) (event->event.key.keysym));
      break;
    case process_event:
      ((markobj) (event->event.process.process));
      break;
    case timeout_event:
      ((markobj) (event->event.timeout.function));
      ((markobj) (event->event.timeout.object));
      break;
    case eval_event:
    case misc_user_event:
      ((markobj) (event->event.eval.function));
      ((markobj) (event->event.eval.object));
      break;
    case magic_eval_event:
      ((markobj) (event->event.magic_eval.object));
      break;
    case button_press_event:
    case button_release_event:
    case pointer_motion_event:
    case magic_event:
    case empty_event:
    case dead_event:
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event:
#endif
      break;
    default:
      abort ();
    }
  ((markobj) (event->channel));
  return event->next;
}

static void
print_event_1 (CONST char *str, Lisp_Object obj, Lisp_Object printcharfun)
{
  char buf[255];
  write_c_string (str, printcharfun);
  format_event_object (buf, XEVENT (obj), 0);
  write_c_string (buf, printcharfun);
}

static void
print_event (Lisp_Object obj, Lisp_Object printcharfun, int escapeflag)
{
  if (print_readably)
    error ("printing unreadable object #<event>");

  switch (XEVENT (obj)->event_type)
    {
    case key_press_event:
      print_event_1 ("#<keypress-event ", obj, printcharfun);
      break;
    case button_press_event:
      print_event_1 ("#<buttondown-event ", obj, printcharfun);
      break;
    case button_release_event:
      print_event_1 ("#<buttonup-event ", obj, printcharfun);
      break;
    case magic_event:
    case magic_eval_event:
      print_event_1 ("#<magic-event ", obj, printcharfun);
      break;
    case pointer_motion_event:
      {
	char buf[64];
	Lisp_Object Vx, Vy;
	Vx = Fevent_x_pixel (obj);
	assert (INTP (Vx));
	Vy = Fevent_y_pixel (obj);
	assert (INTP (Vy));
	sprintf (buf, "#<motion-event %d, %d", XINT (Vx), XINT (Vy));
	write_c_string (buf, printcharfun);
	break;
      }
    case process_event:
	write_c_string ("#<process-event ", printcharfun);
	print_internal (XEVENT (obj)->event.process.process, printcharfun, 1);
	break;
    case timeout_event:
	write_c_string ("#<timeout-event ", printcharfun);
	print_internal (XEVENT (obj)->event.timeout.object, printcharfun, 1);
	break;
    case empty_event:
	write_c_string ("#<empty-event", printcharfun);
	break;
    case misc_user_event:
    case eval_event:
	write_c_string ("#<", printcharfun);
	if (XEVENT (obj)->event_type == misc_user_event)
	  write_c_string ("misc-user", printcharfun);
	else
	  write_c_string ("eval", printcharfun);
	write_c_string ("-event (", printcharfun);
	print_internal (XEVENT (obj)->event.eval.function, printcharfun, 1);
	write_c_string (" ", printcharfun);
	print_internal (XEVENT (obj)->event.eval.object, printcharfun, 1);
	write_c_string (")", printcharfun);
	break;
    case dead_event:
	write_c_string ("#<DEALLOCATED-EVENT", printcharfun);
	break;
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event:
      print_event_1 ("#<dnd-drop-event ", obj, printcharfun);
      break;
#endif
    default:
	write_c_string ("#<UNKNOWN-EVENT-TYPE", printcharfun);
	break;
      }
  write_c_string (">", printcharfun);
}

static int
event_equal (Lisp_Object o1, Lisp_Object o2, int depth)
{
  struct Lisp_Event *e1 = XEVENT (o1);
  struct Lisp_Event *e2 = XEVENT (o2);

  if (e1->event_type != e2->event_type) return 0;
  if (!EQ (e1->channel, e2->channel)) return 0;
/*  if (e1->timestamp != e2->timestamp) return 0; */
  switch (e1->event_type)
    {
    case process_event:
      return EQ (e1->event.process.process, e2->event.process.process);

    case timeout_event:
      return (internal_equal (e1->event.timeout.function,
			      e2->event.timeout.function, 0) &&
	      internal_equal (e1->event.timeout.object,
			      e2->event.timeout.object, 0));

    case key_press_event:
      return (EQ (e1->event.key.keysym, e2->event.key.keysym) &&
	      (e1->event.key.modifiers == e2->event.key.modifiers));

    case button_press_event:
    case button_release_event:
      return (e1->event.button.button    == e2->event.button.button &&
	      e1->event.button.modifiers == e2->event.button.modifiers);

    case pointer_motion_event:
      return (e1->event.motion.x == e2->event.motion.x &&
	      e1->event.motion.y == e2->event.motion.y);

    case misc_user_event:
    case eval_event:
      return (internal_equal (e1->event.eval.function,
			      e2->event.eval.function, 0) &&
	      internal_equal (e1->event.eval.object,
			      e2->event.eval.object, 0));

    case magic_eval_event:
      return (e1->event.magic_eval.internal_function ==
	      e2->event.magic_eval.internal_function &&
	      internal_equal (e1->event.magic_eval.object,
			      e2->event.magic_eval.object, 0));

#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event:
      return (e1->event.dnd_drop.button    == e2->event.dnd_drop.button &&
	      e1->event.dnd_drop.modifiers == e2->event.dnd_drop.modifiers &&
	      EQ (e1->event.dnd_drop.data, e2->event.dnd_drop.data));
#endif

    case magic_event:
      {
	struct console *con = XCONSOLE (CDFW_CONSOLE (e1->channel));

#ifdef HAVE_X_WINDOWS
	if (CONSOLE_X_P (con))
	  return (e1->event.magic.underlying_x_event.xany.serial ==
		  e2->event.magic.underlying_x_event.xany.serial);
#endif
#ifdef HAVE_TTY
	if (CONSOLE_TTY_P (con))
	return (e1->event.magic.underlying_tty_event ==
		e2->event.magic.underlying_tty_event);
#endif
#ifdef HAVE_MS_WINDOWS
	if (CONSOLE_MSWINDOWS_P (con))
	return (!memcmp(&e1->event.magic.underlying_mswindows_event,
		&e2->event.magic.underlying_mswindows_event,
		sizeof(union magic_data)));
#endif
	return 1; /* not reached */
      }

    case empty_event:      /* Empty and deallocated events are equal. */
    case dead_event:
      return 1;

    default:
      abort ();
      return 0;                 /* not reached; warning suppression */
    }
}

static unsigned long
event_hash (Lisp_Object obj, int depth)
{
  struct Lisp_Event *e = XEVENT (obj);
  unsigned long hash;

  hash = HASH2 (e->event_type, LISP_HASH (e->channel));
  switch (e->event_type)
    {
    case process_event:
      return HASH2 (hash, LISP_HASH (e->event.process.process));

    case timeout_event:
      return HASH3 (hash, internal_hash (e->event.timeout.function, depth + 1),
		    internal_hash (e->event.timeout.object, depth + 1));

    case key_press_event:
      return HASH3 (hash, LISP_HASH (e->event.key.keysym),
		    e->event.key.modifiers);

    case button_press_event:
    case button_release_event:
      return HASH3 (hash, e->event.button.button, e->event.button.modifiers);

    case pointer_motion_event:
      return HASH3 (hash, e->event.motion.x, e->event.motion.y);

    case misc_user_event:
    case eval_event:
      return HASH3 (hash, internal_hash (e->event.eval.function, depth + 1),
		    internal_hash (e->event.eval.object, depth + 1));

    case magic_eval_event:
      return HASH3 (hash,
		    (unsigned long) e->event.magic_eval.internal_function,
		    internal_hash (e->event.magic_eval.object, depth + 1));

#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event:
      return HASH4 (hash, e->event.dnd_drop.button, e->event.dnd_drop.modifiers,
		    LISP_HASH(e->event.dnd_drop.data));
#endif

    case magic_event:
      {
	struct console *con = XCONSOLE (CDFW_CONSOLE (EVENT_CHANNEL (e)));
#ifdef HAVE_X_WINDOWS
	if (CONSOLE_X_P (con))
	  return HASH2 (hash, e->event.magic.underlying_x_event.xany.serial);
#endif
#ifdef HAVE_TTY
	if (CONSOLE_TTY_P (con))
	  return HASH2 (hash, e->event.magic.underlying_tty_event);
#endif
#ifdef HAVE_MS_WINDOWS
	if (CONSOLE_MSWINDOWS_P (con))
	  return HASH2 (hash, e->event.magic.underlying_mswindows_event);
#endif
      }

    case empty_event:
    case dead_event:
      return hash;

    default:
      abort ();
    }

  return 0; /* unreached */
}

DEFINE_BASIC_LRECORD_IMPLEMENTATION ("event", event,
				     mark_event, print_event, 0, event_equal,
				     event_hash, struct Lisp_Event);


DEFUN ("make-event", Fmake_event, 0, 2, 0, /*
Create a new event of type TYPE, with properties described by PLIST.

TYPE is a symbol, either `empty', `key-press', `button-press',
 `button-release', `motion' or `dnd-drop'.  If TYPE is nil, it
 defaults to `empty'.

PLIST is a property list, the properties being compatible to those
 returned by `event-properties'.  The following properties are
 allowed:

 channel	-- The event channel, a frame or a console.  For
		   button-press, button-release, motion and dnd-drop
		   events, this must be a frame.  For key-press
		   events, it must be a console.  If channel is
		   unspecified, it will be set to the selected frame
		   or selected console, as appropriate.
 key		-- The event key, a symbol or character.  Allowed only for
		   keypress events.
 button		-- The event button, integer 1, 2 or 3.  Allowed for
		   button-press, button-release and dnd-drag events.
 modifiers	-- The event modifiers, a list of modifier symbols.  Allowed
		   for key-press, button-press, button-release, motion and
		   dnd-drop events.
 x		-- The event X coordinate, an integer.  This is relative
		   to the left of CHANNEL's root window.  Allowed for
		   motion, button-press, button-release and dnd-drop events.
 y		-- The event Y coordinate, an integer.  This is relative
		   to the top of CHANNEL's root window.  Allowed for
		   motion, button-press, button-release and dnd-drop events.
 dnd-data	-- The event DND data, a list of (INTEGER DATA).  Allowed
		   for dnd-drop events, if support for DND has been
		   compiled into XEmacs.
 timestamp	-- The event timestamp, a non-negative integer.  Allowed for
		   all types of events.  If unspecified, it will be set to 0
		   by default.

For event type `empty', PLIST must be nil.
 `button-release', or `motion'.  If TYPE is left out, it defaults to
 `empty'.
PLIST is a list of properties, as returned by `event-properties'.  Not
 all properties are allowed for all kinds of events, and some are
 required.

WARNING: the event object returned may be a reused one; see the function
 `deallocate-event'.
*/
       (type, plist))
{
  Lisp_Object tail, keyword, value;
  Lisp_Object event = Qnil;
  Lisp_Object dnd_data = Qnil;
  struct Lisp_Event *e;
  EMACS_INT coord_x = 0, coord_y = 0;
  struct gcpro gcpro1, gcpro2;

  GCPRO2 (event, dnd_data);

  if (NILP (type))
    type = Qempty;

  if (!NILP (Vevent_resource))
    {
      event = Vevent_resource;
      Vevent_resource = XEVENT_NEXT (event);
    }
  else
    {
      event = allocate_event ();
    }
  e = XEVENT (event);
  zero_event (e);

  if (EQ (type, Qempty))
    {
      /* For empty event, we return immediately, without processing
         PLIST.  In fact, processing PLIST would be wrong, because the
         sanitizing process would fill in the properties
         (e.g. CHANNEL), which we don't want in empty events.  */
      e->event_type = empty_event;
      if (!NILP (plist))
	error ("Cannot set properties of empty event");
      UNGCPRO;
      return event;
    }
  else if (EQ (type, Qkey_press))
    e->event_type = key_press_event;
  else if (EQ (type, Qbutton_press))
    e->event_type = button_press_event;
  else if (EQ (type, Qbutton_release))
    e->event_type = button_release_event;
  else if (EQ (type, Qmotion))
    e->event_type = pointer_motion_event;
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
  else if (EQ (type, Qdnd_drop))
    {
      e->event_type = dnd_drop_event;
      e->event.dnd_drop.data = Qnil;
    }
#endif
  else
    {
      /* Not allowed: Qmisc_user, Qprocess, Qtimeout, Qmagic, Qeval,
	 Qmagic_eval.  */
      /* #### Should we allow misc-user events?  */
      signal_simple_error ("Invalid event type", type);
    }

  plist = Fcopy_sequence (plist);
  Fcanonicalize_plist (plist, Qnil);

  /* Process the plist. */
  EXTERNAL_PROPERTY_LIST_LOOP (tail, keyword, value, plist)
    {
      if (EQ (keyword, Qchannel))
	{
	  if (e->event_type == key_press_event)
	    {
	      if (!CONSOLEP (value))
		wrong_type_argument (Qconsolep, value);
	    }
	  else
	    {
	      if (!FRAMEP (value))
		wrong_type_argument (Qframep, value);
	    }
	  EVENT_CHANNEL (e) = value;
	}
      else if (EQ (keyword, Qkey))
	{
	  if (e->event_type != key_press_event)
	    signal_simple_error ("Invalid event type for `key' property",
				 type);
	  if (!SYMBOLP (value) && !CHARP (value))
	    signal_simple_error ("Invalid event key", value);
	  e->event.key.keysym = value;
	}
      else if (EQ (keyword, Qbutton))
	{
	  CHECK_NATNUM (value);
	  check_int_range (XINT(value), 1, 3);
	  if (e->event_type != button_press_event
	      && e->event_type != button_release_event)
	    signal_simple_error ("Invalid event type for `button' property",
				 type);
	  e->event.button.button = XINT (value);
	}
      else if (EQ (keyword, Qmodifiers))
	{
	  Lisp_Object modtail, sym;
	  int modifiers = 0;

	  if (e->event_type != key_press_event
	      && e->event_type != button_press_event
	      && e->event_type != button_release_event
	      && e->event_type != pointer_motion_event)
	    /* Currently unreached. */
	    signal_simple_error ("Invalid event type for modifiers", type);

	  EXTERNAL_LIST_LOOP (modtail, value)
	    {
	      sym = XCAR (modtail);
	      if (EQ (sym, Qcontrol))      modifiers |= MOD_CONTROL;
	      else if (EQ (sym, Qmeta))    modifiers |= MOD_META;
	      else if (EQ (sym, Qsuper))   modifiers |= MOD_SUPER;
	      else if (EQ (sym, Qhyper))   modifiers |= MOD_HYPER;
	      else if (EQ (sym, Qalt))     modifiers |= MOD_ALT;
	      else if (EQ (sym, Qsymbol))  modifiers |= MOD_ALT;
	      else if (EQ (sym, Qshift))   modifiers |= MOD_SHIFT;
	      else
		signal_simple_error ("Invalid key modifier", XCAR (modtail));
	    }
	  if (e->event_type == key_press_event)
	    e->event.key.modifiers = modifiers;
	  else if (e->event_type == button_press_event
		   || e->event_type == button_release_event)
	    e->event.button.modifiers = modifiers;
	  else /* pointer_motion_event */
	    e->event.motion.modifiers = modifiers;
	}
      else if (EQ (keyword, Qx))
	{
	  /* Allow negative values, so we can specify toolbar
             positions.  */
	  CHECK_INT (value);
	  if (e->event_type != pointer_motion_event
	      && e->event_type != button_press_event
	      && e->event_type != button_release_event)
	    {
	      signal_simple_error ("Cannot assign `x' property to event",
				   type);
	    }
	  coord_x = XINT (value);
	}
      else if (EQ (keyword, Qy))
	{
	  /* Allow negative values; see above. */
	  CHECK_INT (value);
	  if (e->event_type != pointer_motion_event
	      && e->event_type != button_press_event
	      && e->event_type != button_release_event)
	    {
	      signal_simple_error ("Cannot assign `y' property to event",
				   type);
	    }
	  coord_y = XINT (value);
	}
      else if (EQ (keyword, Qtimestamp))
	{
	  CHECK_NATNUM (value);
	  e->timestamp = XINT (value);
	}
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
      else if (EQ (keyword, Qdnd_data))
	{
	  Lisp_Object dnd_tail;
	  /* Value is either nil, or a list of (TYPE DATA).  TYPE is
             an integer.  DATA is a list. */
	  if (!NILP (value))
	    {
	      CHECK_CONS (value);
	      /* To be changed to CHECK_SYMBOL. */
	      CHECK_NATNUM (XCAR (value));
	      CHECK_CONS (XCDR (value));
	      if (XINT (Flength (value)) != 2)
		signal_simple_error ("should be a two-element list", value);
	      /* Check validity of DATA. */
	      EXTERNAL_LIST_LOOP (dnd_tail, XCAR (XCDR (value)))
		{
		  /* Every element must be a string. */
		  CHECK_STRING (XCAR (dnd_tail));
		}
	      /* And now, copy it all. */
	      e->event.dnd_drop.data = Fcopy_tree (value, Qnil);
	    }
	}
#endif /* HAVE_OFFIX_DND || HAVE_MS_WINDOWS */
      else
	signal_simple_error ("Invalid property", keyword);
    } /* while */

  /* Insert the channel, if missing. */
  if (NILP (EVENT_CHANNEL (e)))
    {
      if (e->event_type == key_press_event)
	EVENT_CHANNEL (e) = Vselected_console;
      else
	EVENT_CHANNEL (e) = Fselected_frame (Qnil);
    }

  /* Fevent_properties, Fevent_x_pixel, etc. work with pixels relative
     to the frame, so we must adjust accordingly.  */
  if (e->event_type == pointer_motion_event
      || e->event_type == button_press_event
      || e->event_type == button_release_event
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
      || e->event_type == dnd_drop_event
#endif
      )
    {
      struct frame *f = XFRAME (EVENT_CHANNEL (e));

      coord_x += FRAME_REAL_LEFT_TOOLBAR_WIDTH (f);
      coord_y += FRAME_REAL_TOP_TOOLBAR_HEIGHT (f);

      if (e->event_type == pointer_motion_event)
	{
	  e->event.motion.x = coord_x;
	  e->event.motion.y = coord_y;
	}
      else if (e->event_type == button_press_event
	       || e->event_type == button_release_event
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
	       || e->event_type == dnd_drop_event
#endif
	       )
	{
	  e->event.button.x = coord_x;
	  e->event.button.y = coord_y;
	}
    }

  /* Finally, do some more validation.  */
  switch (e->event_type)
    {
    case key_press_event:
      if (!(SYMBOLP (e->event.key.keysym) || CHARP (e->event.key.keysym)))
	error ("Undefined key for keypress event");
      break;
    case button_press_event:
    case button_release_event:
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event:
#endif
      if (!e->event.button.button)
	error ("Undefined button for %s event",
	       e->event_type == button_press_event
	       ? "buton-press" :
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
	       e->event_type == button_release_event
	       ? "button-release" : "dnd-drop"
#else
	       "button-release"
#endif
	       );
      break;
    default:
      break;
    }

  UNGCPRO;
  return event;
}

DEFUN ("deallocate-event", Fdeallocate_event, 1, 1, 0, /*
Allow the given event structure to be reused.
You MUST NOT use this event object after calling this function with it.
You will lose.  It is not necessary to call this function, as event
objects are garbage-collected like all other objects; however, it may
be more efficient to explicitly deallocate events when you are sure
that it is safe to do so.
*/
       (event))
{
  CHECK_EVENT (event);

  if (XEVENT_TYPE (event) == dead_event)
    error ("this event is already deallocated!");

  assert (XEVENT_TYPE (event) <= last_event_type);

#if 0
  {
    int i, len;
    extern Lisp_Object Vlast_command_event;
    extern Lisp_Object Vlast_input_event, Vunread_command_event;
    extern Lisp_Object Vthis_command_keys, Vrecent_keys_ring;

    if (EQ (event, Vlast_command_event) ||
	EQ (event, Vlast_input_event)   ||
	EQ (event, Vunread_command_event))
      abort ();

    len = XVECTOR_LENGTH (Vthis_command_keys);
    for (i = 0; i < len; i++)
      if (EQ (event, XVECTOR_DATA (Vthis_command_keys) [i]))
	abort ();
    if (!NILP (Vrecent_keys_ring))
      {
	int recent_ring_len = XVECTOR_LENGTH (Vrecent_keys_ring);
	for (i = 0; i < recent_ring_len; i++)
	  if (EQ (event, XVECTOR_DATA (Vrecent_keys_ring) [i]))
	    abort ();
      }
  }
#endif /* 0 */

  assert (!EQ (event, Vevent_resource));
  deinitialize_event (event);
#ifndef ALLOC_NO_POOLS
  XSET_EVENT_NEXT (event, Vevent_resource);
  Vevent_resource = event;
#endif
  return Qnil;
}

DEFUN ("copy-event", Fcopy_event, 1, 2, 0, /*
Make a copy of the given event object.
If a second argument is given, the first event is copied into the second
and the second is returned.  If the second argument is not supplied (or
is nil) then a new event will be made as with `allocate-event.'  See also
the function `deallocate-event'.
*/
       (event1, event2))
{
  CHECK_LIVE_EVENT (event1);
  if (NILP (event2))
    event2 = Fmake_event (Qnil, Qnil);
  else CHECK_LIVE_EVENT (event2);
  if (EQ (event1, event2))
    return signal_simple_continuable_error_2
      ("copy-event called with `eq' events", event1, event2);

  assert (XEVENT_TYPE (event1) <= last_event_type);
  assert (XEVENT_TYPE (event2) <= last_event_type);

  {
    Lisp_Object save_next = XEVENT_NEXT (event2);

    *XEVENT (event2) = *XEVENT (event1);
    XSET_EVENT_NEXT (event2, save_next);
    return event2;
  }
}



/* Given a chain of events (or possibly nil), deallocate them all. */

void
deallocate_event_chain (Lisp_Object event_chain)
{
  while (!NILP (event_chain))
    {
      Lisp_Object next = XEVENT_NEXT (event_chain);
      Fdeallocate_event (event_chain);
      event_chain = next;
    }
}

/* Return the last event in a chain.
   NOTE: You cannot pass nil as a value here!  The routine will
   abort if you do. */

Lisp_Object
event_chain_tail (Lisp_Object event_chain)
{
  while (1)
    {
      Lisp_Object next = XEVENT_NEXT (event_chain);
      if (NILP (next))
	return event_chain;
      event_chain = next;
    }
}

/* Enqueue a single event onto the end of a chain of events.
   HEAD points to the first event in the chain, TAIL to the last event.
   If the chain is empty, both values should be nil. */

void
enqueue_event (Lisp_Object event, Lisp_Object *head, Lisp_Object *tail)
{
  assert (NILP (XEVENT_NEXT (event)));
  assert (!EQ (*tail, event));

  if (!NILP (*tail))
    XSET_EVENT_NEXT (*tail, event);
  else
   *head = event;
  *tail = event;

  assert (!EQ (event, XEVENT_NEXT (event)));
}

/* Remove an event off the head of a chain of events and return it.
   HEAD points to the first event in the chain, TAIL to the last event. */

Lisp_Object
dequeue_event (Lisp_Object *head, Lisp_Object *tail)
{
  Lisp_Object event;

  event = *head;
  *head = XEVENT_NEXT (event);
  XSET_EVENT_NEXT (event, Qnil);
  if (NILP (*head))
    *tail = Qnil;
  return event;
}

/* Enqueue a chain of events (or possibly nil) onto the end of another
   chain of events.  HEAD points to the first event in the chain being
   queued onto, TAIL to the last event.  If the chain is empty, both values
   should be nil. */

void
enqueue_event_chain (Lisp_Object event_chain, Lisp_Object *head,
		     Lisp_Object *tail)
{
  if (NILP (event_chain))
    return;

  if (NILP (*head))
    {
      *head = event_chain;
      *tail = event_chain;
    }
  else
    {
      XSET_EVENT_NEXT (*tail, event_chain);
      *tail = event_chain_tail (event_chain);
    }
}

/* Return the number of events (possibly 0) on an event chain. */

int
event_chain_count (Lisp_Object event_chain)
{
  Lisp_Object event;
  int n = 0;

  EVENT_CHAIN_LOOP (event, event_chain)
    n++;

  return n;
}

/* Find the event before EVENT in an event chain.  This aborts
   if the event is not in the chain. */

Lisp_Object
event_chain_find_previous (Lisp_Object event_chain, Lisp_Object event)
{
  Lisp_Object previous = Qnil;

  while (!NILP (event_chain))
    {
      if (EQ (event_chain, event))
	return previous;
      previous = event_chain;
      event_chain = XEVENT_NEXT (event_chain);
    }

  abort ();
  return Qnil;
}

Lisp_Object
event_chain_nth (Lisp_Object event_chain, int n)
{
  Lisp_Object event;
  EVENT_CHAIN_LOOP (event, event_chain)
    {
      if (!n)
	return event;
      n--;
    }
  return Qnil;
}

Lisp_Object
copy_event_chain (Lisp_Object event_chain)
{
  Lisp_Object new_chain = Qnil;
  Lisp_Object new_chain_tail = Qnil;
  Lisp_Object event;

  EVENT_CHAIN_LOOP (event, event_chain)
    {
      Lisp_Object copy = Fcopy_event (event, Qnil);
      enqueue_event (copy, &new_chain, &new_chain_tail);
    }

  return new_chain;
}



Lisp_Object QKbackspace, QKtab, QKlinefeed, QKreturn, QKescape,
 QKspace, QKdelete;

int
command_event_p (Lisp_Object event)
{
  switch (XEVENT_TYPE (event))
    {
    case key_press_event:
    case button_press_event:
    case button_release_event:
    case misc_user_event:
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event:
#endif
      return 1;
    default:
      return 0;
    }
}


void
character_to_event (Emchar c, struct Lisp_Event *event, struct console *con,
		    int use_console_meta_flag)
{
  Lisp_Object k = Qnil;
  unsigned int m = 0;
  if (event->event_type == dead_event)
    error ("character-to-event called with a deallocated event!");

#ifndef MULE
  c &= 255;
#endif
  if (c > 127 && c <= 255)
    {
      int meta_flag = 1;
      if (use_console_meta_flag && CONSOLE_TTY_P (con))
	meta_flag = TTY_FLAGS (con).meta_key;
      switch (meta_flag)
	{
	case 0: /* ignore top bit; it's parity */
	  c -= 128;
	  break;
	case 1: /* top bit is meta */
	  c -= 128;
	  m = MOD_META;
	  break;
	default: /* this is a real character */
	  break;
	}
    }
  if (c < ' ') c += '@', m |= MOD_CONTROL;
  if (m & MOD_CONTROL)
    {
      switch (c)
	{
	case 'I': k = QKtab;	  m &= ~MOD_CONTROL; break;
	case 'J': k = QKlinefeed; m &= ~MOD_CONTROL; break;
	case 'M': k = QKreturn;	  m &= ~MOD_CONTROL; break;
	case '[': k = QKescape;	  m &= ~MOD_CONTROL; break;
#ifdef HAVE_TTY
	default:
	  if (CHARP (con->tty_erase_char) &&
	      c - '@' == XCHAR (con->tty_erase_char)) {
	    k = QKbackspace;
	    m &= ~MOD_CONTROL;
	  }
	  break;
#endif
	}
      if (c >= 'A' && c <= 'Z') c -= 'A'-'a';
    }
#ifdef HAVE_TTY
  else if (CHARP (con->tty_erase_char) &&
	   c == XCHAR(con->tty_erase_char))
    k = QKbackspace;
#endif
  else if (c == 127)
    k = QKdelete;
  else if (c == ' ')
    k = QKspace;

  event->event_type	     = key_press_event;
  event->timestamp	     = 0; /* #### */
  event->channel	     = make_console (con);
  event->event.key.keysym    = (!NILP (k) ? k : make_char (c));
  event->event.key.modifiers = m;
}


/* This variable controls what character name -> character code mapping
   we are using.  Window-system-specific code sets this to some symbol,
   and we use that symbol as the plist key to convert keysyms into 8-bit
   codes.  In this way one can have several character sets predefined and
   switch them by changing this.
 */
Lisp_Object Vcharacter_set_property;

Emchar
event_to_character (struct Lisp_Event *event,
		    int allow_extra_modifiers,
		    int allow_meta,
		    int allow_non_ascii)
{
  Emchar c = 0;
  Lisp_Object code;

  if (event->event_type != key_press_event)
    {
      if (event->event_type == dead_event) abort ();
      return -1;
    }
  if (!allow_extra_modifiers &&
      event->event.key.modifiers & (MOD_SUPER|MOD_HYPER|MOD_ALT))
    return -1;
  if (CHAR_OR_CHAR_INTP (event->event.key.keysym))
    c = XCHAR_OR_CHAR_INT (event->event.key.keysym);
  else if (!SYMBOLP (event->event.key.keysym))
    abort ();
  else if (allow_non_ascii && !NILP (Vcharacter_set_property)
	   /* Allow window-system-specific extensibility of
	      keysym->code mapping */
	   && CHAR_OR_CHAR_INTP (code = Fget (event->event.key.keysym,
					      Vcharacter_set_property,
					      Qnil)))
    c = XCHAR_OR_CHAR_INT (code);
  else if (CHAR_OR_CHAR_INTP (code = Fget (event->event.key.keysym,
					   Qascii_character, Qnil)))
    c = XCHAR_OR_CHAR_INT (code);
  else
    return -1;

  if (event->event.key.modifiers & MOD_CONTROL)
    {
      if (c >= 'a' && c <= 'z')
	c -= ('a' - 'A');
      else
	/* reject Control-Shift- keys */
	if (c >= 'A' && c <= 'Z' && !allow_extra_modifiers)
	  return -1;

      if (c >= '@' && c <= '_')
	c -= '@';
      else if (c == ' ')  /* C-space and C-@ are the same. */
	c = 0;
      else
	/* reject keys that can't take Control- modifiers */
	if (! allow_extra_modifiers) return -1;
    }

  if (event->event.key.modifiers & MOD_META)
    {
      if (! allow_meta) return -1;
      if (c & 0200) return -1;		/* don't allow M-oslash (overlap) */
#ifdef MULE
      if (c >= 256) return -1;
#endif
      c |= 0200;
    }
  return c;
}

DEFUN ("event-to-character", Fevent_to_character, 1, 4, 0, /*
Return the closest ASCII approximation to the given event object.
If the event isn't a keypress, this returns nil.
If the ALLOW-EXTRA-MODIFIERS argument is non-nil, then this is lenient in
 its translation; it will ignore modifier keys other than control and meta,
 and will ignore the shift modifier on those characters which have no
 shifted ASCII equivalent (Control-Shift-A for example, will be mapped to
 the same ASCII code as Control-A).
If the ALLOW-META argument is non-nil, then the Meta modifier will be
 represented by turning on the high bit of the byte returned; otherwise, nil
 will be returned for events containing the Meta modifier.
If the ALLOW-NON-ASCII argument is non-nil, then characters which are
 present in the prevailing character set (see the `character-set-property'
 variable) will be returned as their code in that character set, instead of
 the return value being restricted to ASCII.
Note that specifying both ALLOW-META and ALLOW-NON-ASCII is ambiguous, as
 both use the high bit; `M-x' and `oslash' will be indistinguishable.
*/
     (event, allow_extra_modifiers, allow_meta, allow_non_ascii))
{
  Emchar c;
  CHECK_LIVE_EVENT (event);
  c = event_to_character (XEVENT (event),
			  !NILP (allow_extra_modifiers),
			  !NILP (allow_meta),
			  !NILP (allow_non_ascii));
  return c < 0 ? Qnil : make_char (c);
}

DEFUN ("character-to-event", Fcharacter_to_event, 1, 4, 0, /*
Converts a keystroke specifier into an event structure, replete with
bucky bits.  The keystroke is the first argument, and the event to fill
in is the second.  This function contains knowledge about what the codes
``mean'' -- for example, the number 9 is converted to the character ``Tab'',
not the distinct character ``Control-I''.

Note that CH (the keystroke specifier) can be an integer, a character,
a symbol such as 'clear, or a list such as '(control backspace).

If the optional second argument is an event, it is modified;
otherwise, a new event object is created.

Optional third arg CONSOLE is the console to store in the event, and
defaults to the selected console.

If CH is an integer or character, the high bit may be interpreted as the
meta key. (This is done for backward compatibility in lots of places.)
If USE-CONSOLE-META-FLAG is nil, this will always be the case.  If
USE-CONSOLE-META-FLAG is non-nil, the `meta' flag for CONSOLE affects
whether the high bit is interpreted as a meta key. (See `set-input-mode'.)
If you don't want this silly meta interpretation done, you should pass
in a list containing the character.

Beware that character-to-event and event-to-character are not strictly
inverse functions, since events contain much more information than the
ASCII character set can encode.
*/
       (ch, event, console, use_console_meta_flag))
{
  struct console *con = decode_console (console);
  if (NILP (event))
    event = Fmake_event (Qnil, Qnil);
  else
    CHECK_LIVE_EVENT (event);
  if (CONSP (ch) || SYMBOLP (ch))
    key_desc_list_to_event (ch, event, 1);
  else
    {
      CHECK_CHAR_COERCE_INT (ch);
      character_to_event (XCHAR (ch), XEVENT (event), con,
			  !NILP (use_console_meta_flag));
    }
  return event;
}

void
nth_of_key_sequence_as_event (Lisp_Object seq, int n, Lisp_Object event)
{
  assert (STRINGP (seq) || VECTORP (seq));
  assert (n < XINT (Flength (seq)));

  if (STRINGP (seq))
    {
      Emchar ch = string_char (XSTRING (seq), n);
      Fcharacter_to_event (make_char (ch), event, Qnil, Qnil);
    }
  else
    {
      Lisp_Object keystroke = XVECTOR_DATA (seq)[n];
      if (EVENTP (keystroke))
	Fcopy_event (keystroke, event);
      else
	Fcharacter_to_event (keystroke, event, Qnil, Qnil);
    }
}

Lisp_Object
key_sequence_to_event_chain (Lisp_Object seq)
{
  int len = XINT (Flength (seq));
  int i;
  Lisp_Object head = Qnil, tail = Qnil;

  for (i = 0; i < len; i++)
    {
      Lisp_Object event = Fmake_event (Qnil, Qnil);
      nth_of_key_sequence_as_event (seq, i, event);
      enqueue_event (event, &head, &tail);
    }

  return head;
}

void
format_event_object (char *buf, struct Lisp_Event *event, int brief)
{
  int mouse_p = 0;
  int mod = 0;
  Lisp_Object key;
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
  int dnd_p = 0;
#endif

  switch (event->event_type)
    {
    case key_press_event:
      {
        mod = event->event.key.modifiers;
        key = event->event.key.keysym;
        /* Hack. */
        if (! brief && CHARP (key) &&
            mod & (MOD_CONTROL | MOD_META | MOD_SUPER | MOD_HYPER))
	{
	  int k = XCHAR (key);
	  if (k >= 'a' && k <= 'z')
	    key = make_char (k - ('a' - 'A'));
	  else if (k >= 'A' && k <= 'Z')
	    mod |= MOD_SHIFT;
	}
        break;
      }
    case button_release_event:
      mouse_p++;
      /* Fall through */
    case button_press_event:
      {
        mouse_p++;
        mod = event->event.button.modifiers;
        key = make_char (event->event.button.button + '0');
        break;
      }
    case magic_event:
      {
        CONST char *name = NULL;

#ifdef HAVE_X_WINDOWS
	{
	  Lisp_Object console = CDFW_CONSOLE (EVENT_CHANNEL (event));
	  if (CONSOLE_X_P (XCONSOLE (console)))
	    name = x_event_name (event->event.magic.underlying_x_event.type);
	}
#endif /* HAVE_X_WINDOWS */
	if (name) strcpy (buf, name);
	else strcpy (buf, "???");
	return;
      }
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event:
      {
	dnd_p++;
        mod = event->event.dnd_drop.modifiers;
        key = make_char (event->event.dnd_drop.button + '0');
        break;
      }
#endif
    case magic_eval_event:	strcpy (buf, "magic-eval"); return;
    case pointer_motion_event:	strcpy (buf, "motion");	    return;
    case misc_user_event:	strcpy (buf, "misc-user");  return;
    case eval_event:		strcpy (buf, "eval");	    return;
    case process_event:		strcpy (buf, "process");    return;
    case timeout_event:		strcpy (buf, "timeout");    return;
    case empty_event:		strcpy (buf, "empty");	    return;
    case dead_event:		strcpy (buf, "DEAD-EVENT"); return;
    default:
      abort ();
    }
#define modprint1(x)  { strcpy (buf, (x)); buf += sizeof (x)-1; }
#define modprint(x,y) { if (brief) modprint1 (y) else modprint1 (x) }
  if (mod & MOD_CONTROL) modprint ("control-", "C-");
  if (mod & MOD_META)    modprint ("meta-",    "M-");
  if (mod & MOD_SUPER)   modprint ("super-",   "S-");
  if (mod & MOD_HYPER)   modprint ("hyper-",   "H-");
  if (mod & MOD_ALT)	 modprint ("alt-",     "A-");
  if (mod & MOD_SHIFT)   modprint ("shift-",   "Sh-");
  if (mouse_p)
    {
      modprint1 ("button");
      --mouse_p;
    }

#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
  switch (dnd_p)
    {
    case 1:
      modprint1 ("drop");
    }
#endif

#undef modprint
#undef modprint1

  if (CHARP (key))
    {
      buf += set_charptr_emchar ((Bufbyte *) buf, XCHAR (key));
      *buf = 0;
    }
  else if (SYMBOLP (key))
    {
      CONST char *str = 0;
#if 0 /* obsolete keynames */
      if (brief)
	{
	  if      (EQ (key, QKlinefeed))  str = "LFD";
	  else if (EQ (key, QKtab))       str = "TAB";
	  else if (EQ (key, QKreturn))    str = "RET";
	  else if (EQ (key, QKescape))    str = "ESC";
	  else if (EQ (key, QKdelete))    str = "DEL";
	  else if (EQ (key, QKspace))     str = "SPC";
	  else if (EQ (key, QKbackspace)) str = "BS";
	}
#endif
      if (str)
	{
	  int i = strlen (str);
	  memcpy (buf, str, i+1);
	  str += i;
	}
      else
	{
	  struct Lisp_String *name = XSYMBOL (key)->name;
	  memcpy (buf, string_data (name), string_length (name) + 1);
	  str += string_length (name);
	}
    }
  else
    abort ();
  if (mouse_p)
    strncpy (buf, "up", 4);
}

DEFUN ("eventp", Feventp, 1, 1, 0, /*
True if OBJECT is an event object.
*/
       (object))
{
  return EVENTP (object) ? Qt : Qnil;
}

DEFUN ("event-live-p", Fevent_live_p, 1, 1, 0, /*
True if OBJECT is an event object that has not been deallocated.
*/
       (object))
{
  return EVENTP (object) && XEVENT (object)->event_type != dead_event ?
    Qt : Qnil;
}

#if 0 /* debugging functions */

xxDEFUN ("event-next", Fevent_next, 1, 1, 0, /*
Return the event object's `next' event, or nil if it has none.
The `next-event' field is changed by calling `set-next-event'.
*/
	 (event))
{
  struct Lisp_Event *e;
  CHECK_LIVE_EVENT (event);

  return XEVENT_NEXT (event);
}

xxDEFUN ("set-event-next", Fset_event_next, 2, 2, 0, /*
Set the `next event' of EVENT to NEXT-EVENT.
NEXT-EVENT must be an event object or nil.
*/
	 (event, next_event))
{
  Lisp_Object ev;

  CHECK_LIVE_EVENT (event);
  if (NILP (next_event))
    {
      XSET_EVENT_NEXT (event, Qnil);
      return Qnil;
    }

  CHECK_LIVE_EVENT (next_event);

  EVENT_CHAIN_LOOP (ev, XEVENT_NEXT (event))
    {
      QUIT;
      if (EQ (ev, event))
	signal_error (Qerror,
		      list3 (build_string ("Cyclic event-next"),
			     event,
			     next_event));
    }
  XSET_EVENT_NEXT (event, next_event);
  return next_event;
}

#endif /* 0 */

DEFUN ("event-type", Fevent_type, 1, 1, 0, /*
Return the type of EVENT.
This will be a symbol; one of

key-press	A key was pressed.
button-press	A mouse button was pressed.
button-release	A mouse button was released.
misc-user	Some other user action happened; typically, this is
		a menu selection or scrollbar action.
motion		The mouse moved.
process		Input is available from a subprocess.
timeout		A timeout has expired.
eval		This causes a specified action to occur when dispatched.
magic		Some window-system-specific event has occurred.
empty		The event has been allocated but not assigned.

*/
       (event))
{
  CHECK_LIVE_EVENT (event);
  switch (XEVENT (event)->event_type)
    {
    case key_press_event:	return Qkey_press;
    case button_press_event:	return Qbutton_press;
    case button_release_event:	return Qbutton_release;
    case misc_user_event:	return Qmisc_user;
    case pointer_motion_event:	return Qmotion;
    case process_event:		return Qprocess;
    case timeout_event:		return Qtimeout;
    case eval_event:		return Qeval;
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event:	return Qdnd_drop;
#endif
    case magic_event:
    case magic_eval_event:
      return Qmagic;

    case empty_event:
      return Qempty;

    default:
      abort ();
      return Qnil;
    }
}

DEFUN ("event-timestamp", Fevent_timestamp, 1, 1, 0, /*
Return the timestamp of the event object EVENT.
*/
       (event))
{
  CHECK_LIVE_EVENT (event);
  /* This junk is so that timestamps don't get to be negative, but contain
     as many bits as this particular emacs will allow.
   */
  return make_int (((1L << (VALBITS - 1)) - 1) &
		      XEVENT (event)->timestamp);
}

#define CHECK_EVENT_TYPE(e,t1,sym) do {		\
  CHECK_LIVE_EVENT (e);				\
  if (XEVENT(e)->event_type != (t1))		\
    e = wrong_type_argument ((sym),(e));	\
} while (0)

#define CHECK_EVENT_TYPE2(e,t1,t2,sym) do {	\
  CHECK_LIVE_EVENT (e);				\
  if (XEVENT(e)->event_type != (t1) &&		\
      XEVENT(e)->event_type != (t2))		\
    e = wrong_type_argument ((sym),(e));	\
} while (0)

DEFUN ("event-key", Fevent_key, 1, 1, 0, /*
Return the Keysym of the key-press event EVENT.
This will be a character if the event is associated with one, else a symbol.
*/
       (event))
{
  CHECK_EVENT_TYPE (event, key_press_event, Qkey_press_event_p);
  return XEVENT (event)->event.key.keysym;
}

DEFUN ("event-button", Fevent_button, 1, 1, 0, /*
Return the button-number of the given button-press or button-release event.
*/
       (event))
{
#if !defined(HAVE_OFFIX_DND) && !defined(HAVE_MS_WINDOWS)

  CHECK_EVENT_TYPE2 (event, button_press_event, button_release_event,
		     Qbutton_event_p);
#ifdef HAVE_WINDOW_SYSTEM
  return make_int (XEVENT (event)->event.button.button);
#else /* !HAVE_WINDOW_SYSTEM */
  return Qzero;
#endif /* !HAVE_WINDOW_SYSTEM */

#else /* HAVE_OFFIX_DND || HAVE_MS_WINDOWS */

  CHECK_LIVE_EVENT (event);
  if (XEVENT(event)->event_type == (button_press_event) ||
      XEVENT(event)->event_type == (button_release_event))
    /* we always have X if we have OffiX !! */
    return make_int (XEVENT (event)->event.button.button);
  else if (XEVENT(event)->event_type == (dnd_drop_event))
    /* we always have X if we have OffiX !! */
    return make_int (XEVENT (event)->event.button.button);
  else
    return wrong_type_argument ((Qbutton_event_p),(event));

#endif
}

DEFUN ("event-modifier-bits", Fevent_modifier_bits, 1, 1, 0, /*
Return a number representing the modifier keys which were down
when the given mouse or keyboard event was produced.
See also the function event-modifiers.
*/
       (event))
{
 again:
  CHECK_LIVE_EVENT (event);
  switch (XEVENT (event)->event_type)
    {
    case key_press_event:
      return make_int (XEVENT (event)->event.key.modifiers);
    case button_press_event:
    case button_release_event:
      return make_int (XEVENT (event)->event.button.modifiers);
    case pointer_motion_event:
      return make_int (XEVENT (event)->event.motion.modifiers);
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event:
      return make_int (XEVENT (event)->event.dnd_drop.modifiers);
#endif
    default:
      event = wrong_type_argument (intern ("key-or-mouse-event-p"), event);
      goto again;
    }
}

DEFUN ("event-modifiers", Fevent_modifiers, 1, 1, 0, /*
Return a list of symbols, the names of the modifier keys
which were down when the given mouse or keyboard event was produced.
See also the function event-modifier-bits.
*/
       (event))
{
  int mod = XINT (Fevent_modifier_bits (event));
  Lisp_Object result = Qnil;
  if (mod & MOD_SHIFT)   result = Fcons (Qshift, result);
  if (mod & MOD_ALT)	 result = Fcons (Qalt, result);
  if (mod & MOD_HYPER)   result = Fcons (Qhyper, result);
  if (mod & MOD_SUPER)   result = Fcons (Qsuper, result);
  if (mod & MOD_META)    result = Fcons (Qmeta, result);
  if (mod & MOD_CONTROL) result = Fcons (Qcontrol, result);
  return result;
}

static int
event_x_y_pixel_internal (Lisp_Object event, int *x, int *y, int relative)
{
  struct window *w;
  struct frame *f;

  if (XEVENT (event)->event_type == pointer_motion_event)
    {
      *x = XEVENT (event)->event.motion.x;
      *y = XEVENT (event)->event.motion.y;
    }
  else if (XEVENT (event)->event_type == button_press_event ||
	   XEVENT (event)->event_type == button_release_event)
    {
      *x = XEVENT (event)->event.button.x;
      *y = XEVENT (event)->event.button.y;
    }
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
  else if (XEVENT (event)->event_type == dnd_drop_event)
    {
      *x = XEVENT (event)->event.dnd_drop.x;
      *y = XEVENT (event)->event.dnd_drop.y;
    }
#endif
  else
    return 0;

  f = XFRAME (EVENT_CHANNEL (XEVENT (event)));

  if (relative)
    {
      w = find_window_by_pixel_pos (*x, *y, f->root_window);

      if (!w)
	return 1;	/* #### What should really happen here. */

      *x -= w->pixel_left;
      *y -= w->pixel_top;
    }
  else
    {
      *y -= FRAME_REAL_TOP_TOOLBAR_HEIGHT (f) -
	FRAME_REAL_TOP_TOOLBAR_BORDER_WIDTH (f);
      *x -= FRAME_REAL_LEFT_TOOLBAR_WIDTH (f) -
	FRAME_REAL_LEFT_TOOLBAR_BORDER_WIDTH (f);
    }

  return 1;
}

DEFUN ("event-window-x-pixel", Fevent_window_x_pixel, 1, 1, 0, /*
Return the X position in pixels of mouse event EVENT.
The value returned is relative to the window the event occurred in.
This will signal an error if the event is not a mouse event.
See also `mouse-event-p' and `event-x-pixel'.
*/
       (event))
{
  int x, y;

  CHECK_LIVE_EVENT (event);

  if (!event_x_y_pixel_internal (event, &x, &y, 1))
    return wrong_type_argument (Qmouse_event_p, event);
  else
    return make_int (x);
}

DEFUN ("event-window-y-pixel", Fevent_window_y_pixel, 1, 1, 0, /*
Return the Y position in pixels of mouse event EVENT.
The value returned is relative to the window the event occurred in.
This will signal an error if the event is not a mouse event.
See also `mouse-event-p' and `event-y-pixel'.
*/
       (event))
{
  int x, y;

  CHECK_LIVE_EVENT (event);

  if (!event_x_y_pixel_internal (event, &x, &y, 1))
    return wrong_type_argument (Qmouse_event_p, event);
  else
    return make_int (y);
}

DEFUN ("event-x-pixel", Fevent_x_pixel, 1, 1, 0, /*
Return the X position in pixels of mouse event EVENT.
The value returned is relative to the frame the event occurred in.
This will signal an error if the event is not a mouse event.
See also `mouse-event-p' and `event-window-x-pixel'.
*/
       (event))
{
  int x, y;

  CHECK_LIVE_EVENT (event);

  if (!event_x_y_pixel_internal (event, &x, &y, 0))
    return wrong_type_argument (Qmouse_event_p, event);
  else
    return make_int (x);
}

DEFUN ("event-y-pixel", Fevent_y_pixel, 1, 1, 0, /*
Return the Y position in pixels of mouse event EVENT.
The value returned is relative to the frame the event occurred in.
This will signal an error if the event is not a mouse event.
See also `mouse-event-p' `event-window-y-pixel'.
*/
       (event))
{
  int x, y;

  CHECK_LIVE_EVENT (event);

  if (!event_x_y_pixel_internal (event, &x, &y, 0))
    return wrong_type_argument (Qmouse_event_p, event);
  else
    return make_int (y);
}

/* Given an event, return a value:

     OVER_TOOLBAR:	over one of the 4 frame toolbars
     OVER_MODELINE:	over a modeline
     OVER_BORDER:	over an internal border
     OVER_NOTHING:	over the text area, but not over text
     OVER_OUTSIDE:	outside of the frame border
     OVER_TEXT:		over text in the text area

   and return:

   The X char position in CHAR_X, if not a null pointer.
   The Y char position in CHAR_Y, if not a null pointer.
   (These last two values are relative to the window the event is over.)
   The window it's over in W, if not a null pointer.
   The buffer position it's over in BUFP, if not a null pointer.
   The closest buffer position in CLOSEST, if not a null pointer.

   OBJ_X, OBJ_Y, OBJ1, and OBJ2 are as in pixel_to_glyph_translation().
*/

static int
event_pixel_translation (Lisp_Object event, int *char_x, int *char_y,
			 int *obj_x, int *obj_y,
			 struct window **w, Bufpos *bufp, Bufpos *closest,
			 Charcount *modeline_closest,
			 Lisp_Object *obj1, Lisp_Object *obj2)
{
  int pix_x = 0;
  int pix_y = 0;
  int result;
  Lisp_Object frame = Qnil;

  int ret_x, ret_y, ret_obj_x, ret_obj_y;
  struct window *ret_w;
  Bufpos ret_bufp, ret_closest;
  Charcount ret_modeline_closest;
  Lisp_Object ret_obj1, ret_obj2;

  CHECK_LIVE_EVENT (event);
  frame = XEVENT (event)->channel;
  switch (XEVENT (event)->event_type)
    {
    case pointer_motion_event :
      pix_x = XEVENT (event)->event.motion.x;
      pix_y = XEVENT (event)->event.motion.y;
      break;
    case button_press_event :
    case button_release_event :
      pix_x = XEVENT (event)->event.button.x;
      pix_y = XEVENT (event)->event.button.y;
      break;
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event :
      pix_x = XEVENT (event)->event.dnd_drop.x;
      pix_y = XEVENT (event)->event.dnd_drop.y;
      break;
#endif
    default:
      dead_wrong_type_argument (Qmouse_event_p, event);
    }

  result = pixel_to_glyph_translation (XFRAME (frame), pix_x, pix_y,
				       &ret_x, &ret_y, &ret_obj_x, &ret_obj_y,
				       &ret_w, &ret_bufp, &ret_closest,
				       &ret_modeline_closest,
				       &ret_obj1, &ret_obj2);

  if (result == OVER_NOTHING || result == OVER_OUTSIDE)
    ret_bufp = 0;
  else if (ret_w && NILP (ret_w->buffer))
    /* Why does this happen?  (Does it still happen?)
       I guess the window has gotten reused as a non-leaf... */
    ret_w = 0;

  /* #### pixel_to_glyph_translation() sometimes returns garbage...
     The word has type Lisp_Type_Record (presumably meaning `extent') but the
     pointer points to random memory, often filled with 0, sometimes not.
   */
  /* #### Chuck, do we still need this crap? */
  if (!NILP (ret_obj1) && !(GLYPHP (ret_obj1)
#ifdef HAVE_TOOLBARS
			    || TOOLBAR_BUTTONP (ret_obj1)
#endif
     ))
    abort ();
  if (!NILP (ret_obj2) && !(EXTENTP (ret_obj2) || CONSP (ret_obj2)))
    abort ();

  if (char_x)
    *char_x = ret_x;
  if (char_y)
    *char_y = ret_y;
  if (obj_x)
    *obj_x = ret_obj_x;
  if (obj_y)
    *obj_y = ret_obj_y;
  if (w)
    *w = ret_w;
  if (bufp)
    *bufp = ret_bufp;
  if (closest)
    *closest = ret_closest;
  if (modeline_closest)
    *modeline_closest = ret_modeline_closest;
  if (obj1)
    *obj1 = ret_obj1;
  if (obj2)
    *obj2 = ret_obj2;

  return result;
}

DEFUN ("event-over-text-area-p", Fevent_over_text_area_p, 1, 1, 0, /*
Return t if the mouse event EVENT occurred over the text area of a window.
The modeline is not considered to be part of the text area.
*/
       (event))
{
  int result = event_pixel_translation (event, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  return result == OVER_TEXT || result == OVER_NOTHING ? Qt : Qnil;
}

DEFUN ("event-over-modeline-p", Fevent_over_modeline_p, 1, 1, 0, /*
Return t if the mouse event EVENT occurred over the modeline of a window.
*/
       (event))
{
  int result = event_pixel_translation (event, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  return result == OVER_MODELINE ? Qt : Qnil;
}

DEFUN ("event-over-border-p", Fevent_over_border_p, 1, 1, 0, /*
Return t if the mouse event EVENT occurred over an internal border.
*/
       (event))
{
  int result = event_pixel_translation (event, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  return result == OVER_BORDER ? Qt : Qnil;
}

DEFUN ("event-over-toolbar-p", Fevent_over_toolbar_p, 1, 1, 0, /*
Return t if the mouse event EVENT occurred over a toolbar.
*/
       (event))
{
  int result = event_pixel_translation (event, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  return result == OVER_TOOLBAR ? Qt : Qnil;
}

struct console *
event_console_or_selected (Lisp_Object event)
{
  Lisp_Object channel = EVENT_CHANNEL (XEVENT (event));
  Lisp_Object console = CDFW_CONSOLE (channel);

  if (NILP (console))
    console = Vselected_console;

  return XCONSOLE (console);
}

DEFUN ("event-channel", Fevent_channel, 1, 1, 0, /*
Return the channel that the event EVENT occurred on.
This will be a frame, device, console, or nil for some types
of events (e.g. eval events).
*/
       (event))
{
  CHECK_LIVE_EVENT (event);
  return EVENT_CHANNEL (XEVENT (event));
}

DEFUN ("event-window", Fevent_window, 1, 1, 0, /*
Return the window over which mouse event EVENT occurred.
This may be nil if the event occurred in the border or over a toolbar.
The modeline is considered to be within the window it describes.
*/
       (event))
{
  struct window *w;

  event_pixel_translation (event, 0, 0, 0, 0, &w, 0, 0, 0, 0, 0);

  if (!w)
    return Qnil;
  else
    {
      Lisp_Object window;

      XSETWINDOW (window, w);
      return window;
    }
}

DEFUN ("event-point", Fevent_point, 1, 1, 0, /*
Return the character position of the mouse event EVENT.
If the event did not occur over a window, or did not occur over text,
then this returns nil.  Otherwise, it returns a position in the buffer
visible in the event's window.
*/
       (event))
{
  Bufpos bufp;
  struct window *w;

  event_pixel_translation (event, 0, 0, 0, 0, &w, &bufp, 0, 0, 0, 0);

  return w && bufp ? make_int (bufp) : Qnil;
}

DEFUN ("event-closest-point", Fevent_closest_point, 1, 1, 0, /*
Return the character position closest to the mouse event EVENT.
If the event did not occur over a window or over text, return the
closest point to the location of the event.  If the Y pixel position
overlaps a window and the X pixel position is to the left of that
window, the closest point is the beginning of the line containing the
Y position.  If the Y pixel position overlaps a window and the X pixel
position is to the right of that window, the closest point is the end
of the line containing the Y position.  If the Y pixel position is
above a window, return 0.  If it is below the last character in a window,
return the value of (window-end).
*/
       (event))
{
  Bufpos bufp;

  event_pixel_translation (event, 0, 0, 0, 0, 0, 0, &bufp, 0, 0, 0);

  return bufp ? make_int (bufp) : Qnil;
}

DEFUN ("event-x", Fevent_x, 1, 1, 0, /*
Return the X position of the mouse event EVENT in characters.
This is relative to the window the event occurred over.
*/
       (event))
{
  int char_x;

  event_pixel_translation (event, &char_x, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  return make_int (char_x);
}

DEFUN ("event-y", Fevent_y, 1, 1, 0, /*
Return the Y position of the mouse event EVENT in characters.
This is relative to the window the event occurred over.
*/
       (event))
{
  int char_y;

  event_pixel_translation (event, 0, &char_y, 0, 0, 0, 0, 0, 0, 0, 0);

  return make_int (char_y);
}

DEFUN ("event-modeline-position", Fevent_modeline_position, 1, 1, 0, /*
Return the character position in the modeline that EVENT occurred over.
EVENT should be a mouse event.  If EVENT did not occur over a modeline,
nil is returned.  You can determine the actual character that the
event occurred over by looking in `generated-modeline-string' at the
returned character position.  Note that `generated-modeline-string'
is buffer-local, and you must use EVENT's buffer when retrieving
`generated-modeline-string' in order to get accurate results.
*/
       (event))
{
  Charcount mbufp;
  int where;

  where = event_pixel_translation (event, 0, 0, 0, 0, 0, 0, 0, &mbufp, 0, 0);

  return (mbufp < 0 || where != OVER_MODELINE) ? Qnil : make_int (mbufp);
}

DEFUN ("event-glyph", Fevent_glyph, 1, 1, 0, /*
Return the glyph that the mouse event EVENT occurred over, or nil.
*/
       (event))
{
  Lisp_Object glyph;
  struct window *w;

  event_pixel_translation (event, 0, 0, 0, 0, &w, 0, 0, 0, &glyph, 0);

  return w && GLYPHP (glyph) ? glyph : Qnil;
}

DEFUN ("event-glyph-extent", Fevent_glyph_extent, 1, 1, 0, /*
Return the extent of the glyph that the mouse event EVENT occurred over.
If the event did not occur over a glyph, nil is returned.
*/
       (event))
{
  Lisp_Object extent;
  struct window *w;

  event_pixel_translation (event, 0, 0, 0, 0, &w, 0, 0, 0, 0, &extent);

  return w && EXTENTP (extent) ? extent : Qnil;
}

DEFUN ("event-glyph-x-pixel", Fevent_glyph_x_pixel, 1, 1, 0, /*
Return the X pixel position of EVENT relative to the glyph it occurred over.
EVENT should be a mouse event.  If the event did not occur over a glyph,
nil is returned.
*/
       (event))
{
  Lisp_Object extent;
  struct window *w;
  int obj_x;

  event_pixel_translation (event, 0, 0, &obj_x, 0, &w, 0, 0, 0, 0, &extent);

  return w && EXTENTP (extent) ? make_int (obj_x) : Qnil;
}

DEFUN ("event-glyph-y-pixel", Fevent_glyph_y_pixel, 1, 1, 0, /*
Return the Y pixel position of EVENT relative to the glyph it occurred over.
EVENT should be a mouse event.  If the event did not occur over a glyph,
nil is returned.
*/
       (event))
{
  Lisp_Object extent;
  struct window *w;
  int obj_y;

  event_pixel_translation (event, 0, 0, 0, &obj_y, &w, 0, 0, 0, 0, &extent);

  return w && EXTENTP (extent) ? make_int (obj_y) : Qnil;
}

DEFUN ("event-toolbar-button", Fevent_toolbar_button, 1, 1, 0, /*
Return the toolbar button that the mouse event EVENT occurred over.
If the event did not occur over a toolbar button, nil is returned.
*/
       (event))
{
#ifdef HAVE_TOOLBARS
  Lisp_Object button;

  int result = event_pixel_translation (event, 0, 0, 0, 0, 0, 0, 0, 0, &button, 0);

  return result == OVER_TOOLBAR && TOOLBAR_BUTTONP (button) ? button : Qnil;
#else
	return Qnil;
#endif
}

DEFUN ("event-process", Fevent_process, 1, 1, 0, /*
Return the process of the given process-output event.
*/
       (event))
{
  CHECK_EVENT_TYPE (event, process_event, Qprocess_event_p);
  return XEVENT (event)->event.process.process;
}

DEFUN ("event-function", Fevent_function, 1, 1, 0, /*
Return the callback function of EVENT.
EVENT should be a timeout, misc-user, or eval event.
*/
       (event))
{
  CHECK_LIVE_EVENT (event);
  switch (XEVENT (event)->event_type)
    {
    case timeout_event:
      return XEVENT (event)->event.timeout.function;
    case misc_user_event:
    case eval_event:
      return XEVENT (event)->event.eval.function;
    default:
      return wrong_type_argument (intern ("timeout-or-eval-event-p"), event);
    }
}

DEFUN ("event-object", Fevent_object, 1, 1, 0, /*
Return the callback function argument of EVENT.
EVENT should be a timeout, misc-user, or eval event.
*/
       (event))
{
 again:
  CHECK_LIVE_EVENT (event);
  switch (XEVENT (event)->event_type)
    {
    case timeout_event:
      return XEVENT (event)->event.timeout.object;
    case misc_user_event:
    case eval_event:
      return XEVENT (event)->event.eval.object;
    default:
      event = wrong_type_argument (intern ("timeout-or-eval-event-p"), event);
      goto again;
    }
}

DEFUN ("event-drag-and-drop-data", Fevent_drag_and_drop_data, 1, 1, 0, /*
Return the Dnd data list of EVENT.
EVENT should be a dnd_drop event.
*/
       (event))
{
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
 again:
  CHECK_LIVE_EVENT (event);
  switch (XEVENT (event)->event_type)
    {
    case dnd_drop_event:
      return XEVENT (event)->event.dnd_drop.data;
    default:
      event = wrong_type_argument (Qdnd_drop_event_p, event);
      goto again;
    }
#else /* !(HAVE_OFFIX_DND || HAVE_MS_WINDOWS) */
  return Qnil;
#endif /* HAVE_OFFIX_DND || HAVE_MS_WINDOWS */
}

DEFUN ("event-properties", Fevent_properties, 1, 1, 0, /*
Return a list of all of the properties of EVENT.
This is in the form of a property list (alternating keyword/value pairs).
*/
       (event))
{
  Lisp_Object props = Qnil;
  struct Lisp_Event *e;
  struct gcpro gcpro1;

  CHECK_LIVE_EVENT (event);
  e = XEVENT (event);
  GCPRO1 (props);

  props = Fcons (Qtimestamp, Fcons (Fevent_timestamp (event), props));

  switch (e->event_type)
    {
    case process_event:
      props = Fcons (Qprocess, Fcons (e->event.process.process, props));
      break;

    case timeout_event:
      props = Fcons (Qobject, Fcons (Fevent_object (event), props));
      props = Fcons (Qfunction, Fcons (Fevent_function (event), props));
      props = Fcons (Qid, Fcons (make_int (e->event.timeout.id_number),
				 props));
      break;

    case key_press_event:
      props = Fcons (Qmodifiers, Fcons (Fevent_modifiers (event), props));
      props = Fcons (Qkey, Fcons (Fevent_key (event), props));
      break;

    case button_press_event:
    case button_release_event:
      props = Fcons (Qy, Fcons (Fevent_y_pixel (event), props));
      props = Fcons (Qx, Fcons (Fevent_x_pixel (event), props));
      props = Fcons (Qmodifiers, Fcons (Fevent_modifiers (event), props));
      props = Fcons (Qbutton, Fcons (Fevent_button (event), props));
      break;

    case pointer_motion_event:
      props = Fcons (Qmodifiers, Fcons (Fevent_modifiers (event), props));
      props = Fcons (Qy, Fcons (Fevent_y_pixel (event), props));
      props = Fcons (Qx, Fcons (Fevent_x_pixel (event), props));
      break;

    case misc_user_event:
    case eval_event:
      props = Fcons (Qobject, Fcons (Fevent_object (event), props));
      props = Fcons (Qfunction, Fcons (Fevent_function (event), props));
      break;

#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
    case dnd_drop_event:
      props = Fcons (Qy, Fcons (Fevent_y_pixel (event), props));
      props = Fcons (Qx, Fcons (Fevent_x_pixel (event), props));
      props = Fcons (Qmodifiers, Fcons (Fevent_modifiers (event), props));
      props = Fcons (Qbutton, Fcons (Fevent_button (event), props));
      props = Fcons (Qdnd_data, Fcons (Fevent_drag_and_drop_data (event), props));
      break;
#endif

    case magic_eval_event:
    case magic_event:
      break;

    case empty_event:
      RETURN_UNGCPRO (Qnil);
      break;

    default:
      abort ();
      break;                 /* not reached; warning suppression */
    }

  props = Fcons (Qchannel, Fcons (Fevent_channel (event), props));
  UNGCPRO;

  return props;
}


/************************************************************************/
/*                            initialization                            */
/************************************************************************/

void
syms_of_events (void)
{
  DEFSUBR (Fcharacter_to_event);
  DEFSUBR (Fevent_to_character);

  DEFSUBR (Fmake_event);
  DEFSUBR (Fdeallocate_event);
  DEFSUBR (Fcopy_event);
  DEFSUBR (Feventp);
  DEFSUBR (Fevent_live_p);
  DEFSUBR (Fevent_type);
  DEFSUBR (Fevent_properties);

  DEFSUBR (Fevent_timestamp);
  DEFSUBR (Fevent_key);
  DEFSUBR (Fevent_button);
  DEFSUBR (Fevent_modifier_bits);
  DEFSUBR (Fevent_modifiers);
  DEFSUBR (Fevent_x_pixel);
  DEFSUBR (Fevent_y_pixel);
  DEFSUBR (Fevent_window_x_pixel);
  DEFSUBR (Fevent_window_y_pixel);
  DEFSUBR (Fevent_over_text_area_p);
  DEFSUBR (Fevent_over_modeline_p);
  DEFSUBR (Fevent_over_border_p);
  DEFSUBR (Fevent_over_toolbar_p);
  DEFSUBR (Fevent_channel);
  DEFSUBR (Fevent_window);
  DEFSUBR (Fevent_point);
  DEFSUBR (Fevent_closest_point);
  DEFSUBR (Fevent_x);
  DEFSUBR (Fevent_y);
  DEFSUBR (Fevent_modeline_position);
  DEFSUBR (Fevent_glyph);
  DEFSUBR (Fevent_glyph_extent);
  DEFSUBR (Fevent_glyph_x_pixel);
  DEFSUBR (Fevent_glyph_y_pixel);
  DEFSUBR (Fevent_toolbar_button);
  DEFSUBR (Fevent_process);
  DEFSUBR (Fevent_function);
  DEFSUBR (Fevent_object);
  DEFSUBR (Fevent_drag_and_drop_data);

  defsymbol (&Qeventp, "eventp");
  defsymbol (&Qevent_live_p, "event-live-p");
  defsymbol (&Qkey_press_event_p, "key-press-event-p");
  defsymbol (&Qbutton_event_p, "button-event-p");
  defsymbol (&Qmouse_event_p, "mouse-event-p");
  defsymbol (&Qprocess_event_p, "process-event-p");
  defsymbol (&Qkey_press, "key-press");
  defsymbol (&Qbutton_press, "button-press");
  defsymbol (&Qbutton_release, "button-release");
  defsymbol (&Qmisc_user, "misc-user");
  defsymbol (&Qascii_character, "ascii-character");
#if defined(HAVE_OFFIX_DND) || defined(HAVE_MS_WINDOWS)
  defsymbol (&Qdnd_drop_event_p, "dnd-drop-event-p");
  defsymbol (&Qdnd_drop, "dnd-drop");
#endif
}

void
vars_of_events (void)
{
  DEFVAR_LISP ("character-set-property", &Vcharacter_set_property /*
A symbol used to look up the 8-bit character of a keysym.
To convert a keysym symbol to an 8-bit code, as when that key is
bound to self-insert-command, we will look up the property that this
variable names on the property list of the keysym-symbol.  The window-
system-specific code will set up appropriate properties and set this
variable.
*/ );
  Vcharacter_set_property = Qnil;

  Vevent_resource = Qnil;

  QKbackspace = KEYSYM ("backspace");
  QKtab       = KEYSYM ("tab");
  QKlinefeed  = KEYSYM ("linefeed");
  QKreturn    = KEYSYM ("return");
  QKescape    = KEYSYM ("escape");
  QKspace     = KEYSYM ("space");
  QKdelete    = KEYSYM ("delete");

  staticpro (&QKbackspace);
  staticpro (&QKtab);
  staticpro (&QKlinefeed);
  staticpro (&QKreturn);
  staticpro (&QKescape);
  staticpro (&QKspace);
  staticpro (&QKdelete);
}
