/**************************************************************************
 *                                                                        *
 *  statustran                                                            *
o *                                                                        *
 *  StatusNet translator for the GNU operating system (requires curl)     *
 *                                                                        *
 *  Copyright 1995, 1996, 1997, 1998, 1999, 2001, 2002, 2007              *
 *            Free Software Foundation, Inc.                              *
 *                                                                        *
 *  Copyright 2010  Matt Windsor <hayashi@archhurd.org>                   *
 *                                                                        * 
 *  Based on work by Gordon Matzigkeit <gord@fig.org>                     *
 *               and Miles Bader       <miles@gnu.org>                    *
 *                                                                        * 
 *                                                                        *
 *  This program is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  This program is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                        *
 **************************************************************************/

#define _GNU_SOURCE 1

#include <hurd/trivfs.h>

#include <argp.h>
#include <argz.h>
#include <stdlib.h>   /* exit () */
#include <unistd.h>   /* fork (), execl (), _exit () */
#include <error.h>    /* Error numers */
#include <fcntl.h>    /* O_READ etc.  */
#include <sys/mman.h> /* MAP_ANON etc.  */

#include "main.h"

/* Constants. */
const char *argp_program_version = "statustran 0.0";
const char URL[] = "http://identi.ca/api/statuses/update.xml";
const char ORIGIN_STANZA[] = "source=statustran";

/* Trivfs hooks.  */
int trivfs_fstype = FSTYPE_MISC;  /* Generic trivfs server */
int trivfs_fsid = 0;    /* Should always be 0 on startup */

int trivfs_allow_open = O_READ | O_WRITE;

/* Actual supported modes: */
int trivfs_support_read  = 1;
int trivfs_support_write = 1;
int trivfs_support_exec  = 0;

char *userauth = NULL;
static size_t userauth_len = sizeof userauth - 1;
char buffer[MESSAGE_SIZE + 10]; /**< Buffer to store the post in. */

/* Options processing.  We accept the same options on the command line
   and from fsys_set_options.  */

static const struct argp_option options[] =
  {
    {"userauth", 'u', "STRING", 0,
     "The login for the site, in username:pass format.", 0},
    {0, 0, 0, 0, 0, 0}
  };

static error_t
parse_opt (int opt, char *arg, struct argp_state *state)
{
  switch (opt)
    {
    default:
      return ARGP_ERR_UNKNOWN;
    case ARGP_KEY_INIT:
    case ARGP_KEY_SUCCESS:
    case ARGP_KEY_ERROR:
      break;
    case 'u':
      {
	char *new = strdup (arg);
	if (new == NULL)
	  return ENOMEM;
	if (userauth)
	  free (userauth);
	userauth = new;
	userauth_len = strlen (new);
	break;
      }
    }
  return 0;
}

/* This will be called from libtrivfs to help construct the answer
   to an fsys_get_options RPC.  */
error_t
trivfs_append_args (struct trivfs_control *fsys,
		    char **argz, size_t *argz_len)
{
  error_t err;
  char *opt;

  if (asprintf (&opt, "--userauth=%s", userauth) < 0)
    return ENOMEM;

  err = argz_add (argz, argz_len, opt);

  free (opt);

  return err;
}

static struct argp sys_argp =
  {
    options,
    parse_opt,
    0,
    "A translator for StatusNet.",
    0,
    0,
    0
  };

/* Setting this variable makes libtrivfs use our argp to
   parse options passed in an fsys_set_options RPC.  */
struct argp *trivfs_runtime_argp = &sys_argp;

/* May do nothing...  */
void trivfs_modify_stat (struct trivfs_protid *cred, io_statbuf_t *st)
{
  /* ..  and we do nothing */
}

error_t trivfs_goaway (struct trivfs_control *cntl, int flags)
{
  exit (EXIT_SUCCESS);
}

error_t
trivfs_S_io_read (trivfs_protid_t cred,
		  mach_port_t reply,
		  mach_msg_type_name_t reply_type,
		  data_t *data,
		  mach_msg_type_number_t *data_len,
		  loff_t offs,
		  mach_msg_type_number_t amount)
{
  char *dac; /* Data as char stream. */

  /* Deny access if they have bad credentials.  */
  if (!cred)
    return EOPNOTSUPP;
  else if (!(cred->po->openmodes & O_READ))
    return EBADF;

  /* Make sure the input buffer exists. */
  if (buffer == NULL)
    return 0; 

  if (amount > 0)
    {
      /* Prune to MESSAGE_SIZE characters. */
      amount = MIN (amount, MESSAGE_SIZE);

      /* Possibly allocate a new buffer.  */
      if (*data_len < amount)
	*data = (data_t) mmap (0, amount, PROT_READ|PROT_WRITE,
			       MAP_ANON, 0, 0);

      /* Set up the char pointer. */
      dac = (char*) *data;

      strncpy (dac, buffer, amount);
      *data_len = amount;
    }
  return 0;
}

