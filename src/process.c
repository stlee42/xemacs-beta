/* Asynchronous subprocess control for XEmacs.
   Copyright (C) 1985, 1986, 1987, 1988, 1992, 1993, 1994, 1995
   Free Software Foundation, Inc.
   Copyright (C) 1995 Sun Microsystems, Inc.
   Copyright (C) 1995, 1996, 2001, 2002 Ben Wing.

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

/* This file has been Mule-ized. */

/* This file has been split into process.c and process-unix.c by
   Kirill M. Katsnelson <kkm@kis.ru>, so please bash him and not
   the original author(s).

   Non-synch-subprocess stuff (mostly process environment) moved from
   callproc.c, 4-3-02, Ben Wing. */

#include <config.h>

#if defined (NO_SUBPROCESSES)
#error "We don't support this anymore."
#endif

#include "lisp.h"

#include "buffer.h"
#include "commands.h"
#include "device.h"
#include "events.h"
#include "file-coding.h"
#include "frame.h"
#include "hash.h"
#include "insdel.h"
#include "lstream.h"
#include "opaque.h"
#include "process.h"
#include "procimpl.h"
#include "window.h"

#include "sysfile.h"
#include "sysproc.h"
#include "syssignal.h"
#include "systime.h"
#include "systty.h"
#include "syswait.h"

Lisp_Object Qprocessp, Qprocess_live_p;

/* Process methods */
struct process_methods the_process_methods;

/* a process object is a network connection when its pid field a cons
   (name of name of port we are connected to . foreign host name) */

/* Valid values of process->status_symbol */
Lisp_Object Qrun, Qstop;
/* Qrun => Qopen, Qexit => Qclosed for "network connection" processes */
Lisp_Object Qopen, Qclosed;
/* Protocol families */
Lisp_Object Qtcp, Qudp;

#ifdef HAVE_MULTICAST
Lisp_Object Qmulticast; /* Will be used for occasional warnings */
#endif

/* t means use pty, nil means use a pipe,
   maybe other values to come.  */
Lisp_Object Vprocess_connection_type;

/* Read comments to DEFVAR of this */
int windowed_process_io;

#ifdef PROCESS_IO_BLOCKING
/* List of port numbers or port names to set a blocking I/O mode.
   Nil means set a non-blocking I/O mode [default]. */
Lisp_Object network_stream_blocking_port_list;
#endif  /* PROCESS_IO_BLOCKING */

/* Number of events of change of status of a process.  */
volatile int process_tick;

/* Number of events for which the user or sentinel has been notified.  */
static int update_tick;

/* Nonzero means delete a process right away if it exits.  */
int delete_exited_processes;

/* Hash table which maps USIDs as returned by create_stream_pair_cb to
   process objects. Processes are not GC-protected through this! */
struct hash_table *usid_to_process;

/* List of process objects. */
Lisp_Object Vprocess_list;

Lisp_Object Vnull_device;

/* Cons of coding systems used to initialize process I/O on a newly-
   created process. */
Lisp_Object Vdefault_process_coding_system;

Lisp_Object Qprocess_error;
Lisp_Object Qnetwork_error;

Fixnum debug_process_io;

Lisp_Object Vshell_file_name;

/* The environment to pass to all subprocesses when they are started.
   This is in the semi-bogus format of ("VAR=VAL" "VAR2=VAL2" ... )
 */
Lisp_Object Vprocess_environment;

/* Make sure egetenv() not called too soon */
int env_initted;

Lisp_Object Vlisp_EXEC_SUFFIXES;



static Lisp_Object
mark_process (Lisp_Object object)
{
  Lisp_Process *process = XPROCESS (object);
  MAYBE_PROCMETH (mark_process_data, (process));
  mark_object (process->name);
  mark_object (process->command);
  mark_object (process->filter);
  mark_object (process->sentinel);
  mark_object (process->buffer);
  mark_object (process->mark);
  mark_object (process->pid);
  mark_object (process->pipe_instream);
  mark_object (process->pipe_outstream);
  mark_object (process->coding_instream);
  mark_object (process->coding_outstream);
  return process->status_symbol;
}

static void
print_process (Lisp_Object object, Lisp_Object printcharfun, int escapeflag)
{
  Lisp_Process *process = XPROCESS (object);

  if (print_readably)
    printing_unreadable_object ("#<process %s>", XSTRING_DATA (process->name));

  if (!escapeflag)
    {
      print_internal (process->name, printcharfun, 0);
    }
  else
    {
      int netp = network_connection_p (object);
      write_c_string ((netp ? GETTEXT ("#<network connection ") :
		       GETTEXT ("#<process ")), printcharfun);
      print_internal (process->name, printcharfun, 1);
      write_c_string ((netp ? " " : " pid "), printcharfun);
      print_internal (process->pid, printcharfun, 1);
      write_fmt_string_lisp (printcharfun, " state:%S", 1, process->status_symbol);
      MAYBE_PROCMETH (print_process_data, (process, printcharfun));
      write_c_string (">", printcharfun);
    }
}

#ifdef HAVE_WINDOW_SYSTEM
extern void debug_process_finalization (Lisp_Process *p);
#endif /* HAVE_WINDOW_SYSTEM */

static void
finalize_process (void *header, int for_disksave)
{
  /* #### this probably needs to be tied into the tty event loop */
  /* #### when there is one */
  Lisp_Process *p = (Lisp_Process *) header;
#ifdef HAVE_WINDOW_SYSTEM
  if (!for_disksave)
    {
      debug_process_finalization (p);
    }
#endif /* HAVE_WINDOW_SYSTEM */

  if (p->process_data)
    {
      MAYBE_PROCMETH (finalize_process_data, (p, for_disksave));
      if (!for_disksave)
	xfree (p->process_data);
    }
}

DEFINE_LRECORD_IMPLEMENTATION ("process", process,
                               mark_process, print_process, finalize_process,
                               0, 0, 0, Lisp_Process);

/************************************************************************/
/*                       basic process accessors                        */
/************************************************************************/

/* This function returns low-level streams, connected directly to the child
   process, rather than en/decoding streams */
void
get_process_streams (Lisp_Process *p, Lisp_Object *instr, Lisp_Object *outstr)
{
  assert (p);
  assert (NILP (p->pipe_instream) || LSTREAMP(p->pipe_instream));
  assert (NILP (p->pipe_outstream) || LSTREAMP(p->pipe_outstream));
  *instr = p->pipe_instream;
  *outstr = p->pipe_outstream;
}

Lisp_Process *
get_process_from_usid (USID usid)
{
  const void *vval;

  assert (usid != USID_ERROR && usid != USID_DONTHASH);

  if (gethash ((const void*)usid, usid_to_process, &vval))
    {
      Lisp_Object process;
      CVOID_TO_LISP (process, vval);
      return XPROCESS (process);
    }
  else
    return 0;
}

int
get_process_selected_p (Lisp_Process *p)
{
  return p->selected;
}

void
set_process_selected_p (Lisp_Process *p, int selected_p)
{
  p->selected = !!selected_p;
}

int
connected_via_filedesc_p (Lisp_Process *p)
{
  return MAYBE_INT_PROCMETH (tooltalk_connection_p, (p));
}

#ifdef HAVE_SOCKETS
int
network_connection_p (Lisp_Object process)
{
  return CONSP (XPROCESS (process)->pid);
}
#endif

DEFUN ("processp", Fprocessp, 1, 1, 0, /*
Return t if OBJECT is a process.
*/
       (object))
{
  return PROCESSP (object) ? Qt : Qnil;
}

DEFUN ("process-live-p", Fprocess_live_p, 1, 1, 0, /*
Return t if OBJECT is a process that is alive.
*/
       (object))
{
  return PROCESSP (object) && PROCESS_LIVE_P (XPROCESS (object))
    ? Qt : Qnil;
}

DEFUN ("process-list", Fprocess_list, 0, 0, 0, /*
Return a list of all processes.
*/
       ())
{
  return Fcopy_sequence (Vprocess_list);
}

DEFUN ("get-process", Fget_process, 1, 1, 0, /*
Return the process named PROCESS-NAME (a string), or nil if there is none.
PROCESS-NAME may also be a process; if so, the value is that process.
*/
       (process_name))
{
  if (PROCESSP (process_name))
    return process_name;

  if (!gc_in_progress)
    /* this only gets called during GC when emacs is going away as a result
       of a signal or crash. */
    CHECK_STRING (process_name);

  {
    LIST_LOOP_2 (process, Vprocess_list)
      if (internal_equal (process_name, XPROCESS (process)->name, 0))
        return process;
  }
  return Qnil;
}

DEFUN ("get-buffer-process", Fget_buffer_process, 1, 1, 0, /*
Return the (or, a) process associated with BUFFER.
BUFFER may be a buffer or the name of one.
*/
       (buffer))
{
  if (NILP (buffer)) return Qnil;
  buffer = Fget_buffer (buffer);
  if (NILP (buffer)) return Qnil;

  {
    LIST_LOOP_2 (process, Vprocess_list)
      if (EQ (XPROCESS (process)->buffer, buffer))
        return process;
  }
  return Qnil;
}

/* This is how commands for the user decode process arguments.  It
   accepts a process, a process name, a buffer, a buffer name, or nil.
   Buffers denote the first process in the buffer, and nil denotes the
   current buffer.  */

static Lisp_Object
get_process (Lisp_Object name)
{
  Lisp_Object buffer;

#ifdef I18N3
  /* #### Look more closely into translating process names. */
#endif

  /* This may be called during a GC from process_send_signal() from
     kill_buffer_processes() if emacs decides to abort(). */
  if (PROCESSP (name))
    return name;
  else if (STRINGP (name))
    {
      Lisp_Object object = Fget_process (name);
      if (PROCESSP (object))
	return object;

      buffer = Fget_buffer (name);
      if (BUFFERP (buffer))
	goto have_buffer_object;

      invalid_argument ("Process does not exist", name);
    }
  else if (NILP (name))
    {
      buffer = Fcurrent_buffer ();
      goto have_buffer_object;
    }
  else if (BUFFERP (name))
    {
      Lisp_Object process;
      buffer = name;

    have_buffer_object:
      process = Fget_buffer_process (buffer);
      if (PROCESSP (process))
	return process;

      invalid_argument ("Buffer has no process", buffer);
    }
  else
    return get_process (Fsignal (Qwrong_type_argument,
				 (list2 (build_msg_string ("process or buffer or nil"),
				 name))));
}

