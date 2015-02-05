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

#include <barray.h>
#include <comm.h>
#include <semaphore.h>

struct jm_thread_data {
    int * socket_table [2];
    task_FIFO tasks_list; 
};

struct task_FIFO {
    struct task_elem * first;           // Points to oldest added element.
    struct task_elem * last;            // Points to newest added element.
    pthread_mutex_t lock;              // lock responsible for the FIFO of tasks
    sem_t num_requests;                 // number of pending requests to deal with. 
};

struct task_elem {
    struct byte_array * ba;             // Byte array of message received.
    enum message_type type;             // Type of message received. 
    struct task_elem * next;            // Pointer to next task.
};

void job_manager(int argc, char *argv[], char *so, struct byte_array *final_result);

#endif /* __SPITZ_JOB_MANAGER_H__ */
