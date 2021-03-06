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
#include "journal.h"
#include "spitz.h"
#include <pthread.h> 
#include <sys/time.h>


// Responsible for the commit thread. May or not be used, check spitz.h. 
void * commit_worker(void *ptr) {
    struct cm_thread_data * cd = (struct cm_thread_data *) ptr;
    int any_work = 1;
    void (*commit_pit) (void *, struct byte_array *);
    struct cm_result_node * res;
    int j_id=0;
    struct j_entry * entry;

    // Main reason for this function: make this work in parallel.
    *(void **)(&commit_pit) = dlsym(cd->handle, "spits_commit_pit");

    if(CM_KEEP_JOURNAL > 0) {
        j_id = JOURNAL_get_id(cd->dia, 'W');
    }

    debug("[commit_worker] starting");

    while(any_work) {
        sem_wait(&cd->r_counter); 
        debug("[commit_worker] Received work in Commit Worker.");

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
            if(CM_KEEP_JOURNAL > 0) {
                entry = JOURNAL_new_entry(cd->dia, j_id);
                entry->action = 'P';
                gettimeofday(&entry->start, NULL);
            }

            commit_pit(cd->user_data, res->ba);

            if(CM_KEEP_JOURNAL > 0) {
                gettimeofday(&entry->end, NULL);
            }

            byte_array_free(res->ba);
            free(res->ba);  
            free(res);
        }
        else {
            any_work = 0;
        }
    } 

    // Warn main thread about the end. It will then commit_job.
    pthread_mutex_unlock(&cd->f_lock);
    debug("[commit_worker] exiting");
    pthread_exit(NULL);
}