DEFUN ("process-id", Fprocess_id, 1, 1, 0, /*
Return the process id of PROCESS.
This is the pid of the Unix process which PROCESS uses or talks to.
For a network connection, this value is a cons of
 (foreign-network-port . foreign-host-name).
*/
       (process))
{
  Lisp_Object pid;
  CHECK_PROCESS (process);

  pid = XPROCESS (process)->pid;
  if (network_connection_p (process))
    /* return Qnil; */
    return Fcons (Fcar (pid), Fcdr (pid));
  else
    return pid;
}

DEFUN ("process-name", Fprocess_name, 1, 1, 0, /*
Return the name of PROCESS, as a string.
This is the name of the program invoked in PROCESS,
possibly modified to make it unique among process names.
*/
       (process))
{
  CHECK_PROCESS (process);
  return XPROCESS (process)->name;
}

DEFUN ("process-command", Fprocess_command, 1, 1, 0, /*
Return the command that was executed to start PROCESS.
This is a list of strings, the first string being the program executed
and the rest of the strings being the arguments given to it.
*/
       (process))
{
  CHECK_PROCESS (process);
  return XPROCESS (process)->command;
}


/************************************************************************/
/*                          creating a process                          */
/************************************************************************/

DOESNT_RETURN
report_process_error (const char *string, Lisp_Object data)
{
  report_error_with_errno (Qprocess_error, string, data);
}

DOESNT_RETURN
report_network_error (const char *string, Lisp_Object data)
{
  report_error_with_errno (Qnetwork_error, string, data);
}

Lisp_Object
make_process_internal (Lisp_Object name)
{
  Lisp_Object val, name1;
  int i;
  Lisp_Process *p = alloc_lcrecord_type (Lisp_Process, &lrecord_process);

  /* If name is already in use, modify it until it is unused.  */
  name1 = name;
  for (i = 1; ; i++)
    {
      char suffix[10];
      Lisp_Object tem = Fget_process (name1);
      if (NILP (tem))
        break;
      sprintf (suffix, "<%d>", i);
      name1 = concat2 (name, build_string (suffix));
    }
  name = name1;
  p->name = name;

  p->command  = Qnil;
  p->filter   = Qnil;
  p->sentinel = Qnil;
  p->buffer   = Qnil;
  p->mark = Fmake_marker ();
  p->pid = Qnil;
  p->status_symbol = Qrun;
  p->exit_code = 0;
  p->core_dumped = 0;
  p->filter_does_read = 0;
  p->kill_without_query = 0;
  p->selected = 0;
  p->tick = 0;
  p->update_tick = 0;
  p->pipe_instream  = Qnil;
  p->pipe_outstream = Qnil;
  p->coding_instream  = Qnil;
  p->coding_outstream = Qnil;

  p->process_data = 0;
  MAYBE_PROCMETH (alloc_process_data, (p));

  val = wrap_process (p);

  Vprocess_list = Fcons (val, Vprocess_list);
  return val;
}

void
init_process_io_handles (Lisp_Process *p, void* in, void* out, int flags)
{
  USID usid;
  Lisp_Object incode, outcode;

  if (!CONSP (Vdefault_process_coding_system) ||
      NILP (incode = (find_coding_system_for_text_file
		      (Fcar (Vdefault_process_coding_system), 1))) ||
      NILP (outcode = (find_coding_system_for_text_file
		       (Fcdr (Vdefault_process_coding_system), 0))))
    signal_error (Qinvalid_state,
		  "Bogus value for `default-process-coding-system'",
		  Vdefault_process_coding_system);

  if (!NILP (Vcoding_system_for_read) &&
      NILP (incode = (find_coding_system_for_text_file
		      (Vcoding_system_for_read, 1))))
    signal_error (Qinvalid_state,
		  "Bogus value for `coding-system-for-read'",
		  Vcoding_system_for_read);

  if (!NILP (Vcoding_system_for_write) &&
      NILP (outcode = (find_coding_system_for_text_file
		       (Vcoding_system_for_write, 0))))
    signal_error (Qinvalid_state,
		  "Bogus value for `coding-system-for-write'",
		  Vcoding_system_for_write);

  usid = event_stream_create_stream_pair (in, out,
					  &p->pipe_instream,
					  &p->pipe_outstream,
					  flags);

  if (usid == USID_ERROR)
    signal_error (Qprocess_error, "Setting up communication with subprocess",
		  Qunbound);

  if (usid != USID_DONTHASH)
    {
      Lisp_Object process = Qnil;
      process = wrap_process (p);
      puthash ((const void *) usid, LISP_TO_VOID (process), usid_to_process);
    }

  MAYBE_PROCMETH (init_process_io_handles, (p, in, out, flags));

  p->coding_instream =
    make_coding_input_stream (XLSTREAM (p->pipe_instream), incode,
			      CODING_DECODE, 0);
  p->coding_outstream =
    make_coding_output_stream (XLSTREAM (p->pipe_outstream), outcode,
			       CODING_ENCODE, 0);
}

static void
create_process (Lisp_Object process, Lisp_Object *argv, int nargv,
		Lisp_Object program, Lisp_Object cur_dir)
{
  Lisp_Process *p = XPROCESS (process);
  int pid;

  /* *_create_process may change status_symbol, if the process
     is a kind of "fire-and-forget" (no I/O, unwaitable) */
  p->status_symbol = Qrun;
  p->exit_code = 0;

  pid = PROCMETH (create_process, (p, argv, nargv, program, cur_dir));

  p->pid = make_int (pid);
  if (PROCESS_LIVE_P (p))
    event_stream_select_process (p);
}

/* This function is the unwind_protect form for Fstart_process_internal.  If
   PROCESS doesn't have its pid set, then we know someone has signalled
   an error and the process wasn't started successfully, so we should
   remove it from the process list.  */
static void remove_process (Lisp_Object process);
static Lisp_Object
start_process_unwind (Lisp_Object process)
{
  /* Was PROCESS started successfully?  */
  if (EQ (XPROCESS (process)->pid, Qnil))
    remove_process (process);
  return Qnil;
}

DEFUN ("start-process-internal", Fstart_process_internal, 3, MANY, 0, /*
Start a program in a subprocess.  Return the process object for it.
Args are NAME BUFFER PROGRAM &rest PROGRAM-ARGS
NAME is name for process.  It is modified if necessary to make it unique.
BUFFER is the buffer or (buffer-name) to associate with the process.
 Process output goes at end of that buffer, unless you specify
 an output stream or filter function to handle the output.
 BUFFER may be also nil, meaning that this process is not associated
 with any buffer
Third arg is program file name.  It is searched for as in the shell.
Remaining arguments are strings to give program as arguments.
INCODE and OUTCODE specify the coding-system objects used in input/output
 from/to the process.
*/
       (int nargs, Lisp_Object *args))
{
  /* This function can call lisp */
  Lisp_Object buffer, name, program, process, current_dir;
  Lisp_Object tem;
  int speccount = specpdl_depth ();
  struct gcpro gcpro1, gcpro2, gcpro3;

  name = args[0];
  buffer = args[1];
  program = args[2];
  current_dir = Qnil;

  /* Protect against various file handlers doing GCs below. */
  GCPRO3 (buffer, program, current_dir);

  if (!NILP (buffer))
    buffer = Fget_buffer_create (buffer);

  CHECK_STRING (name);
  CHECK_STRING (program);

  /* Make sure that the child will be able to chdir to the current
     buffer's current directory, or its unhandled equivalent. [[ We
     can't just have the child check for an error when it does the
     chdir, since it's in a vfork. ]] -- not any more, we don't use
     vfork. -ben

     Note: These calls are spread out to insure that the return values
     of the calls (which may be newly-created strings) are properly
     GC-protected. */
  current_dir = current_buffer->directory;
    /* If the current dir has no terminating slash, we'll get undesirable
       results, so put the slash back. */
  current_dir = Ffile_name_as_directory (current_dir);
  current_dir = Funhandled_file_name_directory (current_dir);
  current_dir = expand_and_dir_to_file (current_dir, Qnil);

#if 0	/* This loser breaks ange-ftp */
  /* dmoore - if you re-enable this code, you have to gcprotect
     current_buffer through the above calls. */
  if (NILP (Ffile_accessible_directory_p (current_dir)))
    signal_error (Qprocess_error, "Setting current directory",
		  current_buffer->directory);
#endif /* 0 */

  /* If program file name is not absolute, search our path for it */
  if (!IS_DIRECTORY_SEP (XSTRING_BYTE (program, 0))
      && !(XSTRING_LENGTH (program) > 1
	   && IS_DEVICE_SEP (XSTRING_BYTE (program, 1))))
    {
      struct gcpro ngcpro1;

      tem = Qnil;
      NGCPRO1 (tem);
      locate_file (Vexec_path, program, Vlisp_EXEC_SUFFIXES, &tem, X_OK);
      if (NILP (tem))
	signal_error (Qprocess_error, "Searching for program", program);
      program = Fexpand_file_name (tem, Qnil);
      NUNGCPRO;
    }
  else
    {
      /* we still need to canonicalize it and ensure it has the proper
	 ending, e.g. .exe */
      struct gcpro ngcpro1;

      tem = Qnil;
      NGCPRO1 (tem);
      locate_file (list1 (build_string ("")), program, Vlisp_EXEC_SUFFIXES,
		   &tem, X_OK);
      if (NILP (tem))
	signal_error (Qprocess_error, "Searching for program", program);
      program = tem;
      NUNGCPRO;
    }

  if (!NILP (Ffile_directory_p (program)))
    invalid_operation ("Specified program for new process is a directory",
		       program);

  process = make_process_internal (name);

  XPROCESS (process)->buffer = buffer;
  XPROCESS (process)->command = Flist (nargs - 2, args + 2);

  /* Make the process marker point into the process buffer (if any).  */
  if (!NILP (buffer))
    Fset_marker (XPROCESS (process)->mark,
		 make_int (BUF_ZV (XBUFFER (buffer))), buffer);

  /* If an error occurs and we can't start the process, we want to
     remove it from the process list.  This means that each error
     check in create_process doesn't need to call remove_process
     itself; it's all taken care of here.  */
  record_unwind_protect (start_process_unwind, process);

  create_process (process, args + 3, nargs - 3, program, current_dir);

  UNGCPRO;
  return unbind_to_1 (speccount, process);
}


