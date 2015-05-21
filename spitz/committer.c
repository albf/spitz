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

#include "committer.h"

#include <dlfcn.h>
#include "comm.h"
#include "barray.h"
#include "log.h"
#include "spitz.h"
#include <pthread.h> 


// Responsible for the commit thread. May or not be used, check spitz.h. 
void * commit_worker(void *ptr) {
    struct cm_thread_data * cd = (struct cm_thread_data *) ptr;
    int any_work = 1;
    void (*commit_pit) (void *, struct byte_array *);
    struct cm_result_node * res;

    // Main reason for this function: make this work in parallel.
    *(void **)(&commit_pit) = dlsym(cd->handle, "spits_commit_pit");

    while(any_work) {
        sem_wait(&cd->r_counter); 
        debug("Received work in Commit Worker.");

        // Get/Pop head result to compute.
        pthread_mutex_lock(&cd->r_lock);                          
        res = cd->head;
        if(res != NULL) {
            if(cd->head == cd->results) {
                cd->head = NULL;
                cd->results = NULL;
            }
            else {
                cd->head = cd->head->next;
            }
        }

        pthread_mutex_unlock(&cd->r_lock);                          

        // If found anything, commit, if not, just exit.
        if(res != NULL) {
            commit_pit(cd->user_data, res->ba);
            byte_array_free(res->ba);
            free(res->ba);  
        }
        else {
            any_work = 0;
        }
    } 

    // Warn main thread about the end. It will then commit_job.
    pthread_mutex_unlock(&cd->f_lock);
    pthread_exit(NULL);
}

// Responsible for the committer. Receives results, warns JM, process them. May use commit_worker.
void committer(int argc, char *argv[], struct cm_thread_data * cd)
{
    int is_finished=0;                                              // Indicate if it's finished.
    int origin_socket=0;                                            // Socket of the requester, return by COMM_wait_request.
    enum message_type type;                                         // Type of received message.
    uint64_t socket_cl;                                             // Closing socket, packed by the COMM_wait_request.
    int retries;                                                    // Auxiliary to establish connection with VM Task Manager.
    uint64_t buffer;                                                // Used for receiving 64 bits.
   
    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc(sizeof(struct byte_array));
    byte_array_init(ba, 100);

    // Changed: Indexing by task id, 0 for not committed and 1 for already committed.
    // Once a new task arrive, if it's more them actual cap, realloc using it's id*2
    int cap = 10;                                                    // Initial capacity.
    int i, old_cap;                                                  // Auxiliary to capacity
    int *committed = malloc(sizeof(int) * cap);                      // List indexed by the task id. 
                                                                     // 1 for committed, 0 for not yet.
    int task_id;                                                     // Task id of received result.
    int tm_rank;                                                     // Rank id of the worker that completed the work.
    struct cm_result_node * result;
    
    // Loads the user functions.
    void * (*setup) (int, char **);
    void (*commit_pit) (void *, struct byte_array *);
    void (*commit_job) (void *, struct byte_array *);

    *(void **)(&setup)      = dlsym(cd->handle, "spits_setup_commit");  // Loads the user functions.
    if(COMMIT_THREAD <= 0) {
        *(void **)(&commit_pit) = dlsym(cd->handle, "spits_commit_pit");
    }
    *(void **)(&commit_job) = dlsym(cd->handle, "spits_commit_job");
    //setup_free = dlsym("spits_setup_free_commit");
    cd->user_data = setup(argc, argv);

    for(i=0; i<cap; i++) {                                          // initialize the task array
        committed[i]=0;
    }
        
    info("Starting committer main loop");
    while (1) {
        COMM_wait_request(&type, &origin_socket, ba, NULL);
        
        switch (type) {
            case MSG_RESULT:
                byte_array_unpack64(ba, &buffer);
                task_id = (int) buffer;
                byte_array_unpack64(ba, &buffer);
                tm_rank = (int) buffer;

                debug("Got a RESULT message for task %d from %d", task_id, tm_rank);
                //debug("Size of Result: %d", (int)ba->len);
                if(task_id>=cap) {                                   // If id higher them actual cap
                    old_cap = cap;
                    cap=2*task_id;
                    committed = realloc(committed, sizeof(size_t)*cap);

                    for(i=old_cap; i<cap; i++) {
                        committed[i]=0;
                    }
                }

                if (committed[task_id] == 0) {                      // If not committed yet
                    committed[task_id] = 1;
                    info("Sending task %d to Job Manager.", task_id);
                    COMM_send_message(ba, MSG_DONE, socket_manager);

                    // Optional optimization.                    
                    if(COMMIT_THREAD > 0) {
                        result = (struct cm_result_node *) malloc(sizeof(struct cm_result_node));

                        // Push result obtained to FIFO. 
                        pthread_mutex_lock(&cd->r_lock);                          
                        result->ba = ba;
                        result->next = NULL;
                        if(cd->results != NULL) {
                            cd->results->next = result;
                        }
                        else {
                            cd->head = result;
                        }

                        cd->results = result;
                        pthread_mutex_unlock(&cd->r_lock);

                        // Warn thread responsible for committing pits.
                        sem_post(&cd->r_counter);
                        
                        // HAVE TO ALLOCATE NEW BA: the other one is linked in result FIFO.
                        ba = (struct byte_array *) malloc (sizeof(struct byte_array));
                        byte_array_init(ba, 100);
                    }

                    else {
                        // Wihout optimization: Just commit in the same thread.
                        commit_pit(cd->user_data, ba);
                        byte_array_free(ba);
                        free(ba);  
                    }
                }

                break;
            case MSG_KILL:                                          // Received kill from Job Manager.
                info("Got a KILL message, committing job");
                byte_array_clear(ba);
                
                if(COMMIT_THREAD > 0) {
                    sem_post(&cd->r_counter); 
                    pthread_mutex_lock(&cd->f_lock);                          
                }

                if (commit_job) {
                    commit_job(cd->user_data, ba);
                    COMM_send_message(ba, MSG_RESULT, origin_socket); 
                }
                is_finished = 1;
                break;
            case MSG_NEW_CONNECTION:
                COMM_create_new_connection();
                break;
            case MSG_CLOSE_CONNECTION:  ;
                _byte_array_unpack64(ba, &socket_cl);
                COMM_close_connection((int)socket_cl);
                break;
             case MSG_NEW_VM_TASK_MANAGER:
                retries = 3;
                info("Received information about VM task manager waiting connection.");
                COMM_connect_to_vm_task_manager(&retries, ba);
                break;
            default:
                break;
        }

        // If he is the only member alive and the work is finished.
        if (is_finished==1){
            info("Work is done, time to die");
            COMM_close_all_connections(); 
            break;
        }
    }

    byte_array_free(ba);
    free(cd->user_data);
    free(committed);
    free(ba);
    info("Terminating committer");
}
