/*
 *  Window Maker miscelaneous function library
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "wconfig.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <WUtil.h>
#include <WINGsP.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
static Bool syslog_initialized = False;
#endif

void syslog_open(char *prog_name)
{
#ifdef HAVE_SYSLOG
	int options;

	if (!prog_name)
		prog_name = "WINGs";

	options = LOG_PID;
	openlog(prog_name, options, LOG_DAEMON);
	syslog_initialized = True;
#else
	(void) prog_name;
#endif
}

void syslog_message(int prio, char *prog_name, char *msg)
{
#ifdef HAVE_SYSLOG
	if (!syslog_initialized)
		syslog_open(prog_name);

	//jump over the program name cause syslog is already displaying it
	syslog(prio, "%s", msg+strlen(prog_name));
#else
	(void) prio;
	(void) prog_name;
	(void) msg;
#endif
}

void syslog_shutdown(void)
{
#ifdef HAVE_SYSLOG
	if (syslog_initialized) {
		closelog();
		syslog_initialized = False;
	}
#endif
}

void __wmessage(const char *func, const char *file, int line, int type, const char *msg, ...)
{
	va_list args;
	char *buf;
	static int linemax = 0;
	int truncated = 0;
#ifdef HAVE_SYSLOG
	int syslog_priority = LOG_INFO;
	const char *syslog_prefix = "INFO";
#endif

	if (linemax == 0) {
#ifdef HAVE_SYSCONF
		linemax = sysconf(_SC_LINE_MAX);
		if (linemax == -1) {
			/* I'd like to know of this ever fires */
			fprintf(stderr, "%s %d: sysconf(_SC_LINE_MAX) returned error\n",
				__FILE__, __LINE__);
			linemax = 512;
		}
#else /* !HAVE_SYSCONF */
		fprintf(stderr, "%s %d: Your system does not have sysconf(3); "
			"let wmaker-dev@windowmaker.org know.\n", __FILE__, __LINE__);
		linemax = 512;
#endif /* HAVE_SYSCONF */
	}

	buf = wmalloc(linemax);

	fflush(stdout);

	/* message format: <wings_progname>(function(file:line): <type?>: <message>"\n" */
	strncat(buf, _WINGS_progname ? _WINGS_progname : "WINGs", linemax - 1);
	snprintf(buf + strlen(buf), linemax - strlen(buf), "(%s(%s:%d))", func, file, line);
	strncat(buf, ": ", linemax - 1 - strlen(buf));

	switch (type) {
		case WMESSAGE_TYPE_FATAL:
			strncat(buf, _("fatal: "), linemax - 1 - strlen(buf));
#ifdef HAVE_SYSLOG
			syslog_priority = LOG_CRIT;
			syslog_prefix = "FATAL";
#endif
		break;
		case WMESSAGE_TYPE_ERROR:
			strncat(buf, _("error: "), linemax - 1 - strlen(buf));
#ifdef HAVE_SYSLOG
			syslog_priority = LOG_ERR;
			syslog_prefix = "ERROR";
#endif
		break;
		case WMESSAGE_TYPE_WARNING:
			strncat(buf, _("warn: "), linemax - 1 - strlen(buf));
#ifdef HAVE_SYSLOG
			syslog_priority = LOG_WARNING;
			syslog_prefix = "WARNING";
#endif
		break;
		case WMESSAGE_TYPE_MESSAGE:
			/* FALLTHROUGH */
		default:	/* should not happen, but doesn't hurt either */
		break;
	}

	va_start(args, msg);
	if (vsnprintf(buf + strlen(buf), linemax - strlen(buf), msg, args) >= linemax - strlen(buf))
		truncated = 1;

	va_end(args);

	fputs(buf, stderr);
#ifdef HAVE_SYSLOG
	syslog_message(syslog_priority, _WINGS_progname ? _WINGS_progname : "WINGs", buf);
#endif

	if (truncated)
		fputs("*** message truncated ***", stderr);

	fputs("\n", stderr);

	wfree(buf);
}

