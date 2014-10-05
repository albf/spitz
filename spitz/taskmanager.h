/*
 * Copyright 2014 Ian Liu Rodrigues <ian.liu@ggaunicamp.com>
 * Copyright 2014 Alber Tabone Novo <alber.tabone@ggaunicamp.com>
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

#ifndef __SPITZ_TASKMANAGER_H__
#define __SPITZ_TASKMANAGER_H__

#include <semaphore.h>
#include "barray.h"
#include "cfifo.h"

struct result_node {
    struct byte_array ba;
    struct result_node *next;
};

struct thread_data {
    int id;
    struct cfifo f;
    pthread_mutex_t tlock;              // lock responsible for the fifo of tasks
    pthread_mutex_t rlock;
    sem_t tcount;                       // number of tasks available
    sem_t sem;
    char running;
    struct result_node *results;
    void *handle;
    int argc;
    char **argv;
    int is_blocking_flush;              // 1 if in blocking flush, 0 if not. 
    pthread_mutex_t bf_mutex;           // semaphore to unlock the blocking flush.
    int bf_remaining_tasks;             // remaining tasks in blocking flush.
};

enum blocking {
    BLOCKING,
    NONBLOCKING
};


int get_number_of_cores();
void *worker(void *ptr);
int flush_results(struct thread_data *d, int min_results, enum blocking b);
void task_manager(struct thread_data *d);

#endif /* __SPITZ_TASKMANAGER_H__ */
