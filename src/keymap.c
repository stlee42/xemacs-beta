/* Manipulation of keymaps
   Copyright (C) 1985, 1991-1995 Free Software Foundation, Inc.
   Copyright (C) 1995 Board of Trustees, University of Illinois.
   Copyright (C) 1995 Sun Microsystems, Inc.
   Totally redesigned by jwz in 1991.

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

/* Synched up with: Mule 2.0.  Not synched with FSF.  Substantially
   different from FSF. */


#include <config.h>
#include "lisp.h"

#include "buffer.h"
#include "bytecode.h"
#include "commands.h"
#include "console.h"
#include "elhash.h"
#include "events.h"
#include "frame.h"
#include "insdel.h"
#include "keymap.h"
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


/* A keymap contains four slots:

   parents	   Ordered list of keymaps to search after
                   this one if no match is found.
		   Keymaps can thus be arranged in a hierarchy.

   table	   A hash table, hashing keysyms to their bindings.
   		   It will be one of the following:

		   -- a symbol, e.g. 'home
		   -- a character, representing something printable
		      (not ?\C-c meaning C-c, for instance)
		   -- an integer representing a modifier combination

   inverse_table   A hash table, hashing bindings to the list of keysyms
		   in this keymap which are bound to them.  This is to make
		   the Fwhere_is_internal() function be fast.  It needs to be
		   fast because we want to be able to call it in realtime to
		   update the keyboard-equivalents on the pulldown menus.
                   Values of the table are either atoms (keysyms)
                   or a dotted list of keysyms.

   sub_maps_cache  An alist; for each entry in this keymap whose binding is
		   a keymap (that is, Fkeymapp()) this alist associates that
		   keysym with that binding.  This is used to optimize both
		   Fwhere_is_internal() and Faccessible_keymaps().  This slot
		   gets set to the symbol `t' every time a change is made to
		   this keymap, causing it to be recomputed when next needed.

   prompt          See `set-keymap-prompt'.

   default_binding See `set-keymap-default-binding'.

   Sequences of keys are stored in the obvious way: if the sequence of keys
   "abc" was bound to some command `foo', the hierarchy would look like

      keymap-1: associates "a" with keymap-2
      keymap-2: associates "b" with keymap-3
      keymap-3: associates "c" with foo

   However, bucky bits ("modifiers" to the X-minded) are represented in the
   keymap hierarchy as well. (This lets us use EQable objects as hash keys.)
   Each combination of modifiers (e.g. control-hyper) gets its own submap
   off of the main map.  The hash key for a modifier combination is
   an integer, computed by MAKE_MODIFIER_HASH_KEY().

   If the key `C-a' was bound to some command, the hierarchy would look like

      keymap-1: associates the integer MOD_CONTROL with keymap-2
      keymap-2: associates "a" with the command

   Similarly, if the key `C-H-a' was bound to some command, the hierarchy
   would look like

      keymap-1: associates the integer (MOD_CONTROL | MOD_HYPER)
                with keymap-2
      keymap-2: associates "a" with the command

   Note that a special exception is made for the meta modifier, in order
   to deal with ESC/meta lossage.  Any key combination containing the
   meta modifier is first indexed off of the main map into the meta
   submap (with hash key MOD_META) and then indexed off of the
   meta submap with the meta modifier removed from the key combination.
   For example, when associating a command with C-M-H-a, we'd have

      keymap-1: associates the integer MOD_META with keymap-2
      keymap-2: associates the integer (MOD_CONTROL | MOD_HYPER)
                with keymap-3
      keymap-3: associates "a" with the command

   Note that keymap-2 might have normal bindings in it; these would be
   for key combinations containing only the meta modifier, such as
   M-y or meta-backspace.

   If the command that "a" was bound to in keymap-3 was itself a keymap,
   then that would make the key "C-M-H-a" be a prefix character.

   Note that this new model of keymaps takes much of the magic away from
   the Escape key: the value of the variable `esc-map' is no longer indexed
   in the `global-map' under the ESC key.  It's indexed under the integer
   MOD_META.  This is not user-visible, however; none of the "bucky"
   maps are.

   There is a hack in Flookup_key() that makes (lookup-key global-map "\^[")
   and (define-key some-random-map "\^[" my-esc-map) work as before, for
   compatibility.

   Since keymaps are opaque, the only way to extract information from them
   is with the functions lookup-key, key-binding, local-key-binding, and
   global-key-binding, which work just as before, and the new function
   map-keymap, which is roughly analagous to maphash.

   Note that map-keymap perpetuates the illusion that the "bucky" submaps
   don't exist: if you map over a keymap with bucky submaps, it will also
   map over those submaps.  It does not, however, map over other random
   submaps of the keymap, just the bucky ones.

   One implication of this is that when you map over `global-map', you will
   also map over `esc-map'.  It is merely for compatibility that the esc-map
   is accessible at all; I think that's a bad thing, since it blurs the
   distinction between ESC and "meta" even more.  "M-x" is no more a two-
   key sequence than "C-x" is.

 */

struct keymap
{
  struct lcrecord_header header;
  Lisp_Object parents;		/* Keymaps to be searched after this one
				 *  An ordered list */
  Lisp_Object prompt;           /* Qnil or a string to print in the minibuffer
                                 *  when reading from this keymap */

  Lisp_Object table;		/* The contents of this keymap */
  Lisp_Object inverse_table;	/* The inverse mapping of the above */

  Lisp_Object default_binding;  /* Use this if no other binding is found
                                 *  (this overrides parent maps and the
                                 *   normal global-map lookup). */


  Lisp_Object sub_maps_cache;	/* Cache of directly inferior keymaps;
				   This holds an alist, of the key and the
				   maps, or the modifier bit and the map.
				   If this is the symbol t, then the cache
				   needs to be recomputed.
				 */
  int fullness;			/* How many entries there are in this table.
 				   This should be the same as the fullness
 				   of the `table', but hash.c is broken. */
  Lisp_Object name;             /* Just for debugging convenience */
};

#define XKEYMAP(x) XRECORD (x, keymap, struct keymap)
#define XSETKEYMAP(x, p) XSETRECORD (x, p, keymap)
#define KEYMAPP(x) RECORDP (x, keymap)
#define CHECK_KEYMAP(x) CHECK_RECORD (x, keymap)

#define MAKE_MODIFIER_HASH_KEY(modifier) make_int (modifier)
#define MODIFIER_HASH_KEY_BITS(x) (INTP (x) ? XINT (x) : 0)



/* Actually allocate storage for these variables */

static Lisp_Object Vcurrent_global_map; /* Always a keymap */

static Lisp_Object Vmouse_grabbed_buffer;

/* Alist of minor mode variables and keymaps.  */
static Lisp_Object Qminor_mode_map_alist;

static Lisp_Object Voverriding_local_map;

static Lisp_Object Vkey_translation_map;

/* This is incremented whenever a change is made to a keymap.  This is
   so that things which care (such as the menubar code) can recompute
   privately-cached data when the user has changed keybindings.
 */
int keymap_tick;

/* Prefixing a key with this character is the same as sending a meta bit. */
Lisp_Object Vmeta_prefix_char;

Lisp_Object Qkeymapp;

Lisp_Object Vsingle_space_string;

Lisp_Object Qsuppress_keymap;

Lisp_Object Qmodeline_map;
Lisp_Object Qtoolbar_map;

static void describe_command (Lisp_Object definition);
static void describe_map (Lisp_Object keymap, Lisp_Object elt_prefix,
			  void (*elt_describer) (Lisp_Object),
			  int partial,
			  Lisp_Object shadow,
			  int mice_only_p);
Lisp_Object Qcontrol, Qctrl, Qmeta, Qsuper, Qhyper, Qalt, Qshift;
/* Lisp_Object Qsymbol;	defined in general.c */
Lisp_Object Qbutton0, Qbutton1, Qbutton2, Qbutton3, Qbutton4, Qbutton5,
  Qbutton6, Qbutton7;
Lisp_Object Qbutton0up, Qbutton1up, Qbutton2up, Qbutton3up, Qbutton4up,
  Qbutton5up, Qbutton6up, Qbutton7up;
#ifdef HAVE_OFFIX_DND
Lisp_Object Qdrop0, Qdrop1, Qdrop2, Qdrop3, Qdrop4, Qdrop5, Qdrop6, Qdrop7;
#endif
Lisp_Object Qmenu_selection;
/* Emacs compatibility */
Lisp_Object Qdown_mouse_1, Qdown_mouse_2, Qdown_mouse_3;
Lisp_Object Qmouse_1, Qmouse_2, Qmouse_3;

/* Kludge kludge kludge */
Lisp_Object QLFD, QTAB, QRET, QESC, QDEL, QSPC, QBS;


/************************************************************************/
/*                     The keymap Lisp object                           */
/************************************************************************/

static Lisp_Object mark_keymap (Lisp_Object, void (*) (Lisp_Object));
static void print_keymap (Lisp_Object, Lisp_Object, int);
/* No need for keymap_equal #### Why not? */
DEFINE_LRECORD_IMPLEMENTATION ("keymap", keymap,
                               mark_keymap, print_keymap, 0, 0, 0,
			       struct keymap);
static Lisp_Object
mark_keymap (Lisp_Object obj, void (*markobj) (Lisp_Object))
{
  struct keymap *keymap = XKEYMAP (obj);
  ((markobj) (keymap->parents));
  ((markobj) (keymap->prompt));
  ((markobj) (keymap->inverse_table));
  ((markobj) (keymap->sub_maps_cache));
  ((markobj) (keymap->default_binding));
  ((markobj) (keymap->name));
  return keymap->table;
}

static void
print_keymap (Lisp_Object obj, Lisp_Object printcharfun, int escapeflag)
{
  /* This function can GC */
  struct keymap *keymap = XKEYMAP (obj);
  char buf[200];
  int size = XINT (Fkeymap_fullness (obj));
  if (print_readably)
    error ("printing unreadable object #<keymap 0x%x>", keymap->header.uid);
  write_c_string ("#<keymap ", printcharfun);
  if (!NILP (keymap->name))
    print_internal (keymap->name, printcharfun, 1);
  sprintf (buf, "%s%d entr%s 0x%x>",
           ((NILP (keymap->name)) ? "" : " "),
           size,
           ((size == 1) ? "y" : "ies"),
           keymap->header.uid);
  write_c_string (buf, printcharfun);
}


/************************************************************************/
/*                Traversing keymaps and their parents                  */
/************************************************************************/

static Lisp_Object
traverse_keymaps (Lisp_Object start_keymap, Lisp_Object start_parents,
                  Lisp_Object (*mapper) (Lisp_Object keymap, void *mapper_arg),
                  void *mapper_arg)
{
  /* This function can GC */
  Lisp_Object keymap;
  Lisp_Object tail = start_parents;
  Lisp_Object malloc_sucks[10];
  Lisp_Object malloc_bites = Qnil;
  int stack_depth = 0;
  struct gcpro gcpro1, gcpro2, gcpro3, gcpro4;
  GCPRO4 (*malloc_sucks, malloc_bites, start_keymap, tail);
  gcpro1.nvars = 0;

  start_keymap = get_keymap (start_keymap, 1, 1);
  keymap = start_keymap;
  /* Hack special-case parents at top-level */
  tail = ((!NILP (tail)) ? tail : XKEYMAP (keymap)->parents);

  for (;;)
    {
      Lisp_Object result;

      QUIT;
      result = ((mapper) (keymap, mapper_arg));
      if (!NILP (result))
	{
	  while (CONSP (malloc_bites))
	    {
	      struct Lisp_Cons *victim = XCONS (malloc_bites);
	      malloc_bites = victim->cdr;
	      free_cons (victim);
	    }
	  UNGCPRO;
	  return result;
	}
      if (NILP (tail))
	{
	  if (stack_depth == 0)
	    {
	      UNGCPRO;
	      return Qnil;          /* Nothing found */
	    }
	  stack_depth--;
	  if (CONSP (malloc_bites))
	    {
	      struct Lisp_Cons *victim = XCONS (malloc_bites);
	      tail = victim->car;
	      malloc_bites = victim->cdr;
	      free_cons (victim);
	    }
	  else
	    {
	      tail = malloc_sucks[stack_depth];
	      gcpro1.nvars = stack_depth;
	    }
	  keymap = XCAR (tail);
	  tail = XCDR (tail);
	}
      else
	{
	  Lisp_Object parents;

	  keymap = XCAR (tail);
	  tail = XCDR (tail);
	  parents = XKEYMAP (keymap)->parents;
	  if (!CONSP (parents))
	    ;
	  else if (NILP (tail))
	    /* Tail-recurse */
	    tail = parents;
	  else
	    {
	      if (CONSP (malloc_bites))
		malloc_bites = noseeum_cons (tail, malloc_bites);
	      else if (stack_depth < countof (malloc_sucks))
		{
		  malloc_sucks[stack_depth++] = tail;
		  gcpro1.nvars = stack_depth;
		}
	      else
		{
		  /* *&@##[*&^$ C. @#[$*&@# Unix.  Losers all. */
		  int i;
		  for (i = 0, malloc_bites = Qnil;
		       i < countof (malloc_sucks);
		       i++)
		    malloc_bites = noseeum_cons (malloc_sucks[i],
						 malloc_bites);
		  gcpro1.nvars = 0;
		}
	      tail = parents;
	    }
	}
      keymap = get_keymap (keymap, 1, 1);
      if (EQ (keymap, start_keymap))
	{
	  signal_simple_error ("Cyclic keymap indirection",
			       start_keymap);
	}
    }
}


/************************************************************************/
/*                     Some low-level functions                         */
/************************************************************************/

static unsigned int
bucky_sym_to_bucky_bit (Lisp_Object sym)
{
  if (EQ (sym, Qcontrol)) return MOD_CONTROL;
  if (EQ (sym, Qmeta))    return MOD_META;
  if (EQ (sym, Qsuper))   return MOD_SUPER;
  if (EQ (sym, Qhyper))   return MOD_HYPER;
  if (EQ (sym, Qalt))     return MOD_ALT;
  if (EQ (sym, Qsymbol))  return MOD_ALT; /* #### - reverse compat */
  if (EQ (sym, Qshift))   return MOD_SHIFT;

  return 0;
}

static Lisp_Object
control_meta_superify (Lisp_Object frob, unsigned int modifiers)
{
  if (modifiers == 0)
    return frob;
  frob = Fcons (frob, Qnil);
  if (modifiers & MOD_SHIFT)   frob = Fcons (Qshift,   frob);
  if (modifiers & MOD_ALT)     frob = Fcons (Qalt,     frob);
  if (modifiers & MOD_HYPER)   frob = Fcons (Qhyper,   frob);
  if (modifiers & MOD_SUPER)   frob = Fcons (Qsuper,   frob);
  if (modifiers & MOD_CONTROL) frob = Fcons (Qcontrol, frob);
  if (modifiers & MOD_META)    frob = Fcons (Qmeta,    frob);
  return frob;
}

static Lisp_Object
make_key_description (CONST struct key_data *key, int prettify)
{
  Lisp_Object keysym = key->keysym;
  unsigned int modifiers = key->modifiers;

  if (prettify && CHARP (keysym))
    {
      /* This is a little slow, but (control a) is prettier than (control 65).
	 It's now ok to do this for digit-chars too, since we've fixed the
	 bug where \9 read as the integer 9 instead of as the symbol with
	 "9" as its name.
       */
      /* !!#### I'm not sure how correct this is. */
      Bufbyte str [1 + MAX_EMCHAR_LEN];
      Bytecount count = set_charptr_emchar (str, XCHAR (keysym));
      str[count] = 0;
      keysym = intern ((char *) str);
    }
  return control_meta_superify (keysym, modifiers);
}


/************************************************************************/
/*                   Low-level keymap-store functions                   */
/************************************************************************/

static Lisp_Object
raw_lookup_key (Lisp_Object keymap,
                CONST struct key_data *raw_keys, int raw_keys_count,
                int keys_so_far, int accept_default);

/* Relies on caller to gc-protect args */
static Lisp_Object
keymap_lookup_directly (Lisp_Object keymap,
                        Lisp_Object keysym, unsigned int modifiers)
{
  struct keymap *k;

  if ((modifiers & ~(MOD_CONTROL | MOD_META | MOD_SUPER | MOD_HYPER
                     | MOD_ALT | MOD_SHIFT)) != 0)
    abort ();

  k = XKEYMAP (keymap);

  /* If the keysym is a one-character symbol, use the char code instead. */
  if (SYMBOLP (keysym) && string_char_length (XSYMBOL (keysym)->name) == 1)
    {
      Lisp_Object i_fart_on_gcc =
	make_char (string_char (XSYMBOL (keysym)->name, 0));
      keysym = i_fart_on_gcc;
    }

  if (modifiers & MOD_META)     /* Utterly hateful ESC lossage */
    {
      Lisp_Object submap = Fgethash (MAKE_MODIFIER_HASH_KEY (MOD_META),
                                     k->table, Qnil);
      if (NILP (submap))
        return Qnil;
      k = XKEYMAP (submap);
      modifiers &= ~MOD_META;
    }

  if (modifiers != 0)
    {
      Lisp_Object submap = Fgethash (MAKE_MODIFIER_HASH_KEY (modifiers),
                                     k->table, Qnil);
      if (NILP (submap))
        return Qnil;
      k = XKEYMAP (submap);
    }
  return Fgethash (keysym, k->table, Qnil);
}

static void
keymap_store_inverse_internal (Lisp_Object inverse_table,
                               Lisp_Object keysym,
                               Lisp_Object value)
{
  Lisp_Object keys = Fgethash (value, inverse_table, Qunbound);

  if (UNBOUNDP (keys))
    {
      keys = keysym;
      /* Don't cons this unless necessary */
      /* keys = Fcons (keysym, Qnil); */
      Fputhash (value, keys, inverse_table);
    }
  else if (!CONSP (keys))
    {
      /* Now it's necessary to cons */
      keys = Fcons (keys, keysym);
      Fputhash (value, keys, inverse_table);
    }
  else
    {
      while (CONSP (Fcdr (keys)))
	keys = XCDR (keys);
      XCDR (keys) = Fcons (XCDR (keys), keysym);
      /* No need to call puthash because we've destructively
         modified the list tail in place */
    }
}


