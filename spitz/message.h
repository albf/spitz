/*
 * Copyright 2014 Ian Liu Rodrigues <ian.liu@ggaunicamp.com>
 * Copyright 2014 Alber Tabone Novo <alber.tabone@ggaunicamp.com>
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

#ifndef __SPITZ_MESSAGE_H__
#define __SPITZ_MESSAGE_H__

#include <barray.h>

enum actor {
	JOB_MANAGER = 0,
	COMMITTER   = 1,
};

enum message_type {
	MSG_READY,
	MSG_TASK,
	MSG_RESULT,	
	MSG_KILL,	
	MSG_DONE,	
	MSG_GET_ALIVE,
	MSG_GET_COMMITTER,
	MSG_GET_PATH,
	MSG_GET_RANK,
	MSG_GET_RUNNUM,
	MSG_SET_COMMITTER,	
};

void get_message(struct byte_array *ba, enum message_type *type, int *rank);

void send_message(struct byte_array *ba, int type, int rank);

#endif /* __SPITZ_MESSAGE_H__ */
