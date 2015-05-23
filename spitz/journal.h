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

#ifndef JOURNAL_H
#define JOURNAL_H

#include "taskmanager.h"
#include <semaphore.h>

struct journal {
    // Entries part.
    struct j_entry ** entries;
    int * size;
    int * capacity;
    int num_threads;
    struct timeval zero;
    char * id_type;

    // Id part.
    pthread_mutex_t id_lock;
    int c_id;
};

struct j_entry {
    char action;
    struct timeval start;
    struct timeval end;
};

// Diary manipulation functions.
void JOURNAL_init(struct journal *dia, int num_threads);
int JOURNAL_get_id(struct journal *dia, char my_type);
struct j_entry * JOURNAL_new_entry(struct journal *dia, int id);
void JOURNAL_remove_entry(struct journal *dia, int id);
void JOURNAL_free(struct journal *dia);

// Info
char * JOURNAL_generate_info(struct journal *dia, char * filename);

#endif	/* JOURNAL_H */

