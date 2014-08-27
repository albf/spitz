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

#include "message.h"

void get_message(struct byte_array *ba, enum message_type *type, int *rank)
{
	struct byte_array _ba;
	MPI_Status status;

	int len;
	MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	MPI_Get_count(&status, MPI_CHAR, &len);

	if (!ba) {
		_ba.ptr = NULL;
		_ba.len = 0;
		ba = &_ba;
	} else {
		byte_array_resize(ba, len);
		ba->len = len, ba->iptr = ba->ptr;
	}

	if (type)
		*type = status.MPI_TAG;

	if (rank)
		*rank = status.MPI_SOURCE;

	MPI_Recv(ba->ptr, ba->len, MPI_CHAR, status.MPI_SOURCE,
			status.MPI_TAG, MPI_COMM_WORLD, &status);
}

void send_message(struct byte_array *ba, int type, int rank)
{
	struct byte_array _ba;
	if (!ba) {
		_ba.ptr = NULL;
		_ba.len = 0;
		ba = &_ba;
	}
	MPI_Send(ba->ptr, ba->len, MPI_BYTE, rank, type, MPI_COMM_WORLD);
}