static void
keymap_delete_inverse_internal (Lisp_Object inverse_table,
                                Lisp_Object keysym,
                                Lisp_Object value)
{
  Lisp_Object keys = Fgethash (value, inverse_table, Qunbound);
  Lisp_Object new_keys = keys;
  Lisp_Object tail;
  Lisp_Object *prev;

  if (UNBOUNDP (keys))
    abort ();

  for (prev = &new_keys, tail = new_keys;
       ;
       prev = &(XCDR (tail)), tail = XCDR (tail))
    {
      if (EQ (tail, keysym))
	{
	  *prev = Qnil;
	  break;
	}
      else if (EQ (keysym, XCAR (tail)))
	{
	  *prev = XCDR (tail);
	  break;
	}
    }

  if (NILP (new_keys))
    Fremhash (value, inverse_table);
  else if (!EQ (keys, new_keys))
    /* Removed the first elt */
    Fputhash (value, new_keys, inverse_table);
  /* else the list's tail has been modified, so we don't need to
     touch the hash table again (the pointer in there is ok).
   */
}


static void
keymap_store_internal (Lisp_Object keysym, struct keymap *keymap,
		       Lisp_Object value)
{
  Lisp_Object prev_value = Fgethash (keysym, keymap->table, Qnil);

  if (EQ (prev_value, value))
      return;
  if (!NILP (prev_value))
    keymap_delete_inverse_internal (keymap->inverse_table,
                                    keysym, prev_value);
  if (NILP (value))
    {
      keymap->fullness--;
      if (keymap->fullness < 0) abort ();
      Fremhash (keysym, keymap->table);
    }
  else
    {
      if (NILP (prev_value))
	keymap->fullness++;
      Fputhash (keysym, value, keymap->table);
      keymap_store_inverse_internal (keymap->inverse_table,
                                     keysym, value);
    }
  keymap_tick++;
}


static Lisp_Object
create_bucky_submap (struct keymap *k, unsigned int modifiers,
                     Lisp_Object parent_for_debugging_info)
{
  Lisp_Object submap = Fmake_sparse_keymap (Qnil);
  /* User won't see this, but it is nice for debugging Emacs */
  XKEYMAP (submap)->name
    = control_meta_superify (parent_for_debugging_info, modifiers);
  /* Invalidate cache */
  k->sub_maps_cache = Qt;
  keymap_store_internal (MAKE_MODIFIER_HASH_KEY (modifiers), k, submap);
  return submap;
}


/* Relies on caller to gc-protect keymap, keysym, value */
static void
keymap_store (Lisp_Object keymap, CONST struct key_data *key,
              Lisp_Object value)
{
  Lisp_Object keysym = key->keysym;
  unsigned int modifiers = key->modifiers;
  struct keymap *k;

  if ((modifiers & ~(MOD_CONTROL | MOD_META | MOD_SUPER | MOD_HYPER
                     | MOD_ALT | MOD_SHIFT)) != 0)
    abort ();

  k = XKEYMAP (keymap);

  /* If the keysym is a one-character symbol, use the char code instead. */
  if (SYMBOLP (keysym) && string_char_length (XSYMBOL (keysym)->name) == 1)
    {
      Lisp_Object run_the_gcc_developers_over_with_a_steamroller =
	make_char (string_char (XSYMBOL (keysym)->name, 0));
      keysym = run_the_gcc_developers_over_with_a_steamroller;
    }

  if (modifiers & MOD_META)     /* Utterly hateful ESC lossage */
    {
      Lisp_Object submap = Fgethash (MAKE_MODIFIER_HASH_KEY (MOD_META),
				     k->table, Qnil);
      if (NILP (submap))
	submap = create_bucky_submap (k, MOD_META, keymap);
      k = XKEYMAP (submap);
      modifiers &= ~MOD_META;
    }

  if (modifiers != 0)
    {
      Lisp_Object submap = Fgethash (MAKE_MODIFIER_HASH_KEY (modifiers),
				     k->table, Qnil);
      if (NILP (submap))
	submap = create_bucky_submap (k, modifiers, keymap);
      k = XKEYMAP (submap);
    }
  k->sub_maps_cache = Qt; /* Invalidate cache */
  keymap_store_internal (keysym, k, value);
}


/************************************************************************/
/*                   Listing the submaps of a keymap                    */
/************************************************************************/

struct keymap_submaps_closure
{
  Lisp_Object *result_locative;
};

static void
keymap_submaps_mapper_0 (CONST void *hash_key, void *hash_contents,
                         void *keymap_submaps_closure)
{
  /* This function can GC */
  Lisp_Object contents;
  VOID_TO_LISP (contents, hash_contents);
  /* Perform any autoloads, etc */
  Fkeymapp (contents);
}

static void
keymap_submaps_mapper (CONST void *hash_key, void *hash_contents,
                       void *keymap_submaps_closure)
{
  /* This function can GC */
  Lisp_Object key, contents;
  Lisp_Object *result_locative;
  struct keymap_submaps_closure *cl =
    (struct keymap_submaps_closure *) keymap_submaps_closure;
  CVOID_TO_LISP (key, hash_key);
  VOID_TO_LISP (contents, hash_contents);
  result_locative = cl->result_locative;

  if (!NILP (Fkeymapp (contents)))
    *result_locative = Fcons (Fcons (key, contents), *result_locative);
}

static int map_keymap_sort_predicate (Lisp_Object obj1, Lisp_Object obj2,
                                      Lisp_Object pred);

static Lisp_Object
keymap_submaps (Lisp_Object keymap)
{
  /* This function can GC */
  struct keymap *k = XKEYMAP (keymap);

  if (EQ (k->sub_maps_cache, Qt)) /* Unknown */
    {
      Lisp_Object result = Qnil;
      struct gcpro gcpro1, gcpro2;
      struct keymap_submaps_closure keymap_submaps_closure;

      GCPRO2 (keymap, result);
      keymap_submaps_closure.result_locative = &result;
      /* Do this first pass to touch (and load) any autoloaded maps */
      elisp_maphash (keymap_submaps_mapper_0, k->table,
		     &keymap_submaps_closure);
      result = Qnil;
      elisp_maphash (keymap_submaps_mapper, k->table,
		     &keymap_submaps_closure);
      /* keep it sorted so that the result of accessible-keymaps is ordered */
      k->sub_maps_cache = list_sort (result,
				     Qnil,
				     map_keymap_sort_predicate);
      UNGCPRO;
    }
  return k->sub_maps_cache;
}


/************************************************************************/
/*                    Basic operations on keymaps                       */
/************************************************************************/

static Lisp_Object
make_keymap (int size)
{
  Lisp_Object result = Qnil;
  struct keymap *keymap = alloc_lcrecord_type (struct keymap, lrecord_keymap);

  XSETKEYMAP (result, keymap);

  keymap->parents = Qnil;
  keymap->table = Qnil;
  keymap->prompt = Qnil;
  keymap->default_binding = Qnil;
  keymap->inverse_table = Qnil;
  keymap->sub_maps_cache = Qnil; /* No possible submaps */
  keymap->fullness = 0;
  if (size != 0) /* hack for copy-keymap */
    {
      keymap->table = Fmake_hashtable (make_int (size), Qnil);
      /* Inverse table is often less dense because of duplicate key-bindings.
         If not, it will grow anyway. */
      keymap->inverse_table = Fmake_hashtable (make_int (size * 3 / 4), Qnil);
    }
  keymap->name = Qnil;
  return result;
}

DEFUN ("make-keymap", Fmake_keymap, 0, 1, 0, /*
Construct and return a new keymap object.
All entries in it are nil, meaning "command undefined".

Optional argument NAME specifies a name to assign to the keymap,
as in `set-keymap-name'.  This name is only a debugging convenience;
it is not used except when printing the keymap.
*/
       (name))
{
  Lisp_Object keymap = make_keymap (60);
  if (!NILP (name))
    Fset_keymap_name (keymap, name);
  return keymap;
}

DEFUN ("make-sparse-keymap", Fmake_sparse_keymap, 0, 1, 0, /*
Construct and return a new keymap object.
All entries in it are nil, meaning "command undefined".  The only
difference between this function and make-keymap is that this function
returns a "smaller" keymap (one that is expected to contain fewer
entries).  As keymaps dynamically resize, the distinction is not great.

Optional argument NAME specifies a name to assign to the keymap,
as in `set-keymap-name'.  This name is only a debugging convenience;
it is not used except when printing the keymap.
*/
       (name))
{
  Lisp_Object keymap = make_keymap (8);
  if (!NILP (name))
    Fset_keymap_name (keymap, name);
  return keymap;
}

DEFUN ("keymap-parents", Fkeymap_parents, 1, 1, 0, /*
Return the `parent' keymaps of the given keymap, or nil.
The parents of a keymap are searched for keybindings when a key sequence
isn't bound in this one.  `(current-global-map)' is the default parent
of all keymaps.
*/
       (keymap))
{
  keymap = get_keymap (keymap, 1, 1);
  return Fcopy_sequence (XKEYMAP (keymap)->parents);
}



static Lisp_Object
traverse_keymaps_noop (Lisp_Object keymap, void *arg)
{
  return Qnil;
}

DEFUN ("set-keymap-parents", Fset_keymap_parents, 2, 2, 0, /*
Sets the `parent' keymaps of the given keymap.
The parents of a keymap are searched for keybindings when a key sequence
isn't bound in this one.  `(current-global-map)' is the default parent
of all keymaps.
*/
       (keymap, parents))
{
  /* This function can GC */
  Lisp_Object k;
  struct gcpro gcpro1, gcpro2;

  GCPRO2 (keymap, parents);
  keymap = get_keymap (keymap, 1, 1);

  if (KEYMAPP (parents))	/* backwards-compatibility */
    parents = list1 (parents);
  if (!NILP (parents))
    {
      Lisp_Object tail = parents;
      while (!NILP (tail))
	{
	  QUIT;
	  CHECK_CONS (tail);
	  k = XCAR (tail);
	  /* Require that it be an actual keymap object, rather than a symbol
	     with a (crockish) symbol-function which is a keymap */
	  CHECK_KEYMAP (k); /* get_keymap (k, 1, 1); */
	  tail = XCDR (tail);
	}
    }

  /* Check for circularities */
  traverse_keymaps (keymap, parents, traverse_keymaps_noop, 0);
  keymap_tick++;
  XKEYMAP (keymap)->parents = Fcopy_sequence (parents);
  UNGCPRO;
  return parents;
}

DEFUN ("set-keymap-name", Fset_keymap_name, 2, 2, 0, /*
Set the `name' of the KEYMAP to NEW-NAME.
The name is only a debugging convenience; it is not used except
when printing the keymap.
*/
       (keymap, new_name))
{
  keymap = get_keymap (keymap, 1, 1);

  XKEYMAP (keymap)->name = new_name;
  return new_name;
}

DEFUN ("keymap-name", Fkeymap_name, 1, 1, 0, /*
Return the `name' of KEYMAP.
The name is only a debugging convenience; it is not used except
when printing the keymap.
*/
       (keymap))
{
  keymap = get_keymap (keymap, 1, 1);

  return XKEYMAP (keymap)->name;
}

DEFUN ("set-keymap-prompt", Fset_keymap_prompt, 2, 2, 0, /*
Sets the `prompt' of KEYMAP to string NEW-PROMPT, or `nil'
if no prompt is desired.  The prompt is shown in the echo-area
when reading a key-sequence to be looked-up in this keymap.
*/
       (keymap, new_prompt))
{
  keymap = get_keymap (keymap, 1, 1);

  if (!NILP (new_prompt))
    CHECK_STRING (new_prompt);

  XKEYMAP (keymap)->prompt = new_prompt;
  return new_prompt;
}

static Lisp_Object
keymap_prompt_mapper (Lisp_Object keymap, void *arg)
{
  return XKEYMAP (keymap)->prompt;
}


DEFUN ("keymap-prompt", Fkeymap_prompt, 1, 2, 0, /*
Return the `prompt' of the given keymap.
If non-nil, the prompt is shown in the echo-area
when reading a key-sequence to be looked-up in this keymap.
*/
       (keymap, use_inherited))
{
  /* This function can GC */
  Lisp_Object prompt;

  keymap = get_keymap (keymap, 1, 1);
  prompt = XKEYMAP (keymap)->prompt;
  if (!NILP (prompt) || NILP (use_inherited))
    return prompt;
  else
    return traverse_keymaps (keymap, Qnil, keymap_prompt_mapper, 0);
}

DEFUN ("set-keymap-default-binding", Fset_keymap_default_binding, 2, 2, 0, /*
Sets the default binding of KEYMAP to COMMAND, or `nil'
if no default is desired.  The default-binding is returned when
no other binding for a key-sequence is found in the keymap.
If a keymap has a non-nil default-binding, neither the keymap's
parents nor the current global map are searched for key bindings.
*/
       (keymap, command))
{
  /* This function can GC */
  keymap = get_keymap (keymap, 1, 1);

  XKEYMAP (keymap)->default_binding = command;
  return command;
}

DEFUN ("keymap-default-binding", Fkeymap_default_binding, 1, 1, 0, /*
Return the default binding of KEYMAP, or `nil' if it has none.
The default-binding is returned when no other binding for a key-sequence
is found in the keymap.
If a keymap has a non-nil default-binding, neither the keymap's
parents nor the current global map are searched for key bindings.
*/
       (keymap))
{
  /* This function can GC */
  keymap = get_keymap (keymap, 1, 1);
  return XKEYMAP (keymap)->default_binding;
}

DEFUN ("keymapp", Fkeymapp, 1, 1, 0, /*
Return t if ARG is a keymap object.
The keymap may be autoloaded first if necessary.
*/
       (object))
{
  /* This function can GC */
  return KEYMAPP (get_keymap (object, 0, 0)) ? Qt : Qnil;
}

/* Check that OBJECT is a keymap (after dereferencing through any
   symbols).  If it is, return it.

   If AUTOLOAD is non-zero and OBJECT is a symbol whose function value
   is an autoload form, do the autoload and try again.
   If AUTOLOAD is nonzero, callers must assume GC is possible.

   ERRORP controls how we respond if OBJECT isn't a keymap.
   If ERRORP is non-zero, signal an error; otherwise, just return Qnil.

   Note that most of the time, we don't want to pursue autoloads.
   Functions like Faccessible_keymaps which scan entire keymap trees
   shouldn't load every autoloaded keymap.  I'm not sure about this,
   but it seems to me that only read_key_sequence, Flookup_key, and
   Fdefine_key should cause keymaps to be autoloaded.  */

Lisp_Object
get_keymap (Lisp_Object object, int errorp, int autoload)
{
  /* This function can GC */
  while (1)
    {
      Lisp_Object tem = indirect_function (object, 0);

      if (KEYMAPP (tem))
	return tem;
      /* Should we do an autoload?  */
      else if (autoload
               /* (autoload "filename" doc nil keymap) */
               && SYMBOLP (object)
               && CONSP (tem)
               && EQ (XCAR (tem), Qautoload)
               && EQ (Fcar (Fcdr (Fcdr (Fcdr (Fcdr (tem))))), Qkeymap))
	{
	  struct gcpro gcpro1, gcpro2;
	  GCPRO2 (tem, object);
	  do_autoload (tem, object);
	  UNGCPRO;
	}
      else if (errorp)
	object = wrong_type_argument (Qkeymapp, object);
      else
	return Qnil;
    }
}

/* Given OBJECT which was found in a slot in a keymap,
   trace indirect definitions to get the actual definition of that slot.
   An indirect definition is a list of the form
   (KEYMAP . INDEX), where KEYMAP is a keymap or a symbol defined as one
   and INDEX is an ASCII code, or a cons of (KEYSYM . MODIFIERS).
 */
static Lisp_Object
get_keyelt (Lisp_Object object, int accept_default)
{
  /* This function can GC */
  Lisp_Object map;

 tail_recurse:
  if (!CONSP (object))
    return object;

  {
    struct gcpro gcpro1;
    GCPRO1 (object);
    map = XCAR (object);
    map = get_keymap (map, 0, 1);
    UNGCPRO;
  }
  /* If the contents are (KEYMAP . ELEMENT), go indirect.  */
  if (!NILP (map))
    {
      Lisp_Object idx = Fcdr (object);
      struct key_data indirection;
      if (CHARP (idx))
	{
	  struct Lisp_Event event;
	  event.event_type = empty_event;
	  character_to_event (XCHAR (idx), &event,
			      XCONSOLE (Vselected_console), 0);
	  indirection = event.event.key;
	}
      else if (CONSP (idx))
	{
	  if (!INTP (XCDR (idx)))
	    return Qnil;
	  indirection.keysym = XCAR (idx);
	  indirection.modifiers = XINT (XCDR (idx));
	}
      else if (SYMBOLP (idx))
	{
	  indirection.keysym = idx;
	  indirection.modifiers = 0;
	}
      else
	{
	  /* Random junk */
	  return Qnil;
	}
      return raw_lookup_key (map, &indirection, 1, 0, accept_default);
    }
  else if (STRINGP (XCAR (object)))
    {
      /* If the keymap contents looks like (STRING . DEFN),
	 use DEFN.
	 Keymap alist elements like (CHAR MENUSTRING . DEFN)
	 will be used by HierarKey menus.  */
      object = XCDR (object);
      goto tail_recurse;
    }
  else
    {
      /* Anything else is really the value.  */
      return object;
    }
}

static Lisp_Object
keymap_lookup_1 (Lisp_Object keymap, CONST struct key_data *key,
                 int accept_default)
{
  /* This function can GC */
  return get_keyelt (keymap_lookup_directly (keymap,
					     key->keysym, key->modifiers),
		     accept_default);
}


/************************************************************************/
/*                          Copying keymaps                             */
/************************************************************************/

struct copy_keymap_inverse_closure
{
  Lisp_Object inverse_table;
};

static void
copy_keymap_inverse_mapper (CONST void *hash_key, void *hash_contents,
                            void *copy_keymap_inverse_closure)
{
  Lisp_Object key, inverse_table, inverse_contents;
  struct copy_keymap_inverse_closure *closure =
    (struct copy_keymap_inverse_closure *) copy_keymap_inverse_closure;

  VOID_TO_LISP (inverse_table, closure);
  VOID_TO_LISP (inverse_contents, hash_contents);
  CVOID_TO_LISP (key, hash_key);
  /* copy-sequence deals with dotted lists. */
  if (CONSP (inverse_contents))
    inverse_contents = Fcopy_sequence (inverse_contents);
  Fputhash (key, inverse_contents, closure->inverse_table);
}


