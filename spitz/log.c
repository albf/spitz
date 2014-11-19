/*
 * Copyright 2014 Ian Liu Rodrigues <ian.liu@ggaunicamp.com>
 *
 * This file is part of spitz.
 *
 * spitz is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * spitz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with spitz.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "log.h"

#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include "comm.h"

extern int LOG_LEVEL;

enum color {
	RED,
	GREEN,
	YELLOW,
	BLUE,
	NORMAL
};

const char *color_codes[] = {
	"\033[1;31m",
	"\033[1;32m",
	"\033[1;33m",
	"\033[1;34m",
	"\033[0m",
};

static void vmessage(FILE *f, const char *msg, const char *prefix,
		enum color color, va_list ap)
{
	int rank = COMM_get_rank_id();
	
	const char *actor;
	if (type == JOB_MANAGER) {
            actor = "JM";
        }
	else if (type == COMMITTER) {
            actor = "CO";
        }
	else if (type == TASK_MANAGER) {
            actor = "TM";
        }
        else if (type == MONITOR) { 
            actor = "MO";
        }
        else { 
            actor = "??";
        }

	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);

	fprintf(f, "[%ld:%09ld %s:%d] ", tp.tv_sec, tp.tv_nsec, actor, rank);
	if (prefix) {
		if (isatty(fileno(f)))
			fprintf(f, "%s%s%s ",
					color_codes[color],
					prefix,
					color_codes[NORMAL]);
		else
			fprintf(f, "%s ", prefix);
	}
	vfprintf(f, msg, ap);
	fprintf(f, "\n");
}

void info(const char *msg, ...)
{
	if (LOG_LEVEL < 1)
		return;
	
	int rank = COMM_get_rank_id();	
	
	va_list ap;
	va_start(ap, msg);
	vmessage(stdout, msg, "info:", BLUE, ap);
	va_end(ap);
}

void debug(const char *msg, ...)
{
	if (LOG_LEVEL < 2)
		return;
	
	int rank = COMM_get_rank_id();
	
	va_list ap;
	va_start(ap, msg);
	vmessage(stdout, msg, "debug:", GREEN, ap);
	va_end(ap);
}

void warning(const char *msg, ...)
{
	int rank = COMM_get_rank_id();
	
	va_list ap;
	va_start(ap, msg);
	vmessage(stderr, msg, "warning:", YELLOW, ap);
	va_end(ap);
}

void error(const char *msg, ...)
{
	int rank = COMM_get_rank_id();
	
	va_list ap;
	va_start(ap, msg);
	vmessage(stderr, msg, "error:", RED, ap);
	va_end(ap);
}
