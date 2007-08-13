/* Lisp functions pertaining to editing.
   Copyright (C) 1985-1987, 1989, 1992-1995 Free Software Foundation, Inc.
   Copyright (C) 1995 Tinker Systems and INS Engineering Corp.
   Copyright (C) 1996 Ben Wing.

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

/* Synched up with: Mule 2.0, FSF 19.30. */

/* This file has been Mule-ized. */

/* Hacked on for Mule by Ben Wing, December 1994. */

#include <config.h>
#include "lisp.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "buffer.h"
#include "commands.h"
#include "events.h"             /* for EVENTP */
#include "extents.h"
#include "frame.h"
#include "insdel.h"
#include "window.h"
#include "line-number.h"

#include "systime.h"
#include "sysdep.h"
#include "syspwd.h"

/* Some static data, and a function to initialize it for each run */

Lisp_Object Vsystem_name;	/* #### - I don't see why this should be */
				/* static, either...  --Stig */
#if 0				/* XEmacs - this is now dynamic */
				/* if at some point it's deemed desirable to
				   use lisp variables here, then they can be
				   initialized to nil and then set to their
				   real values upon the first call to the
				   functions that generate them. --stig */
Lisp_Object Vuser_real_login_name; /* login name of current user ID */
Lisp_Object Vuser_login_name;	/* user name from LOGNAME or USER.  */
#endif

/* It's useful to be able to set this as user customization, so we'll
   keep it. */
Lisp_Object Vuser_full_name;
EXFUN (Fuser_full_name, 1);

char *get_system_name (void);

Lisp_Object Qformat;

Lisp_Object Qpoint, Qmark, Qregion_beginning, Qregion_end;

Lisp_Object Quser_files_and_directories;

/* This holds the value of `environ' produced by the previous
   call to Fset_time_zone_rule, or 0 if Fset_time_zone_rule
   has never been called.  */
static char **environbuf;

void
init_editfns (void)
{
/* Only used in removed code below. */
  char *p;

  environbuf = 0;

  /* Set up system_name even when dumping.  */
  init_system_name ();

#ifndef CANNOT_DUMP
  if (!initialized)
    return;
#endif

  if ((p = getenv ("NAME")))
    /* I don't think it's the right thing to do the ampersand
       modification on NAME.  Not that it matters anymore...  -hniksic */
    Vuser_full_name = build_ext_string (p, FORMAT_OS);
  else
    Vuser_full_name = Fuser_full_name (Qnil);
}

DEFUN ("char-to-string", Fchar_to_string, 1, 1, 0, /*
Convert arg CH to a one-character string containing that character.
*/
       (ch))
{
  Bytecount len;
  Bufbyte str[MAX_EMCHAR_LEN];

  if (EVENTP (ch))
    {
      Lisp_Object ch2 = Fevent_to_character (ch, Qt, Qnil, Qnil);
      if (NILP (ch2))
	return
	  signal_simple_continuable_error
	    ("character has no ASCII equivalent:", Fcopy_event (ch, Qnil));
      ch = ch2;
    }

  CHECK_CHAR_COERCE_INT (ch);

  len = set_charptr_emchar (str, XCHAR (ch));
  return make_string (str, len);
}

DEFUN ("string-to-char", Fstring_to_char, 1, 1, 0, /*
Convert arg STRING to a character, the first character of that string.
An empty string will return the constant `nil'.
*/
       (str))
{
  struct Lisp_String *p;
  CHECK_STRING (str);

  p = XSTRING (str);
  if (string_length (p) != 0)
    return make_char (string_char (p, 0));
  else
    /* This used to return Qzero.  That is broken, broken, broken. */
    /* It might be kinder to signal an error directly. -slb */
    return Qnil;
}


static Lisp_Object
buildmark (Bufpos val, Lisp_Object buffer)
{
  Lisp_Object mark = Fmake_marker ();
  Fset_marker (mark, make_int (val), buffer);
  return mark;
}