static Lisp_Object
copy_keymap_internal (struct keymap *keymap)
{
  Lisp_Object nkm = make_keymap (0);
  struct keymap *new_keymap = XKEYMAP (nkm);
  struct copy_keymap_inverse_closure copy_keymap_inverse_closure;
  copy_keymap_inverse_closure.inverse_table = keymap->inverse_table;

  new_keymap->parents = Fcopy_sequence (keymap->parents);
  new_keymap->fullness = keymap->fullness;
  new_keymap->sub_maps_cache = Qnil; /* No submaps */
  new_keymap->table = Fcopy_hashtable (keymap->table);
  new_keymap->inverse_table = Fcopy_hashtable (keymap->inverse_table);
  /* After copying the inverse map, we need to copy the conses which
     are its values, lest they be shared by the copy, and mangled.
   */
  elisp_maphash (copy_keymap_inverse_mapper, keymap->inverse_table,
		 &copy_keymap_inverse_closure);
  return nkm;
}


static Lisp_Object copy_keymap (Lisp_Object keymap);

struct copy_keymap_closure
{
  struct keymap *self;
};

static void
copy_keymap_mapper (CONST void *hash_key, void *hash_contents,
                    void *copy_keymap_closure)
{
  /* This function can GC */
  Lisp_Object key, contents;
  struct copy_keymap_closure *closure =
    (struct copy_keymap_closure *) copy_keymap_closure;

  CVOID_TO_LISP (key, hash_key);
  VOID_TO_LISP (contents, hash_contents);
  /* When we encounter a keymap which is indirected through a
     symbol, we need to copy the sub-map.  In v18, the form
       (lookup-key (copy-keymap global-map) "\C-x")
     returned a new keymap, not the symbol 'Control-X-prefix.
   */
  contents = get_keymap (contents,
			 0, 1); /* #### autoload GC-safe here? */
  if (KEYMAPP (contents))
    keymap_store_internal (key, closure->self,
			   copy_keymap (contents));
}

static Lisp_Object
copy_keymap (Lisp_Object keymap)
{
  /* This function can GC */
  struct copy_keymap_closure copy_keymap_closure;

  keymap = copy_keymap_internal (XKEYMAP (keymap));
  copy_keymap_closure.self = XKEYMAP (keymap);
  elisp_maphash (copy_keymap_mapper,
		 XKEYMAP (keymap)->table,
		 &copy_keymap_closure);
  return keymap;
}

DEFUN ("copy-keymap", Fcopy_keymap, 1, 1, 0, /*
Return a copy of the keymap KEYMAP.
The copy starts out with the same definitions of KEYMAP,
but changing either the copy or KEYMAP does not affect the other.
Any key definitions that are subkeymaps are recursively copied.
*/
       (keymap))
{
  /* This function can GC */
  keymap = get_keymap (keymap, 1, 1);
  return copy_keymap (keymap);
}


static int
keymap_fullness (Lisp_Object keymap)
{
  /* This function can GC */
  int fullness;
  Lisp_Object sub_maps;
  struct gcpro gcpro1, gcpro2;

  keymap = get_keymap (keymap, 1, 1);
  fullness = XKEYMAP (keymap)->fullness;
  sub_maps = keymap_submaps (keymap);
  GCPRO2 (keymap, sub_maps);
  for (; !NILP (sub_maps); sub_maps = XCDR (sub_maps))
    {
      if (MODIFIER_HASH_KEY_BITS (XCAR (XCAR (sub_maps))) != 0)
	{
	  Lisp_Object sub_map = XCDR (XCAR (sub_maps));
	  fullness--; /* don't count bucky maps */
	  fullness += keymap_fullness (sub_map);
	}
    }
  UNGCPRO;
  return fullness;
}

DEFUN ("keymap-fullness", Fkeymap_fullness, 1, 1, 0, /*
Return the number of bindings in the keymap.
*/
       (keymap))
{
  /* This function can GC */
  return make_int (keymap_fullness (get_keymap (keymap, 1, 1)));
}


/************************************************************************/
/*                        Defining keys in keymaps                      */
/************************************************************************/

/* Given a keysym (should be a symbol, int, char), make sure it's valid
   and perform any necessary canonicalization. */

static void
define_key_check_and_coerce_keysym (Lisp_Object spec,
				    Lisp_Object *keysym,
				    unsigned int modifiers)
{
  /* Now, check and massage the trailing keysym specifier. */
  if (SYMBOLP (*keysym))
    {
      if (string_char_length (XSYMBOL (*keysym)->name) == 1)
	{
	  Lisp_Object ream_gcc_up_the_ass =
	    make_char (string_char (XSYMBOL (*keysym)->name, 0));
	  *keysym = ream_gcc_up_the_ass;
	  goto fixnum_keysym;
	}
    }
  else if (CHAR_OR_CHAR_INTP (*keysym))
    {
      CHECK_CHAR_COERCE_INT (*keysym);
    fixnum_keysym:
      if (XCHAR (*keysym) < ' '
	  /* || (XCHAR (*keysym) >= 128 && XCHAR (*keysym) < 160) */)
	/* yuck!  Can't make the above restriction; too many compatibility
	   problems ... */
	signal_simple_error ("keysym char must be printable", *keysym);
      /* #### This bites!  I want to be able to write (control shift a) */
      if (modifiers & MOD_SHIFT)
	signal_simple_error
	  ("the `shift' modifier may not be applied to ASCII keysyms",
	   spec);
    }
  else
    {
      signal_simple_error ("unknown keysym specifier",
			   *keysym);
    }

  if (SYMBOLP (*keysym))
    {
      char *name = (char *)
	string_data (XSYMBOL (*keysym)->name);

      /* FSFmacs uses symbols with the printed representation of keysyms in
	 their names, like 'M-x, and we use the syntax '(meta x).  So, to avoid
	 confusion, notice the M-x syntax and signal an error - because
	 otherwise it would be interpreted as a regular keysym, and would even
	 show up in the list-buffers output, causing confusion to the naive.

	 We can get away with this because none of the X keysym names contain
	 a hyphen (some contain underscore, however).

	 It might be useful to reject keysyms which are not x-valid-keysym-
	 name-p, but that would interfere with various tricks we do to
	 sanitize the Sun keyboards, and would make it trickier to
	 conditionalize a .emacs file for multiple X servers.
	 */
      if (((int) strlen (name) >= 2 && name[1] == '-')
#if 1
          ||
	  /* Ok, this is a bit more dubious - prevent people from doing things
	     like (global-set-key 'RET 'something) because that will have the
	     same problem as above.  (Gag!)  Maybe we should just silently
	     accept these as aliases for the "real" names?
	     */
	  (string_length (XSYMBOL (*keysym)->name) <= 3 &&
	   (!strcmp (name, "LFD") ||
	    !strcmp (name, "TAB") ||
	    !strcmp (name, "RET") ||
	    !strcmp (name, "ESC") ||
	    !strcmp (name, "DEL") ||
	    !strcmp (name, "SPC") ||
	    !strcmp (name, "BS")))
#endif /* unused */
          )
	signal_simple_error
          ("Invalid (FSF Emacs) key format (see doc of define-key)",
	   *keysym);

      /* #### Ok, this is a bit more dubious - make people not lose if they
	 do things like (global-set-key 'RET 'something) because that would
	 otherwise have the same problem as above.  (Gag!)  We silently
	 accept these as aliases for the "real" names.
	 */
      else if (!strncmp(name, "kp_", 3)) {
	/* Likewise, the obsolete keysym binding of kp_.* should not lose. */
	char temp[50];

	strncpy(temp, name, sizeof (temp));
	temp[sizeof (temp) - 1] = '\0';
	temp[2] = '-';
	*keysym = Fintern_soft(make_string((Bufbyte *)temp,
					   strlen(temp)),
			       Qnil);
      } else if (EQ (*keysym, QLFD))
	*keysym = QKlinefeed;
      else if (EQ (*keysym, QTAB))
	*keysym = QKtab;
      else if (EQ (*keysym, QRET))
	*keysym = QKreturn;
      else if (EQ (*keysym, QESC))
	*keysym = QKescape;
      else if (EQ (*keysym, QDEL))
	*keysym = QKdelete;
      else if (EQ (*keysym, QBS))
	*keysym = QKbackspace;
      /* Emacs compatibility */
      else if (EQ(*keysym, Qdown_mouse_1))
	*keysym = Qbutton1;
      else if (EQ(*keysym, Qdown_mouse_2))
	*keysym = Qbutton2;
      else if (EQ(*keysym, Qdown_mouse_3))
	*keysym = Qbutton3;
      else if (EQ(*keysym, Qmouse_1))
	*keysym = Qbutton1up;
      else if (EQ(*keysym, Qmouse_2))
	*keysym = Qbutton2up;
      else if (EQ(*keysym, Qmouse_3))
	*keysym = Qbutton3up;
    }
}


/* Given any kind of key-specifier, return a keysym and modifier mask.
   Proper canonicalization is performed:

   -- integers are converted into the equivalent characters.
   -- one-character strings are converted into the equivalent characters.
 */

static void
define_key_parser (Lisp_Object spec, struct key_data *returned_value)
{
  if (CHAR_OR_CHAR_INTP (spec))
    {
      struct Lisp_Event event;
      event.event_type = empty_event;
      character_to_event (XCHAR_OR_CHAR_INT (spec), &event,
			  XCONSOLE (Vselected_console), 0);
      returned_value->keysym    = event.event.key.keysym;
      returned_value->modifiers = event.event.key.modifiers;
    }
  else if (EVENTP (spec))
    {
      switch (XEVENT (spec)->event_type)
	{
	case key_press_event:
          {
            returned_value->keysym    = XEVENT (spec)->event.key.keysym;
            returned_value->modifiers = XEVENT (spec)->event.key.modifiers;
	    break;
          }
	case button_press_event:
	case button_release_event:
	  {
	    int down = (XEVENT (spec)->event_type == button_press_event);
	    switch (XEVENT (spec)->event.button.button)
	      {
	      case 1:
		returned_value->keysym = (down ? Qbutton1 : Qbutton1up); break;
	      case 2:
		returned_value->keysym = (down ? Qbutton2 : Qbutton2up); break;
	      case 3:
		returned_value->keysym = (down ? Qbutton3 : Qbutton3up); break;
	      case 4:
		returned_value->keysym = (down ? Qbutton4 : Qbutton4up); break;
	      case 5:
		returned_value->keysym = (down ? Qbutton5 : Qbutton5up); break;
	      case 6:
		returned_value->keysym = (down ? Qbutton6 : Qbutton6up); break;
	      case 7:
		returned_value->keysym = (down ? Qbutton7 : Qbutton7up); break;
	      default:
		returned_value->keysym = (down ? Qbutton0 : Qbutton0up); break;
	      }
	    returned_value->modifiers = XEVENT (spec)->event.button.modifiers;
	    break;
	  }
#ifdef HAVE_OFFIX_DND
	case dnd_drop_event:
	  {
	    switch (XEVENT (spec)->event.dnd_drop.button)
	      {
	      case 1:
		returned_value->keysym    = Qdrop1; break;
	      case 2:
		returned_value->keysym    = Qdrop2; break;
	      case 3:
		returned_value->keysym    = Qdrop3; break;
	      case 4:
		returned_value->keysym    = Qdrop4; break;
	      case 5:
		returned_value->keysym    = Qdrop5; break;
	      case 6:
		returned_value->keysym    = Qdrop6; break;
	      case 7:
		returned_value->keysym    = Qdrop7; break;
	      default:
		returned_value->keysym    = Qdrop0; break;
	      }
            returned_value->modifiers = XEVENT (spec)->event.dnd_drop.modifiers;
	    break;
	  }
#endif
	default:
	  signal_error (Qwrong_type_argument,
			list2 (build_translated_string
			       ("unable to bind this type of event"),
			       spec));
	}
    }
  else if (SYMBOLP (spec))
    {
      /* Be nice, allow = to mean (=) */
      if (bucky_sym_to_bucky_bit (spec) != 0)
        signal_simple_error ("Key is a modifier name", spec);
      define_key_check_and_coerce_keysym (spec, &spec, 0);
      returned_value->keysym = spec;
      returned_value->modifiers = 0;
    }
  else if (CONSP (spec))
    {
      unsigned int modifiers = 0;
      Lisp_Object keysym = Qnil;
      Lisp_Object rest = spec;

      /* First, parse out the leading modifier symbols. */
      while (CONSP (rest))
	{
	  unsigned int modifier;

	  keysym = XCAR (rest);
	  modifier = bucky_sym_to_bucky_bit (keysym);
	  modifiers |= modifier;
	  if (!NILP (XCDR (rest)))
	    {
	      if (! modifier)
		signal_simple_error ("unknown modifier", keysym);
	    }
	  else
	    {
	      if (modifier)
		signal_simple_error ("nothing but modifiers here",
				     spec);
	    }
	  rest = XCDR (rest);
	  QUIT;
	}
      if (!NILP (rest))
        signal_simple_error ("dotted list", spec);

      define_key_check_and_coerce_keysym (spec, &keysym, modifiers);
      returned_value->keysym = keysym;
      returned_value->modifiers = modifiers;
    }
  else
    {
      signal_simple_error ("unknown key-sequence specifier",
			   spec);
    }
}

/* Used by character-to-event */
void
key_desc_list_to_event (Lisp_Object list, Lisp_Object event,
                        int allow_menu_events)
{
  struct key_data raw_key;

  if (allow_menu_events &&
      CONSP (list) &&
      /* #### where the hell does this come from? */
      EQ (XCAR (list), Qmenu_selection))
    {
      Lisp_Object fn, arg;
      if (! NILP (Fcdr (Fcdr (list))))
	signal_simple_error ("invalid menu event desc", list);
      arg = Fcar (Fcdr (list));
      if (SYMBOLP (arg))
	fn = Qcall_interactively;
      else
	fn = Qeval;
      XSETFRAME (XEVENT (event)->channel, selected_frame ());
      XEVENT (event)->event_type = misc_user_event;
      XEVENT (event)->event.eval.function = fn;
      XEVENT (event)->event.eval.object = arg;
      return;
    }

  define_key_parser (list, &raw_key);

  if (EQ (raw_key.keysym, Qbutton0) || EQ (raw_key.keysym, Qbutton0up) ||
      EQ (raw_key.keysym, Qbutton1) || EQ (raw_key.keysym, Qbutton1up) ||
      EQ (raw_key.keysym, Qbutton2) || EQ (raw_key.keysym, Qbutton2up) ||
      EQ (raw_key.keysym, Qbutton3) || EQ (raw_key.keysym, Qbutton3up) ||
      EQ (raw_key.keysym, Qbutton4) || EQ (raw_key.keysym, Qbutton4up) ||
      EQ (raw_key.keysym, Qbutton5) || EQ (raw_key.keysym, Qbutton5up) ||
      EQ (raw_key.keysym, Qbutton6) || EQ (raw_key.keysym, Qbutton6up) ||
      EQ (raw_key.keysym, Qbutton7) || EQ (raw_key.keysym, Qbutton7up)
#ifdef HAVE_OFFIX_DND
      || EQ (raw_key.keysym, Qdrop0)   || EQ (raw_key.keysym, Qdrop1)     ||
      EQ (raw_key.keysym, Qdrop2)   || EQ (raw_key.keysym, Qdrop3)     ||
      EQ (raw_key.keysym, Qdrop4)   || EQ (raw_key.keysym, Qdrop5)     ||
      EQ (raw_key.keysym, Qdrop6)   || EQ (raw_key.keysym, Qdrop7)
#endif
      )
    error ("Mouse-clicks can't appear in saved keyboard macros.");

  XEVENT (event)->channel = Vselected_console;
  XEVENT (event)->event_type = key_press_event;
  XEVENT (event)->event.key.keysym = raw_key.keysym;
  XEVENT (event)->event.key.modifiers = raw_key.modifiers;
}


int
event_matches_key_specifier_p (struct Lisp_Event *event,
			       Lisp_Object key_specifier)
{
  Lisp_Object event2;
  int retval;
  struct gcpro gcpro1;

  if (event->event_type != key_press_event || NILP (key_specifier) ||
      (INTP (key_specifier) && !CHAR_INTP (key_specifier)))
    return 0;

  /* if the specifier is an integer such as 27, then it should match
     both of the events 'escape' and 'control ['.  Calling
     Fcharacter_to_event() will only match 'escape'. */
  if (CHAR_OR_CHAR_INTP (key_specifier))
    return (XCHAR_OR_CHAR_INT (key_specifier)
	    == event_to_character (event, 0, 0, 0));

  /* Otherwise, we cannot call event_to_character() because we may
     be dealing with non-ASCII keystrokes.  In any case, if I ask
     for 'control [' then I should get exactly that, and not
     'escape'.

     However, we have to behave differently on TTY's, where 'control ['
     is silently converted into 'escape' by the keyboard driver.
     In this case, ASCII is the only thing we know about, so we have
     to compare the ASCII values. */

  GCPRO1 (event2);
  event2 = Fmake_event (Qnil, Qnil);
  Fcharacter_to_event (key_specifier, event2, Qnil, Qnil);
  if (XEVENT (event2)->event_type != key_press_event)
    retval = 0;
  else if (CONSOLE_TTY_P (XCONSOLE (EVENT_CHANNEL (event))))
    {
      int ch1, ch2;

      ch1 = event_to_character (event, 0, 0, 0);
      ch2 = event_to_character (XEVENT (event2), 0, 0, 0);
      retval = (ch1 >= 0 && ch2 >= 0 && ch1 == ch2);
    }
  else if (EQ (event->event.key.keysym, XEVENT (event2)->event.key.keysym) &&
	   event->event.key.modifiers == XEVENT (event2)->event.key.modifiers)
    retval = 1;
  else
    retval = 0;
  Fdeallocate_event (event2);
  UNGCPRO;
  return retval;
}

static int
meta_prefix_char_p (CONST struct key_data *key)
{
  struct Lisp_Event event;

  event.event_type = key_press_event;
  event.channel = Vselected_console;
  event.event.key.keysym = key->keysym;
  event.event.key.modifiers = key->modifiers;
  return event_matches_key_specifier_p (&event, Vmeta_prefix_char);
}

DEFUN ("event-matches-key-specifier-p", Fevent_matches_key_specifier_p, 2, 2, 0, /*
Return non-nil if EVENT matches KEY-SPECIFIER.
This can be useful, e.g., to determine if the user pressed `help-char' or
`quit-char'.
*/
       (event, key_specifier))
{
  CHECK_LIVE_EVENT (event);
  return (event_matches_key_specifier_p (XEVENT (event), key_specifier)
	  ? Qt : Qnil);
}

/* ASCII grunge.
   Given a keysym, return another keysym/modifier pair which could be
   considered the same key in an ASCII world.  Backspace returns ^H, for
   example.
 */
