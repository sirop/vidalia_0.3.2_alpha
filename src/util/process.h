/*
**  This file is part of Vidalia, and is subject to the license terms in the
**  LICENSE file, found in the top level directory of this distribution. If you
**  did not receive the LICENSE file with this file, you may obtain it from the
**  Vidalia source package distributed by the Vidalia Project at
**  http://www.vidalia-project.net/. No part of Vidalia, including this file,
**  may be copied, modified, propagated, or distributed except according to the
**  terms described in the LICENSE file.
*/

/*
** \file process.h
** \version $Id$
** \brief Process information and pidfile functions
*/

#ifndef _PROCESS_H
#define _PROCESS_H

#include <QString>

#if defined(Q_OS_WIN)
#include "win32.h"
#else
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#endif


/** Returns the PID of the current process. */
qint64 get_pid();

/** Returns true if a process with the given PID is running. */
bool is_process_running(qint64 pid);

/** Writes the given file to disk containing the current process's PID. */
bool write_pidfile(QString pidfile, QString *errmsg = 0);

/** Reads the giiven pidfile and returns the value in it. If the file does not
 * exist, -1 is returned. */
qint64 read_pidfile(QString pidfile, QString *errmsg = 0);

#endif

