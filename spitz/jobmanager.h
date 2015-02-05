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

#ifndef __SPITZ_JOB_MANAGER_H__
#define __SPITZ_JOB_MANAGER_H__

#define RESTORE_RATE 10
#define JM_EXTRA_THREADS 1 

#include <barray.h>
#include <comm.h>
#include <semaphore.h>

struct jm_thread_data {
    int * socket_table [2];             // Table of sockets, to manage what each thread are doing.
    struct request_FIFO * request_list; // FIFO of request, to manage client requests. 
    pthread_mutex_t lock;               // lock responsible for the FIFO of requests.
    sem_t num_requests;                 // number of pending requests to deal with. 
    int task_counter;                   // Current/Next task.
    pthread_mutex_t tc_lock;            // lock responsible for task_counter.
    int all_generated;                  // Indicates (0=No, 1=Yes) if all tasks were already generated.
    int is_finished;                    // Indicates if it's finished. 
    pthread_mutex_t tl_lock;            // lock responsible for task_list.
    struct task_list * tasks;             // List of already genreated tasks.
};

// Regular first in first out list. 
struct request_FIFO{
    struct request_elem * first;           // Points to oldest added element.
    struct request_elem * last;            // Points to newest added element.
};

struct request_elem {
    struct byte_array * ba;             // Byte array of message received.
    enum message_type type;             // Type of message received. 
    struct request_elem * next;            // Pointer to next task.
};

// Special FIFO list, will repeat after ended. Will not remove when accessed (no pop).
struct task_list {
   struct task *home;
   struct task *mark;
   struct task *head;
};

struct task {
    size_t id;
    struct byte_array data;
    struct task *next;
};

void job_manager(int argc, char *argv[], char *so, struct byte_array *final_result);

#endif /* __SPITZ_JOB_MANAGER_H__ */
