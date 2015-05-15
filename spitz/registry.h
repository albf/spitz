/*
 * Copyright 2014 Alexandre Luiz Brisighello Filho <albf.unicamp@gmail.com>
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

 // Simple linked list used to store the ip and port from works and committers.

#ifndef REGISTRY_H
#define REGISTRY_H

#include "jobmanager.h"

struct task_registry {
    int task_id;
    int tm_id;
    struct timeval * send_time;
    struct timeval * completed_time;
    struct task_registry *next;
};

// Registry manipulation functions.
void REGISTRY_add_registry(struct jm_thread_data *td, int task_id, int tm_id);
int REGISTRY_check_registry(struct jm_thread_data * td, int task_id, int tm_id);
void REGISTRY_add_completion_registry (struct jm_thread_data *td, size_t task_id, int tm_id);
void REGISTRY_free(struct jm_thread_data *td);

// Info
char * REGISTRY_generate_info(struct jm_thread_data *td, char * filename);

#endif	/* LIST_H */

