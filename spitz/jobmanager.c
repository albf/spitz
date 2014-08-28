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

#include "jobmanager.h"

#include <dlfcn.h>
#include "comm.h"
#include "log.h"
#include "barray.h"

typedef void * (*spitz_ctor_t) (int, char **);
typedef int    (*spitz_tgen_t) (void *, struct byte_array *);

struct task {
	size_t id;
	struct byte_array data;
	struct task *next;
};

void job_manager(int argc, char *argv[], char *so, struct byte_array *final_result)
{
	void *ptr = dlopen(so, RTLD_LAZY);

	if (!ptr) {
		error("could not open %s", so);
		return;
	}

	int alive = COMM_get_alive();	// get alive nodes from  job manager.

	struct task *iter, *prev, *aux, *sent = NULL;

	spitz_ctor_t ctor = dlsym(ptr, "spits_job_manager_new");
	spitz_tgen_t tgen = dlsym(ptr, "spits_job_manager_next_task");

	enum message_type type;
	struct byte_array ba;
	byte_array_init(&ba, 10);
	void *user_data = ctor(argc, argv);
	size_t tid, task_id = 0;
	while (1) {
		int rank;
		struct task *node;

		get_message(&ba, &type, &rank);
		switch (type) {
			case MSG_READY:
				byte_array_clear(&ba);
				byte_array_pack64(&ba, task_id);
				if (tgen(user_data, &ba)) {
					node = malloc(sizeof(*node));
					node->id = task_id;
					byte_array_init(&node->data, ba.len);
					byte_array_pack8v(&node->data, ba.ptr, ba.len);
					debug("sending generated task %d to %d", task_id, rank);
					node->next = sent;
					sent = node;
					aux = sent;
					send_message(&ba, MSG_TASK, rank);
					task_id++;
				} else if (sent != NULL) {
					send_message(&aux->data, MSG_TASK, rank);
					debug("replicating task %d", aux->id);
					aux = aux->next;
					if (!aux)
						aux = sent;
				} else {
					debug("sending KILL to rank %d", rank);
					send_message(&ba, MSG_KILL, rank);
					alive--;
				}
				break;
			case MSG_DONE:
				byte_array_unpack64(&ba, &tid);
				iter = sent;
				prev = NULL;
				while (iter->id != tid) {
					prev = iter;
					iter = iter->next;
				}
				if (prev)
					prev->next = iter->next;
				else
					sent = iter->next;

				if (aux == iter) {
					aux = aux->next;
					if (!aux)
						aux = sent;
				}

				debug("TASK %d is complete! sent = %p", tid, sent);
				byte_array_free(&iter->data);
				free(iter);
				break;
			default:
				break;
		}

		if (alive == 2) {
			info("sending KILL to committer");
			send_message(&ba, MSG_KILL, COMMITTER);

			info("fetching final result");
			get_message(final_result, NULL, NULL);
			break;
		}
	}

	byte_array_free(&ba);

	info("terminating job manager");
}