static void
define_key_alternate_name (struct key_data *key,
                           struct key_data *returned_value)
{
  Lisp_Object keysym = key->keysym;
  unsigned int modifiers = key->modifiers;
  unsigned int modifiers_sans_control = (modifiers & (~MOD_CONTROL));
  unsigned int modifiers_sans_meta = (modifiers & (~MOD_META));
  returned_value->keysym = Qnil; /* By default, no "alternate" key */
  returned_value->modifiers = 0;
#define MACROLET(k,m) do { returned_value->keysym = (k); \
			   returned_value->modifiers = (m); \
                           RETURN__; } while (0)
  if (modifiers_sans_meta == MOD_CONTROL)
    {
      if EQ (keysym, QKspace)
        MACROLET (make_char ('@'), modifiers);
      else if (!CHARP (keysym))
        return;
      else switch (XCHAR (keysym))
        {
        case '@':               /* c-@ => c-space */
          MACROLET (QKspace, modifiers);
        case 'h':               /* c-h => backspace */
          MACROLET (QKbackspace, modifiers_sans_control);
        case 'i':               /* c-i => tab */
          MACROLET (QKtab, modifiers_sans_control);
        case 'j':               /* c-j => linefeed */
          MACROLET (QKlinefeed, modifiers_sans_control);
        case 'm':               /* c-m => return */
          MACROLET (QKreturn, modifiers_sans_control);
        case '[':               /* c-[ => escape */
          MACROLET (QKescape, modifiers_sans_control);
        default:
          return;
	}
    }
  else if (modifiers_sans_meta != 0)
    return;
  else if (EQ (keysym, QKbackspace)) /* backspace => c-h */
    MACROLET (make_char ('h'), (modifiers | MOD_CONTROL));
  else if (EQ (keysym, QKtab))       /* tab => c-i */
    MACROLET (make_char ('i'), (modifiers | MOD_CONTROL));
  else if (EQ (keysym, QKlinefeed))  /* linefeed => c-j */
    MACROLET (make_char ('j'), (modifiers | MOD_CONTROL));
  else if (EQ (keysym, QKreturn))    /* return => c-m */
    MACROLET (make_char ('m'), (modifiers | MOD_CONTROL));
  else if (EQ (keysym, QKescape))    /* escape => c-[ */
    MACROLET (make_char ('['), (modifiers | MOD_CONTROL));
  else
    return;
#undef MACROLET
}


static void
ensure_meta_prefix_char_keymapp (Lisp_Object keys, int indx,
                                 Lisp_Object keymap)
{
  /* This function can GC */
  char buf [255];
  Lisp_Object new_keys;
  int i;
  Lisp_Object mpc_binding;
  struct key_data meta_key;

  if (NILP (Vmeta_prefix_char) ||
      (INTP (Vmeta_prefix_char) && !CHAR_INTP (Vmeta_prefix_char)))
    return;

  define_key_parser (Vmeta_prefix_char, &meta_key);
  mpc_binding = keymap_lookup_1 (keymap, &meta_key, 0);
  if (NILP (mpc_binding) || !NILP (Fkeymapp (mpc_binding)))
    return;

  if (indx == 0)
    new_keys = keys;
  else if (STRINGP (keys))
    new_keys = Fsubstring (keys, Qzero, make_int (indx));
  else if (VECTORP (keys))
    {
      new_keys = make_vector (indx, Qnil);
      for (i = 0; i < indx; i++)
	XVECTOR_DATA (new_keys) [i] = XVECTOR_DATA (keys) [i];
    }
  else
    abort ();
  if (EQ (keys, new_keys))
    sprintf (buf, GETTEXT ("can't bind %s: %s has a non-keymap binding"),
	     (char *) XSTRING_DATA (Fkey_description (keys)),
	     (char *) XSTRING_DATA (Fsingle_key_description
				    (Vmeta_prefix_char)));
  else
    sprintf (buf, GETTEXT ("can't bind %s: %s %s has a non-keymap binding"),
	     (char *) XSTRING_DATA (Fkey_description (keys)),
	     (char *) XSTRING_DATA (Fkey_description (new_keys)),
	     (char *) XSTRING_DATA (Fsingle_key_description
				    (Vmeta_prefix_char)));
  signal_simple_error (buf, mpc_binding);
}

DEFUN ("define-key", Fdefine_key, 3, 3, 0, /*
Define key sequence KEYS, in KEYMAP, as DEF.
KEYMAP is a keymap object.
KEYS is the sequence of keystrokes to bind, described below.
DEF is anything that can be a key's definition:
 nil (means key is undefined in this keymap);
 a command (a Lisp function suitable for interactive calling);
 a string or key sequence vector (treated as a keyboard macro);
 a keymap (to define a prefix key);
 a symbol; when the key is looked up, the symbol will stand for its
    function definition, that should at that time be one of the above,
    or another symbol whose function definition is used, and so on.
 a cons (STRING . DEFN), meaning that DEFN is the definition
    (DEFN should be a valid definition in its own right);
 or a cons (KEYMAP . CHAR), meaning use definition of CHAR in map KEYMAP.

Contrary to popular belief, the world is not ASCII.  When running under a
window manager, XEmacs can tell the difference between, for example, the
keystrokes control-h, control-shift-h, and backspace.  You can, in fact,
bind different commands to each of these.

A `key sequence' is a set of keystrokes.  A `keystroke' is a keysym and some
set of modifiers (such as control and meta).  A `keysym' is what is printed
on the keys on your keyboard.

A keysym may be represented by a symbol, or (if and only if it is equivalent
to an ASCII character in the range 32 - 255) by a character or its equivalent
ASCII code.  The `A' key may be represented by the symbol `A', the character
`?A', or by the number 65.  The `break' key may be represented only by the
symbol `break'.

A keystroke may be represented by a list: the last element of the list
is the key (a symbol, character, or number, as above) and the
preceding elements are the symbolic names of modifier keys (control,
meta, super, hyper, alt, and shift).  Thus, the sequence control-b is
represented by the forms `(control b)', `(control ?b)', and `(control
98)'.  A keystroke may also be represented by an event object, as
returned by the `next-command-event' and `read-key-sequence'
functions.

Note that in this context, the keystroke `control-b' is *not* represented
by the number 2 (the ASCII code for ^B) or the character `?\^B'.  See below.

The `shift' modifier is somewhat of a special case.  You should not (and
cannot) use `(meta shift a)' to mean `(meta A)', since for characters that
have ASCII equivalents, the state of the shift key is implicit in the
keysym (a vs. A).  You also cannot say `(shift =)' to mean `+', as that
sort of thing varies from keyboard to keyboard.  The shift modifier is for
use only with characters that do not have a second keysym on the same key,
such as `backspace' and `tab'.

A key sequence is a vector of keystrokes.  As a degenerate case, elements
of this vector may also be keysyms if they have no modifiers.  That is,
the `A' keystroke is represented by all of these forms:
	A	?A	65	(A)	(?A)	(65)
	[A]	[?A]	[65]	[(A)]	[(?A)]	[(65)]

the `control-a' keystroke is represented by these forms:
	(control A)	(control ?A)	(control 65)
	[(control A)]	[(control ?A)]	[(control 65)]
the key sequence `control-c control-a' is represented by these forms:
	[(control c) (control a)]	[(control ?c) (control ?a)]
	[(control 99) (control 65)]	etc.

Mouse button clicks work just like keypresses: (control button1) means
pressing the left mouse button while holding down the control key.
\[(control c) (shift button3)] means control-c, hold shift, click right.

Commands may be bound to the mouse-button up-stroke rather than the down-
stroke as well.  `button1' means the down-stroke, and `button1up' means the
up-stroke.  Different commands may be bound to the up and down strokes,
though that is probably not what you want, so be careful.

For backward compatibility, a key sequence may also be represented by a
string.  In this case, it represents the key sequence(s) that would
produce that sequence of ASCII characters in a purely ASCII world.  For
example, a string containing the ASCII backspace character, "\\^H", would
represent two key sequences: `(control h)' and `backspace'.  Binding a
command to this will actually bind both of those key sequences.  Likewise
for the following pairs:

		control h	backspace
		control i   	tab
		control m   	return
		control j   	linefeed
		control [   	escape
		control @	control space

After binding a command to two key sequences with a form like

	(define-key global-map "\\^X\\^I" \'command-1)

it is possible to redefine only one of those sequences like so:

	(define-key global-map [(control x) (control i)] \'command-2)
	(define-key global-map [(control x) tab] \'command-3)

Of course, all of this applies only when running under a window system.  If
you're talking to XEmacs through a TTY connection, you don't get any of
these features.
*/
       (keymap, keys, def))
{
  /* This function can GC */
  int idx;
  int metized = 0;
  int len;
  int ascii_hack;
  struct gcpro gcpro1, gcpro2, gcpro3;

  if (VECTORP (keys))
    len = XVECTOR_LENGTH (keys);
  else if (STRINGP (keys))
    len = string_char_length (XSTRING (keys));
  else if (CHAR_OR_CHAR_INTP (keys) || SYMBOLP (keys) || CONSP (keys))
    {
      if (!CONSP (keys)) keys = list1 (keys);
      len = 1;
      keys = make_vector (1, keys); /* this is kinda sleazy. */
    }
  else
    {
      keys = wrong_type_argument (Qsequencep, keys);
      len = XINT (Flength (keys));
    }
  if (len == 0)
    return Qnil;

  GCPRO3 (keymap, keys, def);

  /* ASCII grunge.
     When the user defines a key which, in a strictly ASCII world, would be
     produced by two different keys (^J and linefeed, or ^H and backspace,
     for example) then the binding will be made for both keysyms.

     This is done if the user binds a command to a string, as in
     (define-key map "\^H" 'something), but not when using one of the new
     syntaxes, like (define-key map '(control h) 'something).
     */
  ascii_hack = (STRINGP (keys));

  keymap = get_keymap (keymap, 1, 1);

  idx = 0;
  while (1)
    {
      Lisp_Object c;
      struct key_data raw_key1;
      struct key_data raw_key2;

      if (STRINGP (keys))
	c = make_char (string_char (XSTRING (keys), idx));
      else
	c = XVECTOR_DATA (keys) [idx];

      define_key_parser (c, &raw_key1);

      if (!metized && ascii_hack && meta_prefix_char_p (&raw_key1))
	{
	  if (idx == (len - 1))
	    {
	      /* This is a hack to prevent a binding for the meta-prefix-char
		 from being made in a map which already has a non-empty "meta"
		 submap.  That is, we can't let both "escape" and "meta" have
		 a binding in the same keymap.  This implies that the idiom
		 (define-key my-map "\e" my-escape-map)
		 (define-key my-escape-map "a" 'my-command)
		 no longer works.  That's ok.  Instead the luser should do
		 (define-key my-map "\ea" 'my-command)
		 or, more correctly
		 (define-key my-map "\M-a" 'my-command)
		 and then perhaps
		 (defvar my-escape-map (lookup-key my-map "\e"))
		 if the luser really wants the map in a variable.
		 */
	      Lisp_Object mmap;
              struct gcpro ngcpro1;

              NGCPRO1 (c);
              mmap = Fgethash (MAKE_MODIFIER_HASH_KEY (MOD_META),
                               XKEYMAP (keymap)->table, Qnil);
	      if (!NILP (mmap)
		  && keymap_fullness (mmap) != 0)
		{
                  Lisp_Object desc
                    = Fsingle_key_description (Vmeta_prefix_char);
		  signal_simple_error_2
		    ("Map contains meta-bindings, can't bind", desc, keymap);
		}
              NUNGCPRO;
	    }
	  else
	    {
	      metized = 1;
	      idx++;
	      continue;
	    }
	}

      if (ascii_hack)
	define_key_alternate_name (&raw_key1, &raw_key2);
      else
	{
	  raw_key2.keysym = Qnil;
	  raw_key2.modifiers = 0;
	}

      if (metized)
	{
	  raw_key1.modifiers  |= MOD_META;
	  raw_key2.modifiers |= MOD_META;
	  metized = 0;
	}

      /* This crap is to make sure that someone doesn't bind something like
	 "C-x M-a" while "C-x ESC" has a non-keymap binding. */
      if (raw_key1.modifiers & MOD_META)
	ensure_meta_prefix_char_keymapp (keys, idx, keymap);

      if (++idx == len)
	{
	  keymap_store (keymap, &raw_key1, def);
	  if (ascii_hack && !NILP (raw_key2.keysym))
	    keymap_store (keymap, &raw_key2, def);
	  UNGCPRO;
	  return def;
	}

      {
        Lisp_Object cmd;
        struct gcpro ngcpro1;
        NGCPRO1 (c);

        cmd = keymap_lookup_1 (keymap, &raw_key1, 0);
	if (NILP (cmd))
	  {
	    cmd = Fmake_sparse_keymap (Qnil);
	    XKEYMAP (cmd)->name /* for debugging */
	      = list2 (make_key_description (&raw_key1, 1), keymap);
	    keymap_store (keymap, &raw_key1, cmd);
	  }
	if (NILP (Fkeymapp (cmd)))
          signal_simple_error_2 ("invalid prefix keys in sequence",
				 c, keys);

	if (ascii_hack && !NILP (raw_key2.keysym) &&
	    NILP (keymap_lookup_1 (keymap, &raw_key2, 0)))
	  keymap_store (keymap, &raw_key2, cmd);

	keymap = get_keymap (cmd, 1, 1);
        NUNGCPRO;
      }
    }
}


/************************************************************************/
/*                      Looking up keys in keymaps                      */
/************************************************************************/

/* We need a very fast (i.e., non-consing) version of lookup-key in order
   to make where-is-internal really fly. */

struct raw_lookup_key_mapper_closure
{
  int remaining;
  CONST struct key_data *raw_keys;
  int raw_keys_count;
  int keys_so_far;
  int accept_default;
};

static Lisp_Object raw_lookup_key_mapper (Lisp_Object k, void *);

/* Caller should gc-protect args (keymaps may autoload) */
static Lisp_Object
raw_lookup_key (Lisp_Object keymap,
                CONST struct key_data *raw_keys, int raw_keys_count,
                int keys_so_far, int accept_default)
{
  /* This function can GC */
  struct raw_lookup_key_mapper_closure c;
  c.remaining = raw_keys_count - 1;
  c.raw_keys = raw_keys;
  c.raw_keys_count = raw_keys_count;
  c.keys_so_far = keys_so_far;
  c.accept_default = accept_default;

  return traverse_keymaps (keymap, Qnil, raw_lookup_key_mapper, &c);
}

static Lisp_Object
raw_lookup_key_mapper (Lisp_Object k, void *arg)
{
  /* This function can GC */
  struct raw_lookup_key_mapper_closure *c =
    (struct raw_lookup_key_mapper_closure *) arg;
  int accept_default = c->accept_default;
  int remaining = c->remaining;
  int keys_so_far = c->keys_so_far;
  CONST struct key_data *raw_keys = c->raw_keys;
  Lisp_Object cmd;

  if (! meta_prefix_char_p (&(raw_keys[0])))
    {
      /* Normal case: every case except the meta-hack (see below). */
      cmd = keymap_lookup_1 (k, &(raw_keys[0]), accept_default);

      if (remaining == 0)
	/* Return whatever we found if we're out of keys */
	;
      else if (NILP (cmd))
	/* Found nothing (though perhaps parent map may have binding) */
	;
      else if (NILP (Fkeymapp (cmd)))
	/* Didn't find a keymap, and we have more keys.
	 * Return a fixnum to indicate that keys were too long.
	 */
	cmd = make_int (keys_so_far + 1);
      else
	cmd = raw_lookup_key (cmd, raw_keys + 1, remaining,
			      keys_so_far + 1, accept_default);
    }
  else
    {
      /* This is a hack so that looking up a key-sequence whose last
       * element is the meta-prefix-char will return the keymap that
       * the "meta" keys are stored in, if there is no binding for
       * the meta-prefix-char (and if this map has a "meta" submap).
       * If this map doesnt have a "meta" submap, then the
       * meta-prefix-char is looked up just like any other key.
       */
      if (remaining == 0)
	{
	  /* First look for the prefix-char directly */
	  cmd = keymap_lookup_1 (k, &(raw_keys[0]), accept_default);
	  if (NILP (cmd))
	    {
	      /* Do kludgy return of the meta-map */
	      cmd = Fgethash (MAKE_MODIFIER_HASH_KEY (MOD_META),
			      XKEYMAP (k)->table, Qnil);
	    }
	}
      else
	{
	  /* Search for the prefix-char-prefixed sequence directly */
	  cmd = keymap_lookup_1 (k, &(raw_keys[0]), accept_default);
	  cmd = get_keymap (cmd, 0, 1);
	  if (!NILP (cmd))
	    cmd = raw_lookup_key (cmd, raw_keys + 1, remaining,
				  keys_so_far + 1, accept_default);
	  else if ((raw_keys[1].modifiers & MOD_META) == 0)
	    {
	      struct key_data metified;
	      metified.keysym = raw_keys[1].keysym;
	      metified.modifiers = raw_keys[1].modifiers | MOD_META;

	      /* Search for meta-next-char sequence directly */
	      cmd = keymap_lookup_1 (k, &metified, accept_default);
	      if (remaining == 1)
		;
	      else
		{
		  cmd = get_keymap (cmd, 0, 1);
		  if (!NILP (cmd))
		    cmd = raw_lookup_key (cmd, raw_keys + 2, remaining - 1,
					  keys_so_far + 2,
					  accept_default);
		}
	    }
	}
    }
  if (accept_default && NILP (cmd))
    cmd = XKEYMAP (k)->default_binding;
  return cmd;
}

/* Value is number if `keys' is too long; NIL if valid but has no definition.*/
/* Caller should gc-protect arguments */
static Lisp_Object
lookup_keys (Lisp_Object keymap, int nkeys, Lisp_Object *keys,
             int accept_default)
{
  /* This function can GC */
  struct key_data kkk[20];
  struct key_data *raw_keys;
  int i;

  if (nkeys == 0)
    return Qnil;

  if (nkeys < (countof (kkk)))
    raw_keys = kkk;
  else
    raw_keys = alloca_array (struct key_data, nkeys);

  for (i = 0; i < nkeys; i++)
    {
      define_key_parser (keys[i], &(raw_keys[i]));
    }
  return raw_lookup_key (keymap, raw_keys, nkeys, 0, accept_default);
}

