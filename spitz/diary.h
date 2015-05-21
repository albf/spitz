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

#ifndef DIARY_H
#define DIARY_H

#include "taskmanager.h"
#include <semaphore.h>

struct diary {
    // Entries part.
    struct entry ** entries;
    int * size;
    int * capacity;
    int num_threads;
    struct timeval zero;

    // Id part.
    pthread_mutex_t id_lock;
    int c_id;
};

struct entry {
    char action;
    struct timeval start;
    struct timeval end;
};

// Diary manipulation functions.
void DIARY_init(struct tm_thread_data *td, int num_threads);
int DIARY_get_id(struct tm_thread_data *td);
struct entry * DIARY_new_entry(struct tm_thread_data *td, int id);
void DIARY_free(struct tm_thread_data *td);

// Info
char * DIARY_generate_info(struct tm_thread_data *td, char * filename);

#endif	/* DIARY_H */