kern_return_t
trivfs_S_io_write (trivfs_protid_t cred,
    mach_port_t reply, mach_msg_type_name_t replytype,
    data_t data, mach_msg_type_number_t datalen,
    loff_t offs, vm_size_t *amount)
{
  unsigned int i;
  pid_t pid;
  char ebuf[OUTBUF_SIZE];
  char snt[2];

  if (!cred)
    return EOPNOTSUPP;
  else if (!(cred->po->openmodes & O_WRITE))
    return EBADF;

  *amount = MIN (datalen, MESSAGE_SIZE);

  memset (buffer, '\0', sizeof (buffer));
  strncpy (buffer, data, datalen);

  sprintf (ebuf, "status=");

  /* URL encode. */
  for (i = 0; i < strlen (buffer) - 1; i++)
    {
      switch (buffer[i])
	{
	case '<':
	  strcat (ebuf, "%3C\0");
	  break;
	case '>':
	  strcat (ebuf, "%3E\0");
	  break;
	case '~':
	  strcat (ebuf, "%7E\0");
	  break;
	case '.':
	  strcat (ebuf, "%2E\0");
	  break;
	case '\"':
	  strcat (ebuf, "%22\0");
	  break;
	case '{':
	  strcat (ebuf, "%7B\0");
	  break;
	case '}':
	  strcat (ebuf, "%7D\0");
	  break;
	case '|':
	  strcat (ebuf, "%7C\0");
	  break;
	case '\\':
	  strcat (ebuf, "%5C\0");
	  break;
	case '-':
	  strcat (ebuf, "%2D\0");
	  break;
	case '`':
	  strcat (ebuf, "%60\0");
	  break;
	case '_':
	  strcat (ebuf, "%5F\0");
	  break;
	case '^':
	  strcat (ebuf, "%5E\0");
	  break;
	case '%':
	  strcat (ebuf, "%25\0");
	  break;
	case ' ':
	  strcat (ebuf, "%20\0");
	  break;
	default:
	  sprintf (snt, "%c", buffer[i]);
	  strcat (ebuf, snt);
	  break;
	}
    }

  /* Fork a child process to run curl. */
  pid = fork ();

  if (pid == 0)
    {
      execl ("/bin/curl", "/bin/curl", "-u", userauth,
	     URL, "-d", ebuf, "-d", ORIGIN_STANZA, 
	     "-s", NULL);
      _exit (0);
    }

  return 0;
}

/* Tell how much data can be read from the object without blocking for
   a "long time" (this should be the same meaning of "long time" used
   by the nonblocking flag.  */
kern_return_t
trivfs_S_io_readable (trivfs_protid_t cred,
  mach_port_t reply, mach_msg_type_name_t replytype,
  vm_size_t *amount)
{
  if (!cred)
    return EOPNOTSUPP;
  else if (!(cred->po->openmodes & O_READ))
    return EINVAL;
  else
    *amount = 10000; /* Dummy value */
  return 0;
}

/* Truncate file.  */
kern_return_t
trivfs_S_file_set_size (trivfs_protid_t trunc_file,
			mach_port_t reply,
			mach_msg_type_name_t replyPoly,
			loff_t new_size)
{
  if (!trunc_file)
    return EOPNOTSUPP;
  else
    return 0;
}

/* Change current read/write offset */
kern_return_t
trivfs_S_io_seek (trivfs_protid_t cred, mach_port_t reply,
   mach_msg_type_name_t reply_type, loff_t offs, int whence,
   loff_t *new_offs)
{
  if (!cred)
    return EOPNOTSUPP;
  else
    return 0;
}

/* SELECT_TYPE is the bitwise OR of SELECT_READ, SELECT_WRITE, and
   SELECT_URG.  Block until one of the indicated types of i/o can be
   done "quickly", and return the types that are then available.
   TAG is returned as passed; it is just for the convenience of the
   user in matching up reply messages with specific requests sent.  */
kern_return_t
trivfs_S_io_select (trivfs_protid_t io_object,
		    mach_port_t reply,
		    mach_msg_type_name_t replyPoly,
		    int *select_type)
{
  if (!io_object)
    return EOPNOTSUPP;
  else
    if (((*select_type & SELECT_READ) && !(io_object->po->openmodes & O_READ)) ||
        ((*select_type & SELECT_WRITE) && !(io_object->po->openmodes & O_WRITE)))
      return EBADF;
    else
      *select_type &= ~SELECT_URG;
  return 0;
}

/* Well, we have to define these four functions, so here we go: */

kern_return_t
trivfs_S_io_get_openmodes (trivfs_protid_t cred, mach_port_t reply,
  mach_msg_type_name_t replytype, int *bits)
{
  if (!cred)
    return EOPNOTSUPP;
  else
    {
      *bits = cred->po->openmodes;
      return 0;
    }
}

error_t
trivfs_S_io_set_all_openmodes (trivfs_protid_t cred,
 mach_port_t reply,
 mach_msg_type_name_t replytype,
 int mode)
{
  if (!cred)
    return EOPNOTSUPP;
  else
    return 0;
}

kern_return_t
trivfs_S_io_set_some_openmodes (trivfs_protid_t cred,
  mach_port_t reply,
  mach_msg_type_name_t replytype,
  int bits)
{
  if (!cred)
    return EOPNOTSUPP;
  else
    return 0;
}

kern_return_t
trivfs_S_io_clear_some_openmodes (trivfs_protid_t cred,
    mach_port_t reply,
    mach_msg_type_name_t replytype,
    int bits)
{
  if (!cred)
    return EOPNOTSUPP;
  else
    return 0;
}

int
main (int argc, char **argv)
{
  error_t err;
  mach_port_t bootstrap;
  struct trivfs_control *fsys;

  /* We use the same argp for options available at startup
     as for options we'll accept in an fsys_set_options RPC.  */
  argp_parse (&sys_argp, argc, argv, 0, 0, 0);

  task_get_bootstrap_port (mach_task_self (), &bootstrap);
  if (bootstrap == MACH_PORT_NULL)
    error (1, 0, "Must be started as a translator");

  strcpy(buffer, "\0");

  /* Reply to our parent */
  err = trivfs_startup (bootstrap, 0, 0, 0, 0, 0, &fsys);
  mach_port_deallocate (mach_task_self (), bootstrap);
  if (err)
    error (1, err, "trivfs_startup failed");

  /* Launch.  */
  ports_manage_port_operations_one_thread (fsys->pi.bucket,
   trivfs_demuxer, 0);

  return 0;
}