static Lisp_Object
lookup_events (Lisp_Object event_head, int nmaps, Lisp_Object keymaps[],
               int accept_default)
{
  /* This function can GC */
  struct key_data kkk[20];
  Lisp_Object event;

  int nkeys;
  struct key_data *raw_keys;
  Lisp_Object tem = Qnil;
  struct gcpro gcpro1, gcpro2;
  int iii;

  CHECK_LIVE_EVENT (event_head);

  nkeys = event_chain_count (event_head);

  if (nkeys < (countof (kkk)))
    raw_keys = kkk;
  else
    raw_keys = alloca_array (struct key_data, nkeys);

  nkeys = 0;
  EVENT_CHAIN_LOOP (event, event_head)
    define_key_parser (event, &(raw_keys[nkeys++]));
  GCPRO2 (keymaps[0], event_head);
  gcpro1.nvars = nmaps;
  /* ####raw_keys[].keysym slots aren't gc-protected.  We rely (but shouldn't)
   * on somebody else somewhere (obarray) having a pointer to all keysyms. */
  for (iii = 0; iii < nmaps; iii++)
    {
      tem = raw_lookup_key (keymaps[iii], raw_keys, nkeys, 0,
			    accept_default);
      if (INTP (tem))
	{
	  /* Too long in some local map means don't look at global map */
	  tem = Qnil;
	  break;
	}
      else if (!NILP (tem))
	break;
    }
  UNGCPRO;
  return tem;
}

DEFUN ("lookup-key", Flookup_key, 2, 3, 0, /*
In keymap KEYMAP, look up key-sequence KEYS.  Return the definition.
Nil is returned if KEYS is unbound.  See documentation of `define-key'
for valid key definitions and key-sequence specifications.
A number is returned if KEYS is "too long"; that is, the leading
characters fail to be a valid sequence of prefix characters in KEYMAP.
The number is how many characters at the front of KEYS
it takes to reach a non-prefix command.
*/
       (keymap, keys, accept_default))
{
  /* This function can GC */
  if (VECTORP (keys))
    {
      return lookup_keys (keymap,
			  XVECTOR_LENGTH (keys),
                          XVECTOR_DATA (keys),
                          !NILP (accept_default));
    }
  else if (SYMBOLP (keys) || CHAR_OR_CHAR_INTP (keys) || CONSP (keys))
    {
      return lookup_keys (keymap, 1, &keys,
			  !NILP (accept_default));
    }
  else if (!STRINGP (keys))
    {
      keys = wrong_type_argument (Qsequencep, keys);
      return Flookup_key (keymap, keys, accept_default);
    }
  else /* STRINGP (keys) */
    {
      int length = string_char_length (XSTRING (keys));
      int i;
      struct key_data *raw_keys = alloca_array (struct key_data, length);
      if (length == 0)
	return Qnil;

      for (i = 0; i < length; i++)
	{
          Emchar n = string_char (XSTRING (keys), i);
	  define_key_parser (make_char (n), &(raw_keys[i]));
	}
      return raw_lookup_key (keymap, raw_keys, length, 0,
			     !NILP (accept_default));
    }
}

/* Given a key sequence, returns a list of keymaps to search for bindings.
   Does all manner of semi-hairy heuristics, like looking in the current
   buffer's map before looking in the global map and looking in the local
   map of the buffer in which the mouse was clicked in event0 is a click.

   It would be kind of nice if this were in Lisp so that this semi-hairy
   semi-heuristic command-lookup behaviour could be readily understood and
   customised.  However, this needs to be pretty fast, or performance of
   keyboard macros goes to shit; putting this in lisp slows macros down
   2-3x.  And they're already slower than v18 by 5-6x.
 */

struct relevant_maps
  {
    int nmaps;
    unsigned int max_maps;
    Lisp_Object *maps;
    struct gcpro *gcpro;
  };

static void get_relevant_extent_keymaps (Lisp_Object pos,
                                         Lisp_Object buffer_or_string,
                                         Lisp_Object glyph,
                                         struct relevant_maps *closure);
static void get_relevant_minor_maps (Lisp_Object buffer,
                                     struct relevant_maps *closure);

static void
relevant_map_push (Lisp_Object map, struct relevant_maps *closure)
{
  unsigned int nmaps = closure->nmaps;

  if (!KEYMAPP (map))
    return;
  closure->nmaps = nmaps + 1;
  if (nmaps < closure->max_maps)
    {
      closure->maps[nmaps] = map;
      closure->gcpro->nvars = nmaps;
    }
}

static int
get_relevant_keymaps (Lisp_Object keys,
                      int max_maps, Lisp_Object maps[])
{
  /* This function can GC */
  Lisp_Object terminal = Qnil;
  struct gcpro gcpro1;
  struct relevant_maps closure;
  struct console *con;

  GCPRO1 (*maps);
  gcpro1.nvars = 0;
  closure.nmaps = 0;
  closure.max_maps = max_maps;
  closure.maps = maps;
  closure.gcpro = &gcpro1;

  if (EVENTP (keys))
    terminal = event_chain_tail (keys);
  else if (VECTORP (keys))
    {
      int len = XVECTOR_LENGTH (keys);
      if (len > 0)
	terminal = XVECTOR_DATA (keys)[len - 1];
    }

  if (EVENTP (terminal))
    {
      CHECK_LIVE_EVENT (terminal);
      con = event_console_or_selected (terminal);
    }
  else
    con = XCONSOLE (Vselected_console);

  if (KEYMAPP (con->overriding_terminal_local_map)
      || KEYMAPP (Voverriding_local_map))
    {
      if (KEYMAPP (con->overriding_terminal_local_map))
	relevant_map_push (con->overriding_terminal_local_map, &closure);
      if (KEYMAPP (Voverriding_local_map))
	relevant_map_push (Voverriding_local_map, &closure);
    }
  else if (!EVENTP (terminal)
           || (XEVENT (terminal)->event_type != button_press_event
               && XEVENT (terminal)->event_type != button_release_event))
    {
      Lisp_Object tem;
      XSETBUFFER (tem, current_buffer);
      /* It's not a mouse event; order of keymaps searched is:
	 o  keymap of any/all extents under the mouse
	 o  minor-mode maps
	 o  local-map of current-buffer
	 o  global-map
	 */
      /* The terminal element of the lookup may be nil or a keysym.
         In those cases we don't want to check for an extent
         keymap. */
      if (EVENTP (terminal))
	{
	  get_relevant_extent_keymaps (make_int (BUF_PT (current_buffer)),
				       tem, Qnil, &closure);
	}
      get_relevant_minor_maps (tem, &closure);

      tem = current_buffer->keymap;
      if (!NILP (tem))
	relevant_map_push (tem, &closure);
    }
#ifdef HAVE_WINDOW_SYSTEM
  else
    {
      /* It's a mouse event; order of keymaps searched is:
	 o  local-map of mouse-grabbed-buffer
	 o  keymap of any/all extents under the mouse
	 if the mouse is over a modeline:
	 o  modeline-map of buffer corresponding to that modeline
	 o  else, local-map of buffer under the mouse
	 o  minor-mode maps
	 o  local-map of current-buffer
	 o  global-map
	 */
      Lisp_Object window = Fevent_window (terminal);

      if (BUFFERP (Vmouse_grabbed_buffer))
	{
	  Lisp_Object map = XBUFFER (Vmouse_grabbed_buffer)->keymap;

	  get_relevant_minor_maps (Vmouse_grabbed_buffer, &closure);
	  if (!NILP (map))
	    relevant_map_push (map, &closure);
	}

      if (!NILP (window))
	{
	  Lisp_Object buffer = Fwindow_buffer (window);

	  if (!NILP (buffer))
	    {
	      if (!NILP (Fevent_over_modeline_p (terminal)))
		{
		  Lisp_Object map = symbol_value_in_buffer (Qmodeline_map,
							    buffer);

		  get_relevant_extent_keymaps
		    (Fevent_modeline_position (terminal),
		     XBUFFER (buffer)->generated_modeline_string,
		     /* #### third arg should maybe be a glyph. */
		     Qnil, &closure);

		  if (!UNBOUNDP (map) && !NILP (map))
		    relevant_map_push (get_keymap (map, 1, 1), &closure);
		}
	      else
		{
		  get_relevant_extent_keymaps (Fevent_point (terminal), buffer,
					       Fevent_glyph_extent (terminal),
					       &closure);
		}

	      if (!EQ (buffer, Vmouse_grabbed_buffer)) /* already pushed */
		{
		  Lisp_Object map = XBUFFER (buffer)->keymap;

		  get_relevant_minor_maps (buffer, &closure);
		  if (!NILP(map))
		    relevant_map_push (map, &closure);
		}
	    }
	}
      else if (!NILP (Fevent_over_toolbar_p (terminal)))
	{
	  Lisp_Object map = Fsymbol_value (Qtoolbar_map);

	  if (!UNBOUNDP (map) && !NILP (map))
	    relevant_map_push (map, &closure);
	}
    }
#endif /* HAVE_WINDOW_SYSTEM */

  {
    int nmaps = closure.nmaps;
    /* Silently truncate at 100 keymaps to prevent infinite losssage */
    if (nmaps >= max_maps && max_maps > 0)
      maps[max_maps - 1] = Vcurrent_global_map;
    else
      maps[nmaps] = Vcurrent_global_map;
    UNGCPRO;
    return nmaps + 1;
  }
}

/* Returns a set of keymaps extracted from the extents at POS in
   BUFFER_OR_STRING.  The GLYPH arg, if specified, is one more extent
   to look for a keymap in, and if it has one, its keymap will be the
   first element in the list returned.  This is so we can correctly
   search the keymaps associated with glyphs which may be physically
   disjoint from their extents: for example, if a glyph is out in the
   margin, we should still consult the kemyap of that glyph's extent,
   which may not itself be under the mouse.
 */

static void
get_relevant_extent_keymaps (Lisp_Object pos, Lisp_Object buffer_or_string,
                             Lisp_Object glyph,
                             struct relevant_maps *closure)
{
  /* This function can GC */
  /* the glyph keymap, if any, comes first.
     (Processing it twice is no big deal: noop.) */
  if (!NILP (glyph))
    {
      Lisp_Object keymap = Fextent_property (glyph, Qkeymap, Qnil);
      if (!NILP (keymap))
	relevant_map_push (get_keymap (keymap, 1, 1), closure);
    }

  /* Next check the extents at the text position, if any */
  if (!NILP (pos))
    {
      Lisp_Object extent;
      for (extent = Fextent_at (pos, buffer_or_string, Qkeymap, Qnil, Qnil);
	   !NILP (extent);
	   extent = Fextent_at (pos, buffer_or_string, Qkeymap, extent, Qnil))
	{
	  Lisp_Object keymap = Fextent_property (extent, Qkeymap, Qnil);
	  if (!NILP (keymap))
	    relevant_map_push (get_keymap (keymap, 1, 1), closure);
	  QUIT;
	}
    }
}

static Lisp_Object
minor_mode_keymap_predicate (Lisp_Object assoc, Lisp_Object buffer)
{
  /* This function can GC */
  if (CONSP (assoc))
    {
      Lisp_Object sym = XCAR (assoc);
      if (SYMBOLP (sym))
	{
	  Lisp_Object val = symbol_value_in_buffer (sym, buffer);
	  if (!NILP (val) && !UNBOUNDP (val))
	    {
	      Lisp_Object map = get_keymap (XCDR (assoc), 0, 1);
	      return map;
	    }
	}
    }
  return Qnil;
}

static void
get_relevant_minor_maps (Lisp_Object buffer, struct relevant_maps *closure)
{
  /* This function can GC */
  Lisp_Object alist;

  /* Will you ever lose badly if you make this circular! */
  for (alist = symbol_value_in_buffer (Qminor_mode_map_alist, buffer);
       CONSP (alist);
       alist = XCDR (alist))
    {
      Lisp_Object m = minor_mode_keymap_predicate (XCAR (alist),
						   buffer);
      if (!NILP (m)) relevant_map_push (m, closure);
      QUIT;
    }
}

/* #### Would map-current-keymaps be a better thing?? */
DEFUN ("current-keymaps", Fcurrent_keymaps, 0, 1, 0, /*
Return a list of the current keymaps that will be searched for bindings.
This lists keymaps such as the current local map and the minor-mode maps,
 but does not list the parents of those keymaps.
EVENT-OR-KEYS controls which keymaps will be listed.
If EVENT-OR-KEYS is a mouse event (or a vector whose last element is a
 mouse event), the keymaps for that mouse event will be listed (see
 `key-binding').  Otherwise, the keymaps for key presses will be listed.
*/
       (event_or_keys))
{
  /* This function can GC */
  struct gcpro gcpro1;
  Lisp_Object maps[100];
  Lisp_Object *gubbish = maps;
  int nmaps;

  GCPRO1 (event_or_keys);
  nmaps = get_relevant_keymaps (event_or_keys, countof (maps),
				gubbish);
  if (nmaps > countof (maps))
    {
      gubbish = alloca_array (Lisp_Object, nmaps);
      nmaps = get_relevant_keymaps (event_or_keys, nmaps, gubbish);
    }
  UNGCPRO;
  return Flist (nmaps, gubbish);
}

DEFUN ("key-binding", Fkey_binding, 1, 2, 0, /*
Return the binding for command KEYS in current keymaps.
KEYS is a string, a vector of events, or a vector of key-description lists
as described in the documentation for the `define-key' function.
The binding is probably a symbol with a function definition; see
the documentation for `lookup-key' for more information.

For key-presses, the order of keymaps searched is:
  - the `keymap' property of any extent(s) at point;
  - any applicable minor-mode maps;
  - the current-local-map of the current-buffer;
  - the current global map.

For mouse-clicks, the order of keymaps searched is:
  - the current-local-map of the `mouse-grabbed-buffer' if any;
  - the `keymap' property of any extent(s) at the position of the click
    (this includes modeline extents);
  - the modeline-map of the buffer corresponding to the modeline under
    the mouse (if the click happened over a modeline);
  - the value of toolbar-map in the current-buffer (if the click
    happened over a toolbar);
  - the current-local-map of the buffer under the mouse (does not
    apply to toolbar clicks);
  - any applicable minor-mode maps;
  - the current global map.

Note that if `overriding-local-map' or `overriding-terminal-local-map'
is non-nil, *only* those two maps and the current global map are searched.
*/
       (keys, accept_default))
{
  /* This function can GC */
  int i;
  Lisp_Object maps[100];
  int nmaps;
  struct gcpro gcpro1, gcpro2;
  GCPRO2 (keys, accept_default); /* get_relevant_keymaps may autoload */

  nmaps = get_relevant_keymaps (keys, countof (maps), maps);

  UNGCPRO;

  if (EVENTP (keys))           /* unadvertised "feature" for the future */
    return lookup_events (keys, nmaps, maps, !NILP (accept_default));

  for (i = 0; i < nmaps; i++)
    {
      Lisp_Object tem = Flookup_key (maps[i], keys,
				     accept_default);
      if (INTP (tem))
	{
	  /* Too long in some local map means don't look at global map */
	  return Qnil;
	}
      else if (!NILP (tem))
	return tem;
    }
  return Qnil;
}

static Lisp_Object
process_event_binding_result (Lisp_Object result)
{
  if (EQ (result, Qundefined))
    /* The suppress-keymap function binds keys to 'undefined - special-case
       that here, so that being bound to that has the same error-behavior as
       not being defined at all.
       */
    result = Qnil;
  if (!NILP (result))
    {
      Lisp_Object map;
      /* Snap out possible keymap indirections */
      map = get_keymap (result, 0, 1);
      if (!NILP (map))
	result = map;
    }

  return result;
}

/* Attempts to find a command corresponding to the event-sequence
   whose head is event0 (sequence is threaded though event_next).

   The return value will be

      -- nil (there is no binding; this will also be returned
              whenever the event chain is "too long", i.e. there
	      is a non-nil, non-keymap binding for a prefix of
	      the event chain)
      -- a keymap (part of a command has been specified)
      -- a command (anything that satisfies `commandp'; this includes
                    some symbols, lists, subrs, strings, vectors, and
		    compiled-function objects) */
Lisp_Object
event_binding (Lisp_Object event0, int accept_default)
{
  /* This function can GC */
  Lisp_Object maps[100];
  int nmaps;

  assert (EVENTP (event0));

  nmaps = get_relevant_keymaps (event0, countof (maps), maps);
  if (nmaps > countof (maps))
    nmaps = countof (maps);
  return process_event_binding_result (lookup_events (event0, nmaps, maps,
						      accept_default));
}

/* like event_binding, but specify a keymap to search */

Lisp_Object
event_binding_in (Lisp_Object event0, Lisp_Object keymap, int accept_default)
{
  /* This function can GC */
  if (!KEYMAPP (keymap))
    return Qnil;

  return process_event_binding_result (lookup_events (event0, 1, &keymap,
						      accept_default));
}

/* Attempts to find a function key mapping corresponding to the
   event-sequence whose head is event0 (sequence is threaded through
   event_next).  The return value will be the same as for event_binding(). */
Lisp_Object
munging_key_map_event_binding (Lisp_Object event0,
			       enum munge_me_out_the_door munge)
{
  Lisp_Object keymap = (munge == MUNGE_ME_FUNCTION_KEY) ?
    CONSOLE_FUNCTION_KEY_MAP (event_console_or_selected (event0)) :
    Vkey_translation_map;

  if (NILP (keymap))
    return Qnil;

  return process_event_binding_result (lookup_events (event0, 1, &keymap, 1));
}


/************************************************************************/
/*               Setting/querying the global and local maps             */
/************************************************************************/

DEFUN ("use-global-map", Fuse_global_map, 1, 1, 0, /*
Select KEYMAP as the global keymap.
*/
       (keymap))
{
  /* This function can GC */
  keymap = get_keymap (keymap, 1, 1);
  Vcurrent_global_map = keymap;
  return Qnil;
}

DEFUN ("use-local-map", Fuse_local_map, 1, 2, 0, /*
Select KEYMAP as the local keymap in BUFFER.
If KEYMAP is nil, that means no local keymap.
If BUFFER is nil, the current buffer is assumed.
*/
       (keymap, buffer))
{
  /* This function can GC */
  struct buffer *b = decode_buffer (buffer, 0);
  if (!NILP (keymap))
    keymap = get_keymap (keymap, 1, 1);

  b->keymap = keymap;

  return Qnil;
}

DEFUN ("current-local-map", Fcurrent_local_map, 0, 1, 0, /*
Return BUFFER's local keymap, or nil if it has none.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 0);
  return b->keymap;
}

DEFUN ("current-global-map", Fcurrent_global_map, 0, 0, 0, /*
Return the current global keymap.
*/
       ())
{
  return Vcurrent_global_map;
}


/************************************************************************/
/*                    Mapping over keymap elements                      */
/************************************************************************/