// Threads responsible for reading things from TMs in parallel.
void * read_worker(void *ptr) {
    struct cm_thread_data * cd = (struct cm_thread_data *) ptr;
    struct byte_array * ba;
    struct socket_entry * se;
    enum message_type mtype;
    int a_socket = COMM_connect_to_itself(PORT_COMMITTER);
    int ret;

    // Journal
    int j_id=0;
    struct j_entry * entry;

    debug("[read_worker] starting");

    if(CM_KEEP_JOURNAL > 0) {
        j_id = JOURNAL_get_id(cd->dia, 'R');
    }

    sem_wait(&cd->blacklist);
    while(cd->r_kill <= 0) {
        debug("[read_worker] someone will send a task.");
        pthread_mutex_lock(&cd->sb_lock);                          
        se = cd->sb->home;
        while(se->mark != 0) {
            se = se->next;
        }
        se->mark = 1;
        pthread_mutex_unlock(&cd->sb_lock);                          

        // Need to check if message was send sucessfully.
        ba = (struct byte_array *) malloc(sizeof(struct byte_array));
        byte_array_init(ba,10);

        if(CM_KEEP_JOURNAL > 0){
            entry = JOURNAL_new_entry(cd->dia, j_id);
            entry->action = 'R';
            gettimeofday(&entry->start, NULL);
        }

        ret = COMM_read_message(ba, &mtype, se->socket);

        if(CM_KEEP_JOURNAL > 0){
            if(mtype == MSG_RESULT) {
                debug("Received result of size : %d", (int)ba->len);
                gettimeofday(&entry->end, NULL);
                entry->size = (int)ba->len;
            }
            else {
                JOURNAL_remove_entry(cd->dia, j_id);
            }
        }
        
        debug("[read_worker] done reading result.");

        // Mark read_request as done.
        se->ba = ba;
        if(ret >= 0) {
            se->mark = 2;
        }
        else {  // Problem occured, remove this element 
            se->mark = -2;
        }

        // Send refresh message to myself, remove from blacklist.
        COMM_send_message(NULL, MSG_REFRESH, a_socket);
        sem_wait(&cd->blacklist);
    }

    debug("[read_worker] Exiting.");
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
    uint64_t buffer, buffer2;                                       // Used for receiving 64 bits.
   
    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc(sizeof(struct byte_array));
    struct byte_array * msg = (struct byte_array *) malloc(sizeof(struct byte_array));
    byte_array_init(ba, 100);
    byte_array_init(msg, 32);

    // Changed: Indexing by task id, 0 for not committed and 1 for already committed.
    // Once a new task arrive, if it's more them actual cap, realloc using it's id*2
    int cap = 10;                                                    // Initial capacity.
    int i, old_cap;                                                  // Auxiliary to capacity
    int *committed = malloc(sizeof(int) * cap);                      // List indexed by the task id. 
                                                                     // 1 for committed, 0 for not yet.
    int task_id;                                                     // Task id of received result.
    int tm_rank;                                                     // Rank id of the worker that completed the work.
    struct cm_result_node * result;                                  // Pass task to commit_worker.
    
    // Loads the user functions.
    void * (*setup) (int, char **);
    void (*commit_pit) (void *, struct byte_array *);
    void (*commit_job) (void *, struct byte_array *);

    // Journal
    int j_id=0;
    struct j_entry * entry;

    // Read Threads auxiliar 
    struct socket_entry * se, * prev;                                   // Auxiliar for blacklist.
    int is_refresh = 0;

    *(void **)(&setup)      = dlsym(cd->handle, "spits_setup_commit");  // Loads the user functions.
    if(CM_COMMIT_THREAD <= 0) {
        *(void **)(&commit_pit) = dlsym(cd->handle, "spits_commit_pit");
    }
    *(void **)(&commit_job) = dlsym(cd->handle, "spits_commit_job");
    //setup_free = dlsym("spits_setup_free_commit");
    cd->user_data = setup(argc, argv);

    for(i=0; i<cap; i++) {                                              // initialize the task array
        committed[i]=0;
    }

    if(CM_KEEP_JOURNAL > 0) {
        j_id = JOURNAL_get_id(cd->dia, 'C');
    }

    info("Starting committer main loop");
    while (1) {
        if(CM_READ_THREADS > 0) {
            if(cd->sb->size > 0) {
                COMM_wait_request(&type, &origin_socket, ba, NULL, j_id, cd->dia, cd->sb);
            }
            else {
                COMM_wait_request(&type, &origin_socket, ba, NULL, j_id, cd->dia, NULL);
            }
        }
        else {
            COMM_wait_request(&type, &origin_socket, ba, NULL, j_id, cd->dia, NULL);
        }
        
        switch (type) {
            case MSG_OFFER_RESULT: ;
                byte_array_unpack64(ba, &buffer);
                task_id = (int) buffer;
                debug("Received result offer for task %d.", task_id); 

                if(task_id>=cap) {                                   // If id higher them actual cap
                    old_cap = cap;
                    cap=2*task_id;
                    committed = realloc(committed, sizeof(size_t)*cap);

                    for(i=old_cap; i<cap; i++) {
                        committed[i]=0;
                    }
                }

                if(committed[task_id] == 0) {
                    //debug("Will ask for the task. \n");
                    buffer2 = 1;

                    // if have separate threads to read, make them know.
                    if(CM_READ_THREADS > 0) {
                        se = (struct socket_entry *) malloc (sizeof(struct socket_entry));
                        se->socket = origin_socket;
                        se->mark = 0;
                        se->next = NULL;
                        if(cd->sb->home == NULL) {
                            cd->sb->home = se;
                            cd->sb->head = se;
                            cd->sb->size = 1;
                        }
                        else {
                            cd->sb->head->next = se;
                            cd->sb->head = se;
                            cd->sb->size++;
                        }
                        sem_post(&cd->blacklist);
                    }
                }
                else {
                    //debug("Will drop the request. \n");
                    buffer2 = -1;
                }

                byte_array_clear(msg);
                byte_array_pack64(msg, buffer2);
                COMM_send_message(msg, MSG_DONE, origin_socket);
                break;

            case MSG_REFRESH:
                if(CM_READ_THREADS <= 0) {
                    error("Main thread shouldn't received MSG_REFRESH when CM_READ_THREADS <= 0"); 
                    return;
                }
                is_refresh = 1;
                debug("Received MSG_REFRESH");

                // First find the someone finished receiving.
                prev = NULL;
                se = cd->sb->home;
                // Must use absolute value, might be 2 (success) or -2 (failures)
                while((se!=NULL) && (abs(se->mark) < 2)) {
                    prev = se;
                    se = se->next;
                }

                // Couldn't found? A bug?
                if(se == NULL) {
                    error("MSG Refresh was sent but couldn't found result in FIFO.");
                    break;
                }

                // Someone finished! Lets get this bad boy.
                else {
                    pthread_mutex_lock(&cd->sb_lock);                          
                    if(prev == NULL) {
                       cd->sb->home = se->next; 
                    }
                    else {
                        prev->next = se->next;
                        if(se == cd->sb->head) {
                            cd->sb->head = prev;
                        }
                    }
                    cd->sb->size--;
                    pthread_mutex_unlock(&cd->sb_lock);                          

                    // Used the result for this iteration.
                    if(se->mark > 0) {
                        type = MSG_RESULT;
                        ba = se->ba;
                    }
                    // Check for erros.
                    else {
                        free(ba);
                        break;
                    }
                }

            // DONT'T BREAK, refresh means some result was sucesfully received.
            case MSG_RESULT:
                if(CM_READ_THREADS > 0) {
                    if(is_refresh == 0) {
                        error("Main thread shouldn't received MSG_RESULT when CM_READ_THREADS > 0"); 
                    }
                    else {
                        is_refresh = 0;
                    }
                }

                byte_array_unpack64(ba, &buffer);
                task_id = (int) buffer;
                byte_array_unpack64(ba, &buffer2);
                tm_rank = (int) buffer2;

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

                    byte_array_clear(msg);
                    byte_array_pack64(msg, buffer);
                    byte_array_pack64(msg, buffer2);

                    COMM_send_message(msg, MSG_DONE, socket_manager);

                    // Optional optimization.                    
                    if(CM_COMMIT_THREAD > 0) {
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
                
                if(CM_COMMIT_THREAD > 0) {
                    sem_post(&cd->r_counter); 
                    pthread_mutex_lock(&cd->f_lock);                          
                }

                if (commit_job) {
                    if(CM_KEEP_JOURNAL > 0) {
                        entry = JOURNAL_new_entry(cd->dia, j_id);
                        entry->action = 'J';
                        gettimeofday(&entry->start, NULL);
                    }

                    commit_job(cd->user_data, ba);

                    if(CM_KEEP_JOURNAL > 0) {
                        gettimeofday(&entry->end, NULL);
                    }

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
                COMM_connect_to_vm_task_manager(&retries, ba, NULL);
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
    byte_array_free(msg);
    free(cd->user_data);
    free(committed);
    free(ba);
    free(msg);
    info("Terminating committer");
}