#ifdef HAVE_SOCKETS


/* #### The network support is fairly synthetical. What we actually
   need is a single function, which supports all datagram, stream and
   packet stream connections, arbitrary protocol families should they
   be supported by the target system, multicast groups, in both data
   and control rooted/nonrooted flavors, service quality etc whatever
   is supported by the underlying network.

   It must accept a property list describing the connection. The current
   functions must then go to lisp and provide a suitable list for the
   generalized connection function.

   Both UNIX and Win32 support BSD sockets, and there are many extensions
   available (Sockets 2 spec).

   A todo is define a consistent set of properties abstracting a
   network connection.   -kkm
*/


/* open a TCP network connection to a given HOST/SERVICE.  Treated
   exactly like a normal process when reading and writing.  Only
   differences are in status display and process deletion.  A network
   connection has no PID; you cannot signal it.  All you can do is
   deactivate and close it via delete-process */

DEFUN ("open-network-stream-internal", Fopen_network_stream_internal, 4, 5,
       0, /*
Open a TCP connection for a service to a host.
Return a process object to represent the connection.
Input and output work as for subprocesses; `delete-process' closes it.

NAME is name for process.  It is modified if necessary to make it unique.
BUFFER is the buffer (or buffer-name) to associate with the process.
 Process output goes at end of that buffer, unless you specify
 an output stream or filter function to handle the output.
 BUFFER may also be nil, meaning that this process is not associated
 with any buffer.
Third arg HOST (a string) is  the name of the host to connect to,
 or its IP address.
Fourth arg SERVICE is the name of the service desired (a string),
 or an integer specifying a port number to connect to.
Optional fifth arg PROTOCOL is a network protocol.  Currently only 'tcp
 (Transmission Control Protocol) and 'udp (User Datagram Protocol) are
 supported.  When omitted, 'tcp is assumed.

Output via `process-send-string' and input via buffer or filter (see
`set-process-filter') are stream-oriented.  That means UDP datagrams are
not guaranteed to be sent and received in discrete packets. (But small
datagrams around 500 bytes that are not truncated by `process-send-string'
are usually fine.)  Note further that the UDP protocol does not guard
against lost packets.
*/
       (name, buffer, host, service, protocol))
{
  /* This function can GC */
  Lisp_Object process = Qnil;
  struct gcpro gcpro1, gcpro2, gcpro3, gcpro4, gcpro5, ngcpro1;
  void *inch, *outch;

  GCPRO5 (name, buffer, host, service, protocol);
  CHECK_STRING (name);

  if (NILP (protocol))
    protocol = Qtcp;
  else
    CHECK_SYMBOL (protocol);

  /* Since this code is inside HAVE_SOCKETS, existence of
     open_network_stream is mandatory */
  PROCMETH (open_network_stream, (name, host, service, protocol,
				  &inch, &outch));

  if (!NILP (buffer))
    buffer = Fget_buffer_create (buffer);
  process = make_process_internal (name);
  NGCPRO1 (process);

  XPROCESS (process)->pid = Fcons (service, host);
  XPROCESS (process)->buffer = buffer;
  init_process_io_handles (XPROCESS (process), (void *) inch, (void *) outch,
			   STREAM_NETWORK_CONNECTION);

  event_stream_select_process (XPROCESS (process));

  UNGCPRO;
  NUNGCPRO;
  return process;
}

#ifdef HAVE_MULTICAST

DEFUN ("open-multicast-group-internal", Fopen_multicast_group_internal, 5, 5, 0, /*
Open a multicast connection on the specified dest/port/ttl.
Return a process object to represent the connection.
Input and output work as for subprocesses; `delete-process' closes it.

NAME is name for process.  It is modified if necessary to make it unique.
BUFFER is the buffer (or buffer-name) to associate with the process.
 Process output goes at end of that buffer, unless you specify
 an output stream or filter function to handle the output.
 BUFFER may also be nil, meaning that this process is not associated
 with any buffer.
Third, fourth and fifth args are the multicast destination group, port and ttl.
 dest must be an internet address between 224.0.0.0 and 239.255.255.255
 port is a communication port like in traditional unicast
 ttl is the time-to-live (15 for site, 63 for region and 127 for world)
*/
       (name, buffer, dest, port, ttl))
{
  /* This function can GC */
  Lisp_Object process = Qnil;
  struct gcpro gcpro1;
  void *inch, *outch;

  CHECK_STRING (name);

  /* Since this code is inside HAVE_MULTICAST, existence of
     open_multicast_group is mandatory */
  PROCMETH (open_multicast_group, (name, dest, port, ttl,
				   &inch, &outch));

  if (!NILP (buffer))
    buffer = Fget_buffer_create (buffer);

  process = make_process_internal (name);
  GCPRO1 (process);

  XPROCESS (process)->pid = Fcons (port, dest);
  XPROCESS (process)->buffer = buffer;
  init_process_io_handles (XPROCESS (process), (void*)inch, (void*)outch,
			   STREAM_NETWORK_CONNECTION);

  event_stream_select_process (XPROCESS (process));

  UNGCPRO;
  return process;
}
#endif /* HAVE_MULTICAST */

#endif	/* HAVE_SOCKETS */

Lisp_Object
canonicalize_host_name (Lisp_Object host)
{
  return PROCMETH_OR_GIVEN (canonicalize_host_name, (host), host);
}


DEFUN ("set-process-window-size", Fset_process_window_size, 3, 3, 0, /*
Tell PROCESS that it has logical window size HEIGHT and WIDTH.
*/
       (process, height, width))
{
  CHECK_PROCESS (process);
  CHECK_NATNUM (height);
  CHECK_NATNUM (width);
  return
    MAYBE_INT_PROCMETH (set_window_size,
			(XPROCESS (process), XINT (height), XINT (width))) <= 0
    ? Qnil : Qt;
}


/************************************************************************/
/*                              Process I/O                             */
/************************************************************************/

/* Read pending output from the process channel,
   starting with our buffered-ahead character if we have one.
   Yield number of characters read.

   This function reads at most 1024 bytes.
   If you want to read all available subprocess output,
   you must call it repeatedly until it returns zero.  */

Charcount
read_process_output (Lisp_Object process)
{
  /* This function can GC */
  Bytecount nbytes, nchars;
  Intbyte chars[1025];
  Lisp_Object outstream;
  Lisp_Process *p = XPROCESS (process);

  /* If there is a lot of output from the subprocess, the loop in
     execute_internal_event() might call read_process_output() more
     than once.  If the filter that was executed from one of these
     calls set the filter to t, we have to stop now.  Return -1 rather
     than 0 so execute_internal_event() doesn't close the process.
     Really, the loop in execute_internal_event() should check itself
     for a process-filter change, like in status_notify(); but the
     struct Lisp_Process is not exported outside of this file. */
  if (!PROCESS_LIVE_P (p))
    return -1; /* already closed */

  if (!NILP (p->filter) && (p->filter_does_read))
    {
      Lisp_Object filter_result;

      /* Some weird FSFmacs crap here with
	 Vdeactivate_mark and current_buffer->keymap */
      running_asynch_code = 1;
      filter_result = call2_trapping_errors ("Error in process filter",
					     p->filter, process, Qnil);
      running_asynch_code = 0;
      restore_match_data ();
      CHECK_INT (filter_result);
      return XINT (filter_result);
    }

  nbytes = Lstream_read (XLSTREAM (DATA_INSTREAM (p)), chars,
			 sizeof (chars) - 1);
  if (nbytes <= 0) return nbytes;

  if (debug_process_io)
    {
      chars[nbytes] = '\0';
      stderr_out ("Read: %s\n", chars);
    }

  /* !!#### if the coding system changed as a result of reading, we
     need to change the output coding system accordingly. */
  nchars = bytecount_to_charcount (chars, nbytes);
  outstream = p->filter;
  if (!NILP (outstream))
    {
      /* We used to bind inhibit-quit to t here, but
	 call2_trapping_errors() does that for us. */
      running_asynch_code = 1;
      call2_trapping_errors ("Error in process filter",
			     outstream, process, make_string (chars, nbytes));
      running_asynch_code = 0;
      restore_match_data ();
      return nchars;
    }

  /* If no filter, write into buffer if it isn't dead.  */
  if (!NILP (p->buffer) && BUFFER_LIVE_P (XBUFFER (p->buffer)))
    {
      Lisp_Object old_read_only = Qnil;
      Charbpos old_point;
      Charbpos old_begv;
      Charbpos old_zv;
      struct gcpro gcpro1, gcpro2;
      struct buffer *buf = XBUFFER (p->buffer);

      GCPRO2 (process, old_read_only);

      old_point = BUF_PT (buf);
      old_begv = BUF_BEGV (buf);
      old_zv = BUF_ZV (buf);
      old_read_only = buf->read_only;
      buf->read_only = Qnil;

      /* Insert new output into buffer
	 at the current end-of-output marker,
	 thus preserving logical ordering of input and output.  */
      if (XMARKER (p->mark)->buffer)
	BUF_SET_PT (buf,
		    charbpos_clip_to_bounds (old_begv, marker_position (p->mark),
					   old_zv));
      else
	BUF_SET_PT (buf, old_zv);

      /* If the output marker is outside of the visible region, save
	 the restriction and widen.  */
      if (! (BUF_BEGV (buf) <= BUF_PT (buf) &&
	     BUF_PT (buf) <= BUF_ZV (buf)))
	Fwiden (p->buffer);

      /* Make sure opoint floats ahead of any new text, just as point
	 would.  */
      if (BUF_PT (buf) <= old_point)
	old_point += nchars;

      /* Insert after old_begv, but before old_zv.  */
      if (BUF_PT (buf) < old_begv)
	old_begv += nchars;
      if (BUF_PT (buf) <= old_zv)
	old_zv += nchars;

#if 0
      /* This screws up initial display of the window.  jla */

      /* Insert before markers in case we are inserting where
	 the buffer's mark is, and the user's next command is Meta-y.  */
      buffer_insert_raw_string_1 (buf, -1, chars,
				  nbytes, INSDEL_BEFORE_MARKERS);
#else
      buffer_insert_raw_string (buf, chars, nbytes);
#endif

      Fset_marker (p->mark, make_int (BUF_PT (buf)), p->buffer);

      MARK_MODELINE_CHANGED;

      /* If the restriction isn't what it should be, set it.  */
      if (old_begv != BUF_BEGV (buf) || old_zv != BUF_ZV (buf))
	{
	  Fwiden(p->buffer);
	  old_begv = charbpos_clip_to_bounds (BUF_BEG (buf),
					    old_begv,
					    BUF_Z (buf));
	  old_zv = charbpos_clip_to_bounds (BUF_BEG (buf),
					  old_zv,
					  BUF_Z (buf));
	  Fnarrow_to_region (make_int (old_begv), make_int (old_zv),
			     p->buffer);
	}

      buf->read_only = old_read_only;
      old_point = charbpos_clip_to_bounds (BUF_BEGV (buf),
					 old_point,
					 BUF_ZV (buf));
      BUF_SET_PT (buf, old_point);

      UNGCPRO;
    }
  return nchars;
}