/* Since keymaps are arranged in a hierarchy, one keymap per bucky bit or
   prefix key, it's not entirely obvious what map-keymap should do, but
   what it does is: map over all keys in this map; then recursively map
   over all submaps of this map that are "bucky" submaps.  This means that,
   when mapping over a keymap, it appears that "x" and "C-x" are in the
   same map, although "C-x" is really in the "control" submap of this one.
   However, since we don't recursively descend the submaps that are bound
   to prefix keys (like C-x, C-h, etc) the caller will have to recurse on
   those explicitly, if that's what they want.

   So the end result of this is that the bucky keymaps (the ones indexed
   under the large integers returned from MAKE_MODIFIER_HASH_KEY()) are
   invisible from elisp.  They're just an implementation detail that code
   outside of this file doesn't need to know about.
 */

struct map_keymap_unsorted_closure
{
  void (*fn) (CONST struct key_data *, Lisp_Object binding, void *arg);
  void *arg;
  unsigned int modifiers;
};

/* used by map_keymap() */
static void
map_keymap_unsorted_mapper (CONST void *hash_key, void *hash_contents,
                            void *map_keymap_unsorted_closure)
{
  /* This function can GC */
  Lisp_Object keysym;
  Lisp_Object contents;
  struct map_keymap_unsorted_closure *closure =
    (struct map_keymap_unsorted_closure *) map_keymap_unsorted_closure;
  unsigned int modifiers = closure->modifiers;
  unsigned int mod_bit;
  CVOID_TO_LISP (keysym, hash_key);
  VOID_TO_LISP (contents, hash_contents);
  mod_bit = MODIFIER_HASH_KEY_BITS (keysym);
  if (mod_bit != 0)
    {
      int omod = modifiers;
      closure->modifiers = (modifiers | mod_bit);
      contents = get_keymap (contents, 1, 0);
      elisp_maphash (map_keymap_unsorted_mapper,
		     XKEYMAP (contents)->table,
		     map_keymap_unsorted_closure);
      closure->modifiers = omod;
    }
  else
    {
      struct key_data key;
      key.keysym = keysym;
      key.modifiers = modifiers;
      ((*closure->fn) (&key, contents, closure->arg));
    }
}


struct map_keymap_sorted_closure
{
  Lisp_Object *result_locative;
};

/* used by map_keymap_sorted() */
static void
map_keymap_sorted_mapper (CONST void *hash_key, void *hash_contents,
                          void *map_keymap_sorted_closure)
{
  struct map_keymap_sorted_closure *cl =
    (struct map_keymap_sorted_closure *) map_keymap_sorted_closure;
  Lisp_Object key, contents;
  Lisp_Object *list = cl->result_locative;
  CVOID_TO_LISP (key, hash_key);
  VOID_TO_LISP (contents, hash_contents);
  *list = Fcons (Fcons (key, contents), *list);
}


/* used by map_keymap_sorted(), describe_map_sort_predicate(),
   and keymap_submaps().
 */
static int
map_keymap_sort_predicate (Lisp_Object obj1, Lisp_Object obj2,
                           Lisp_Object pred)
{
  /* obj1 and obj2 are conses with keysyms in their cars.  Cdrs are ignored.
   */
  unsigned int bit1, bit2;
  int sym1_p = 0;
  int sym2_p = 0;
  obj1 = XCAR (obj1);
  obj2 = XCAR (obj2);

  if (EQ (obj1, obj2))
    return -1;
  bit1 = MODIFIER_HASH_KEY_BITS (obj1);
  bit2 = MODIFIER_HASH_KEY_BITS (obj2);

  /* If either is a symbol with a character-set-property, then sort it by
     that code instead of alphabetically.
     */
  if (! bit1 && SYMBOLP (obj1))
    {
      Lisp_Object code = Fget (obj1, Vcharacter_set_property, Qnil);
      if (CHAR_OR_CHAR_INTP (code))
	{
	  obj1 = code;
	  CHECK_CHAR_COERCE_INT (obj1);
	  sym1_p = 1;
	}
    }
  if (! bit2 && SYMBOLP (obj2))
    {
      Lisp_Object code = Fget (obj2, Vcharacter_set_property, Qnil);
      if (CHAR_OR_CHAR_INTP (code))
	{
	  obj2 = code;
	  CHECK_CHAR_COERCE_INT (obj2);
	  sym2_p = 1;
	}
    }

  /* all symbols (non-ASCIIs) come after characters (ASCIIs) */
  if (XTYPE (obj1) != XTYPE (obj2))
    return SYMBOLP (obj2) ? 1 : -1;

  if (! bit1 && CHARP (obj1)) /* they're both ASCII */
    {
      int o1 = XCHAR (obj1);
      int o2 = XCHAR (obj2);
      if (o1 == o2 &&		/* If one started out as a symbol and the */
	  sym1_p != sym2_p)	/* other didn't, the symbol comes last. */
	return sym2_p ? 1 : -1;

      return o1 < o2 ? 1 : -1;	/* else just compare them */
    }

  /* else they're both symbols.  If they're both buckys, then order them. */
  if (bit1 && bit2)
    return bit1 < bit2 ? 1 : -1;

  /* if only one is a bucky, then it comes later */
  if (bit1 || bit2)
    return bit2 ? 1 : -1;

  /* otherwise, string-sort them. */
  {
    char *s1 = (char *) string_data (XSYMBOL (obj1)->name);
    char *s2 = (char *) string_data (XSYMBOL (obj2)->name);
#ifdef I18N2
    return 0 > strcoll (s1, s2) ? 1 : -1;
#else
    return 0 > strcmp  (s1, s2) ? 1 : -1;
#endif
  }
}


/* used by map_keymap() */
static void
map_keymap_sorted (Lisp_Object keymap_table,
                   unsigned int modifiers,
                   void (*function) (CONST struct key_data *key,
                                     Lisp_Object binding,
                                     void *map_keymap_sorted_closure),
                   void *map_keymap_sorted_closure)
{
  /* This function can GC */
  struct gcpro gcpro1;
  Lisp_Object contents = Qnil;

  if (XINT (Fhashtable_fullness (keymap_table)) == 0)
    return;

  GCPRO1 (contents);

  {
    struct map_keymap_sorted_closure c1;
    c1.result_locative = &contents;
    elisp_maphash (map_keymap_sorted_mapper, keymap_table, &c1);
  }
  contents = list_sort (contents, Qnil, map_keymap_sort_predicate);
  for (; !NILP (contents); contents = XCDR (contents))
    {
      Lisp_Object keysym = XCAR (XCAR (contents));
      Lisp_Object binding = XCDR (XCAR (contents));
      unsigned int sub_bits = MODIFIER_HASH_KEY_BITS (keysym);
      if (sub_bits != 0)
	map_keymap_sorted (XKEYMAP (get_keymap (binding,
						1, 1))->table,
			   (modifiers | sub_bits),
			   function,
			   map_keymap_sorted_closure);
      else
	{
	  struct key_data k;
	  k.keysym = keysym;
	  k.modifiers = modifiers;
	  ((*function) (&k, binding, map_keymap_sorted_closure));
	}
    }
  UNGCPRO;
}


/* used by Fmap_keymap() */
static void
map_keymap_mapper (CONST struct key_data *key,
                   Lisp_Object binding,
                   void *function)
{
  /* This function can GC */
  Lisp_Object fn;
  VOID_TO_LISP (fn, function);
  call2 (fn, make_key_description (key, 1), binding);
}


static void
map_keymap (Lisp_Object keymap_table, int sort_first,
            void (*function) (CONST struct key_data *key,
                              Lisp_Object binding,
                              void *fn_arg),
            void *fn_arg)
{
  /* This function can GC */
  if (sort_first)
    map_keymap_sorted (keymap_table, 0, function, fn_arg);
  else
    {
      struct map_keymap_unsorted_closure map_keymap_unsorted_closure;
      map_keymap_unsorted_closure.fn = function;
      map_keymap_unsorted_closure.arg = fn_arg;
      map_keymap_unsorted_closure.modifiers = 0;
      elisp_maphash (map_keymap_unsorted_mapper, keymap_table,
		     &map_keymap_unsorted_closure);
    }
}

DEFUN ("map-keymap", Fmap_keymap, 2, 3, 0, /*
Apply FUNCTION to each element of KEYMAP.
FUNCTION will be called with two arguments: a key-description list, and
the binding.  The order in which the elements of the keymap are passed to
the function is unspecified.  If the function inserts new elements into
the keymap, it may or may not be called with them later.  No element of
the keymap will ever be passed to the function more than once.

The function will not be called on elements of this keymap's parents
(see the function `keymap-parents') or upon keymaps which are contained
within this keymap (multi-character definitions).
It will be called on "meta" characters since they are not really
two-character sequences.

If the optional third argument SORT-FIRST is non-nil, then the elements of
the keymap will be passed to the mapper function in a canonical order.
Otherwise, they will be passed in hash (that is, random) order, which is
faster.
*/
     (function, keymap, sort_first))
{
  /* This function can GC */
  struct gcpro gcpro1, gcpro2;

 /* tolerate obviously transposed args */
  if (!NILP (Fkeymapp (function)))
    {
      Lisp_Object tmp = function;
      function = keymap;
      keymap = tmp;
    }
  GCPRO2 (function, keymap);
  keymap = get_keymap (keymap, 1, 1);
  map_keymap (XKEYMAP (keymap)->table, !NILP (sort_first),
	      map_keymap_mapper, LISP_TO_VOID (function));
  UNGCPRO;
  return Qnil;
}



/************************************************************************/
/*                          Accessible keymaps                          */
/************************************************************************/

struct accessible_keymaps_closure
  {
    Lisp_Object tail;
  };


static void
accessible_keymaps_mapper_1 (Lisp_Object keysym, Lisp_Object contents,
                             unsigned int modifiers,
                             struct accessible_keymaps_closure *closure)
{
  /* This function can GC */
  unsigned int subbits = MODIFIER_HASH_KEY_BITS (keysym);

  if (subbits != 0)
    {
      Lisp_Object submaps;

      contents = get_keymap (contents, 1, 1);
      submaps = keymap_submaps (contents);
      for (; !NILP (submaps); submaps = XCDR (submaps))
	{
	  accessible_keymaps_mapper_1 (XCAR (XCAR (submaps)),
                                       XCDR (XCAR (submaps)),
                                       (subbits | modifiers),
                                       closure);
	}
    }
  else
    {
      Lisp_Object thisseq = Fcar (Fcar (closure->tail));
      Lisp_Object cmd = get_keyelt (contents, 1);
      Lisp_Object vec;
      int j;
      int len;
      struct key_data key;
      key.keysym = keysym;
      key.modifiers = modifiers;

      if (NILP (cmd))
	abort ();
      cmd = get_keymap (cmd, 0, 1);
      if (!KEYMAPP (cmd))
	abort ();

      vec = make_vector (XVECTOR_LENGTH (thisseq) + 1, Qnil);
      len = XVECTOR_LENGTH (thisseq);
      for (j = 0; j < len; j++)
	XVECTOR_DATA (vec) [j] = XVECTOR_DATA (thisseq) [j];
      XVECTOR_DATA (vec) [j] = make_key_description (&key, 1);

      nconc2 (closure->tail, list1 (Fcons (vec, cmd)));
    }
}


static Lisp_Object
accessible_keymaps_keymap_mapper (Lisp_Object thismap, void *arg)
{
  /* This function can GC */
  struct accessible_keymaps_closure *closure =
    (struct accessible_keymaps_closure *) arg;
  Lisp_Object submaps = keymap_submaps (thismap);

  for (; !NILP (submaps); submaps = XCDR (submaps))
    {
      accessible_keymaps_mapper_1 (XCAR (XCAR (submaps)),
				   XCDR (XCAR (submaps)),
				   0,
				   closure);
    }
  return Qnil;
}


DEFUN ("accessible-keymaps", Faccessible_keymaps, 1, 2, 0, /*
Find all keymaps accessible via prefix characters from KEYMAP.
Returns a list of elements of the form (KEYS . MAP), where the sequence
KEYS starting from KEYMAP gets you to MAP.  These elements are ordered
so that the KEYS increase in length.  The first element is ([] . KEYMAP).
An optional argument PREFIX, if non-nil, should be a key sequence;
then the value includes only maps for prefixes that start with PREFIX.
*/
       (keymap, prefix))
{
  /* This function can GC */
  struct gcpro gcpro1, gcpro2, gcpro3, gcpro4;
  Lisp_Object accessible_keymaps = Qnil;
  struct accessible_keymaps_closure c;
  c.tail = Qnil;
  GCPRO4 (accessible_keymaps, c.tail, prefix, keymap);

 retry:
  keymap = get_keymap (keymap, 1, 1);
  if (NILP (prefix))
    prefix = make_vector (0, Qnil);
  else if (!VECTORP (prefix) || STRINGP (prefix))
    {
      prefix = wrong_type_argument (Qarrayp, prefix);
      goto retry;
    }
  else
    {
      int len = XINT (Flength (prefix));
      Lisp_Object def = Flookup_key (keymap, prefix, Qnil);
      Lisp_Object p;
      int iii;
      struct gcpro ngcpro1;

      def = get_keymap (def, 0, 1);
      if (!KEYMAPP (def))
	goto RETURN;

      keymap = def;
      p = make_vector (len, Qnil);
      NGCPRO1 (p);
      for (iii = 0; iii < len; iii++)
	{
	  struct key_data key;
	  define_key_parser (Faref (prefix, make_int (iii)), &key);
	  XVECTOR_DATA (p)[iii] = make_key_description (&key, 1);
	}
      NUNGCPRO;
      prefix = p;
    }

  accessible_keymaps = list1 (Fcons (prefix, keymap));

  /* For each map in the list maps,
     look at any other maps it points to
     and stick them at the end if they are not already in the list */

  for (c.tail = accessible_keymaps;
       !NILP (c.tail);
       c.tail = XCDR (c.tail))
    {
      Lisp_Object thismap = Fcdr (Fcar (c.tail));
      CHECK_KEYMAP (thismap);
      traverse_keymaps (thismap, Qnil,
                        accessible_keymaps_keymap_mapper, &c);
    }
 RETURN:
  UNGCPRO;
  return accessible_keymaps;
}



/************************************************************************/
/*              Pretty descriptions of key sequences                    */
/************************************************************************/

DEFUN ("key-description", Fkey_description, 1, 1, 0, /*
Return a pretty description of key-sequence KEYS.
Control characters turn into "C-foo" sequences, meta into "M-foo",
spaces are put between sequence elements, etc...
*/
       (keys))
{
  if (CHAR_OR_CHAR_INTP (keys) || CONSP (keys) || SYMBOLP (keys)
      || EVENTP (keys))
    {
      return Fsingle_key_description (keys);
    }
  else if (VECTORP (keys) ||
	   STRINGP (keys))
    {
      Lisp_Object string = Qnil;
      /* Lisp_Object sep = Qnil; */
      int size = XINT (Flength (keys));
      int i;

      for (i = 0; i < size; i++)
	{
	  Lisp_Object s2 = Fsingle_key_description
	    (((STRINGP (keys))
	      ? make_char (string_char (XSTRING (keys), i))
	      : XVECTOR_DATA (keys)[i]));

	  if (i == 0)
	    string = s2;
	  else
	    {
	      /* if (NILP (sep)) Lisp_Object sep = build_string (" ") */;
	      string = concat2 (string, concat2 (Vsingle_space_string, s2));
	    }
	}
      return string;
    }
  return Fkey_description (wrong_type_argument (Qsequencep, keys));
}

DEFUN ("single-key-description", Fsingle_key_description, 1, 1, 0, /*
Return a pretty description of command character KEY.
Control characters turn into C-whatever, etc.
This differs from `text-char-description' in that it returns a description
of a key read from the user rather than a character from a buffer.
*/
       (key))
{
  if (SYMBOLP (key))
    key = Fcons (key, Qnil); /* sleaze sleaze */

  if (EVENTP (key) || CHAR_OR_CHAR_INTP (key))
    {
      char buf [255];
      if (!EVENTP (key))
	{
	  struct Lisp_Event event;
	  event.event_type = empty_event;
	  CHECK_CHAR_COERCE_INT (key);
	  character_to_event (XCHAR (key), &event,
			      XCONSOLE (Vselected_console), 0);
	  format_event_object (buf, &event, 1);
	}
      else
	format_event_object (buf, XEVENT (key), 1);
      return build_string (buf);
    }

  if (CONSP (key))
    {
      char buf[255];
      char *bufp = buf;
      Lisp_Object rest;
      buf[0] = 0;
      LIST_LOOP (rest, key)
	{
	  Lisp_Object keysym = XCAR (rest);
	  if (EQ (keysym, Qcontrol))    strcpy (bufp, "C-"), bufp += 2;
	  else if (EQ (keysym, Qctrl))  strcpy (bufp, "C-"), bufp += 2;
	  else if (EQ (keysym, Qmeta))  strcpy (bufp, "M-"), bufp += 2;
	  else if (EQ (keysym, Qsuper)) strcpy (bufp, "S-"), bufp += 2;
	  else if (EQ (keysym, Qhyper)) strcpy (bufp, "H-"), bufp += 2;
	  else if (EQ (keysym, Qalt))	strcpy (bufp, "A-"), bufp += 2;
	  else if (EQ (keysym, Qshift)) strcpy (bufp, "Sh-"), bufp += 3;
	  else if (CHAR_OR_CHAR_INTP (keysym))
	    {
	      bufp += set_charptr_emchar ((Bufbyte *) bufp,
					  XCHAR_OR_CHAR_INT (keysym));
	      *bufp = 0;
	    }
	  else
	    {
	      CHECK_SYMBOL (keysym);
#if 0                           /* This is bogus */
	      if (EQ (keysym, QKlinefeed))	 strcpy (bufp, "LFD");
	      else if (EQ (keysym, QKtab))	 strcpy (bufp, "TAB");
	      else if (EQ (keysym, QKreturn))	 strcpy (bufp, "RET");
	      else if (EQ (keysym, QKescape))	 strcpy (bufp, "ESC");
	      else if (EQ (keysym, QKdelete))	 strcpy (bufp, "DEL");
	      else if (EQ (keysym, QKspace))	 strcpy (bufp, "SPC");
	      else if (EQ (keysym, QKbackspace)) strcpy (bufp, "BS");
	      else
#endif
		strcpy (bufp, (char *) string_data (XSYMBOL (keysym)->name));
	      if (!NILP (XCDR (rest)))
		signal_simple_error ("invalid key description",
				     key);
	    }
	}
      return build_string (buf);
    }
  return Fsingle_key_description
    (wrong_type_argument (intern ("char-or-event-p"), key));
}

