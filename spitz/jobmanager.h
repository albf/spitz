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

#include <barray.h>
#include <comm.h>
#include <semaphore.h>
#include <time.h>

struct jm_thread_data {
    void *user_data;                    // User data, used to generate tasks.
    struct request_FIFO * request_list; // FIFO of request, to manage client requests. 
    pthread_mutex_t lock;               // lock responsible for the FIFO of requests.
    sem_t num_requests;                 // number of pending requests to deal with. task_id = 0.
    int num_tasks_total;                // number of tasks to do in current instance. 
    int task_counter;                   // Current/Next task.
    pthread_mutex_t tc_lock;            // lock responsible for task_counter.
    int all_generated;                  // Indicates (0=No, 1=Yes) if all tasks were already generated.
    int is_finished;                    // Indicates if it's finished. 
    pthread_mutex_t tl_lock;            // lock responsible for task_list.
    struct task_list * tasks;           // List of already generated tasks.
    void *handle;                       // Used to find user-defined functions.
    
    // jm_gen_worker-related variables.
    pthread_mutex_t gen_region_lock;    // Separate generate region on worker thread. 
    pthread_mutex_t gen_ready_lock;     // Indicate if task generation is complete.
    pthread_mutex_t jm_gen_lock;        // Control jm_gen_worker behaviour.
    struct byte_array * gen_ba;         // Byte array used for generating tasks.
    int * gen_tid;                      // =tid if generated successfully, -1 otherwise. Pointer to avoid issues.

    // Registry related variables
    pthread_mutex_t registry_lock;      // lock responsible for the registry.
    struct task_registry ** registry;   // Registry itself.
    int registry_capacity;              // Capacity of registry.
};

// Regular first in first out list. 
struct request_FIFO{
    struct request_elem * first;        // Points to oldest added element.
    struct request_elem * last;         // Points to newest added element.
};

struct request_elem {
    struct byte_array * ba;             // Byte array of message received.
    enum message_type type;             // Type of message received. 
    int socket;
    struct request_elem * next;         // Pointer to next task.
};

// Special FIFO list, will repeat after ended. Will not remove when accessed (no pop).
struct task_list {
   struct task *home;
   struct task *head;
};

struct task {
    size_t id;
    struct byte_array * data;
    struct task *next;
};

struct task_registry {
    size_t task_id;
    int tm_id;
    struct timeval * send_time;
    struct timeval * completed_time;
    struct task_registry *next;
};

void job_manager(int argc, char *argv[], char *so, struct byte_array *final_result, struct jm_thread_data * td);
void * jm_gen_worker(void * ptr);
void * jm_worker(void * ptr);

typedef void * (*spitz_ctor_t) (int, char **);
typedef int    (*spitz_tgen_t) (void *, struct byte_array *);

#endif /* __SPITZ_JOB_MANAGER_H__ */