/* Sending data to subprocess */

/* send some data to process PROCESS.  If NONRELOCATABLE is non-NULL, it
   specifies the address of the data.  Otherwise, the data comes from the
   object RELOCATABLE (either a string or a buffer).  START and LEN
   specify the offset and length of the data to send.

   Note that START and LEN are in Charbpos's if RELOCATABLE is a buffer,
   and in Bytecounts otherwise. */

void
send_process (Lisp_Object process,
              Lisp_Object relocatable, const Intbyte *nonrelocatable,
              int start, int len)
{
  /* This function can GC */
  struct gcpro gcpro1, gcpro2;
  Lisp_Object lstream = Qnil;

  GCPRO2 (process, lstream);

  if (NILP (DATA_OUTSTREAM (XPROCESS (process))))
    invalid_operation ("Process not open for writing", process);

  if (nonrelocatable)
    lstream =
      make_fixed_buffer_input_stream (nonrelocatable + start, len);
  else if (BUFFERP (relocatable))
    lstream = make_lisp_buffer_input_stream (XBUFFER (relocatable),
					     start, start + len, 0);
  else
    lstream = make_lisp_string_input_stream (relocatable, start, len);

  if (debug_process_io)
    {
      if (nonrelocatable)
	stderr_out ("Writing: %s\n", nonrelocatable);
      else
	{
	  stderr_out ("Writing: ");
	  print_internal (relocatable, Qexternal_debugging_output, 0);
	}
    }

  PROCMETH (send_process, (process, XLSTREAM (lstream)));

  UNGCPRO;
  Lstream_delete (XLSTREAM (lstream));
}

DEFUN ("process-tty-name", Fprocess_tty_name, 1, 1, 0, /*
Return the name of the terminal PROCESS uses, or nil if none.
This is the terminal that the process itself reads and writes on,
not the name of the pty that Emacs uses to talk with that terminal.
*/
       (process))
{
  CHECK_PROCESS (process);
  return MAYBE_LISP_PROCMETH (get_tty_name, (XPROCESS (process)));
}

DEFUN ("set-process-buffer", Fset_process_buffer, 2, 2, 0, /*
Set buffer associated with PROCESS to BUFFER (a buffer, or nil).
*/
       (process, buffer))
{
  CHECK_PROCESS (process);
  if (!NILP (buffer))
    CHECK_BUFFER (buffer);
  XPROCESS (process)->buffer = buffer;
  return buffer;
}

DEFUN ("process-buffer", Fprocess_buffer, 1, 1, 0, /*
Return the buffer PROCESS is associated with.
Output from PROCESS is inserted in this buffer
unless PROCESS has a filter.
*/
       (process))
{
  CHECK_PROCESS (process);
  return XPROCESS (process)->buffer;
}

DEFUN ("process-mark", Fprocess_mark, 1, 1, 0, /*
Return the marker for the end of the last output from PROCESS.
*/
       (process))
{
  CHECK_PROCESS (process);
  return XPROCESS (process)->mark;
}

void
set_process_filter (Lisp_Object process, Lisp_Object filter, int filter_does_read)
{
  CHECK_PROCESS (process);
  if (PROCESS_LIVE_P (XPROCESS (process))) {
    if (EQ (filter, Qt))
      event_stream_unselect_process (XPROCESS (process));
    else
      event_stream_select_process (XPROCESS (process));
  }

  XPROCESS (process)->filter = filter;
  XPROCESS (process)->filter_does_read = filter_does_read;
}

DEFUN ("set-process-filter", Fset_process_filter, 2, 2, 0, /*
Give PROCESS the filter function FILTER; nil means no filter.
t means stop accepting output from the process.
When a process has a filter, each time it does output
the entire string of output is passed to the filter.
The filter gets two arguments: the process and the string of output.
If the process has a filter, its buffer is not used for output.
*/
       (process, filter))
{
  set_process_filter (process, filter, 0);
  return filter;
}

DEFUN ("process-filter", Fprocess_filter, 1, 1, 0, /*
Return the filter function of PROCESS; nil if none.
See `set-process-filter' for more info on filter functions.
*/
       (process))
{
  CHECK_PROCESS (process);
  return XPROCESS (process)->filter;
}

DEFUN ("process-send-region", Fprocess_send_region, 3, 4, 0, /*
Send current contents of the region between START and END as input to PROCESS.
PROCESS may be a process or the name of a process, or a buffer or the
name of a buffer, in which case the buffer's process is used.  If it
is nil, the current buffer's process is used.
BUFFER specifies the buffer to look in; if nil, the current buffer is used.
If STRING is more than 100 or so characters long, it may be sent in
several chunks.  This may happen even for shorter strings.  Output
from processes can arrive in between chunks.
*/
       (process, start, end, buffer))
{
  /* This function can GC */
  Charbpos bstart, bend;
  struct buffer *buf = decode_buffer (buffer, 0);

  buffer = wrap_buffer (buf);
  process = get_process (process);
  get_buffer_range_char (buf, start, end, &bstart, &bend, 0);

  send_process (process, buffer, 0, bstart, bend - bstart);
  return Qnil;
}

DEFUN ("process-send-string", Fprocess_send_string, 2, 4, 0, /*
Send PROCESS the contents of STRING as input.
PROCESS may be a process or the name of a process, or a buffer or the
name of a buffer, in which case the buffer's process is used.  If it
is nil, the current buffer's process is used.
Optional arguments START and END specify part of STRING; see `substring'.
If STRING is more than 100 or so characters long, it may be sent in
several chunks.  This may happen even for shorter strings.  Output
from processes can arrive in between chunks.
*/
       (process, string, start, end))
{
  /* This function can GC */
  Bytecount bstart, bend;

  process = get_process (process);
  CHECK_STRING (string);
  get_string_range_byte (string, start, end, &bstart, &bend,
			 GB_HISTORICAL_STRING_BEHAVIOR);

  send_process (process, string, 0, bstart, bend - bstart);
  return Qnil;
}


DEFUN ("process-input-coding-system", Fprocess_input_coding_system, 1, 1, 0, /*
Return PROCESS's input coding system.
*/
       (process))
{
  process = get_process (process);
  CHECK_LIVE_PROCESS (process);
  return (coding_stream_detected_coding_system
	  (XLSTREAM (XPROCESS (process)->coding_instream)));
}

DEFUN ("process-output-coding-system", Fprocess_output_coding_system, 1, 1, 0, /*
Return PROCESS's output coding system.
*/
       (process))
{
  process = get_process (process);
  CHECK_LIVE_PROCESS (process);
  return (coding_stream_coding_system
	  (XLSTREAM (XPROCESS (process)->coding_outstream)));
}

DEFUN ("process-coding-system", Fprocess_coding_system, 1, 1, 0, /*
Return a pair of coding-system for decoding and encoding of PROCESS.
*/
       (process))
{
  process = get_process (process);
  CHECK_LIVE_PROCESS (process);
  return Fcons (coding_stream_detected_coding_system
		(XLSTREAM (XPROCESS (process)->coding_instream)),
		coding_stream_coding_system
		(XLSTREAM (XPROCESS (process)->coding_outstream)));
}

DEFUN ("set-process-input-coding-system", Fset_process_input_coding_system,
       2, 2, 0, /*
Set PROCESS's input coding system to CODESYS.
This is used for reading data from PROCESS.
*/
       (process, codesys))
{
  codesys = get_coding_system_for_text_file (codesys, 1);
  process = get_process (process);
  CHECK_LIVE_PROCESS (process);

  set_coding_stream_coding_system
    (XLSTREAM (XPROCESS (process)->coding_instream), codesys);
  return Qnil;
}