DEFUN ("text-char-description", Ftext_char_description, 1, 1, 0, /*
Return a pretty description of file-character CHR.
Unprintable characters turn into "^char" or \\NNN, depending on the value
of the `ctl-arrow' variable.
This differs from `single-key-description' in that it returns a description
of a character from a buffer rather than a key read from the user.
*/
       (chr))
{
  Bufbyte buf[200];
  Bufbyte *p;
  unsigned int c;
  Lisp_Object ctl_arrow = current_buffer->ctl_arrow;
  int ctl_p = !NILP (ctl_arrow);
  Emchar printable_min = (CHAR_OR_CHAR_INTP (ctl_arrow)
			  ? XCHAR_OR_CHAR_INT (ctl_arrow)
			  : ((EQ (ctl_arrow, Qt) || NILP (ctl_arrow))
			     ? 256 : 160));

  if (EVENTP (chr))
    {
      Lisp_Object ch = Fevent_to_character (chr, Qnil, Qnil, Qt);
      if (NILP (ch))
	return
	  signal_simple_continuable_error
	    ("character has no ASCII equivalent", Fcopy_event (chr, Qnil));
      chr = ch;
    }

  CHECK_CHAR_COERCE_INT (chr);

  c = XCHAR (chr);
  p = buf;

  if (c >= printable_min)
    {
      p += set_charptr_emchar (p, c);
    }
  else if (c < 040 && ctl_p)
    {
      *p++ = '^';
      *p++ = c + 64;		/* 'A' - 1 */
    }
  else if (c == 0177)
    {
      *p++ = '^';
      *p++ = '?';
    }
  else if (c >= 0200 || c < 040)
    {
      *p++ = '\\';
#ifdef MULE
      /* !!#### This syntax is not readable.  It will
	 be interpreted as a 3-digit octal number rather
	 than a 7-digit octal number. */
      if (c >= 0400)
	{
	  *p++ = '0' + ((c & 07000000) >> 18);
	  *p++ = '0' + ((c & 0700000) >> 15);
	  *p++ = '0' + ((c & 070000) >> 12);
	  *p++ = '0' + ((c & 07000) >> 9);
	}
#endif
      *p++ = '0' + ((c & 0700) >> 6);
      *p++ = '0' + ((c & 0070) >> 3);
      *p++ = '0' + ((c & 0007));
    }
  else
    {
      p += set_charptr_emchar (p, c);
    }

  *p = 0;
  return build_string ((char *) buf);
}


/************************************************************************/
/*              where-is (mapping bindings to keys)                     */
/************************************************************************/

static Lisp_Object
where_is_internal (Lisp_Object definition, Lisp_Object *maps, int nmaps,
                   Lisp_Object firstonly, char *target_buffer);

DEFUN ("where-is-internal", Fwhere_is_internal, 1, 5, 0, /*
Return list of keys that invoke DEFINITION in KEYMAPS.
KEYMAPS can be either a keymap (meaning search in that keymap and the
current global keymap) or a list of keymaps (meaning search in exactly
those keymaps and no others).  If KEYMAPS is nil, search in the currently
applicable maps for EVENT-OR-KEYS (this is equivalent to specifying
`(current-keymaps EVENT-OR-KEYS)' as the argument to KEYMAPS).

If optional 3rd arg FIRSTONLY is non-nil, return a vector representing
 the first key sequence found, rather than a list of all possible key
 sequences.

If optional 4th arg NOINDIRECT is non-nil, don't follow indirections
 to other keymaps or slots.  This makes it possible to search for an
 indirect definition itself.
*/
       (definition, keymaps, firstonly, noindirect, event_or_keys))
{
  /* This function can GC */
  Lisp_Object maps[100];
  Lisp_Object *gubbish = maps;
  int nmaps;

  /* Get keymaps as an array */
  if (NILP (keymaps))
    {
      nmaps = get_relevant_keymaps (event_or_keys, countof (maps),
				    gubbish);
      if (nmaps > countof (maps))
	{
	  gubbish = alloca_array (Lisp_Object, nmaps);
	  nmaps = get_relevant_keymaps (event_or_keys, nmaps, gubbish);
	}
    }
  else if (CONSP (keymaps))
    {
      Lisp_Object rest;
      int i;

      nmaps = XINT (Flength (keymaps));
      if (nmaps > countof (maps))
	{
	  gubbish = alloca_array (Lisp_Object, nmaps);
	}
      for (rest = keymaps, i = 0; !NILP (rest);
	   rest = XCDR (keymaps), i++)
	{
	  gubbish[i] = get_keymap (XCAR (keymaps), 1, 1);
	}
    }
  else
    {
      nmaps = 1;
      gubbish[0] = get_keymap (keymaps, 1, 1);
      if (!EQ (gubbish[0], Vcurrent_global_map))
	{
	  gubbish[1] = Vcurrent_global_map;
	  nmaps++;
	}
    }

  return where_is_internal (definition, gubbish, nmaps, firstonly, 0);
}

/* This function is like
   (key-description (where-is-internal definition nil t))
   except that it writes its output into a (char *) buffer that you
   provide; it doesn't cons (or allocate memory) at all, so it's
   very fast.  This is used by menubar.c.
 */
void
where_is_to_char (Lisp_Object definition, char *buffer)
{
  /* This function can GC */
  Lisp_Object maps[100];
  Lisp_Object *gubbish = maps;
  int nmaps;

  /* Get keymaps as an array */
  nmaps = get_relevant_keymaps (Qnil, countof (maps), gubbish);
  if (nmaps > countof (maps))
    {
      gubbish = alloca_array (Lisp_Object, nmaps);
      nmaps = get_relevant_keymaps (Qnil, nmaps, gubbish);
    }

  buffer[0] = 0;
  where_is_internal (definition, maps, nmaps, Qt, buffer);
}


static Lisp_Object
raw_keys_to_keys (struct key_data *keys, int count)
{
  Lisp_Object result = make_vector (count, Qnil);
  while (count--)
    XVECTOR_DATA (result) [count] = make_key_description (&(keys[count]), 1);
  return result;
}


static void
format_raw_keys (struct key_data *keys, int count, char *buf)
{
  int i;
  struct Lisp_Event event;
  event.event_type = key_press_event;
  event.channel = Vselected_console;
  for (i = 0; i < count; i++)
    {
      event.event.key.keysym    = keys[i].keysym;
      event.event.key.modifiers = keys[i].modifiers;
      format_event_object (buf, &event, 1);
      buf += strlen (buf);
      if (i < count-1)
	buf[0] = ' ', buf++;
    }
}


/* definition is the thing to look for.
   map is a keymap.
   shadow is an array of shadow_count keymaps; if there is a different
   binding in any of the keymaps of a key that we are considering
   returning, then we reconsider.
   firstonly means give up after finding the first match;
   keys_so_far and modifiers_so_far describe which map we're looking in;
   If we're in the "meta" submap of the map that "C-x 4" is bound to,
   then keys_so_far will be {(control x), \4}, and modifiers_so_far
   will be MOD_META.  That is, keys_so_far is the chain of keys that we
   have followed, and modifiers_so_far_so_far is the bits (partial keys)
   beyond that.

   (keys_so_far is a global buffer and the keys_count arg says how much
   of it we're currently interested in.)

   If target_buffer is provided, then we write a key-description into it,
   to avoid consing a string.  This only works with firstonly on.
   */

struct where_is_closure
  {
    Lisp_Object definition;
    Lisp_Object *shadow;
    int shadow_count;
    int firstonly;
    int keys_count;
    unsigned int modifiers_so_far;
    char *target_buffer;
    struct key_data *keys_so_far;
    int keys_so_far_total_size;
    int keys_so_far_malloced;
  };

static Lisp_Object where_is_recursive_mapper (Lisp_Object map, void *arg);

static Lisp_Object
where_is_recursive_mapper (Lisp_Object map, void *arg)
{
  /* This function can GC */
  struct where_is_closure *c = (struct where_is_closure *) arg;
  Lisp_Object definition = c->definition;
  CONST int firstonly = c->firstonly;
  CONST unsigned int keys_count = c->keys_count;
  CONST unsigned int modifiers_so_far = c->modifiers_so_far;
  char *target_buffer = c->target_buffer;
  Lisp_Object keys = Fgethash (definition,
                               XKEYMAP (map)->inverse_table,
                               Qnil);
  Lisp_Object submaps;
  Lisp_Object result = Qnil;

  if (!NILP (keys))
    {
      /* One or more keys in this map match the definition we're looking for.
	 Verify that these bindings aren't shadowed by other bindings
	 in the shadow maps.  Either nil or number as value from
	 raw_lookup_key() means undefined.  */
      struct key_data *so_far = c->keys_so_far;

      for (;;) /* loop over all keys that match */
	{
	  Lisp_Object k = ((CONSP (keys)) ? XCAR (keys) : keys);
	  int i;

	  so_far [keys_count].keysym = k;
	  so_far [keys_count].modifiers = modifiers_so_far;

	  /* now loop over all shadow maps */
	  for (i = 0; i < c->shadow_count; i++)
	    {
	      Lisp_Object shadowed = raw_lookup_key (c->shadow[i],
						     so_far,
						     keys_count + 1,
						     0, 1);

	      if (NILP (shadowed) || CHARP (shadowed) ||
		  EQ (shadowed, definition))
		continue; /* we passed this test; it's not shadowed here. */
	      else
		/* ignore this key binding, since it actually has a
		   different binding in a shadowing map */
		goto c_doesnt_have_proper_loop_exit_statements;
	    }

	  /* OK, the key is for real */
	  if (target_buffer)
	    {
	      if (!firstonly) abort ();
	      format_raw_keys (so_far, keys_count + 1, target_buffer);
	      return make_int (1);
	    }
	  else if (firstonly)
	    return raw_keys_to_keys (so_far, keys_count + 1);
	  else
	    result = Fcons (raw_keys_to_keys (so_far, keys_count + 1),
			    result);

	c_doesnt_have_proper_loop_exit_statements:
	  /* now on to the next matching key ... */
	  if (!CONSP (keys)) break;
	  keys = XCDR (keys);
	}
    }

  /* Now search the sub-keymaps of this map.
     If we're in "firstonly" mode and have already found one, this
     point is not reached.  If we get one from lower down, either
     return it immediately (in firstonly mode) or tack it onto the
     end of the ones we've gotten so far.
     */
  for (submaps = keymap_submaps (map);
       !NILP (submaps);
       submaps = XCDR (submaps))
    {
      Lisp_Object key    = XCAR (XCAR (submaps));
      Lisp_Object submap = XCDR (XCAR (submaps));
      unsigned int lower_modifiers;
      int lower_keys_count = keys_count;
      unsigned int bucky;

      submap = get_keymap (submap, 0, 0);

      if (EQ (submap, map))
	/* Arrgh!  Some loser has introduced a loop... */
	continue;

      /* If this is not a keymap, then that's probably because someone
	 did an `fset' of a symbol that used to point to a map such that
	 it no longer does.  Sigh.  Ignore this, and invalidate the cache
	 so that it doesn't happen to us next time too.
	 */
      if (NILP (submap))
	{
	  XKEYMAP (map)->sub_maps_cache = Qt;
	  continue;
	}

      /* If the map is a "bucky" map, then add a bit to the
	 modifiers_so_far list.
	 Otherwise, add a new raw_key onto the end of keys_so_far.
	 */
      bucky = MODIFIER_HASH_KEY_BITS (key);
      if (bucky != 0)
	lower_modifiers = (modifiers_so_far | bucky);
      else
	{
	  struct key_data *so_far = c->keys_so_far;
	  lower_modifiers = 0;
	  so_far [lower_keys_count].keysym = key;
	  so_far [lower_keys_count].modifiers = modifiers_so_far;
	  lower_keys_count++;
	}

      if (lower_keys_count >= c->keys_so_far_total_size)
	{
	  int size = lower_keys_count + 50;
	  if (! c->keys_so_far_malloced)
	    {
	      struct key_data *new = xnew_array (struct key_data, size);
	      memcpy ((void *)new, (CONST void *)c->keys_so_far,
		      c->keys_so_far_total_size * sizeof (struct key_data));
	    }
	  else
	    XREALLOC_ARRAY (c->keys_so_far, struct key_data, size);

	  c->keys_so_far_total_size = size;
	  c->keys_so_far_malloced = 1;
	}

      {
	Lisp_Object lower;

	c->keys_count = lower_keys_count;
	c->modifiers_so_far = lower_modifiers;

	lower = traverse_keymaps (submap, Qnil, where_is_recursive_mapper, c);

	c->keys_count = keys_count;
	c->modifiers_so_far = modifiers_so_far;

	if (!firstonly)
	  result = nconc2 (lower, result);
	else if (!NILP (lower))
	  return lower;
      }
    }
  return result;
}


static Lisp_Object
where_is_internal (Lisp_Object definition, Lisp_Object *maps, int nmaps,
                   Lisp_Object firstonly, char *target_buffer)
{
  /* This function can GC */
  Lisp_Object result = Qnil;
  int i;
  struct key_data raw[20];
  struct where_is_closure c;

  c.definition = definition;
  c.shadow = maps;
  c.firstonly = !NILP (firstonly);
  c.target_buffer = target_buffer;
  c.keys_so_far = raw;
  c.keys_so_far_total_size = countof (raw);
  c.keys_so_far_malloced = 0;

  /* Loop over each of the maps, accumulating the keys found.
     For each map searched, all previous maps shadow this one
     so that bogus keys aren't listed. */
  for (i = 0; i < nmaps; i++)
    {
      Lisp_Object this_result;
      c.shadow_count = i;
      /* Reset the things set in each iteration */
      c.keys_count = 0;
      c.modifiers_so_far = 0;

      this_result = traverse_keymaps (maps[i], Qnil, where_is_recursive_mapper,
				      &c);
      if (!NILP (firstonly))
	{
	  result = this_result;
	  if (!NILP (result))
	    break;
	}
      else
	result = nconc2 (this_result, result);
    }

  if (NILP (firstonly))
    result = Fnreverse (result);

  if (c.keys_so_far_malloced)
    xfree (c.keys_so_far);
  return result;
}


/************************************************************************/
/*                         Describing keymaps                           */
/************************************************************************/

DEFUN ("describe-bindings-internal", Fdescribe_bindings_internal, 1, 5, 0, /*
Insert a list of all defined keys and their definitions in MAP.
Optional second argument ALL says whether to include even "uninteresting"
definitions (ie symbols with a non-nil `suppress-keymap' property.
Third argument SHADOW is a list of keymaps whose bindings shadow those
of map; if a binding is present in any shadowing map, it is not printed.
Fourth argument PREFIX, if non-nil, should be a key sequence;
only bindings which start with that key sequence will be printed.
Fifth argument MOUSE-ONLY-P says to only print bindings for mouse clicks.
*/
       (map, all, shadow, prefix, mouse_only_p))
{
  /* This function can GC */
  describe_map_tree (map, NILP (all), shadow, prefix,
		     !NILP (mouse_only_p));
  return Qnil;
}


/* Insert a desription of the key bindings in STARTMAP,
    followed by those of all maps reachable through STARTMAP.
   If PARTIAL is nonzero, omit certain "uninteresting" commands
    (such as `undefined').
   If SHADOW is non-nil, it is a list of other maps;
    don't mention keys which would be shadowed by any of them
   If PREFIX is non-nil, only list bindings which start with those keys.
 */

void
describe_map_tree (Lisp_Object startmap, int partial, Lisp_Object shadow,
                   Lisp_Object prefix, int mice_only_p)
{
  /* This function can GC */
  Lisp_Object maps = Qnil;
  struct gcpro gcpro1, gcpro2;  /* get_keymap may autoload */
  GCPRO2 (maps, shadow);

  maps = Faccessible_keymaps (startmap, prefix);

  for (; !NILP (maps); maps = Fcdr (maps))
    {
      Lisp_Object sub_shadow = Qnil;
      Lisp_Object elt = Fcar (maps);
      Lisp_Object tail;
      int no_prefix = (VECTORP (Fcar (elt))
                       && XINT (Flength (Fcar (elt))) == 0);
      struct gcpro ngcpro1, ngcpro2, ngcpro3;
      NGCPRO3 (sub_shadow, elt, tail);

      for (tail = shadow; CONSP (tail); tail = XCDR (tail))
	{
	  Lisp_Object shmap = XCAR (tail);

	  /* If the sequence by which we reach this keymap is zero-length,
	     then the shadow maps for this keymap are just SHADOW.  */
	  if (no_prefix)
	    ;
	  /* If the sequence by which we reach this keymap actually has
	     some elements, then the sequence's definition in SHADOW is
	     what we should use.  */
	  else
	    {
	      shmap = Flookup_key (shmap, Fcar (elt), Qt);
	      if (CHARP (shmap))
		shmap = Qnil;
	    }

	  if (!NILP (shmap))
	    {
	      Lisp_Object shm = get_keymap (shmap, 0, 1);
	      /* If shmap is not nil and not a keymap, it completely
		 shadows this map, so don't describe this map at all.  */
	      if (!KEYMAPP (shm))
		goto SKIP;
	      sub_shadow = Fcons (shm, sub_shadow);
	    }
	}

      {
        /* Describe the contents of map MAP, assuming that this map
           itself is reached by the sequence of prefix keys KEYS (a vector).
           PARTIAL and SHADOW are as in `describe_map_tree'.  */
        Lisp_Object keysdesc
          = ((!no_prefix)
             ? concat2 (Fkey_description (Fcar (elt)), Vsingle_space_string)
             : Qnil);
        describe_map (Fcdr (elt), keysdesc,
                      describe_command,
                      partial,
                      sub_shadow,
                      mice_only_p);
      }
    SKIP:
      NUNGCPRO;
    }
  UNGCPRO;
}


static void
describe_command (Lisp_Object definition)
{
  /* This function can GC */
  Lisp_Object buffer;
  int keymapp = !NILP (Fkeymapp (definition));
  struct gcpro gcpro1, gcpro2;
  GCPRO2 (definition, buffer);

  XSETBUFFER (buffer, current_buffer);
  Findent_to (make_int (16), make_int (3), buffer);
  if (keymapp)
    buffer_insert_c_string (XBUFFER (buffer), "<< ");

  if (SYMBOLP (definition))
    {
      buffer_insert1 (XBUFFER (buffer), Fsymbol_name (definition));
    }
  else if (STRINGP (definition) || VECTORP (definition))
    {
      buffer_insert_c_string (XBUFFER (buffer), "Kbd Macro: ");
      buffer_insert1 (XBUFFER (buffer), Fkey_description (definition));
    }
  else if (COMPILED_FUNCTIONP (definition))
    buffer_insert_c_string (XBUFFER (buffer), "Anonymous Compiled Function");
  else if (CONSP (definition) && EQ (XCAR (definition), Qlambda))
    buffer_insert_c_string (XBUFFER (buffer), "Anonymous Lambda");
  else if (KEYMAPP (definition))
    {
      Lisp_Object name = XKEYMAP (definition)->name;
      if (STRINGP (name) || (SYMBOLP (name) && !NILP (name)))
	{
	  buffer_insert_c_string (XBUFFER (buffer), "Prefix command ");
	  if (SYMBOLP (name)
	      && EQ (find_symbol_value (name), definition))
	    buffer_insert1 (XBUFFER (buffer), Fsymbol_name (name));
	  else
	    {
	      buffer_insert1 (XBUFFER (buffer), Fprin1_to_string (name, Qnil));
	    }
	}
      else
	buffer_insert_c_string (XBUFFER (buffer), "Prefix Command");
    }
  else
    buffer_insert_c_string (XBUFFER (buffer), "??");

  if (keymapp)
    buffer_insert_c_string (XBUFFER (buffer), " >>");
  buffer_insert_c_string (XBUFFER (buffer), "\n");
  UNGCPRO;
}