DEFUN ("point", Fpoint, 0, 1, 0, /*
Return value of point, as an integer.
Beginning of buffer is position (point-min).
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  return make_int (BUF_PT (b));
}

DEFUN ("point-marker", Fpoint_marker, 0, 2, 0, /*
Return value of point, as a marker object.
This marker is a copy; you may modify it with reckless abandon.
If optional argument DONT-COPY-P is non-nil, then it returns the real
point-marker; modifying the position of this marker will move point.
It is illegal to change the buffer of it, or make it point nowhere.
If BUFFER is nil, the current buffer is assumed.
*/
       (dont_copy_p, buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  if (NILP (dont_copy_p))
    return Fcopy_marker (b->point_marker, Qnil);
  else
    return b->point_marker;
}

/* The following two functions end up being identical but it's
   cleaner to declare them separately. */

Bufpos
bufpos_clip_to_bounds (Bufpos lower, Bufpos num, Bufpos upper)
{
  return (num < lower ? lower :
	  num > upper ? upper :
	  num);
}

Bytind
bytind_clip_to_bounds (Bytind lower, Bytind num, Bytind upper)
{
  return (num < lower ? lower :
	  num > upper ? upper :
	  num);
}

/*
 * Chuck says:
 * There is no absolute way to determine if goto-char is the function
 * being run.  this-command doesn't work because it is often eval'd
 * and this-command ends up set to eval-expression.  So this flag gets
 * added for now.
 *
 * Jamie thinks he's wrong, but we'll leave this in for now.
 */
int atomic_extent_goto_char_p;

DEFUN ("goto-char", Fgoto_char, 1, 2, "NGoto char: ", /*
Set point to POSITION, a number or marker.
Beginning of buffer is position (point-min), end is (point-max).
If BUFFER is nil, the current buffer is assumed.
Return value of POSITION, as an integer.
*/
       (position, buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  Bufpos n = get_buffer_pos_char (b, position, GB_COERCE_RANGE);
  BUF_SET_PT (b, n);
  atomic_extent_goto_char_p = 1;
  return make_int (n);
}

static Lisp_Object
region_limit (int beginningp, struct buffer *b)
{
  Lisp_Object m;

#if 0 /* FSFmacs */
  if (!NILP (Vtransient_mark_mode) && NILP (Vmark_even_if_inactive)
      && NILP (b->mark_active))
    Fsignal (Qmark_inactive, Qnil);
#endif
  m = Fmarker_position (b->mark);
  if (NILP (m)) error ("There is no region now");
  if (!!(BUF_PT (b) < XINT (m)) == !!beginningp)
    return make_int (BUF_PT (b));
  else
    return m;
}

DEFUN ("region-beginning", Fregion_beginning, 0, 1, 0, /*
Return position of beginning of region in BUFFER, as an integer.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  return region_limit (1, decode_buffer (buffer, 1));
}

DEFUN ("region-end", Fregion_end, 0, 1, 0, /*
Return position of end of region in BUFFER, as an integer.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  return region_limit (0, decode_buffer (buffer, 1));
}

/* Whether to use lispm-style active-regions */
int zmacs_regions;

/* Whether the zmacs region is active.  This is not per-buffer because
   there can be only one active region at a time.  #### Now that the
   zmacs region are not directly tied to the X selections this may not
   necessarily have to be true.  */
int zmacs_region_active_p;

int zmacs_region_stays;

Lisp_Object Qzmacs_update_region, Qzmacs_deactivate_region;
Lisp_Object Qzmacs_region_buffer;

void
zmacs_update_region (void)
{
  /* This function can GC */
  if (zmacs_region_active_p)
    call0 (Qzmacs_update_region);
}

void
zmacs_deactivate_region (void)
{
  /* This function can GC */
  if (zmacs_region_active_p)
    call0 (Qzmacs_deactivate_region);
}

Lisp_Object
zmacs_region_buffer (void)
{
  if (zmacs_region_active_p)
    return call0 (Qzmacs_region_buffer);
  else
    return Qnil;
}

DEFUN ("mark-marker", Fmark_marker, 0, 2, 0, /*
Return this buffer's mark, as a marker object.
If `zmacs-regions' is true, then this returns nil unless the region is
currently in the active (highlighted) state.  If optional argument FORCE
is t, this returns the mark (if there is one) regardless of the zmacs-region
state.  You should *generally* not use the mark unless the region is active,
if the user has expressed a preference for the zmacs-region model.
Watch out!  Moving this marker changes the mark position.
If you set the marker not to point anywhere, the buffer will have no mark.
If BUFFER is nil, the current buffer is assumed.
*/
       (force, buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  if (! zmacs_regions || zmacs_region_active_p || !NILP (force))
    return b->mark;
  return Qnil;
}


/* The saved object is a cons:

   (COPY-OF-POINT-MARKER . COPY-OF-MARK)

   We used to have another cons for a VISIBLE-P element, which was t
   if `(eq (current-buffer) (window-buffer (selected-window)))' but it
   was unused for a long time, so I removed it.  --hniksic */
Lisp_Object
save_excursion_save (void)
{
  struct buffer *b;

  /* #### Huh?  --hniksic */
  /*if (preparing_for_armageddon) return Qnil;*/

#ifdef ERROR_CHECK_BUFPOS
  assert (XINT (Fpoint (Qnil)) ==
	  XINT (Fmarker_position (Fpoint_marker (Qt, Qnil))));
#endif

  b = current_buffer;

  return noseeum_cons (noseeum_copy_marker (b->point_marker, Qnil),
		       noseeum_copy_marker (b->mark, Qnil));
}

Lisp_Object
save_excursion_restore (Lisp_Object info)
{
  Lisp_Object buffer = Fmarker_buffer (XCAR (info));

  /* If buffer being returned to is now deleted, avoid error --
     otherwise could get error here while unwinding to top level and
     crash.  In that case, Fmarker_buffer returns nil now.  */
  if (!NILP (buffer))
    {
      struct buffer *buf = XBUFFER (buffer);
      struct gcpro gcpro1;
      GCPRO1 (info);
      set_buffer_internal (buf);
      Fgoto_char (XCAR (info), buffer);
      Fset_marker (buf->mark, XCDR (info), buffer);

#if 0 /* We used to make the current buffer visible in the selected window
	 if that was true previously.  That avoids some anomalies.
	 But it creates others, and it wasn't documented, and it is simpler
	 and cleaner never to alter the window/buffer connections.  */
      /* I'm certain some code somewhere depends on this behavior. --jwz */
      /* Even if it did, it certainly doesn't matter anymore, because
         this has been the behaviour for countless XEmacs releases
         now.  --hniksic */
      if (visible
	  && (current_buffer != XBUFFER (XWINDOW (selected_window)->buffer)))
	switch_to_buffer (Fcurrent_buffer (), Qnil);
#endif

      UNGCPRO;
    }

  /* Free all the junk we allocated, so that a `save-excursion' comes
     for free in terms of GC junk. */
  free_marker (XMARKER (XCAR (info)));
  free_marker (XMARKER (XCDR (info)));
  free_cons (XCONS (info));
  return Qnil;
}

DEFUN ("save-excursion", Fsave_excursion, 0, UNEVALLED, 0, /*
Save point, mark, and current buffer; execute BODY; restore those things.
Executes BODY just like `progn'.
The values of point, mark and the current buffer are restored
even in case of abnormal exit (throw or error).
*/
       (args))
{
  /* This function can GC */
  int speccount = specpdl_depth ();

  record_unwind_protect (save_excursion_restore, save_excursion_save ());

  return unbind_to (speccount, Fprogn (args));
}

Lisp_Object
save_current_buffer_restore (Lisp_Object buffer)
{
  struct buffer *buf = XBUFFER (buffer);
  /* Avoid signaling an error if the buffer is no longer alive.  This
     is for consistency with save-excursion.  */
  if (BUFFER_LIVE_P (buf))
    set_buffer_internal (buf);
  return Qnil;
}

DEFUN ("save-current-buffer", Fsave_current_buffer, 0, UNEVALLED, 0, /*
Save the current buffer; execute BODY; restore the current buffer.
Executes BODY just like `progn'.
*/
       (args))
{
  /* This function can GC */
  int speccount = specpdl_depth ();

  record_unwind_protect (save_current_buffer_restore, Fcurrent_buffer ());

  return unbind_to (speccount, Fprogn (args));
}

DEFUN ("buffer-size", Fbuffer_size, 0, 1, 0, /*
Return the number of characters in BUFFER.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  return make_int (BUF_SIZE (b));
}

DEFUN ("point-min", Fpoint_min, 0, 1, 0, /*
Return the minimum permissible value of point in BUFFER.
This is 1, unless narrowing (a buffer restriction) is in effect.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  return make_int (BUF_BEGV (b));
}

DEFUN ("point-min-marker", Fpoint_min_marker, 0, 1, 0, /*
Return a marker to the minimum permissible value of point in BUFFER.
This is the beginning, unless narrowing (a buffer restriction) is in effect.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  return buildmark (BUF_BEGV (b), make_buffer (b));
}

DEFUN ("point-max", Fpoint_max, 0, 1, 0, /*
Return the maximum permissible value of point in BUFFER.
This is (1+ (buffer-size)), unless narrowing (a buffer restriction)
is in effect, in which case it is less.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  return make_int (BUF_ZV (b));
}

DEFUN ("point-max-marker", Fpoint_max_marker, 0, 1, 0, /*
Return a marker to the maximum permissible value of point BUFFER.
This is (1+ (buffer-size)), unless narrowing (a buffer restriction)
is in effect, in which case it is less.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  return buildmark (BUF_ZV (b), make_buffer (b));
}

DEFUN ("following-char", Ffollowing_char, 0, 1, 0, /*
Return the character following point.
At the end of the buffer or accessible region, return 0.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  if (BUF_PT (b) >= BUF_ZV (b))
    return Qzero;             /* #### Gag me! */
  else
    return make_char (BUF_FETCH_CHAR (b, BUF_PT (b)));
}

DEFUN ("preceding-char", Fpreceding_char, 0, 1, 0, /*
Return the character preceding point.
At the beginning of the buffer or accessible region, return 0.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  if (BUF_PT (b) <= BUF_BEGV (b))
    return Qzero;             /* #### Gag me! */
  else
    return make_char (BUF_FETCH_CHAR (b, BUF_PT (b) - 1));
}

DEFUN ("bobp", Fbobp, 0, 1, 0, /*
Return t if point is at the beginning of the buffer.
If the buffer is narrowed, this means the beginning of the narrowed part.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  return BUF_PT (b) == BUF_BEGV (b) ? Qt : Qnil;
}

DEFUN ("eobp", Feobp, 0, 1, 0, /*
Return t if point is at the end of the buffer.
If the buffer is narrowed, this means the end of the narrowed part.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  return BUF_PT (b) == BUF_ZV (b) ? Qt : Qnil;
}

int
beginning_of_line_p (struct buffer *b, Bufpos pt)
{
  return pt <= BUF_BEGV (b) || BUF_FETCH_CHAR (b, pt - 1) == '\n';
}


DEFUN ("bolp", Fbolp, 0, 1, 0, /*
Return t if point is at the beginning of a line.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);

  return beginning_of_line_p (b, BUF_PT (b)) ? Qt : Qnil;
}

DEFUN ("eolp", Feolp, 0, 1, 0, /*
Return t if point is at the end of a line.
`End of a line' includes point being at the end of the buffer.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  return (BUF_PT (b) == BUF_ZV (b) || BUF_FETCH_CHAR (b, BUF_PT (b)) == '\n')
    ? Qt : Qnil;
}

DEFUN ("char-after", Fchar_after, 0, 2, 0, /*
Return character in BUFFER at position POS.
POS is an integer or a buffer pointer.
If POS is out of range, the value is nil.
If BUFFER is nil, the current buffer is assumed.
if POS is nil, the value of point is assumed.
*/
       (pos, buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  Bufpos n = (NILP (pos) ? BUF_PT (b) :
	      get_buffer_pos_char (b, pos, GB_NO_ERROR_IF_BAD));

  if (n < 0 || n == BUF_ZV (b))
    return Qnil;
  return make_char (BUF_FETCH_CHAR (b, n));
}

DEFUN ("char-before", Fchar_before, 0, 2, 0, /*
Return character in BUFFER before position POS.
POS is an integer or a buffer pointer.
If POS is out of range, the value is nil.
If BUFFER is nil, the current buffer is assumed.
if POS is nil, the value of point is assumed.
*/
       (pos, buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  Bufpos n = ((NILP (pos) ? BUF_PT (b) :
	       get_buffer_pos_char (b, pos, GB_NO_ERROR_IF_BAD)));

  n--;

  if (n < BUF_BEGV (b))
    return Qnil;
  return make_char (BUF_FETCH_CHAR (b, n));
}


DEFUN ("temp-directory", Ftemp_directory, 0, 0, 0, /*
Return the pathname to the directory to use for temporary files.
On NT/MSDOS, this is obtained from the TEMP or TMP environment variables,
defaulting to / if they are both undefined.
On Unix it is obtained from TMPDIR, with /tmp as the default
*/
       ())
{
  char *tmpdir;
#if defined(WINDOWSNT) || defined(MSDOS)
  tmpdir = getenv ("TEMP");
  if (!tmpdir)
    tmpdir = getenv ("TMP");
  if (!tmpdir)
    tmpdir = "/";
#else /* WINDOWSNT || MSDOS */
 tmpdir = getenv ("TMPDIR");
 if (!tmpdir)
   tmpdir = "/tmp";
#endif

  return build_ext_string (tmpdir, FORMAT_FILENAME);
}

DEFUN ("user-login-name", Fuser_login_name, 0, 1, 0, /*
Return the name under which the user logged in, as a string.
This is based on the effective uid, not the real uid.
Also, if the environment variable LOGNAME or USER is set,
that determines the value of this function.
If the optional argument UID is present, then environment variables are
ignored and this function returns the login name for that UID, or nil.
*/
       (uid))
{
  char *returned_name;
  int local_uid;

  if (!NILP (uid))
    {
      CHECK_INT (uid);
      local_uid = XINT(uid);
      returned_name = user_login_name(&local_uid);
    }
  else
    {
      returned_name = user_login_name(NULL);
    }
  /* #### - I believe this should return nil instead of "unknown" when pw==0
     pw=0 is indicated by a null return from user_login_name
  */
  return returned_name ? build_string (returned_name) : Qnil;
}

/* This function may be called from other C routines when a
   character string representation of the user_login_name is
   needed but a Lisp Object is not.  The UID is passed by
   reference.  If UID == NULL, then the USER name
   for the user running XEmacs will be returned.  This
   corresponds to a nil argument to Fuser_login_name.
*/
char*
user_login_name (int *uid)
{
  struct passwd *pw = NULL;
  
  /* uid == NULL to return name of this user */
  if (uid != NULL)
    {
      pw = getpwuid (*uid);
      return pw ? pw->pw_name : NULL;
    }
  else
    {
      /* #### - when euid != uid, then LOGNAME and USER are leftovers from the
	 old environment (I site observed behavior on sunos and linux), so the
	 environment variables should be disregarded in that case.  --Stig */
      char *user_name = getenv ("LOGNAME");
      if (!user_name)
	user_name = getenv (
#ifdef WINDOWSNT
			    "USERNAME" /* it's USERNAME on NT */
#else
			    "USER"
#endif
			    );
      if (user_name)
	return (user_name);
      else
	{
	  pw = getpwuid (geteuid ());
#ifdef __CYGWIN32__
	  /* Since the Cygwin environment may not have an /etc/passwd,
	     return "unknown" instead of the null if the username
	     cannot be determined.
	  */
	  return pw ? pw->pw_name : "unknown";
#else
	  /* For all but Cygwin return NULL (nil) */
	  return pw ? pw->pw_name : NULL;
#endif
	}
    }
}

DEFUN ("user-real-login-name", Fuser_real_login_name, 0, 0, 0, /*
Return the name of the user's real uid, as a string.
This ignores the environment variables LOGNAME and USER, so it differs from
`user-login-name' when running under `su'.
*/
       ())
{
  struct passwd *pw = getpwuid (getuid ());
  /* #### - I believe this should return nil instead of "unknown" when pw==0 */

#ifdef MSDOS
  /* We let the real user name default to "root" because that's quite
     accurate on MSDOG and because it lets Emacs find the init file.
     (The DVX libraries override the Djgpp libraries here.)  */
  Lisp_Object tem = build_string (pw ? pw->pw_name : "root");/* no gettext */
#else
  Lisp_Object tem = build_string (pw ? pw->pw_name : "unknown");/* no gettext */
#endif
  return tem;
}

DEFUN ("user-uid", Fuser_uid, 0, 0, 0, /*
Return the effective uid of Emacs, as an integer.
*/
       ())
{
  return make_int (geteuid ());
}

DEFUN ("user-real-uid", Fuser_real_uid, 0, 0, 0, /*
Return the real uid of Emacs, as an integer.
*/
       ())
{
  return make_int (getuid ());
}

DEFUN ("user-full-name", Fuser_full_name, 0, 1, 0, /*
Return the full name of the user logged in, as a string.
If the optional argument USER is given, then the full name for that
user is returned, or nil.  USER may be either a login name or a uid.

If USER is nil, and `user-full-name' contains a string, the
value of `user-full-name' is returned.
*/
       (user))
{
  Lisp_Object user_name;
  struct passwd *pw = NULL;
  Lisp_Object tem;
  char *p, *q;

  if (NILP (user) && STRINGP (Vuser_full_name))
    return Vuser_full_name;

  user_name = (STRINGP (user) ? user : Fuser_login_name (user));
  if (!NILP (user_name))	/* nil when nonexistent UID passed as arg */
    {
      CONST char *user_name_ext;

      /* Fuck me.  getpwnam() can call select() and (under IRIX at least)
	 things get wedged if a SIGIO arrives during this time. */
      GET_C_STRING_OS_DATA_ALLOCA (user_name, user_name_ext);
      slow_down_interrupts ();
      pw = (struct passwd *) getpwnam (user_name_ext);
      speed_up_interrupts ();
    }

  /* #### - Stig sez: this should return nil instead of "unknown" when pw==0 */
  /* Ben sez: bad idea because it's likely to break something */
#ifndef AMPERSAND_FULL_NAME
  p = ((pw) ? USER_FULL_NAME : "unknown"); /* don't gettext */
  q = strchr (p, ',');
#else
  p = ((pw) ? USER_FULL_NAME : "unknown"); /* don't gettext */
  q = strchr (p, ',');
#endif
  tem = ((!NILP (user) && !pw)
	 ? Qnil
	 : make_ext_string ((Extbyte *) p, (q ? q - p : strlen (p)),
			    FORMAT_OS));

#ifdef AMPERSAND_FULL_NAME
  if (!NILP (tem))
    {
      p = (char *) XSTRING_DATA (tem);
      q = strchr (p, '&');
      /* Substitute the login name for the &, upcasing the first character.  */
      if (q)
	{
	  char *r = (char *) alloca (strlen (p) + XSTRING_LENGTH (user_name) + 1);
	  memcpy (r, p, q - p);
	  r[q - p] = 0;
	  strcat (r, (char *) XSTRING_DATA (user_name));
	  /* #### current_buffer dependency! */
	  r[q - p] = UPCASE (current_buffer, r[q - p]);
	  strcat (r, q + 1);
	  tem = build_string (r);
	}
    }
#endif /* AMPERSAND_FULL_NAME */

  return tem;
}

static char *cached_home_directory;

void
uncache_home_directory (void)
{
  cached_home_directory = NULL;	/* in some cases, this may cause the leaking
				   of a few bytes */
}

char *
get_home_directory (void)
{
  int output_home_warning = 0;

  if (cached_home_directory == NULL)
    {
      if ((cached_home_directory = getenv("HOME")) == NULL)
	{
#if defined(WINDOWSNT) && !defined(__CYGWIN32__)
	  char	*homedrive, *homepath;
 
	  if ((homedrive = getenv("HOMEDRIVE")) != NULL &&
	      (homepath = getenv("HOMEPATH")) != NULL)
	    {
	      cached_home_directory =
		(char *) xmalloc(strlen(homedrive) + strlen(homepath) + 1);
	      sprintf(cached_home_directory, "%s%s", homedrive, homepath);
	    }
	  else
	    {
# if 1
	      /*
	       * Use the current directory.
	       * This preserves the existing XEmacs behavior, but is different
	       * from NT Emacs.
	       */
	      if (initial_directory[0] != '\0')
		{
		  cached_home_directory = initial_directory;
		}
	      else
		{
		  /* This will probably give the wrong value */
		  cached_home_directory = getcwd (NULL, 0);
		}
# else
	      /*
	       * This is NT Emacs behavior
	       */
	      cached_home_directory = "C:\\";
	      output_home_warning = 1;
# endif
	    }
#else	/* !WINDOWSNT */
	  /*
	   * Unix, typically.
	   * Using "/" isn't quite right, but what should we do?
	   * We probably should try to extract pw_dir from /etc/passwd,
	   * before falling back to this.
	   */
	  cached_home_directory = "/";
	  output_home_warning = 1;
#endif	/* !WINDOWSNT */
	}
      if (initialized && output_home_warning)
	{
	  warn_when_safe(Quser_files_and_directories, Qwarning, "\n"
"	XEmacs was unable to determine a good value for the user's $HOME\n"
"	directory, and will be using the value:\n"
"		%s\n"
"	This is probably incorrect.",
			 cached_home_directory
			 );
	}
    }
  return (cached_home_directory);
}

DEFUN ("user-home-directory", Fuser_home_directory, 0, 0, 0, /*
Return the user's home directory, as a string.
*/
       ())
{
  Lisp_Object directory;
  char *path;

  directory = Qnil;
  path = get_home_directory ();
  if (path != NULL)
    {
      directory =
	Fexpand_file_name (Fsubstitute_in_file_name (build_string (path)),
			   Qnil);
    }
  return (directory);
}

DEFUN ("system-name", Fsystem_name, 0, 0, 0, /*
Return the name of the machine you are running on, as a string.
*/
       ())
{
    return Fcopy_sequence (Vsystem_name);
}

/* For the benefit of callers who don't want to include lisp.h.
   Caller must free! */
char *
get_system_name (void)
{
  return xstrdup ((char *) XSTRING_DATA (Vsystem_name));
}

DEFUN ("emacs-pid", Femacs_pid, 0, 0, 0, /*
Return the process ID of Emacs, as an integer.
*/
       ())
{
  return make_int (getpid ());
}

DEFUN ("current-time", Fcurrent_time, 0, 0, 0, /*
Return the current time, as the number of seconds since 1970-01-01 00:00:00.
The time is returned as a list of three integers.  The first has the
most significant 16 bits of the seconds, while the second has the
least significant 16 bits.  The third integer gives the microsecond
count.

The microsecond count is zero on systems that do not provide
resolution finer than a second.
*/
       ())
{
  EMACS_TIME t;

  EMACS_GET_TIME (t);
  return list3 (make_int ((EMACS_SECS (t) >> 16) & 0xffff),
		make_int ((EMACS_SECS (t) >> 0)  & 0xffff),
		make_int (EMACS_USECS (t)));
}

DEFUN ("current-process-time", Fcurrent_process_time, 0, 0, 0, /*
Return the amount of time used by this XEmacs process so far.
The return value is a list of three floating-point numbers, expressing
the user, system, and real times used by the process.  The user time
measures the time actually spent by the CPU executing the code in this
process.  The system time measures time spent by the CPU executing kernel
code on behalf of this process (e.g. I/O requests made by the process).

Note that the user and system times measure processor time, as opposed
to real time, and only accrue when the processor is actually doing
something: Time spent in an idle wait (waiting for user events to come
in or for I/O on a disk drive or other device to complete) does not
count.  Thus, the user and system times will often be considerably
less than the real time.

Some systems do not allow the user and system times to be distinguished.
In this case, the user time will be the total processor time used by
the process, and the system time will be 0.

Some systems do not allow the real and processor times to be distinguished.
In this case, the user and real times will be the same and the system
time will be 0.
*/
       ())
{
  double user, sys, real;

  get_process_times (&user, &sys, &real);
  return list3 (make_float (user), make_float (sys), make_float (real));
}


int lisp_to_time (Lisp_Object specified_time, time_t *result);
int
lisp_to_time (Lisp_Object specified_time, time_t *result)
{
  Lisp_Object high, low;

  if (NILP (specified_time))
    return time (result) != -1;

  CHECK_CONS (specified_time);
  high = XCAR (specified_time);
  low  = XCDR (specified_time);
  if (CONSP (low))
    low = XCAR (low);
  CHECK_INT (high);
  CHECK_INT (low);
  *result = (XINT (high) << 16) + (XINT (low) & 0xffff);
  return *result >> 16 == XINT (high);
}

Lisp_Object time_to_lisp (time_t the_time);
Lisp_Object
time_to_lisp (time_t the_time)
{
  unsigned int item = (unsigned int) the_time;
  return Fcons (make_int (item >> 16), make_int (item & 0xffff));
}

size_t emacs_strftime (char *string, size_t max, CONST char *format,
		       CONST struct tm *tm);
static long difftm (CONST struct tm *a, CONST struct tm *b);


DEFUN ("format-time-string", Fformat_time_string, 1, 2, 0, /*
Use FORMAT-STRING to format the time TIME.
TIME is specified as (HIGH LOW . IGNORED) or (HIGH . LOW), as from
`current-time' and `file-attributes'.  If TIME is not specified it
defaults to the current time.
FORMAT-STRING may contain %-sequences to substitute parts of the time.
%a is replaced by the abbreviated name of the day of week.
%A is replaced by the full name of the day of week.
%b is replaced by the abbreviated name of the month.
%B is replaced by the full name of the month.
%c is a synonym for "%x %X".
%C is a locale-specific synonym, which defaults to "%A, %B %e, %Y" in the C locale.
%d is replaced by the day of month, zero-padded.
%D is a synonym for "%m/%d/%y".
%e is replaced by the day of month, blank-padded.
%h is a synonym for "%b".
%H is replaced by the hour (00-23).
%I is replaced by the hour (00-12).
%j is replaced by the day of the year (001-366).
%k is replaced by the hour (0-23), blank padded.
%l is replaced by the hour (1-12), blank padded.
%m is replaced by the month (01-12).
%M is replaced by the minute (00-59).
%n is a synonym for "\\n".
%p is replaced by AM or PM, as appropriate.
%r is a synonym for "%I:%M:%S %p".
%R is a synonym for "%H:%M".
%S is replaced by the second (00-60).
%t is a synonym for "\\t".
%T is a synonym for "%H:%M:%S".
%U is replaced by the week of the year (00-53), first day of week is Sunday.
%w is replaced by the day of week (0-6), Sunday is day 0.
%W is replaced by the week of the year (00-53), first day of week is Monday.
%x is a locale-specific synonym, which defaults to "%D" in the C locale.
%X is a locale-specific synonym, which defaults to "%T" in the C locale.
%y is replaced by the year without century (00-99).
%Y is replaced by the year with century.
%Z is replaced by the time zone abbreviation.

The number of options reflects the `strftime' function.

BUG: If the charset used by the current locale is not ISO 8859-1, the
characters appearing in the day and month names may be incorrect.
*/
       (format_string, _time))
{
  time_t value;
  size_t size;

  CHECK_STRING (format_string);

  if (! lisp_to_time (_time, &value))
    error ("Invalid time specification");

  /* This is probably enough.  */
  size = XSTRING_LENGTH (format_string) * 6 + 50;

  while (1)
    {
      char *buf = (char *) alloca (size);
      *buf = 1;
      if (emacs_strftime (buf, size,
			  (CONST char *) XSTRING_DATA (format_string),
			  localtime (&value))
	  || !*buf)
	return build_ext_string (buf, FORMAT_BINARY);
      /* If buffer was too small, make it bigger.  */
      size *= 2;
    }
}

DEFUN ("decode-time", Fdecode_time, 0, 1, 0, /*
Decode a time value as (SEC MINUTE HOUR DAY MONTH YEAR DOW DST ZONE).
The optional SPECIFIED-TIME should be a list of (HIGH LOW . IGNORED)
or (HIGH . LOW), as from `current-time' and `file-attributes', or `nil'
to use the current time.  The list has the following nine members:
SEC is an integer between 0 and 60; SEC is 60 for a leap second, which
only some operating systems support.  MINUTE is an integer between 0 and 59.
HOUR is an integer between 0 and 23.  DAY is an integer between 1 and 31.
MONTH is an integer between 1 and 12.  YEAR is an integer indicating the
four-digit year.  DOW is the day of week, an integer between 0 and 6, where
0 is Sunday.  DST is t if daylight savings time is effect, otherwise nil.
ZONE is an integer indicating the number of seconds east of Greenwich.
\(Note that Common Lisp has different meanings for DOW and ZONE.)
*/
       (specified_time))
{
  time_t time_spec;
  struct tm save_tm;
  struct tm *decoded_time;
  Lisp_Object list_args[9];

  if (! lisp_to_time (specified_time, &time_spec))
    error ("Invalid time specification");

  decoded_time = localtime (&time_spec);
  XSETINT (list_args[0], decoded_time->tm_sec);
  XSETINT (list_args[1], decoded_time->tm_min);
  XSETINT (list_args[2], decoded_time->tm_hour);
  XSETINT (list_args[3], decoded_time->tm_mday);
  XSETINT (list_args[4], decoded_time->tm_mon + 1);
  XSETINT (list_args[5], decoded_time->tm_year + 1900);
  XSETINT (list_args[6], decoded_time->tm_wday);
  list_args[7] = (decoded_time->tm_isdst)? Qt : Qnil;

  /* Make a copy, in case gmtime modifies the struct.  */
  save_tm = *decoded_time;
  decoded_time = gmtime (&time_spec);
  if (decoded_time == 0)
    list_args[8] = Qnil;
  else
    XSETINT (list_args[8], difftm (&save_tm, decoded_time));
  return Flist (9, list_args);
}

static void set_time_zone_rule (char *tzstring);

DEFUN ("encode-time", Fencode_time, 6, MANY, 0, /*
  Convert SECOND, MINUTE, HOUR, DAY, MONTH, YEAR and ZONE to internal time.
This is the reverse operation of `decode-time', which see.
ZONE defaults to the current time zone rule.  This can
be a string (as from `set-time-zone-rule'), or it can be a list
\(as from `current-time-zone') or an integer (as from `decode-time')
applied without consideration for daylight savings time.

You can pass more than 7 arguments; then the first six arguments
are used as SECOND through YEAR, and the *last* argument is used as ZONE.
The intervening arguments are ignored.
This feature lets (apply 'encode-time (decode-time ...)) work.

Out-of-range values for SEC, MINUTE, HOUR, DAY, or MONTH are allowed;
for example, a DAY of 0 means the day preceding the given month.
Year numbers less than 100 are treated just like other year numbers.
If you want them to stand for years in this century, you must do that yourself.
*/
       (int nargs, Lisp_Object *args))
{
  time_t _time;
  struct tm tm;
  Lisp_Object zone = (nargs > 6) ? args[nargs - 1] : Qnil;

  CHECK_INT (*args); tm.tm_sec  = XINT (*args++);	/* second */
  CHECK_INT (*args); tm.tm_min  = XINT (*args++);	/* minute */
  CHECK_INT (*args); tm.tm_hour = XINT (*args++);	/* hour */
  CHECK_INT (*args); tm.tm_mday = XINT (*args++);	/* day */
  CHECK_INT (*args); tm.tm_mon  = XINT (*args++) - 1;	/* month */
  CHECK_INT (*args); tm.tm_year = XINT (*args++) - 1900;/* year */

  tm.tm_isdst = -1;

  if (CONSP (zone))
    zone = XCAR (zone);
  if (NILP (zone))
    _time = mktime (&tm);
  else
    {
      char tzbuf[100];
      char *tzstring;
      char **oldenv = environ, **newenv;

      if (STRINGP (zone))
	tzstring = (char *) XSTRING_DATA (zone);
      else if (INTP (zone))
	{
	  int abszone = abs (XINT (zone));
	  sprintf (tzbuf, "XXX%s%d:%02d:%02d", "-" + (XINT (zone) < 0),
		   abszone / (60*60), (abszone/60) % 60, abszone % 60);
	  tzstring = tzbuf;
	}
      else
	error ("Invalid time zone specification");

      /* Set TZ before calling mktime; merely adjusting mktime's returned
	 value doesn't suffice, since that would mishandle leap seconds.  */
      set_time_zone_rule (tzstring);

      _time = mktime (&tm);

      /* Restore TZ to previous value.  */
      newenv = environ;
      environ = oldenv;
      free (newenv);
#ifdef LOCALTIME_CACHE
      tzset ();
#endif
    }

  if (_time == (time_t) -1)
    error ("Specified time is not representable");

  return wasteful_word_to_lisp (_time);
}

DEFUN ("current-time-string", Fcurrent_time_string, 0, 1, 0, /*
Return the current time, as a human-readable string.
Programs can use this function to decode a time,
since the number of columns in each field is fixed.
The format is `Sun Sep 16 01:03:52 1973'.
If an argument is given, it specifies a time to format
instead of the current time.  The argument should have the form:
  (HIGH . LOW)
or the form:
  (HIGH LOW . IGNORED).
Thus, you can use times obtained from `current-time'
and from `file-attributes'.
*/
       (specified_time))
{
  time_t value;
  char buf[30];
  char *tem;

  if (! lisp_to_time (specified_time, &value))
    value = -1;
  tem = (char *) ctime (&value);

  strncpy (buf, tem, 24);
  buf[24] = 0;

  return build_ext_string (buf, FORMAT_BINARY);
}

#define TM_YEAR_ORIGIN 1900

/* Yield A - B, measured in seconds.  */
static long
difftm (CONST struct tm *a, CONST struct tm *b)
{
  int ay = a->tm_year + (TM_YEAR_ORIGIN - 1);
  int by = b->tm_year + (TM_YEAR_ORIGIN - 1);
  /* Some compilers can't handle this as a single return statement.  */
  long days = (
	      /* difference in day of year */
	      a->tm_yday - b->tm_yday
	      /* + intervening leap days */
	      +  ((ay >> 2) - (by >> 2))
	      -  (ay/100 - by/100)
	      +  ((ay/100 >> 2) - (by/100 >> 2))
	      /* + difference in years * 365 */
	      +  (long)(ay-by) * 365
	      );
  return (60*(60*(24*days + (a->tm_hour - b->tm_hour))
	      + (a->tm_min - b->tm_min))
	  + (a->tm_sec - b->tm_sec));
}

DEFUN ("current-time-zone", Fcurrent_time_zone, 0, 1, 0, /*
Return the offset and name for the local time zone.
This returns a list of the form (OFFSET NAME).
OFFSET is an integer number of seconds ahead of UTC (east of Greenwich).
    A negative value means west of Greenwich.
NAME is a string giving the name of the time zone.
If an argument is given, it specifies when the time zone offset is determined
instead of using the current time.  The argument should have the form:
  (HIGH . LOW)
or the form:
  (HIGH LOW . IGNORED).
Thus, you can use times obtained from `current-time'
and from `file-attributes'.

Some operating systems cannot provide all this information to Emacs;
in this case, `current-time-zone' returns a list containing nil for
the data it can't find.
*/
       (specified_time))
{
  time_t value;
  struct tm *t = NULL;

  if (lisp_to_time (specified_time, &value)
      && (t = gmtime (&value)) != 0)
    {
      struct tm gmt = *t;	/* Make a copy, in case localtime modifies *t.  */
      long offset;
      char *s, buf[6];

      t = localtime (&value);
      offset = difftm (t, &gmt);
      s = 0;
#ifdef HAVE_TM_ZONE
      if (t->tm_zone)
	s = (char *)t->tm_zone;
#else /* not HAVE_TM_ZONE */
#ifdef HAVE_TZNAME
      if (t->tm_isdst == 0 || t->tm_isdst == 1)
	s = tzname[t->tm_isdst];
#endif
#endif /* not HAVE_TM_ZONE */
      if (!s)
	{
	  /* No local time zone name is available; use "+-NNNN" instead.  */
	  int am = (offset < 0 ? -offset : offset) / 60;
	  sprintf (buf, "%c%02d%02d", (offset < 0 ? '-' : '+'), am/60, am%60);
	  s = buf;
	}
      return list2 (make_int (offset), build_string (s));
    }
  else
    return list2 (Qnil, Qnil);
}

#ifdef LOCALTIME_CACHE

/* These two values are known to load tz files in buggy implementations,
   i.e. Solaris 1 executables running under either Solaris 1 or Solaris 2.
   Their values shouldn't matter in non-buggy implementations.
   We don't use string literals for these strings,
   since if a string in the environment is in readonly
   storage, it runs afoul of bugs in SVR4 and Solaris 2.3.
   See Sun bugs 1113095 and 1114114, ``Timezone routines
   improperly modify environment''.  */

static char set_time_zone_rule_tz1[] = "TZ=GMT+0";
static char set_time_zone_rule_tz2[] = "TZ=GMT+1";

#endif

/* Set the local time zone rule to TZSTRING.
   This allocates memory into `environ', which it is the caller's
   responsibility to free.  */
static void
set_time_zone_rule (char *tzstring)
{
  int envptrs;
  char **from, **to, **newenv;

  for (from = environ; *from; from++)
    continue;
  envptrs = from - environ + 2;
  newenv = to = (char **) xmalloc (envptrs * sizeof (char *)
				   + (tzstring ? strlen (tzstring) + 4 : 0));
  if (tzstring)
    {
      char *t = (char *) (to + envptrs);
      strcpy (t, "TZ=");
      strcat (t, tzstring);
      *to++ = t;
    }

  for (from = environ; *from; from++)
    if (strncmp (*from, "TZ=", 3) != 0)
      *to++ = *from;
  *to = 0;

  environ = newenv;

#ifdef LOCALTIME_CACHE
  {
    /* In SunOS 4.1.3_U1 and 4.1.4, if TZ has a value like
       "US/Pacific" that loads a tz file, then changes to a value like
       "XXX0" that does not load a tz file, and then changes back to
       its original value, the last change is (incorrectly) ignored.
       Also, if TZ changes twice in succession to values that do
       not load a tz file, tzset can dump core (see Sun bug#1225179).
       The following code works around these bugs.  */

    if (tzstring)
      {
	/* Temporarily set TZ to a value that loads a tz file
	   and that differs from tzstring.  */
	char *tz = *newenv;
	*newenv = (strcmp (tzstring, set_time_zone_rule_tz1 + 3) == 0
		   ? set_time_zone_rule_tz2 : set_time_zone_rule_tz1);
	tzset ();
	*newenv = tz;
      }
    else
      {
	/* The implied tzstring is unknown, so temporarily set TZ to
	   two different values that each load a tz file.  */
	*to = set_time_zone_rule_tz1;
	to[1] = 0;
	tzset ();
	*to = set_time_zone_rule_tz2;
	tzset ();
	*to = 0;
      }

    /* Now TZ has the desired value, and tzset can be invoked safely.  */
  }

  tzset ();
#endif
}

DEFUN ("set-time-zone-rule", Fset_time_zone_rule, 1, 1, 0, /*
Set the local time zone using TZ, a string specifying a time zone rule.
If TZ is nil, use implementation-defined default time zone information.
*/
       (tz))
{
  char *tzstring;

  if (NILP (tz))
    tzstring = 0;
  else
    {
      CHECK_STRING (tz);
      tzstring = (char *) XSTRING_DATA (tz);
    }

  set_time_zone_rule (tzstring);
  if (environbuf)
    xfree (environbuf);
  environbuf = environ;

  return Qnil;
}


void
buffer_insert1 (struct buffer *buf, Lisp_Object arg)
{
  /* This function can GC */
  struct gcpro gcpro1;
  GCPRO1 (arg);
 retry:
  if (CHAR_OR_CHAR_INTP (arg))
    {
      buffer_insert_emacs_char (buf, XCHAR_OR_CHAR_INT (arg));
    }
  else if (STRINGP (arg))
    {
      buffer_insert_lisp_string (buf, arg);
    }
  else
    {
      arg = wrong_type_argument (Qchar_or_string_p, arg);
      goto retry;
    }
  zmacs_region_stays = 0;
  UNGCPRO;
}


/* Callers passing one argument to Finsert need not gcpro the
   argument "array", since the only element of the array will
   not be used after calling insert_emacs_char or insert_lisp_string,
   so we don't care if it gets trashed.  */

DEFUN ("insert", Finsert, 0, MANY, 0, /*
Insert the arguments, either strings or characters, at point.
Point moves forward so that it ends up after the inserted text.
Any other markers at the point of insertion remain before the text.
If a string has non-null string-extent-data, new extents will be created.
*/
       (int nargs, Lisp_Object *args))
{
  /* This function can GC */
  REGISTER int argnum;

  for (argnum = 0; argnum < nargs; argnum++)
    {
      buffer_insert1 (current_buffer, args[argnum]);
    }

  return Qnil;
}

DEFUN ("insert-before-markers", Finsert_before_markers, 0, MANY, 0, /*
Insert strings or characters at point, relocating markers after the text.
Point moves forward so that it ends up after the inserted text.
Any other markers at the point of insertion also end up after the text.
*/
       (int nargs, Lisp_Object *args))
{
  /* This function can GC */
  REGISTER int argnum;
  REGISTER Lisp_Object tem;

  for (argnum = 0; argnum < nargs; argnum++)
    {
      tem = args[argnum];
    retry:
      if (CHAR_OR_CHAR_INTP (tem))
	{
	  buffer_insert_emacs_char_1 (current_buffer, -1,
				      XCHAR_OR_CHAR_INT (tem),
				      INSDEL_BEFORE_MARKERS);
	}
      else if (STRINGP (tem))
	{
	  buffer_insert_lisp_string_1 (current_buffer, -1, tem,
				       INSDEL_BEFORE_MARKERS);
	}
      else
	{
	  tem = wrong_type_argument (Qchar_or_string_p, tem);
	  goto retry;
	}
    }
  zmacs_region_stays = 0;
  return Qnil;
}

DEFUN ("insert-string", Finsert_string, 1, 2, 0, /*
Insert STRING into BUFFER at BUFFER's point.
Point moves forward so that it ends up after the inserted text.
Any other markers at the point of insertion remain before the text.
If a string has non-null string-extent-data, new extents will be created.
BUFFER defaults to the current buffer.
*/
       (string, buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  CHECK_STRING (string);
  buffer_insert_lisp_string (b, string);
  zmacs_region_stays = 0;
  return Qnil;
}

/* Third argument in FSF is INHERIT:

"The optional third arg INHERIT, if non-nil, says to inherit text properties
from adjoining text, if those properties are sticky."

Jamie thinks this is bogus. */


DEFUN ("insert-char", Finsert_char, 1, 4, 0, /*
Insert COUNT (second arg) copies of CHR (first arg).
Point and all markers are affected as in the function `insert'.
COUNT defaults to 1 if omitted.
The optional third arg IGNORED is INHERIT under FSF Emacs.
This is highly bogus, however, and XEmacs always behaves as if
`t' were passed to INHERIT.
The optional fourth arg BUFFER specifies the buffer to insert the
text into.  If BUFFER is nil, the current buffer is assumed.
*/
       (chr, count, ignored, buffer))
{
  /* This function can GC */
  REGISTER Bufbyte *string;
  REGISTER int slen;
  REGISTER int i, j;
  REGISTER Bytecount n;
  REGISTER Bytecount charlen;
  Bufbyte str[MAX_EMCHAR_LEN];
  struct buffer *b = decode_buffer (buffer, 1);
  int cou;

  CHECK_CHAR_COERCE_INT (chr);
  if (NILP (count))
    cou = 1;
  else
    {
      CHECK_INT (count);
      cou = XINT (count);
    }

  charlen = set_charptr_emchar (str, XCHAR (chr));
  n = cou * charlen;
  if (n <= 0)
    return Qnil;
  slen = min (n, 768);
  string = alloca_array (Bufbyte, slen);
  /* Write as many copies of the character into the temp string as will fit. */
  for (i = 0; i + charlen <= slen; i += charlen)
    for (j = 0; j < charlen; j++)
      string[i + j] = str[j];
  slen = i;
  while (n >= slen)
    {
      buffer_insert_raw_string (b, string, slen);
      n -= slen;
    }
  if (n > 0)
#if 0 /* FSFmacs bogosity */
    {
      if (!NILP (inherit))
	insert_and_inherit (string, n);
      else
	insert (string, n);
    }
#else
    buffer_insert_raw_string (b, string, n);
#endif

  zmacs_region_stays = 0;
  return Qnil;
}


/* Making strings from buffer contents.  */

DEFUN ("buffer-substring", Fbuffer_substring, 0, 3, 0, /*
Return the contents of part of BUFFER as a string.
The two arguments START and END are character positions;
they can be in either order.  If omitted, they default to the beginning
and end of BUFFER, respectively.
If there are duplicable extents in the region, the string remembers
them in its extent data.
If BUFFER is nil, the current buffer is assumed.
*/
       (start, end, buffer))
{
  /* This function can GC */
  Bufpos begv, zv;
  struct buffer *b = decode_buffer (buffer, 1);

  get_buffer_range_char (b, start, end, &begv, &zv, GB_ALLOW_NIL);
  return make_string_from_buffer (b, begv, zv - begv);
}

DEFUN ("insert-buffer-substring", Finsert_buffer_substring, 1, 3, 0, /*
Insert before point a substring of the contents of buffer BUFFER.
BUFFER may be a buffer or a buffer name.
Arguments START and END are character numbers specifying the substring.
They default to the beginning and the end of BUFFER.
*/
       (buffer, start, end))
{
  /* This function can GC */
  Bufpos b, e;
  struct buffer *bp;

  bp = XBUFFER (get_buffer (buffer, 1));
  get_buffer_range_char (bp, start, end, &b, &e, GB_ALLOW_NIL);

  if (b < e)
    buffer_insert_from_buffer (current_buffer, bp, b, e - b);

  return Qnil;
}

DEFUN ("compare-buffer-substrings", Fcompare_buffer_substrings, 6, 6, 0, /*
Compare two substrings of two buffers; return result as number.
the value is -N if first string is less after N-1 chars,
+N if first string is greater after N-1 chars, or 0 if strings match.
Each substring is represented as three arguments: BUFFER, START and END.
That makes six args in all, three for each substring.

The value of `case-fold-search' in the current buffer
determines whether case is significant or ignored.
*/
       (buffer1, start1, end1, buffer2, start2, end2))
{
  Bufpos begp1, endp1, begp2, endp2;
  REGISTER Charcount len1, len2, length, i;
  struct buffer *bp1, *bp2;
  Lisp_Object trt = ((!NILP (current_buffer->case_fold_search)) ?
		     current_buffer->case_canon_table : Qnil);

  /* Find the first buffer and its substring.  */

  bp1 = decode_buffer (buffer1, 1);
  get_buffer_range_char (bp1, start1, end1, &begp1, &endp1, GB_ALLOW_NIL);

  /* Likewise for second substring.  */

  bp2 = decode_buffer (buffer2, 1);
  get_buffer_range_char (bp2, start2, end2, &begp2, &endp2, GB_ALLOW_NIL);

  len1 = endp1 - begp1;
  len2 = endp2 - begp2;
  length = len1;
  if (len2 < length)
    length = len2;

  for (i = 0; i < length; i++)
    {
      Emchar c1 = BUF_FETCH_CHAR (bp1, begp1 + i);
      Emchar c2 = BUF_FETCH_CHAR (bp2, begp2 + i);
      if (!NILP (trt))
	{
	  c1 = TRT_TABLE_OF (trt, c1);
	  c2 = TRT_TABLE_OF (trt, c2);
	}
      if (c1 < c2)
	return make_int (- 1 - i);
      if (c1 > c2)
	return make_int (i + 1);
    }

  /* The strings match as far as they go.
     If one is shorter, that one is less.  */
  if (length < len1)
    return make_int (length + 1);
  else if (length < len2)
    return make_int (- length - 1);

  /* Same length too => they are equal.  */
  return Qzero;
}


static Lisp_Object
subst_char_in_region_unwind (Lisp_Object arg)
{
  XBUFFER (XCAR (arg))->undo_list = XCDR (arg);
  return Qnil;
}

static Lisp_Object
subst_char_in_region_unwind_1 (Lisp_Object arg)
{
  XBUFFER (XCAR (arg))->filename = XCDR (arg);
  return Qnil;
}

DEFUN ("subst-char-in-region", Fsubst_char_in_region, 4, 5, 0, /*
From START to END, replace FROMCHAR with TOCHAR each time it occurs.
If optional arg NOUNDO is non-nil, don't record this change for undo
and don't mark the buffer as really changed.
*/
  (start, end, fromchar, tochar, noundo))
{
  /* This function can GC */
  Bufpos pos, stop;
  Emchar fromc, toc;
  int mc_count;
  struct buffer *buf = current_buffer;
  int count = specpdl_depth ();

  get_buffer_range_char (buf, start, end, &pos, &stop, 0);
  CHECK_CHAR_COERCE_INT (fromchar);
  CHECK_CHAR_COERCE_INT (tochar);

  fromc = XCHAR (fromchar);
  toc = XCHAR (tochar);

  /* If we don't want undo, turn off putting stuff on the list.
     That's faster than getting rid of things,
     and it prevents even the entry for a first change.
     Also inhibit locking the file.  */
  if (!NILP (noundo))
    {
      record_unwind_protect (subst_char_in_region_unwind,
			     Fcons (Fcurrent_buffer (), buf->undo_list));
      buf->undo_list = Qt;
      /* Don't do file-locking.  */
      record_unwind_protect (subst_char_in_region_unwind_1,
			     Fcons (Fcurrent_buffer (), buf->filename));
      buf->filename = Qnil;
    }

  mc_count = begin_multiple_change (buf, pos, stop);
  while (pos < stop)
    {
      if (BUF_FETCH_CHAR (buf, pos) == fromc)
	{
	  /* There used to be some code here that set the buffer to
	     unmodified if NOUNDO was specified and there was only
	     one change to the buffer since it was last saved.
	     This is a crock of shit, so I'm not duplicating this
	     behavior.  I think this was left over from when
	     prepare_to_modify_buffer() actually bumped MODIFF,
	     so that code was supposed to undo this change. --ben */
	  buffer_replace_char (buf, pos, toc, !NILP (noundo), 0);

	  /* If noundo is not nil then we don't mark the buffer as
             modified.  In reality that needs to happen externally
             only.  Internally redisplay needs to know that the actual
             contents it should be displaying have changed. */
	  if (!NILP (noundo))
	    Fset_buffer_modified_p (Fbuffer_modified_p (Qnil), Qnil);
	}
      pos++;
    }
  end_multiple_change (buf, mc_count);

  unbind_to (count, Qnil);
  return Qnil;
}

DEFUN ("translate-region", Ftranslate_region, 3, 3, 0, /*
From START to END, translate characters according to TABLE.
TABLE is a string; the Nth character in it is the mapping
for the character with code N.  Returns the number of characters changed.
*/
       (start, end, table))
{
  /* This function can GC */
  Bufpos pos, stop;	/* Limits of the region. */
  REGISTER Emchar oc;		/* Old character. */
  REGISTER Emchar nc;		/* New character. */
  int cnt;		/* Number of changes made. */
  Charcount size;	/* Size of translate table. */
  int mc_count;
  struct buffer *buf = current_buffer;

  get_buffer_range_char (buf, start, end, &pos, &stop, 0);
  CHECK_STRING (table);

  size = XSTRING_CHAR_LENGTH (table);

  cnt = 0;
  mc_count = begin_multiple_change (buf, pos, stop);
  for (; pos < stop; pos++)
    {
      oc = BUF_FETCH_CHAR (buf, pos);
      if (oc >= 0 && oc < size)
	{
	  nc = string_char (XSTRING (table), oc);
	  if (nc != oc)
	    {
	      buffer_replace_char (buf, pos, nc, 0, 0);
	      ++cnt;
	    }
	}
    }
  end_multiple_change (buf, mc_count);

  return make_int (cnt);
}

DEFUN ("delete-region", Fdelete_region, 2, 3, "r", /*
Delete the text between point and mark.
When called from a program, expects two arguments,
positions (integers or markers) specifying the stretch to be deleted.
If BUFFER is nil, the current buffer is assumed.
*/
       (b, e, buffer))
{
  /* This function can GC */
  Bufpos start, end;
  struct buffer *buf = decode_buffer (buffer, 1);

  get_buffer_range_char (buf, b, e, &start, &end, 0);
  buffer_delete_range (buf, start, end, 0);
  zmacs_region_stays = 0;
  return Qnil;
}

void
widen_buffer (struct buffer *b, int no_clip)
{
  if (BUF_BEGV (b) != BUF_BEG (b))
    {
      clip_changed = 1;
      SET_BOTH_BUF_BEGV (b, BUF_BEG (b), BI_BUF_BEG (b));
    }
  if (BUF_ZV (b) != BUF_Z (b))
    {
      clip_changed = 1;
      SET_BOTH_BUF_ZV (b, BUF_Z (b), BI_BUF_Z (b));
    }
  if (clip_changed)
    {
      if (!no_clip)
	MARK_CLIP_CHANGED;
      /* Changing the buffer bounds invalidates any recorded current
         column.  */
      invalidate_current_column ();
      narrow_line_number_cache (b);
    }
}

DEFUN ("widen", Fwiden, 0, 1, "", /*
Remove restrictions (narrowing) from BUFFER.
This allows the buffer's full text to be seen and edited.
If BUFFER is nil, the current buffer is assumed.
*/
       (buffer))
{
  struct buffer *b = decode_buffer (buffer, 1);
  widen_buffer (b, 0);
  zmacs_region_stays = 0;
  return Qnil;
}

DEFUN ("narrow-to-region", Fnarrow_to_region, 2, 3, "r", /*
Restrict editing in BUFFER to the current region.
The rest of the text becomes temporarily invisible and untouchable
but is not deleted; if you save the buffer in a file, the invisible
text is included in the file.  \\[widen] makes all visible again.
If BUFFER is nil, the current buffer is assumed.
See also `save-restriction'.

When calling from a program, pass two arguments; positions (integers
or markers) bounding the text that should remain visible.
*/
       (b, e, buffer))
{
  Bufpos start, end;
  struct buffer *buf = decode_buffer (buffer, 1);
  Bytind bi_start, bi_end;

  get_buffer_range_char (buf, b, e, &start, &end, GB_ALLOW_PAST_ACCESSIBLE);
  bi_start = bufpos_to_bytind (buf, start);
  bi_end = bufpos_to_bytind (buf, end);

  SET_BOTH_BUF_BEGV (buf, start, bi_start);
  SET_BOTH_BUF_ZV (buf, end, bi_end);
  if (BUF_PT (buf) < start)
    BUF_SET_PT (buf, start);
  if (BUF_PT (buf) > end)
    BUF_SET_PT (buf, end);
  MARK_CLIP_CHANGED;
  /* Changing the buffer bounds invalidates any recorded current column.  */
  invalidate_current_column ();
  narrow_line_number_cache (buf);
  zmacs_region_stays = 0;
  return Qnil;
}

Lisp_Object
save_restriction_save (void)
{
  Lisp_Object bottom, top;
  /* Note: I tried using markers here, but it does not win
     because insertion at the end of the saved region
     does not advance mh and is considered "outside" the saved region. */
  bottom = make_int (BUF_BEGV (current_buffer) - BUF_BEG (current_buffer));
  top = make_int (BUF_Z (current_buffer) - BUF_ZV (current_buffer));

  return noseeum_cons (Fcurrent_buffer (), noseeum_cons (bottom, top));
}

Lisp_Object
save_restriction_restore (Lisp_Object data)
{
  struct buffer *buf;
  Charcount newhead, newtail;
  Lisp_Object tem;
  int local_clip_changed = 0;

  buf = XBUFFER (XCAR (data));
  if (!BUFFER_LIVE_P (buf))
    {
      /* someone could have killed the buffer in the meantime ... */
      free_cons (XCONS (XCDR (data)));
      free_cons (XCONS (data));
      return Qnil;
    }
  tem = XCDR (data);
  newhead = XINT (XCAR (tem));
  newtail = XINT (XCDR (tem));

  free_cons (XCONS (XCDR (data)));
  free_cons (XCONS (data));

  if (newhead + newtail > BUF_Z (buf) - BUF_BEG (buf))
    {
      newhead = 0;
      newtail = 0;
    }

  {
    Bufpos start, end;
    Bytind bi_start, bi_end;

    start = BUF_BEG (buf) + newhead;
    end = BUF_Z (buf) - newtail;

    bi_start = bufpos_to_bytind (buf, start);
    bi_end = bufpos_to_bytind (buf, end);

    if (BUF_BEGV (buf) != start)
      {
	local_clip_changed = 1;
	SET_BOTH_BUF_BEGV (buf, start, bi_start);
	narrow_line_number_cache (buf);
      }
    if (BUF_ZV (buf) != end)
      {
	local_clip_changed = 1;
	SET_BOTH_BUF_ZV (buf, end, bi_end);
      }
  }
  if (local_clip_changed)
    MARK_CLIP_CHANGED;

  /* If point is outside the new visible range, move it inside. */
  BUF_SET_PT (buf,
              bufpos_clip_to_bounds (BUF_BEGV (buf),
				     BUF_PT (buf),
				     BUF_ZV (buf)));

  return Qnil;
}

DEFUN ("save-restriction", Fsave_restriction, 0, UNEVALLED, 0, /*
Execute BODY, saving and restoring current buffer's restrictions.
The buffer's restrictions make parts of the beginning and end invisible.
\(They are set up with `narrow-to-region' and eliminated with `widen'.)
This special form, `save-restriction', saves the current buffer's restrictions
when it is entered, and restores them when it is exited.
So any `narrow-to-region' within BODY lasts only until the end of the form.
The old restrictions settings are restored
even in case of abnormal exit (throw or error).

The value returned is the value of the last form in BODY.

`save-restriction' can get confused if, within the BODY, you widen
and then make changes outside the area within the saved restrictions.

Note: if you are using both `save-excursion' and `save-restriction',
use `save-excursion' outermost:
    (save-excursion (save-restriction ...))
*/
       (body))
{
  /* This function can GC */
  int speccount = specpdl_depth ();

  record_unwind_protect (save_restriction_restore, save_restriction_save ());

  return unbind_to (speccount, Fprogn (body));
}


DEFUN ("format", Fformat, 1, MANY, 0, /*
Format a string out of a control-string and arguments.
The first argument is a control string.
The other arguments are substituted into it to make the result, a string.
It may contain %-sequences meaning to substitute the next argument.
%s means print all objects as-is, using `princ'.
%S means print all objects as s-expressions, using `prin1'.
%d or %i means print as an integer in decimal (%o octal, %x lowercase hex,
  %X uppercase hex).
%c means print as a single character.
%f means print as a floating-point number in fixed notation (e.g. 785.200).
%e or %E means print as a floating-point number in scientific notation
  (e.g. 7.85200e+03).
%g or %G means print as a floating-point number in "pretty format";
  depending on the number, either %f or %e/%E format will be used, and
  trailing zeroes are removed from the fractional part.
The argument used for all but %s and %S must be a number.  It will be
  converted to an integer or a floating-point number as necessary.

%$ means reposition to read a specific numbered argument; for example,
  %3$s would apply the `%s' to the third argument after the control string,
  and the next format directive would use the fourth argument, the
  following one the fifth argument, etc. (There must be a positive integer
  between the % and the $).
Zero or more of the flag characters `-', `+', ` ', `0', and `#' may be
  specified between the optional repositioning spec and the conversion
  character; see below.
An optional minimum field width may be specified after any flag characters
  and before the conversion character; it specifies the minimum number of
  characters that the converted argument will take up.  Padding will be
  added on the left (or on the right, if the `-' flag is specified), as
  necessary.  Padding is done with spaces, or with zeroes if the `0' flag
  is specified.
If the field width is specified as `*', the field width is assumed to have
  been specified as an argument.  Any repositioning specification that
  would normally specify the argument to be converted will now specify
  where to find this field width argument, not where to find the argument
  to be converted.  If there is no repositioning specification, the normal
  next argument is used.  The argument to be converted will be the next
  argument after the field width argument unless the precision is also
  specified as `*' (see below).

An optional period character and precision may be specified after any
  minimum field width.  It specifies the minimum number of digits to
  appear in %d, %i, %o, %x, and %X conversions (the number is padded
  on the left with zeroes as necessary); the number of digits printed
  after the decimal point for %f, %e, and %E conversions; the number
  of significant digits printed in %g and %G conversions; and the
  maximum number of non-padding characters printed in %s and %S
  conversions.  The default precision for floating-point conversions
  is six.
If the precision is specified as `*', the precision is assumed to have been
  specified as an argument.  The argument used will be the next argument
  after the field width argument, if any.  If the field width was not
  specified as an argument, any repositioning specification that would
  normally specify the argument to be converted will now specify where to
  find the precision argument.  If there is no repositioning specification,
  the normal next argument is used.

The ` ' and `+' flags mean prefix non-negative numbers with a space or
  plus sign, respectively.
The `#' flag means print numbers in an alternate, more verbose format:
  octal numbers begin with zero; hex numbers begin with a 0x or 0X;
  a decimal point is printed in %f, %e, and %E conversions even if no
  numbers are printed after it; and trailing zeroes are not omitted in
   %g and %G conversions.

Use %% to put a single % into the output.
*/
       (int nargs, Lisp_Object *args))
{
  /* It should not be necessary to GCPRO ARGS, because
     the caller in the interpreter should take care of that.  */

  CHECK_STRING (args[0]);
  return emacs_doprnt_string_lisp (0, args[0], 0, nargs - 1, args + 1);
}


DEFUN ("char-equal", Fchar_equal, 2, 3, 0, /*
Return t if two characters match, optionally ignoring case.
Both arguments must be characters (i.e. NOT integers).
Case is ignored if `case-fold-search' is non-nil in BUFFER.
If BUFFER is nil, the current buffer is assumed.
*/
       (c1, c2, buffer))
{
  Emchar x1, x2;
  struct buffer *b = decode_buffer (buffer, 1);

  CHECK_CHAR_COERCE_INT (c1);
  CHECK_CHAR_COERCE_INT (c2);
  x1 = XCHAR (c1);
  x2 = XCHAR (c2);

  return (!NILP (b->case_fold_search)
	  ? DOWNCASE (b, x1) == DOWNCASE (b, x2)
	  : x1 == x2)
    ? Qt : Qnil;
}

DEFUN ("char=", Fchar_Equal, 2, 3, 0, /*
Return t if two characters match, case is significant.
Both arguments must be characters (i.e. NOT integers).
The optional buffer argument is for symmetry and is ignored.
*/
       (c1, c2, buffer))
{
  CHECK_CHAR_COERCE_INT (c1);
  CHECK_CHAR_COERCE_INT (c2);

  return XCHAR(c1) == XCHAR(c2) ? Qt : Qnil;
}

#if 0 /* Undebugged FSFmacs code */
/* Transpose the markers in two regions of the current buffer, and
   adjust the ones between them if necessary (i.e.: if the regions
   differ in size).

   Traverses the entire marker list of the buffer to do so, adding an
   appropriate amount to some, subtracting from some, and leaving the
   rest untouched.  Most of this is copied from adjust_markers in insdel.c.

   It's the caller's job to see that (start1 <= end1 <= start2 <= end2).  */

void
transpose_markers (Bufpos start1, Bufpos end1, Bufpos start2, Bufpos end2)
{
  Charcount amt1, amt2, diff;
  Lisp_Object marker;
  struct buffer *buf = current_buffer;

  /* Update point as if it were a marker.  */
  if (BUF_PT (buf) < start1)
    ;
  else if (BUF_PT (buf) < end1)
    BUF_SET_PT (buf, BUF_PT (buf) + (end2 - end1));
  else if (BUF_PT (buf) < start2)
    BUF_SET_PT (buf, BUF_PT (buf) + (end2 - start2) - (end1 - start1));
  else if (BUF_PT (buf) < end2)
    BUF_SET_PT (buf, BUF_PT (buf) - (start2 - start1));

  /* We used to adjust the endpoints here to account for the gap, but that
     isn't good enough.  Even if we assume the caller has tried to move the
     gap out of our way, it might still be at start1 exactly, for example;
     and that places it `inside' the interval, for our purposes.  The amount
     of adjustment is nontrivial if there's a `denormalized' marker whose
     position is between GPT and GPT + GAP_SIZE, so it's simpler to leave
     the dirty work to Fmarker_position, below.  */

  /* The difference between the region's lengths */
  diff = (end2 - start2) - (end1 - start1);

  /* For shifting each marker in a region by the length of the other
   * region plus the distance between the regions.
   */
  amt1 = (end2 - start2) + (start2 - end1);
  amt2 = (end1 - start1) + (start2 - end1);

  for (marker = BUF_MARKERS (buf); !NILP (marker);
       marker = XMARKER (marker)->chain)
    {
      Bufpos mpos = marker_position (marker);
      if (mpos >= start1 && mpos < end2)
	{
	  if (mpos < end1)
	    mpos += amt1;
	  else if (mpos < start2)
	    mpos += diff;
	  else
	    mpos -= amt2;
	  set_marker_position (marker, mpos);
	}
    }
}

#endif /* 0 */

DEFUN ("transpose-regions", Ftranspose_regions, 4, 5, 0, /*
Transpose region START1 to END1 with START2 to END2.
The regions may not be overlapping, because the size of the buffer is
never changed in a transposition.

Optional fifth arg LEAVE_MARKERS, if non-nil, means don't transpose
any markers that happen to be located in the regions. (#### BUG: currently
this function always acts as if LEAVE_MARKERS is non-nil.)

Transposing beyond buffer boundaries is an error.
*/
  (startr1, endr1, startr2, endr2, leave_markers))
{
  Bufpos start1, end1, start2, end2;
  Charcount len1, len2;
  Lisp_Object string1, string2;
  struct buffer *buf = current_buffer;

  get_buffer_range_char (buf, startr1, endr1, &start1, &end1, 0);
  get_buffer_range_char (buf, startr2, endr2, &start2, &end2, 0);

  len1 = end1 - start1;
  len2 = end2 - start2;

  if (start2 < end1)
    error ("transposed regions not properly ordered");
  else if (start1 == end1 || start2 == end2)
    error ("transposed region may not be of length 0");

  string1 = make_string_from_buffer (buf, start1, len1);
  string2 = make_string_from_buffer (buf, start2, len2);
  buffer_delete_range (buf, start2, end2, 0);
  buffer_insert_lisp_string_1 (buf, start2, string1, 0);
  buffer_delete_range (buf, start1, end1, 0);
  buffer_insert_lisp_string_1 (buf, start1, string2, 0);

  /* In FSFmacs there is a whole bunch of really ugly code here
     to attempt to transpose the regions without using up any
     extra memory.  Although the intent may be good, the result
     was highly bogus. */

  return Qnil;
}


/************************************************************************/
/*                            initialization                            */
/************************************************************************/

void
syms_of_editfns (void)
{
  defsymbol (&Qpoint, "point");
  defsymbol (&Qmark, "mark");
  defsymbol (&Qregion_beginning, "region-beginning");
  defsymbol (&Qregion_end, "region-end");
  defsymbol (&Qformat, "format");
  defsymbol (&Quser_files_and_directories, "user-files-and-directories");

  DEFSUBR (Fchar_equal);
  DEFSUBR (Fchar_Equal);
  DEFSUBR (Fgoto_char);
  DEFSUBR (Fstring_to_char);
  DEFSUBR (Fchar_to_string);
  DEFSUBR (Fbuffer_substring);

  DEFSUBR (Fpoint_marker);
  DEFSUBR (Fmark_marker);
  DEFSUBR (Fpoint);
  DEFSUBR (Fregion_beginning);
  DEFSUBR (Fregion_end);
  DEFSUBR (Fsave_excursion);
  DEFSUBR (Fsave_current_buffer);

  DEFSUBR (Fbuffer_size);
  DEFSUBR (Fpoint_max);
  DEFSUBR (Fpoint_min);
  DEFSUBR (Fpoint_min_marker);
  DEFSUBR (Fpoint_max_marker);

  DEFSUBR (Fbobp);
  DEFSUBR (Feobp);
  DEFSUBR (Fbolp);
  DEFSUBR (Feolp);
  DEFSUBR (Ffollowing_char);
  DEFSUBR (Fpreceding_char);
  DEFSUBR (Fchar_after);
  DEFSUBR (Fchar_before);
  DEFSUBR (Finsert);
  DEFSUBR (Finsert_string);
  DEFSUBR (Finsert_before_markers);
  DEFSUBR (Finsert_char);

  DEFSUBR (Ftemp_directory);
  DEFSUBR (Fuser_login_name);
  DEFSUBR (Fuser_real_login_name);
  DEFSUBR (Fuser_uid);
  DEFSUBR (Fuser_real_uid);
  DEFSUBR (Fuser_full_name);
  DEFSUBR (Fuser_home_directory);
  DEFSUBR (Femacs_pid);
  DEFSUBR (Fcurrent_time);
  DEFSUBR (Fcurrent_process_time);
  DEFSUBR (Fformat_time_string);
  DEFSUBR (Fdecode_time);
  DEFSUBR (Fencode_time);
  DEFSUBR (Fcurrent_time_string);
  DEFSUBR (Fcurrent_time_zone);
  DEFSUBR (Fset_time_zone_rule);
  DEFSUBR (Fsystem_name);
  DEFSUBR (Fformat);

  DEFSUBR (Finsert_buffer_substring);
  DEFSUBR (Fcompare_buffer_substrings);
  DEFSUBR (Fsubst_char_in_region);
  DEFSUBR (Ftranslate_region);
  DEFSUBR (Fdelete_region);
  DEFSUBR (Fwiden);
  DEFSUBR (Fnarrow_to_region);
  DEFSUBR (Fsave_restriction);
  DEFSUBR (Ftranspose_regions);

  defsymbol (&Qzmacs_update_region, "zmacs-update-region");
  defsymbol (&Qzmacs_deactivate_region, "zmacs-deactivate-region");
  defsymbol (&Qzmacs_region_buffer, "zmacs-region-buffer");
}

void
vars_of_editfns (void)
{
  staticpro (&Vsystem_name);
#if 0
  staticpro (&Vuser_name);
  staticpro (&Vuser_real_name);
#endif
  DEFVAR_BOOL ("zmacs-regions", &zmacs_regions /*
*Whether LISPM-style active regions should be used.
This means that commands which operate on the region (the area between the
point and the mark) will only work while the region is in the ``active''
state, which is indicated by highlighting.  Executing most commands causes
the region to not be in the active state, so (for example) \\[kill-region] will only
work immediately after activating the region.

More specifically:

 - Commands which operate on the region only work if the region is active.
 - Only a very small set of commands cause the region to become active:
   Those commands whose semantics are to mark an area, like mark-defun.
 - The region is deactivated after each command that is executed, except that:
 - "Motion" commands do not change whether the region is active or not.

set-mark-command (C-SPC) pushes a mark and activates the region.  Moving the
cursor with normal motion commands (C-n, C-p, etc) will cause the region
between point and the recently-pushed mark to be highlighted.  It will
remain highlighted until some non-motion command is executed.

exchange-point-and-mark (\\[exchange-point-and-mark]) activates the region.  So if you mark a
region and execute a command that operates on it, you can reactivate the
same region with \\[exchange-point-and-mark] (or perhaps \\[exchange-point-and-mark] \\[exchange-point-and-mark]) to operate on it
again.

Generally, commands which push marks as a means of navigation (like
beginning-of-buffer and end-of-buffer (M-< and M->)) do not activate the
region.  But commands which push marks as a means of marking an area of
text (like mark-defun (\\[mark-defun]), mark-word (\\[mark-word]) or mark-whole-buffer (\\[mark-whole-buffer]))
do activate the region.

The way the command loop actually works with regard to deactivating the
region is as follows:

- If the variable `zmacs-region-stays' has been set to t during the command
  just executed, the region is left alone (this is how the motion commands
  make the region stay around; see the `_' flag in the `interactive'
  specification).  `zmacs-region-stays' is reset to nil before each command
  is executed.
- If the function `zmacs-activate-region' has been called during the command
  just executed, the region is left alone.  Very few functions should
  actually call this function.
- Otherwise, if the region is active, the region is deactivated and
  the `zmacs-deactivate-region-hook' is called.
*/ );
  /* Zmacs style active regions are now ON by default */
  zmacs_regions = 1;

  DEFVAR_BOOL ("zmacs-region-active-p", &zmacs_region_active_p /*
Do not alter this.  It is for internal use only.
*/ );
  zmacs_region_active_p = 0;

  DEFVAR_BOOL ("zmacs-region-stays", &zmacs_region_stays /*
Whether the current command will deactivate the region.
Commands which do not wish to affect whether the region is currently
highlighted should set this to t.  Normally, the region is turned off after
executing each command that did not explicitly turn it on with the function
zmacs-activate-region. Setting this to true lets a command be non-intrusive.
See the variable `zmacs-regions'.

The same effect can be achieved using the `_' interactive specification.
*/ );
  zmacs_region_stays = 0;

  DEFVAR_BOOL ("atomic-extent-goto-char-p", &atomic_extent_goto_char_p /*
Do not use this -- it will be going away soon.
Indicates if `goto-char' has just been run.  This information is allegedly
needed to get the desired behavior for atomic extents and unfortunately
is not available by any other means.
*/ );
  atomic_extent_goto_char_p = 0;
#ifdef AMPERSAND_FULL_NAME
  Fprovide(intern("ampersand-full-name"));
#endif

  DEFVAR_LISP ("user-full-name", &Vuser_full_name /*
*The name of the user.
The function `user-full-name', which will return the value of this
 variable, when called without arguments.
This is initialized to the value of the NAME environment variable.
*/ );
  /* Initialized at run-time. */
  Vuser_full_name = Qnil;
}