DEFUN ("set-process-output-coding-system", Fset_process_output_coding_system,
       2, 2, 0, /*
Set PROCESS's output coding system to CODESYS.
This is used for writing data to PROCESS.
*/
       (process, codesys))
{
  codesys = get_coding_system_for_text_file (codesys, 0);
  process = get_process (process);
  CHECK_LIVE_PROCESS (process);

  set_coding_stream_coding_system
    (XLSTREAM (XPROCESS (process)->coding_outstream), codesys);
  return Qnil;
}

DEFUN ("set-process-coding-system", Fset_process_coding_system,
       1, 3, 0, /*
Set coding-systems of PROCESS to DECODING and ENCODING.
DECODING will be used to decode subprocess output and ENCODING to
encode subprocess input.
*/
       (process, decoding, encoding))
{
  if (!NILP (decoding))
    Fset_process_input_coding_system (process, decoding);

  if (!NILP (encoding))
    Fset_process_output_coding_system (process, encoding);

  return Qnil;
}


/************************************************************************/
/*                             process status                           */
/************************************************************************/

static Lisp_Object
exec_sentinel_unwind (Lisp_Object datum)
{
  Lisp_Cons *d = XCONS (datum);
  XPROCESS (d->car)->sentinel = d->cdr;
  free_cons (d);
  return Qnil;
}

static void
exec_sentinel (Lisp_Object process, Lisp_Object reason)
{
  /* This function can GC */
  int speccount = specpdl_depth ();
  Lisp_Process *p = XPROCESS (process);
  Lisp_Object sentinel = p->sentinel;

  if (NILP (sentinel))
    return;

  /* Some weird FSFmacs crap here with
     Vdeactivate_mark and current_buffer->keymap */

  /* Zilch the sentinel while it's running, to avoid recursive invocations;
     assure that it gets restored no matter how the sentinel exits.  */
  p->sentinel = Qnil;
  record_unwind_protect (exec_sentinel_unwind, noseeum_cons (process, sentinel));
  /* We used to bind inhibit-quit to t here, but call2_trapping_errors()
     does that for us. */
  running_asynch_code = 1;
  call2_trapping_errors ("Error in process sentinel", sentinel, process, reason);
  running_asynch_code = 0;
  restore_match_data ();
  unbind_to (speccount);
}

DEFUN ("set-process-sentinel", Fset_process_sentinel, 2, 2, 0, /*
Give PROCESS the sentinel SENTINEL; nil for none.
The sentinel is called as a function when the process changes state.
It gets two arguments: the process, and a string describing the change.
*/
       (process, sentinel))
{
  CHECK_PROCESS (process);
  XPROCESS (process)->sentinel = sentinel;
  return sentinel;
}

DEFUN ("process-sentinel", Fprocess_sentinel, 1, 1, 0, /*
Return the sentinel of PROCESS; nil if none.
See `set-process-sentinel' for more info on sentinels.
*/
       (process))
{
  CHECK_PROCESS (process);
  return XPROCESS (process)->sentinel;
}


const char *
signal_name (int signum)
{
  if (signum >= 0 && signum < NSIG)
    return (const char *) sys_siglist[signum];

  return (const char *) GETTEXT ("unknown signal");
}

void
update_process_status (Lisp_Object p,
		       Lisp_Object status_symbol,
		       int exit_code,
		       int core_dumped)
{
  XPROCESS (p)->tick++;
  process_tick++;
  XPROCESS (p)->status_symbol = status_symbol;
  XPROCESS (p)->exit_code = exit_code;
  XPROCESS (p)->core_dumped = core_dumped;
}

/* Return a string describing a process status list.  */

static Lisp_Object
status_message (Lisp_Process *p)
{
  Lisp_Object symbol = p->status_symbol;
  int code = p->exit_code;
  int coredump = p->core_dumped;
  Lisp_Object string, string2;

  if (EQ (symbol, Qsignal) || EQ (symbol, Qstop))
    {
      string = build_string (signal_name (code));
      if (coredump)
	string2 = build_msg_string (" (core dumped)\n");
      else
	string2 = build_string ("\n");
      set_string_char (string, 0,
		       DOWNCASE (0, XSTRING_CHAR (string, 0)));
      return concat2 (string, string2);
    }
  else if (EQ (symbol, Qexit))
    {
      if (code == 0)
	return build_msg_string ("finished\n");
      string = Fnumber_to_string (make_int (code));
      if (coredump)
	string2 = build_msg_string (" (core dumped)\n");
      else
	string2 = build_string ("\n");
      return concat2 (build_msg_string ("exited abnormally with code "),
		      concat2 (string, string2));
    }
  else
    return Fcopy_sequence (Fsymbol_name (symbol));
}

/* Tell status_notify() to check for terminated processes.  We do this
   because on some systems we sometimes miss SIGCHLD calls. (Not sure
   why.) */

void
kick_status_notify (void)
{
  process_tick++;
}


/* Report all recent events of a change in process status
   (either run the sentinel or output a message).
   This is done while Emacs is waiting for keyboard input.  */

void
status_notify (void)
{
  /* This function can GC */
  Lisp_Object tail = Qnil;
  Lisp_Object symbol = Qnil;
  Lisp_Object msg = Qnil;
  struct gcpro gcpro1, gcpro2, gcpro3;
  /* process_tick is volatile, so we have to remember it now.
     Otherwise, we get a race condition if SIGCHLD happens during
     this function.

     (Actually, this is not the case anymore.  The code to
     update the process structures has been moved out of the
     SIGCHLD handler.  But for the moment I'm leaving this
     stuff in -- it can't hurt.) */
  int temp_process_tick;

  MAYBE_PROCMETH (reap_exited_processes, ());

  temp_process_tick = process_tick;

  if (update_tick == temp_process_tick)
    return;

  /* We need to gcpro tail; if read_process_output calls a filter
     which deletes a process and removes the cons to which tail points
     from Vprocess_alist, and then causes a GC, tail is an unprotected
     reference.  */
  GCPRO3 (tail, symbol, msg);

  for (tail = Vprocess_list; CONSP (tail); tail = XCDR (tail))
    {
      Lisp_Object process = XCAR (tail);
      Lisp_Process *p = XPROCESS (process);
      /* p->tick is also volatile.  Same thing as above applies. */
      int this_process_tick;

      /* #### extra check for terminated processes, in case a SIGCHLD
	 got missed (this seems to happen sometimes, I'm not sure why).
       */
      if (INTP (p->pid))
	MAYBE_PROCMETH (update_status_if_terminated, (p));

      this_process_tick = p->tick;
      if (this_process_tick != p->update_tick)
	{
	  p->update_tick = this_process_tick;

	  /* If process is still active, read any output that remains.  */
          while (!EQ (p->filter, Qt)
		 && read_process_output (process) > 0)
            ;

	  /* Get the text to use for the message.  */
	  msg = status_message (p);

	  /* If process is terminated, deactivate it or delete it.  */
	  symbol = p->status_symbol;

	  if (EQ (symbol, Qsignal)
              || EQ (symbol, Qexit))
	    {
	      if (delete_exited_processes)
		remove_process (process);
	      else
		deactivate_process (process);
	    }

	  /* Now output the message suitably.  */
	  if (!NILP (p->sentinel))
	    exec_sentinel (process, msg);
	  /* Don't bother with a message in the buffer
	     when a process becomes runnable.  */
	  else if (!EQ (symbol, Qrun) && !NILP (p->buffer))
	    {
	      Lisp_Object old_read_only = Qnil;
	      Lisp_Object old = Fcurrent_buffer ();
	      Charbpos opoint;
              struct gcpro ngcpro1, ngcpro2;

	      /* Avoid error if buffer is deleted
		 (probably that's why the process is dead, too) */
	      if (!BUFFER_LIVE_P (XBUFFER (p->buffer)))
		continue;

              NGCPRO2 (old, old_read_only);
	      Fset_buffer (p->buffer);
	      opoint = BUF_PT (current_buffer);
	      /* Insert new output into buffer
		 at the current end-of-output marker,
		 thus preserving logical ordering of input and output.  */
	      if (XMARKER (p->mark)->buffer)
		BUF_SET_PT (current_buffer, marker_position (p->mark));
	      else
		BUF_SET_PT (current_buffer, BUF_ZV (current_buffer));
	      if (BUF_PT (current_buffer) <= opoint)
		opoint += (XSTRING_CHAR_LENGTH (msg)
                           + XSTRING_CHAR_LENGTH (p->name)
                           + 10);

	      old_read_only = current_buffer->read_only;
	      current_buffer->read_only = Qnil;
	      buffer_insert_c_string (current_buffer, "\nProcess ");
	      Finsert (1, &p->name);
	      buffer_insert_c_string (current_buffer, " ");
	      Finsert (1, &msg);
	      current_buffer->read_only = old_read_only;
	      Fset_marker (p->mark, make_int (BUF_PT (current_buffer)),
			   p->buffer);

	      opoint = charbpos_clip_to_bounds(BUF_BEGV (XBUFFER (p->buffer)),
					     opoint,
					     BUF_ZV (XBUFFER (p->buffer)));
	      BUF_SET_PT (current_buffer, opoint);
	      Fset_buffer (old);
              NUNGCPRO;
	    }
	}
    } /* end for */

  /* in case buffers use %s in modeline-format */
  MARK_MODELINE_CHANGED;
  redisplay ();

  update_tick = temp_process_tick;

  UNGCPRO;
}

DEFUN ("process-status", Fprocess_status, 1, 1, 0, /*
Return the status of PROCESS.
This is a symbol, one of these:

run    -- for a process that is running.
stop   -- for a process stopped but continuable.
exit   -- for a process that has exited.
signal -- for a process that has got a fatal signal.
open   -- for a network stream connection that is open.
closed -- for a network stream connection that is closed.
nil    -- if arg is a process name and no such process exists.

PROCESS may be a process, a buffer, the name of a process or buffer, or
nil, indicating the current buffer's process.
*/
       (process))
{
  Lisp_Object status_symbol;

  if (STRINGP (process))
    process = Fget_process (process);
  else
    process = get_process (process);

  if (NILP (process))
    return Qnil;

  status_symbol = XPROCESS (process)->status_symbol;
  if (network_connection_p (process))
    {
      if (EQ (status_symbol, Qrun))
	status_symbol = Qopen;
      else if (EQ (status_symbol, Qexit))
	status_symbol = Qclosed;
    }
  return status_symbol;
}