struct describe_map_closure
  {
    Lisp_Object *list;	 /* pointer to the list to update */
    Lisp_Object partial; /* whether to ignore suppressed commands */
    Lisp_Object shadow;	 /* list of maps shadowing this one */
    Lisp_Object self;	 /* this map */
    Lisp_Object self_root; /* this map, or some map that has this map as
                              a parent.  this is the base of the tree */
    int mice_only_p;	 /* whether we are to display only button bindings */
  };

struct describe_map_shadow_closure
  {
    CONST struct key_data *raw_key;
    Lisp_Object self;
  };

static Lisp_Object
describe_map_mapper_shadow_search (Lisp_Object map, void *arg)
{
  struct describe_map_shadow_closure *c =
    (struct describe_map_shadow_closure *) arg;

  if (EQ (map, c->self))
    return Qzero;		/* Not shadowed; terminate search */

  return (!NILP (keymap_lookup_directly (map,
					 c->raw_key->keysym,
					 c->raw_key->modifiers)))
    ? Qt : Qnil;
}


static Lisp_Object
keymap_lookup_inherited_mapper (Lisp_Object km, void *arg)
{
  struct key_data *k = (struct key_data *) arg;
  return keymap_lookup_directly (km, k->keysym, k->modifiers);
}


static void
describe_map_mapper (CONST struct key_data *key,
                     Lisp_Object binding,
		     void *describe_map_closure)
{
  /* This function can GC */
  struct describe_map_closure *closure =
    (struct describe_map_closure *) describe_map_closure;
  Lisp_Object keysym = key->keysym;
  unsigned int modifiers = key->modifiers;

  /* Dont mention suppressed commands.  */
  if (SYMBOLP (binding)
      && !NILP (closure->partial)
      && !NILP (Fget (binding, closure->partial, Qnil)))
    return;

  /* If we're only supposed to display mouse bindings and this isn't one,
     then bug out. */
  if (closure->mice_only_p &&
      (! (EQ (keysym, Qbutton0) ||
	  EQ (keysym, Qbutton1) ||
	  EQ (keysym, Qbutton2) ||
	  EQ (keysym, Qbutton3) ||
	  EQ (keysym, Qbutton4) ||
	  EQ (keysym, Qbutton5) ||
	  EQ (keysym, Qbutton6) ||
	  EQ (keysym, Qbutton7) ||
	  EQ (keysym, Qbutton0up) ||
	  EQ (keysym, Qbutton1up) ||
	  EQ (keysym, Qbutton2up) ||
	  EQ (keysym, Qbutton3up) ||
	  EQ (keysym, Qbutton4up) ||
	  EQ (keysym, Qbutton5up) ||
	  EQ (keysym, Qbutton6up) ||
	  EQ (keysym, Qbutton7up))))
    return;

  /* If this command in this map is shadowed by some other map, ignore it. */
  {
    Lisp_Object tail;

    for (tail = closure->shadow; CONSP (tail); tail = XCDR (tail))
      {
	QUIT;
	if (!NILP (traverse_keymaps (XCAR (tail), Qnil,
				     keymap_lookup_inherited_mapper,
				     /* Cast to discard `const' */
				     (void *)key)))
	  return;
      }
  }

  /* If this key is in some map of which this map is a parent, then ignore
     it (in that case, it has been shadowed).
     */
  {
    Lisp_Object sh;
    struct describe_map_shadow_closure c;
    c.raw_key = key;
    c.self = closure->self;

    sh = traverse_keymaps (closure->self_root, Qnil,
                           describe_map_mapper_shadow_search, &c);
    if (!NILP (sh) && !ZEROP (sh))
      return;
  }

  /* Otherwise add it to the list to be sorted. */
  *(closure->list) = Fcons (Fcons (Fcons (keysym, make_int (modifiers)),
                                   binding),
			    *(closure->list));
}


static int
describe_map_sort_predicate (Lisp_Object obj1, Lisp_Object obj2,
			     Lisp_Object pred)
{
  /* obj1 and obj2 are conses of the form
     ( ( <keysym> . <modifiers> ) . <binding> )
     keysym and modifiers are used, binding is ignored.
   */
  unsigned int bit1, bit2;
  obj1 = XCAR (obj1);
  obj2 = XCAR (obj2);
  bit1 = XINT (XCDR (obj1));
  bit2 = XINT (XCDR (obj2));
  if (bit1 != bit2)
    return bit1 < bit2 ? 1 : -1;
  else
    return map_keymap_sort_predicate (obj1, obj2, pred);
}

/* Elide 2 or more consecutive numeric keysyms bound to the same thing,
   or 2 or more symbolic keysyms that are bound to the same thing and
   have consecutive character-set-properties.
 */
static int
elide_next_two_p (Lisp_Object list)
{
  Lisp_Object s1, s2;

  if (NILP (XCDR (list)))
    return 0;

  /* next two bindings differ */
  if (!EQ (XCDR (XCAR (list)),
	   XCDR (XCAR (XCDR (list)))))
    return 0;

  /* next two modifier-sets differ */
  if (!EQ (XCDR (XCAR (XCAR (list))),
	   XCDR (XCAR (XCAR (XCDR (list))))))
    return 0;

  s1 = XCAR (XCAR (XCAR (list)));
  s2 = XCAR (XCAR (XCAR (XCDR (list))));

  if (SYMBOLP (s1))
    {
      Lisp_Object code = Fget (s1, Vcharacter_set_property, Qnil);
      if (CHAR_OR_CHAR_INTP (code))
	{
	  s1 = code;
	  CHECK_CHAR_COERCE_INT (s1);
	}
      else return 0;
    }
  if (SYMBOLP (s2))
    {
      Lisp_Object code = Fget (s2, Vcharacter_set_property, Qnil);
      if (CHAR_OR_CHAR_INTP (code))
	{
	  s2 = code;
	  CHECK_CHAR_COERCE_INT (s2);
	}
      else return 0;
    }

  return (XCHAR (s1)     == XCHAR (s2) ||
	  XCHAR (s1) + 1 == XCHAR (s2));
}


static Lisp_Object
describe_map_parent_mapper (Lisp_Object keymap, void *arg)
{
  /* This function can GC */
  struct describe_map_closure *describe_map_closure =
    (struct describe_map_closure *) arg;
  describe_map_closure->self = keymap;
  map_keymap (XKEYMAP (keymap)->table,
              0, /* don't sort: we'll do it later */
              describe_map_mapper, describe_map_closure);
  return Qnil;
}


/* Describe the contents of map MAP, assuming that this map itself is
   reached by the sequence of prefix keys KEYS (a string or vector).
   PARTIAL, SHADOW, NOMENU are as in `describe_map_tree' above.  */

static void
describe_map (Lisp_Object keymap, Lisp_Object elt_prefix,
	      void (*elt_describer) (Lisp_Object),
	      int partial,
	      Lisp_Object shadow,
	      int mice_only_p)
{
  /* This function can GC */
  struct describe_map_closure describe_map_closure;
  Lisp_Object list = Qnil;
  struct buffer *buf = current_buffer;
  Emchar printable_min = (CHAR_OR_CHAR_INTP (buf->ctl_arrow)
			  ? XCHAR_OR_CHAR_INT (buf->ctl_arrow)
			  : ((EQ (buf->ctl_arrow, Qt)
			      || EQ (buf->ctl_arrow, Qnil))
			     ? 256 : 160));
  int elided = 0;
  struct gcpro gcpro1, gcpro2, gcpro3, gcpro4;

  keymap = get_keymap (keymap, 1, 1);
  describe_map_closure.partial = (partial ? Qsuppress_keymap : Qnil);
  describe_map_closure.shadow = shadow;
  describe_map_closure.list = &list;
  describe_map_closure.self_root = keymap;
  describe_map_closure.mice_only_p = mice_only_p;

  GCPRO4 (keymap, elt_prefix, shadow, list);

  traverse_keymaps (keymap, Qnil,
                    describe_map_parent_mapper, &describe_map_closure);

  if (!NILP (list))
    {
      list = list_sort (list, Qnil, describe_map_sort_predicate);
      buffer_insert_c_string (buf, "\n");
      while (!NILP (list))
	{
          Lisp_Object elt = XCAR (XCAR (list));
	  Lisp_Object keysym = XCAR (elt);
	  unsigned int modifiers = XINT (XCDR (elt));

	  if (!NILP (elt_prefix))
	    buffer_insert_lisp_string (buf, elt_prefix);

	  if (modifiers & MOD_META)    buffer_insert_c_string (buf, "M-");
	  if (modifiers & MOD_CONTROL) buffer_insert_c_string (buf, "C-");
	  if (modifiers & MOD_SUPER)   buffer_insert_c_string (buf, "S-");
	  if (modifiers & MOD_HYPER)   buffer_insert_c_string (buf, "H-");
	  if (modifiers & MOD_ALT)     buffer_insert_c_string (buf, "Alt-");
	  if (modifiers & MOD_SHIFT)   buffer_insert_c_string (buf, "Sh-");
	  if (SYMBOLP (keysym))
	    {
	      Lisp_Object code = Fget (keysym, Vcharacter_set_property, Qnil);
	      Emchar c = (CHAR_OR_CHAR_INTP (code)
			  ? XCHAR_OR_CHAR_INT (code) : -1);
	      /* Calling Fsingle_key_description() would cons more */
#if 0                           /* This is bogus */
	      if (EQ (keysym, QKlinefeed))
		buffer_insert_c_string (buf, "LFD");
	      else if (EQ (keysym, QKtab))
		buffer_insert_c_string (buf, "TAB");
	      else if (EQ (keysym, QKreturn))
		buffer_insert_c_string (buf, "RET");
	      else if (EQ (keysym, QKescape))
		buffer_insert_c_string (buf, "ESC");
	      else if (EQ (keysym, QKdelete))
		buffer_insert_c_string (buf, "DEL");
	      else if (EQ (keysym, QKspace))
		buffer_insert_c_string (buf, "SPC");
	      else if (EQ (keysym, QKbackspace))
		buffer_insert_c_string (buf, "BS");
	      else
#endif
                if (c >= printable_min)
		  buffer_insert_emacs_char (buf, c);
		else buffer_insert1 (buf, Fsymbol_name (keysym));
	    }
	  else if (CHARP (keysym))
	    buffer_insert_emacs_char (buf, XCHAR (keysym));
	  else
	    buffer_insert_c_string (buf, "---bad keysym---");

	  if (elided)
	    elided = 0;
	  else
	    {
	      int k = 0;

	      while (elide_next_two_p (list))
		{
		  k++;
		  list = XCDR (list);
		}
	      if (k != 0)
		{
		  if (k == 1)
		    buffer_insert_c_string (buf, ", ");
		  else
		    buffer_insert_c_string (buf, " .. ");
		  elided = 1;
		  continue;
		}
	    }

	  /* Print a description of the definition of this character.  */
	  (*elt_describer) (XCDR (XCAR (list)));
	  list = XCDR (list);
	}
    }
  UNGCPRO;
}


void
syms_of_keymap (void)
{
  defsymbol (&Qminor_mode_map_alist, "minor-mode-map-alist");

  defsymbol (&Qkeymapp, "keymapp");

  defsymbol (&Qsuppress_keymap, "suppress-keymap");

  defsymbol (&Qmodeline_map, "modeline-map");
  defsymbol (&Qtoolbar_map, "toolbar-map");

  DEFSUBR (Fkeymap_parents);
  DEFSUBR (Fset_keymap_parents);
  DEFSUBR (Fkeymap_name);
  DEFSUBR (Fset_keymap_name);
  DEFSUBR (Fkeymap_prompt);
  DEFSUBR (Fset_keymap_prompt);
  DEFSUBR (Fkeymap_default_binding);
  DEFSUBR (Fset_keymap_default_binding);

  DEFSUBR (Fkeymapp);
  DEFSUBR (Fmake_keymap);
  DEFSUBR (Fmake_sparse_keymap);

  DEFSUBR (Fcopy_keymap);
  DEFSUBR (Fkeymap_fullness);
  DEFSUBR (Fmap_keymap);
  DEFSUBR (Fevent_matches_key_specifier_p);
  DEFSUBR (Fdefine_key);
  DEFSUBR (Flookup_key);
  DEFSUBR (Fkey_binding);
  DEFSUBR (Fuse_global_map);
  DEFSUBR (Fuse_local_map);
  DEFSUBR (Fcurrent_local_map);
  DEFSUBR (Fcurrent_global_map);
  DEFSUBR (Fcurrent_keymaps);
  DEFSUBR (Faccessible_keymaps);
  DEFSUBR (Fkey_description);
  DEFSUBR (Fsingle_key_description);
  DEFSUBR (Fwhere_is_internal);
  DEFSUBR (Fdescribe_bindings_internal);

  DEFSUBR (Ftext_char_description);

  defsymbol (&Qcontrol, "control");
  defsymbol (&Qctrl, "ctrl");
  defsymbol (&Qmeta, "meta");
  defsymbol (&Qsuper, "super");
  defsymbol (&Qhyper, "hyper");
  defsymbol (&Qalt, "alt");
  defsymbol (&Qshift, "shift");
  defsymbol (&Qbutton0, "button0");
  defsymbol (&Qbutton1, "button1");
  defsymbol (&Qbutton2, "button2");
  defsymbol (&Qbutton3, "button3");
  defsymbol (&Qbutton4, "button4");
  defsymbol (&Qbutton5, "button5");
  defsymbol (&Qbutton6, "button6");
  defsymbol (&Qbutton7, "button7");
  defsymbol (&Qbutton0up, "button0up");
  defsymbol (&Qbutton1up, "button1up");
  defsymbol (&Qbutton2up, "button2up");
  defsymbol (&Qbutton3up, "button3up");
  defsymbol (&Qbutton4up, "button4up");
  defsymbol (&Qbutton5up, "button5up");
  defsymbol (&Qbutton6up, "button6up");
  defsymbol (&Qbutton7up, "button7up");
#ifdef HAVE_OFFIX_DND
  defsymbol (&Qdrop0, "drop0");
  defsymbol (&Qdrop1, "drop1");
  defsymbol (&Qdrop2, "drop2");
  defsymbol (&Qdrop3, "drop3");
  defsymbol (&Qdrop4, "drop4");
  defsymbol (&Qdrop5, "drop5");
  defsymbol (&Qdrop6, "drop6");
  defsymbol (&Qdrop7, "drop7");
#endif
  defsymbol (&Qmouse_1, "mouse-1");
  defsymbol (&Qmouse_2, "mouse-2");
  defsymbol (&Qmouse_3, "mouse-3");
  defsymbol (&Qdown_mouse_1, "down-mouse-1");
  defsymbol (&Qdown_mouse_2, "down-mouse-2");
  defsymbol (&Qdown_mouse_3, "down-mouse-3");
  defsymbol (&Qmenu_selection, "menu-selection");
  defsymbol (&QLFD, "LFD");
  defsymbol (&QTAB, "TAB");
  defsymbol (&QRET, "RET");
  defsymbol (&QESC, "ESC");
  defsymbol (&QDEL, "DEL");
  defsymbol (&QBS, "BS");
}

void
vars_of_keymap (void)
{
  DEFVAR_LISP ("meta-prefix-char", &Vmeta_prefix_char /*
Meta-prefix character.
This character followed by some character `foo' turns into `Meta-foo'.
This can be any form recognized as a single key specifier.
To disable the meta-prefix-char, set it to a negative number.
*/ );
  Vmeta_prefix_char = make_char (033);

  DEFVAR_LISP ("mouse-grabbed-buffer", &Vmouse_grabbed_buffer /*
A buffer which should be consulted first for all mouse activity.
When a mouse-click is processed, it will first be looked up in the
local-map of this buffer, and then through the normal mechanism if there
is no binding for that click.  This buffer's value of `mode-motion-hook'
will be consulted instead of the `mode-motion-hook' of the buffer of the
window under the mouse.  You should *bind* this, not set it.
*/ );
  Vmouse_grabbed_buffer = Qnil;

  DEFVAR_LISP ("overriding-local-map", &Voverriding_local_map /*
Keymap that overrides all other local keymaps.
If this variable is non-nil, it is used as a keymap instead of the
buffer's local map, and the minor mode keymaps and extent-local keymaps.
You should *bind* this, not set it.
*/ );
  Voverriding_local_map = Qnil;

  Fset (Qminor_mode_map_alist, Qnil);

  DEFVAR_LISP ("key-translation-map", &Vkey_translation_map /*
Keymap of key translations that can override keymaps.
This keymap works like `function-key-map', but comes after that,
and applies even for keys that have ordinary bindings.
*/ );

  DEFVAR_INT ("keymap-tick", &keymap_tick /*
Incremented for each change to any keymap.
*/ );
  keymap_tick = 0;

  staticpro (&Vcurrent_global_map);

  Vsingle_space_string = make_pure_string ((CONST Bufbyte *) " ", 1, Qnil, 1);
  staticpro (&Vsingle_space_string);
}

void
complex_vars_of_keymap (void)
{
  /* This function can GC */
  Lisp_Object ESC_prefix = intern ("ESC-prefix");
  Lisp_Object meta_disgustitute;

  Vcurrent_global_map = Fmake_keymap (Qnil);

  meta_disgustitute = Fmake_keymap (Qnil);
  Ffset (ESC_prefix, meta_disgustitute);
  /* no need to protect meta_disgustitute, though */
  keymap_store_internal (MAKE_MODIFIER_HASH_KEY (MOD_META),
                         XKEYMAP (Vcurrent_global_map),
                         meta_disgustitute);
  XKEYMAP (Vcurrent_global_map)->sub_maps_cache = Qt;

  Vkey_translation_map = Fmake_sparse_keymap (intern ("key-translation-map"));
}