DEFUN ("process-exit-status", Fprocess_exit_status, 1, 1, 0, /*
Return the exit status of PROCESS or the signal number that killed it.
If PROCESS has not yet exited or died, return 0.
*/
       (process))
{
  CHECK_PROCESS (process);
  return make_int (XPROCESS (process)->exit_code);
}



static int
decode_signal (Lisp_Object signal_)
{
  if (INTP (signal_))
    return XINT (signal_);
  else
    {
      Intbyte *name;

      CHECK_SYMBOL (signal_);
      name = XSTRING_DATA (XSYMBOL (signal_)->name);

#define handle_signal(sym) do {			\
	if (!qxestrcmp_c ( name, #sym))		\
	  return sym;				\
      } while (0)

      handle_signal (SIGINT);  /* ANSI */
      handle_signal (SIGILL);  /* ANSI */
      handle_signal (SIGABRT); /* ANSI */
      handle_signal (SIGFPE);  /* ANSI */
      handle_signal (SIGSEGV); /* ANSI */
      handle_signal (SIGTERM); /* ANSI */

#ifdef SIGHUP
      handle_signal (SIGHUP);  /* POSIX */
#endif
#ifdef SIGQUIT
      handle_signal (SIGQUIT); /* POSIX */
#endif
#ifdef SIGTRAP
      handle_signal (SIGTRAP); /* POSIX */
#endif
#ifdef SIGKILL
      handle_signal (SIGKILL); /* POSIX */
#endif
#ifdef SIGUSR1
      handle_signal (SIGUSR1); /* POSIX */
#endif
#ifdef SIGUSR2
      handle_signal (SIGUSR2); /* POSIX */
#endif
#ifdef SIGPIPE
      handle_signal (SIGPIPE); /* POSIX */
#endif
#ifdef SIGALRM
      handle_signal (SIGALRM); /* POSIX */
#endif
#ifdef SIGCHLD
      handle_signal (SIGCHLD); /* POSIX */
#endif
#ifdef SIGCONT
      handle_signal (SIGCONT); /* POSIX */
#endif
#ifdef SIGSTOP
      handle_signal (SIGSTOP); /* POSIX */
#endif
#ifdef SIGTSTP
      handle_signal (SIGTSTP); /* POSIX */
#endif
#ifdef SIGTTIN
      handle_signal (SIGTTIN); /* POSIX */
#endif
#ifdef SIGTTOU
      handle_signal (SIGTTOU); /* POSIX */
#endif

#ifdef SIGBUS
      handle_signal (SIGBUS);  /* XPG5 */
#endif
#ifdef SIGPOLL
      handle_signal (SIGPOLL); /* XPG5 */
#endif
#ifdef SIGPROF
      handle_signal (SIGPROF); /* XPG5 */
#endif
#ifdef SIGSYS
      handle_signal (SIGSYS);  /* XPG5 */
#endif
#ifdef SIGURG
      handle_signal (SIGURG);  /* XPG5 */
#endif
#ifdef SIGXCPU
      handle_signal (SIGXCPU); /* XPG5 */
#endif
#ifdef SIGXFSZ
      handle_signal (SIGXFSZ); /* XPG5 */
#endif
#ifdef SIGVTALRM
      handle_signal (SIGVTALRM); /* XPG5 */
#endif

#ifdef SIGIO
      handle_signal (SIGIO); /* BSD 4.2 */
#endif
#ifdef SIGWINCH
      handle_signal (SIGWINCH); /* BSD 4.3 */
#endif

#ifdef SIGEMT
      handle_signal (SIGEMT);
#endif
#ifdef SIGINFO
      handle_signal (SIGINFO);
#endif
#ifdef SIGHWE
      handle_signal (SIGHWE);
#endif
#ifdef SIGPRE
      handle_signal (SIGPRE);
#endif
#ifdef SIGUME
      handle_signal (SIGUME);
#endif
#ifdef SIGDLK
      handle_signal (SIGDLK);
#endif
#ifdef SIGCPULIM
      handle_signal (SIGCPULIM);
#endif
#ifdef SIGIOT
      handle_signal (SIGIOT);
#endif
#ifdef SIGLOST
      handle_signal (SIGLOST);
#endif
#ifdef SIGSTKFLT
      handle_signal (SIGSTKFLT);
#endif
#ifdef SIGUNUSED
      handle_signal (SIGUNUSED);
#endif
#ifdef SIGDANGER
      handle_signal (SIGDANGER); /* AIX */
#endif
#ifdef SIGMSG
      handle_signal (SIGMSG);
#endif
#ifdef SIGSOUND
      handle_signal (SIGSOUND);
#endif
#ifdef SIGRETRACT
      handle_signal (SIGRETRACT);
#endif
#ifdef SIGGRANT
      handle_signal (SIGGRANT);
#endif
#ifdef SIGPWR
      handle_signal (SIGPWR);
#endif

#undef handle_signal

      invalid_constant ("Undefined signal name", signal_);
      RETURN_NOT_REACHED (0)
    }
}

/* Send signal number SIGNO to PROCESS.
   CURRENT-GROUP non-nil means send signal to the current
   foreground process group of the process's controlling terminal rather
   than to the process's own process group.
   This is used for various commands in shell mode.
   If NOMSG is zero, insert signal-announcements into process's buffers
   right away.

   If we can, we try to signal PROCESS by sending control characters
   down the pty.  This allows us to signal inferiors who have changed
   their uid, for which kill() would return an EPERM error, or to
   processes running on another computer through a remote login.  */

static void
process_send_signal (Lisp_Object process, int signo,
                     int current_group, int nomsg)
{
  /* This function can GC */
  process = get_process (process);

  if (network_connection_p (process))
    invalid_operation ("Network connection is not a subprocess", process);
  CHECK_LIVE_PROCESS (process);

  MAYBE_PROCMETH (kill_child_process, (process, signo, current_group, nomsg));
}

DEFUN ("process-send-signal", Fprocess_send_signal, 1, 3, 0, /*
Send signal SIGNAL to process PROCESS.
SIGNAL may be an integer, or a symbol naming a signal, like `SIGSEGV'.
PROCESS may be a process, a buffer, the name of a process or buffer, or
nil, indicating the current buffer's process.
Third arg CURRENT-GROUP non-nil means send signal to the current
foreground process group of the process's controlling terminal rather
than to the process's own process group.
If the process is a shell that supports job control, this means
send the signal to the current subjob rather than the shell.
*/
       (signal_, process, current_group))
{
  /* This function can GC */
  process_send_signal (process, decode_signal (signal_),
		       !NILP (current_group), 0);
  return process;
}

DEFUN ("interrupt-process", Finterrupt_process, 0, 2, 0, /*
Interrupt process PROCESS.
See function `process-send-signal' for more details on usage.
*/
       (process, current_group))
{
  /* This function can GC */
  process_send_signal (process, SIGINT, !NILP (current_group), 0);
  return process;
}

DEFUN ("kill-process", Fkill_process, 0, 2, 0, /*
Kill process PROCESS.
See function `process-send-signal' for more details on usage.
*/
       (process, current_group))
{
  /* This function can GC */
#ifdef SIGKILL
  process_send_signal (process, SIGKILL, !NILP (current_group), 0);
#else
  signal_error (Qunimplemented,
		     "kill-process: Not supported on this system",
		     Qunbound);
#endif
  return process;
}

DEFUN ("quit-process", Fquit_process, 0, 2, 0, /*
Send QUIT signal to process PROCESS.
See function `process-send-signal' for more details on usage.
*/
       (process, current_group))
{
  /* This function can GC */
#ifdef SIGQUIT
  process_send_signal (process, SIGQUIT, !NILP (current_group), 0);
#else
  signal_error (Qunimplemented,
		     "quit-process: Not supported on this system",
		     Qunbound);
#endif
  return process;
}

DEFUN ("stop-process", Fstop_process, 0, 2, 0, /*
Stop process PROCESS.
See function `process-send-signal' for more details on usage.
*/
       (process, current_group))
{
  /* This function can GC */
#ifdef SIGTSTP
  process_send_signal (process, SIGTSTP, !NILP (current_group), 0);
#else
  signal_error (Qunimplemented,
		     "stop-process: Not supported on this system",
		     Qunbound);
#endif
  return process;
}

DEFUN ("continue-process", Fcontinue_process, 0, 2, 0, /*
Continue process PROCESS.
See function `process-send-signal' for more details on usage.
*/
       (process, current_group))
{
  /* This function can GC */
#ifdef SIGCONT
  process_send_signal (process, SIGCONT, !NILP (current_group), 0);
#else
  signal_error (Qunimplemented,
		     "continue-process: Not supported on this system",
		     Qunbound);
#endif
  return process;
}

DEFUN ("signal-process", Fsignal_process, 2, 2,
       "nProcess number: \nnSignal code: ", /*
Send the process with process id PID the signal with code SIGNAL.
PID must be an integer.  The process need not be a child of this Emacs.
SIGNAL may be an integer, or a symbol naming a signal, like `SIGSEGV'.
*/
       (pid, signal_))
{
  CHECK_INT (pid);

  return make_int (PROCMETH_OR_GIVEN (kill_process_by_pid,
				      (XINT (pid), decode_signal (signal_)),
				      -1));
}

DEFUN ("process-send-eof", Fprocess_send_eof, 0, 1, 0, /*
Make PROCESS see end-of-file in its input.
PROCESS may be a process, a buffer, the name of a process or buffer, or
nil, indicating the current buffer's process.
If PROCESS is a network connection, or is a process communicating
through a pipe (as opposed to a pty), then you cannot send any more
text to PROCESS after you call this function.
*/
       (process))
{
  /* This function can GC */
  process = get_process (process);

  /* Make sure the process is really alive.  */
  if (! EQ (XPROCESS (process)->status_symbol, Qrun))
    invalid_operation ("Process not running", process);

  if (!MAYBE_INT_PROCMETH (process_send_eof, (process)))
    {
      if (!NILP (DATA_OUTSTREAM (XPROCESS (process))))
	{
	  Lstream_close (XLSTREAM (DATA_OUTSTREAM (XPROCESS (process))));
	  event_stream_delete_stream_pair (Qnil, XPROCESS (process)->pipe_outstream);
	  XPROCESS (process)->pipe_outstream = Qnil;
	  XPROCESS (process)->coding_outstream = Qnil;
	}
    }

  return process;
}


/************************************************************************/
/*                          deleting a process                          */
/************************************************************************/

void
deactivate_process (Lisp_Object process)
{
  Lisp_Process *p = XPROCESS (process);
  USID usid;

  /* It's possible that we got as far in the process-creation
     process as creating the descriptors but didn't get so
     far as selecting the process for input.  In this
     case, p->pid is nil: p->pid is set at the same time that
     the process is selected for input. */
  /* #### The comment does not look correct. event_stream_unselect_process
     is guarded by process->selected, so this is not a problem. - kkm*/
  /* Must call this before setting the streams to nil */
  event_stream_unselect_process (p);

  if (!NILP (DATA_OUTSTREAM (p)))
    Lstream_close (XLSTREAM (DATA_OUTSTREAM (p)));
  if (!NILP (DATA_INSTREAM (p)))
    Lstream_close (XLSTREAM (DATA_INSTREAM (p)));

  /* Provide minimal implementation for deactivate_process
     if there's no process-specific one */
  if (HAS_PROCMETH_P (deactivate_process))
    usid = PROCMETH (deactivate_process, (p));
  else
    usid = event_stream_delete_stream_pair (p->pipe_instream,
					    p->pipe_outstream);

  if (usid != USID_DONTHASH)
    remhash ((const void*)usid, usid_to_process);

  p->pipe_instream = Qnil;
  p->pipe_outstream = Qnil;
  p->coding_instream = Qnil;
  p->coding_outstream = Qnil;
}

static void
remove_process (Lisp_Object process)
{
  Vprocess_list = delq_no_quit (process, Vprocess_list);
  Fset_marker (XPROCESS (process)->mark, Qnil, Qnil);

  deactivate_process (process);
}

DEFUN ("delete-process", Fdelete_process, 1, 1, 0, /*
Delete PROCESS: kill it and forget about it immediately.
PROCESS may be a process or the name of one, or a buffer name.
*/
       (process))
{
  /* This function can GC */
  Lisp_Process *p;
  process = get_process (process);
  p = XPROCESS (process);
  if (network_connection_p (process))
    {
      p->status_symbol = Qexit;
      p->exit_code = 0;
      p->core_dumped = 0;
      p->tick++;
      process_tick++;
    }
  else if (PROCESS_LIVE_P (p))
    {
      Fkill_process (process, Qnil);
      /* Do this now, since remove_process will make sigchld_handler do nothing.  */
      p->status_symbol = Qsignal;
      p->exit_code = SIGKILL;
      p->core_dumped = 0;
      p->tick++;
      process_tick++;
      status_notify ();
    }
  remove_process (process);
  return Qnil;
}

/* Kill all processes associated with `buffer'.
 If `buffer' is nil, kill all processes  */

void
kill_buffer_processes (Lisp_Object buffer)
{
  LIST_LOOP_2 (process, Vprocess_list)
    if ((NILP (buffer) || EQ (XPROCESS (process)->buffer, buffer)))
      {
	if (network_connection_p (process))
	  Fdelete_process (process);
	else if (PROCESS_LIVE_P (XPROCESS (process)))
	  process_send_signal (process, SIGHUP, 0, 1);
      }
}

DEFUN ("process-kill-without-query", Fprocess_kill_without_query, 1, 2, 0, /*
Say no query needed if PROCESS is running when Emacs is exited.
Optional second argument if non-nil says to require a query.
Value is t if a query was formerly required.
*/
       (process, require_query_p))
{
  int tem;

  CHECK_PROCESS (process);
  tem = XPROCESS (process)->kill_without_query;
  XPROCESS (process)->kill_without_query = NILP (require_query_p);

  return tem ? Qnil : Qt;
}

DEFUN ("process-kill-without-query-p", Fprocess_kill_without_query_p, 1, 1, 0, /*
Return t if PROCESS will be killed without query when emacs is exited.
*/
       (process))
{
  CHECK_PROCESS (process);
  return XPROCESS (process)->kill_without_query ? Qt : Qnil;
}


#if 0

xxDEFUN ("process-connection", Fprocess_connection, 0, 1, 0, /*
Return the connection type of `PROCESS'.  This can be nil (pipe),
t or pty (pty) or stream (socket connection).
*/
	 (process))
{
  return XPROCESS (process)->type;
}

#endif /* 0 */


static int
getenv_internal (const Intbyte *var,
		 Bytecount varlen,
		 Intbyte **value,
		 Bytecount *valuelen)
{
  Lisp_Object scan;

  assert (env_initted);

  for (scan = Vprocess_environment; CONSP (scan); scan = XCDR (scan))
    {
      Lisp_Object entry = XCAR (scan);

      if (STRINGP (entry)
	  && XSTRING_LENGTH (entry) > varlen
	  && XSTRING_BYTE (entry, varlen) == '='
#ifdef WIN32_NATIVE
	  /* NT environment variables are case insensitive.  */
	  && ! memicmp (XSTRING_DATA (entry), var, varlen)
#else  /* not WIN32_NATIVE */
	  && ! memcmp (XSTRING_DATA (entry), var, varlen)
#endif /* not WIN32_NATIVE */
	  )
	{
	  *value    = XSTRING_DATA   (entry) + (varlen + 1);
	  *valuelen = XSTRING_LENGTH (entry) - (varlen + 1);
	  return 1;
	}
    }

  return 0;
}

static void
putenv_internal (const Intbyte *var,
		 Bytecount varlen,
		 const Intbyte *value,
		 Bytecount valuelen)
{
  Lisp_Object scan;

  assert (env_initted);

  for (scan = Vprocess_environment; CONSP (scan); scan = XCDR (scan))
    {
      Lisp_Object entry = XCAR (scan);

      if (STRINGP (entry)
	  && XSTRING_LENGTH (entry) > varlen
	  && XSTRING_BYTE (entry, varlen) == '='
#ifdef WIN32_NATIVE
	  /* NT environment variables are case insensitive.  */
	  && ! memicmp (XSTRING_DATA (entry), var, varlen)
#else  /* not WIN32_NATIVE */
	  && ! memcmp (XSTRING_DATA (entry), var, varlen)
#endif /* not WIN32_NATIVE */
	  )
	{
	  XCAR (scan) = concat3 (make_string (var, varlen),
				 build_string ("="),
				 make_string (value, valuelen));
	  return;
	}
    }

  Vprocess_environment = Fcons (concat3 (make_string (var, varlen),
					 build_string ("="),
					 make_string (value, valuelen)),
				Vprocess_environment);
}

/* NOTE:

   FSF has this as a Lisp function, as follows.  Generally moving things
   out of C and into Lisp is a good idea, but in this case the Lisp
   function is used so early in the startup sequence that it would be ugly
   to rearrange the early dumped code to accommodate this.
   
(defun getenv (variable)
  "Get the value of environment variable VARIABLE.
VARIABLE should be a string.  Value is nil if VARIABLE is undefined in
the environment.  Otherwise, value is a string.

This function consults the variable `process-environment'
for its value."
  (interactive (list (read-envvar-name "Get environment variable: " t)))
  (let ((value (getenv-internal variable)))
    (when (interactive-p)
      (message "%s" (if value value "Not set")))
    value))
*/

DEFUN ("getenv", Fgetenv, 1, 2, "sEnvironment variable: \np", /*
Return the value of environment variable VAR, as a string.
VAR is a string, the name of the variable.
When invoked interactively, prints the value in the echo area.
*/
       (var, interactivep))
{
  Intbyte *value;
  Bytecount valuelen;
  Lisp_Object v = Qnil;
  struct gcpro gcpro1;

  CHECK_STRING (var);
  GCPRO1 (v);
  if (getenv_internal (XSTRING_DATA (var), XSTRING_LENGTH (var),
		       &value, &valuelen))
    v = make_string (value, valuelen);
  if (!NILP (interactivep))
    {
      if (NILP (v))
	message ("%s not defined in environment", XSTRING_DATA (var));
      else
	/* #### Should use Fprin1_to_string or Fprin1 to handle string
           containing quotes correctly.  */
	message ("\"%s\"", value);
    }
  RETURN_UNGCPRO (v);
}

/* A version of getenv that consults Vprocess_environment, easily
   callable from C.

   (At init time, Vprocess_environment is initialized from the
   environment, stored in the global variable environ. [Note that
   at startup time, `environ' should be the same as the envp parameter
   passed to main(); however, later calls to putenv() may change
   `environ', making the envp parameter inaccurate.] Calls to getenv()
   and putenv() consult and modify `environ'.  However, once
   Vprocess_environment is initted, XEmacs C code should *NEVER* call
   getenv() or putenv() directly, because (1) Lisp code that modifies
   the environment only modifies Vprocess_environment, not `environ';
   and (2) Vprocess_environment is in internal format but `environ'
   is in some external format, and getenv()/putenv() are not Mule-
   encapsulated.

   WARNING: This value points into Lisp string data and thus will become
   invalid after a GC. */

Intbyte *
egetenv (const CIntbyte *var)
{
  /* This cannot GC -- 7-28-00 ben */
  Intbyte *value;
  Bytecount valuelen;

  if (getenv_internal ((const Intbyte *) var, strlen (var), &value, &valuelen))
    return value;
  else
    return 0;
}

void
eputenv (const CIntbyte *var, const CIntbyte *value)
{
  putenv_internal ((Intbyte *) var, strlen (var), (Intbyte *) value,
		   strlen (value));
}


/* This is not named init_process in order to avoid a conflict with NS 3.3 */
void
init_xemacs_process (void)
{
  /* This function can GC */

  MAYBE_PROCMETH (init_process, ());

  Vprocess_list = Qnil;

  if (usid_to_process)
    clrhash (usid_to_process);
  else
    usid_to_process = make_hash_table (32);
  
  {
    /* jwz: always initialize Vprocess_environment, so that egetenv()
       works in temacs. */
    char **envp;
    Vprocess_environment = Qnil;
    for (envp = environ; envp && *envp; envp++)
      Vprocess_environment =
	Fcons (build_ext_string (*envp, Qnative), Vprocess_environment);
    /* This gets set back to 0 in disksave_object_finalization() */
    env_initted = 1;
  }

  {
    /* Initialize shell-file-name from environment variables or best guess. */
#ifdef WIN32_NATIVE
    const Intbyte *shell = egetenv ("SHELL");
    if (!shell) shell = egetenv ("COMSPEC");
    /* Should never happen! */
    if (!shell) shell =
      (Intbyte *) (GetVersion () & 0x80000000 ? "command" : "cmd");
#else /* not WIN32_NATIVE */
    const Intbyte *shell = egetenv ("SHELL");
    if (!shell) shell = (Intbyte *) "/bin/sh";
#endif

#if 0 /* defined (WIN32_NATIVE) */
    /* BAD BAD BAD.  We do not wanting to be passing an XEmacs-created
       SHELL var down to some inferior Cygwin process, which might get
       screwed up.
	 
       There are a few broken apps (eterm/term.el, eterm/tshell.el,
       os-utils/terminal.el, texinfo/tex-mode.el) where this will
       cause problems.  Those broken apps don't look at
       shell-file-name, instead just at explicit-shell-file-name,
       ESHELL and SHELL.  They are apparently attempting to borrow
       what `M-x shell' uses, but that latter also looks at
       shell-file-name.  What we want is for all of these apps to look
       at shell-file-name, so that the user can change the value of
       shell-file-name and everything will work out hunky-dorey.
       */
    
    if (!egetenv ("SHELL"))
      {
	Intbyte *faux_var = alloca_array (Intbyte, 7 + qxestrlen (shell));
	qxesprintf (faux_var, "SHELL=%s", shell);
	Vprocess_environment = Fcons (build_intstring (faux_var),
				      Vprocess_environment);
      }
#endif /* 0 */

    Vshell_file_name = build_intstring (shell);
  }
}

void
syms_of_process (void)
{
  INIT_LRECORD_IMPLEMENTATION (process);

  DEFSYMBOL (Qprocessp);
  DEFSYMBOL (Qprocess_live_p);
  DEFSYMBOL (Qrun);
  DEFSYMBOL (Qstop);
  DEFSYMBOL (Qopen);
  DEFSYMBOL (Qclosed);

  DEFSYMBOL (Qtcp);
  DEFSYMBOL (Qudp);

#ifdef HAVE_MULTICAST
  DEFSYMBOL (Qmulticast); /* Used for occasional warnings */
#endif

  DEFERROR_STANDARD (Qprocess_error, Qio_error);
  DEFERROR_STANDARD (Qnetwork_error, Qio_error);

  DEFSUBR (Fprocessp);
  DEFSUBR (Fprocess_live_p);
  DEFSUBR (Fget_process);
  DEFSUBR (Fget_buffer_process);
  DEFSUBR (Fdelete_process);
  DEFSUBR (Fprocess_status);
  DEFSUBR (Fprocess_exit_status);
  DEFSUBR (Fprocess_id);
  DEFSUBR (Fprocess_name);
  DEFSUBR (Fprocess_tty_name);
  DEFSUBR (Fprocess_command);
  DEFSUBR (Fset_process_buffer);
  DEFSUBR (Fprocess_buffer);
  DEFSUBR (Fprocess_mark);
  DEFSUBR (Fset_process_filter);
  DEFSUBR (Fprocess_filter);
  DEFSUBR (Fset_process_window_size);
  DEFSUBR (Fset_process_sentinel);
  DEFSUBR (Fprocess_sentinel);
  DEFSUBR (Fprocess_kill_without_query);
  DEFSUBR (Fprocess_kill_without_query_p);
  DEFSUBR (Fprocess_list);
  DEFSUBR (Fstart_process_internal);
#ifdef HAVE_SOCKETS
  DEFSUBR (Fopen_network_stream_internal);
#ifdef HAVE_MULTICAST
  DEFSUBR (Fopen_multicast_group_internal);
#endif /* HAVE_MULTICAST */
#endif /* HAVE_SOCKETS */
  DEFSUBR (Fprocess_send_region);
  DEFSUBR (Fprocess_send_string);
  DEFSUBR (Fprocess_send_signal);
  DEFSUBR (Finterrupt_process);
  DEFSUBR (Fkill_process);
  DEFSUBR (Fquit_process);
  DEFSUBR (Fstop_process);
  DEFSUBR (Fcontinue_process);
  DEFSUBR (Fprocess_send_eof);
  DEFSUBR (Fsignal_process);
/*  DEFSUBR (Fprocess_connection); */
  DEFSUBR (Fprocess_input_coding_system);
  DEFSUBR (Fprocess_output_coding_system);
  DEFSUBR (Fset_process_input_coding_system);
  DEFSUBR (Fset_process_output_coding_system);
  DEFSUBR (Fprocess_coding_system);
  DEFSUBR (Fset_process_coding_system);
  DEFSUBR (Fgetenv);
}

void
vars_of_process (void)
{
  Fprovide (intern ("subprocesses"));
#ifdef HAVE_SOCKETS
  Fprovide (intern ("network-streams"));
#ifdef HAVE_MULTICAST
  Fprovide (intern ("multicast"));
#endif /* HAVE_MULTICAST */
#endif /* HAVE_SOCKETS */
  staticpro (&Vprocess_list);

  DEFVAR_BOOL ("delete-exited-processes", &delete_exited_processes /*
*Non-nil means delete processes immediately when they exit.
nil means don't delete them until `list-processes' is run.
*/ );

  delete_exited_processes = 1;

  DEFVAR_CONST_LISP ("null-device", &Vnull_device /*
Name of the null device, which differs from system to system.
The null device is a filename that acts as a sink for arbitrary amounts of
data, which is discarded, or as a source for a zero-length file.
It is available on all the systems that we currently support, but with
different names (typically either `/dev/null' or `nul').

Note that there is also a /dev/zero on most modern Unix versions (including
Cygwin), which acts like /dev/null when used as a sink, but as a source
it sends a non-ending stream of zero bytes.  It's used most often along
with memory-mapping.  We don't provide a Lisp variable for this because
the operations needing this are lower level than what ELisp programs
typically do, and in any case no equivalent exists under native MS Windows.
*/ );
  Vnull_device = build_string (NULL_DEVICE);

  DEFVAR_LISP ("process-connection-type", &Vprocess_connection_type /*
Control type of device used to communicate with subprocesses.
Values are nil to use a pipe, or t or `pty' to use a pty.
The value has no effect if the system has no ptys or if all ptys are busy:
then a pipe is used in any case.
The value takes effect when `start-process' is called.
*/ );
  Vprocess_connection_type = Qt;

  DEFVAR_BOOL ("windowed-process-io", &windowed_process_io /*
Enables input/output on standard handles of a windowed process.
When this variable is nil (the default), XEmacs does not attempt to read
standard output handle of a windowed process. Instead, the process is
immediately marked as exited immediately upon successful launching. This is
done because normal windowed processes do not use standard I/O, as they are
not connected to any console.

When launching a specially crafted windowed process, which expects to be
launched by XEmacs, or by other program which pipes its standard input and
output, this variable must be set to non-nil, in which case XEmacs will
treat this process just like a console process.

NOTE: You should never set this variable, only bind it.

Only Windows processes can be "windowed" or "console". This variable has no
effect on UNIX processes, because all UNIX processes are "console".
*/ );
  windowed_process_io = 0;

  DEFVAR_INT ("debug-process-io", &debug_process_io /*
If non-zero, display data sent to or received from a process.
*/ );
  debug_process_io = 0;

  DEFVAR_LISP ("default-process-coding-system",
	       &Vdefault_process_coding_system /*
Cons of coding systems used for process I/O by default.
The car part is used for reading (decoding) data from a process, and
the cdr part is used for writing (encoding) data to a process.
*/ );
  /* This below will get its default set correctly in code-init.el. */
    Vdefault_process_coding_system = Fcons (Qundecided, Qnil);

#ifdef PROCESS_IO_BLOCKING
  DEFVAR_LISP ("network-stream-blocking-port-list", &network_stream_blocking_port_list /*
List of port numbers or port names to set a blocking I/O mode with connection.
Nil value means to set a default(non-blocking) I/O mode.
The value takes effect when `open-network-stream-internal' is called.
*/ );
  network_stream_blocking_port_list = Qnil;
#endif	/* PROCESS_IO_BLOCKING */

  /* This function can GC */
  DEFVAR_LISP ("shell-file-name", &Vshell_file_name /*
*File name to load inferior shells from.
Initialized from the SHELL environment variable.
*/ );

  DEFVAR_LISP ("process-environment", &Vprocess_environment /*
List of environment variables for subprocesses to inherit.
Each element should be a string of the form ENVVARNAME=VALUE.
The environment which Emacs inherits is placed in this variable
when Emacs starts.
*/ );

  Vlisp_EXEC_SUFFIXES = build_string (EXEC_SUFFIXES);
  staticpro (&Vlisp_EXEC_SUFFIXES);
}
