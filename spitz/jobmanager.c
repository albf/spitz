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

#include "jobmanager.h"

#include <dlfcn.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "comm.h"
#include "log.h"
#include "barray.h"
#include "registry.h"
#include "journal.h"
#include "spitz.h"
#include <pthread.h>
#include <unistd.h>

#define ARGS_C 0

// Push a task into the thread FIFO.
void append_request(struct jm_thread_data * td, enum message_type type, int socket) {
    struct request_elem * new_elem = (struct request_elem *) malloc (sizeof(struct request_elem));
    struct request_FIFO * FIFO = td->request_list;
   
    // Allocate new element.
    new_elem->type = type;
    new_elem->socket = socket;
    new_elem->next = NULL;

    // Only one thread poke the FIFO.
    pthread_mutex_lock(&td->lock);
        
    if(FIFO->first== NULL) {
        FIFO->first = new_elem;
        FIFO->last = new_elem;
        new_elem->next = NULL;
    }
    else {
        FIFO->last->next = new_elem;
        FIFO->last = new_elem;
    }

    // Let the FIFO go and mark that one task was added.
    pthread_mutex_unlock(&td->lock);
    sem_post(&td->num_requests);
}

// Pop a task from the task FIFO list. Need to free memory in the future.
struct request_elem * pop_request(struct jm_thread_data * td) {
    struct request_elem * ret;
    
    // Wait for new functions;
    sem_wait(&td->num_requests);        // Important : let sem_wait before mutex_lock
    pthread_mutex_lock(&td->lock);
  
    // Get task and remove the first one.
    ret = td->request_list->first;
    if(ret != NULL) {
        td->request_list->first = td->request_list->first->next;
    }
    
    // Let the FIFO go.
    pthread_mutex_unlock(&td->lock);
    return ret;
}

// Add task to the task list. No repeated tasks will be generated with current algorithm.
void add_task(struct jm_thread_data *td, struct task *node) {
    pthread_mutex_lock(&td->tl_lock);
    
    node->next = NULL;
    if(td->tasks->home == NULL) {
        td->tasks->home = node;
        td->tasks->head = node;
    }
    else {
        td->tasks->head->next = node;
        td->tasks->head = node;
    }

    // Let the FIFO go.
    pthread_mutex_unlock(&td->tl_lock);
}

// Get next task iteration from task list. Return NULL if list is empty, will cycle otherwise.
struct task * next_task (struct jm_thread_data * td, int rank) {
    struct task * ret;
    struct task * aux;

    // Lock the task list.
    pthread_mutex_lock(&td->tl_lock);

    // Check if mark is null, if it is the computation is over or busy generating other tasks.
    if(td->tasks->home == NULL) {
        if ((td->is_num_tasks_total_found > 0) && (td->task_counter >= td->num_tasks_total)) {
            td->is_finished = 1;
        }
        ret = NULL;
    }

    else {
        if(JM_KEEP_REGISTRY > 0) {
            // Get the next new one for the current node and updates list. 
            ret = td->tasks->home;
            aux = NULL;
            while((ret != NULL)&&(REGISTRY_check_registry(td, ret->id, rank) == 1)) {
                aux = ret;
                ret = ret->next;
            }

            if(ret != NULL) {
                // Not home and not head.
                if(aux != NULL)  {
                    if (ret != td->tasks->head) {
                        aux->next = ret->next;
                        td->tasks->head->next = ret;
                        td->tasks->head = ret;
                        ret->next = NULL;
                    }
                }
                // It's home.
                else {
                    ret = td->tasks->home;
                    if(td->tasks->home->next != NULL) {
                        td->tasks->head->next = td->tasks->home;
                        td->tasks->home = td->tasks->home->next;
                        td->tasks->head = td->tasks->head->next;
                        td->tasks->head->next = NULL;
                    }
                }
                
            }
            
        }
        else {
            // Get the marked one and updates list. 
            ret = td->tasks->home;
            if(td->tasks->home->next != NULL) {
                td->tasks->head->next = td->tasks->home;
                td->tasks->home = td->tasks->home->next;
                td->tasks->head = td->tasks->head->next;
                td->tasks->head->next = NULL;
            }
        }
    }
    // Let the FIFO go.
    pthread_mutex_unlock(&td->tl_lock);

    if(ret != NULL) {
        ret->send_counter++;
    }
    
    return ret;
}

// Remove task with the id provided if it exists.
void remove_task (struct jm_thread_data * td, int tid) {
    struct task *iter, *prev, *clean;              // Pointers to iterate through FIFO. 
    
    pthread_mutex_lock(&td->tl_lock);

    iter = td->tasks->home;
    prev = NULL;

    // search for the task that finished
    while ((iter != NULL)&&(iter->id != tid)) {
        prev = iter;
        iter = iter->next;
    }

    if(iter != NULL) {
        // if there is a previous in the list. 
        if(prev) {
            clean = iter;
            prev->next = iter->next;
        }
        // If not, iter = home.
        else {
            debug("Home Task Removed");
            clean = td->tasks->home; 
            td->tasks->home = td->tasks->home->next;
            // If its null and all is generated, computation is over.
            if((td->tasks->home == NULL) && (td->all_generated > 0)) {
                debug("Home is now null. Is it over?");
                pthread_mutex_lock(&td->gc_lock);
                if(td->g_counter == 0) {
                    debug("Finally Over!");
                    td->is_finished = 1; 
                }
                pthread_mutex_unlock(&td->gc_lock);
            }
            // Good debug, but access value even if home is null.
            /*else {
                debug("Not null, td->all_generated:%d", td->all_generated);
                debug("Not null, address of home:%d", td->tasks->home);
                debug("Not null, id of home:%d", td->tasks->home->id);
            }*/
        }

        // If it's the head, updates it.
        if(iter == td->tasks->head) {
            td->tasks->head = prev;
        }

        // Can clean for now, committer will send only one message.
        byte_array_free(clean->data);
        free(clean->data);
        free(clean); 
    }

    // Let the FIFO go.
    pthread_mutex_unlock(&td->tl_lock);
}

// Get the next id used for generating the next task. Returns -1 if all tasks were generated already. 
int next_task_num(struct jm_thread_data *td) {
    int ret;

    // Get current task and just add one.
    pthread_mutex_lock(&td->tc_lock);

    // Get current task counter value and update counter. 
    ret = td->task_counter;
    td->task_counter++;
    
    pthread_mutex_unlock(&td->tc_lock);
    
    return ret;
}

// Function responsible for the generating worker.
void * jm_gen_worker(void * ptr) {
    struct jm_thread_data * td = ptr;
    spitz_tgen_t tgen = dlsym(td ->handle, "spits_job_manager_next_task");
    int j_id=0, len=0;
    uint64_t buffer;
    struct j_entry * entry;
    struct task * node;
    struct byte_array * ba;

    if(JM_KEEP_JOURNAL > 0) {
        j_id = JOURNAL_get_id(td->dia, 'G');
    }

    if(JM_GEN_BUFFER > 0) {
        sem_wait(&td->create_done_unlock);
    }

    // Dies only when gen_kill is set to > 0
    while(td->gen_kill <= 0) {
        // Wait for requests.
        sem_wait(&td->gen_request);

        // Try to generate task.
        debug("[jm_gen_worker] Received task to generate.");
        
        if(td->all_generated == 0) {

            if(JM_KEEP_JOURNAL > 0) {
                entry = JOURNAL_new_entry(td->dia, j_id);
                entry->action = 'G';
                gettimeofday(&entry->start, NULL);
            }

            ba = (struct byte_array *) malloc (sizeof(struct byte_array)); 
            byte_array_init(ba, 10);
            // Keep room to the task_id.
            buffer = 0;
            byte_array_pack64(ba, buffer);

            if(!(tgen(td->user_data, ba))) {
                // Can't generate, assume this is the end.
                pthread_mutex_lock(&td->tc_lock);
                td->num_tasks_total = td->task_counter;
                td->is_num_tasks_total_found = 1;
                pthread_mutex_unlock(&td->tc_lock);

                debug("All tasks have been generated");
                td->all_generated = 1;

                // Free ba memory, as it's not used.
                byte_array_free(ba);
                free(ba);
            } 
            else {
                // If task was generated, add it to queue and send it.
                node = (struct task *) malloc (sizeof(struct task));
                node->id = next_task_num(td);

                // Magic recover of ID.
                len = ba->len;
                ba->len = 0;
                buffer = (uint64_t) node->id;
                byte_array_pack64(ba, buffer);
                ba->len = len;

                node->data = ba;
                node->send_counter = 0;
                add_task(td, node);
            }

            if(JM_KEEP_JOURNAL > 0) {
                gettimeofday(&entry->end, NULL);
            }


        }
        
        // Release waiting task.
        sem_post(&td->gen_completed);
    }
    
    debug("[jm_gen_worker] Exiting, end of computation.");
    pthread_exit(NULL);
}

// Function reponsible for the workers on JM, will send the tasks messages
void * jm_worker(void * ptr) {
    struct jm_thread_data * td = ptr;
    struct request_elem * my_request;
    spitz_tgen_t tgen;
    struct task *node;                                              // Pointer of new task.
    struct connected_ip *client;
    int rank, j_id=0, len;
    uint64_t buffer;
    struct j_entry * entry;
    struct byte_array * ba;

    if(JM_GEN_THREADS <= 0) {
        tgen = dlsym(td ->handle, "spits_job_manager_next_task");
    }

    if(JM_KEEP_JOURNAL > 0) {
        j_id = JOURNAL_get_id(td->dia, 'E');
    }

    while (1) {
        my_request = pop_request(td);

        if(my_request == NULL) {
            break;
        }

        else if(my_request->type == MSG_KILL) {
            debug("[jm_worker] Received kill.");
            break; 
        }

        else if(my_request->type == MSG_READY) {
            debug("[jm_worker] Received MSG_READY.");

            client = LIST_search_socket(COMM_ip_list, my_request->socket);

            if(client == NULL) {
                error("Couldn't find client with socket provided by main thread. Ignoring request.");
            } 

            else {
                rank = client->id;
                debug("Trying to generate a task for TM %d", rank);

                if (td->all_generated <= 0) {
                    // Generate task, if possible in parallel. 
                    if(JM_GEN_THREADS <= 0) {
                        // Update current_gen value
                        if(JM_SEND_THREADS > 1) {
                            pthread_mutex_lock(&td->gc_lock);
                            td->g_counter++;
                            pthread_mutex_unlock(&td->gc_lock);
                        }

                        // Make new entry in journal.
                        if(JM_KEEP_JOURNAL > 0) {
                            entry = JOURNAL_new_entry(td->dia, j_id);
                            entry->action = 'G';
                            gettimeofday(&entry->start, NULL);
                        }

                        ba = (struct byte_array *) malloc (sizeof(struct byte_array)); 
                        byte_array_init(ba, 10);
                        // Keep room to the task_id.
                        buffer = 0;
                        byte_array_pack64(ba, buffer);

                        // try to generate task.
                        if(tgen(td->user_data, ba)) {
                            // If task was generated, add it to queue and send it.
                            node = (struct task *) malloc (sizeof(struct task));
                            node->id = next_task_num(td);
 
                            // Magic recover of ID.
                            len = ba->len;
                            ba->len = 0;
                            buffer = (uint64_t) node->id;
                            byte_array_pack64(ba, buffer);
                            ba->len = len;
 
                           node->data = ba;
                            node->send_counter = 0;
                            add_task(td, node);
                        }
                        else {
                            // Can't generate, assume this is the end.
                            pthread_mutex_lock(&td->tc_lock);
                            td->num_tasks_total = td->task_counter;
                            td->is_num_tasks_total_found = 1;
                            pthread_mutex_unlock(&td->tc_lock);

                            debug("All tasks have been generated");
                            td->all_generated = 1;

                            // Free ba memory, as it's not used.
                            byte_array_free(ba);
                            free(ba);
                        }

                        // Get end time of entry in journal.
                        if(JM_KEEP_JOURNAL > 0) {
                            gettimeofday(&entry->end, NULL);
                        }


                        // Update current_gen value
                        if(JM_SEND_THREADS > 1) {
                            pthread_mutex_lock(&td->gc_lock);
                            td->g_counter--;
                            pthread_mutex_unlock(&td->gc_lock);
                        }

                    }

                    // Have one or more gen_threads. 
                    else {
                        // Update current_gen value
                        if(JM_SEND_THREADS > 1) {
                            pthread_mutex_lock(&td->gc_lock);
                            td->g_counter++;
                            pthread_mutex_unlock(&td->gc_lock);
                        }

                        // Request task to be generated.  
                        sem_post(&td->gen_request);
                        // Wait for a feedback from gen_thread.
                        sem_wait(&td->gen_completed); 

                        // Update current_gen value
                        if(JM_SEND_THREADS > 1) {
                            pthread_mutex_lock(&td->gc_lock);
                            td->g_counter--;
                            pthread_mutex_unlock(&td->gc_lock);
                        }
                    }
                }
 
                // If couldn't generate, try to send a repeated one.
                while(1) {
                    node = next_task(td, rank);
                    if (node == NULL) {
                        // Couldn't find a task because computation is finished.
                        // Logic: If it's finished or there is no one generating anything, why couldn't I find anything to send?
                        //if((td->is_finished > 0)||(GEN_PARALLEL == 0)||(td->g_counter == 0)) {
                        if((td->is_finished > 0) || ((td->all_generated > 0) && (td->g_counter == 0))) {
                            debug("Sending kill message to rank %d and killing worker, nothing to be done",rank);
                            debug("td->is_finished: %d",td->is_finished);
                            debug("td->all_generated: %d",td->all_generated);
                            debug("td->g_counter: %d",td->g_counter);
                            COMM_send_message(NULL, MSG_KILL, my_request->socket);
                            break;
                        }
                        // Couldn't find a task because the computation isn't finished. Wait?
                        else {
                            debug("Sleeping rank %d ---",rank);
                            debug("td->is_finished: %d",td->is_finished);
                            debug("td->all_generated: %d",td->all_generated);
                            debug("td->g_counter: %d",td->g_counter);
                            sleep(1);
                        }
                    }
                    // Found a task, will replicate to another node.
                    else {
                        if(node->send_counter == 1) {
                            debug("Sending generated task %d to %d", node->id, rank);
                        }
                        else {
                            debug("Replicating task %d to rank %d",node->id,rank);
                        }

                        LIST_update_tasks_info (COMM_ip_list, NULL, -1, rank, 1, 0);
                        if(JM_KEEP_REGISTRY>0) {
                            REGISTRY_add_registry(td, node->id,rank);
                        }

                        if(JM_KEEP_JOURNAL > 0) {
                            entry = JOURNAL_new_entry(td->dia, j_id);
                            entry->action = 'S';
                            gettimeofday(&entry->start, NULL);
                        }

                        COMM_send_message(node->data, MSG_TASK, my_request->socket);

                        if(JM_KEEP_JOURNAL > 0) {
                            gettimeofday(&entry->end, NULL);
                        }

                        break;
                    }
                }
            }
            
        }

        else {
            error("Unexpected type of message for jobmanager worker.");
        }
    
        free(my_request);
    }

    if(my_request != NULL) {
        free(my_request);
    }

    pthread_exit(NULL);
}

// Thread responsible for doing VM restore. 
void * jm_vm_healer(void * ptr) {
    struct jm_thread_data * td = ptr;
    int a_socket = COMM_connect_to_itself(PORT_MANAGER);

    //Journal related
    struct j_entry * entry;
    int j_id=0, r_socket;

    debug("[jm_vm_healer] starting");

    if(JM_KEEP_JOURNAL > 0) {
        j_id = JOURNAL_get_id(td->dia, 'H');
    }

    sem_wait(&td->vm_lost);
    while(td->vm_h_kill <= 0) {
        if(JM_KEEP_JOURNAL > 0) {
            entry = JOURNAL_new_entry(td->dia, j_id);
            entry->action = 'V';
            gettimeofday(&entry->start, NULL);
        }

        r_socket = COMM_check_VM_nodes(COMM_ip_list); 

        if(JM_KEEP_JOURNAL > 0) {
            gettimeofday(&entry->end, NULL);
        }

        // In case of success, send refresh message.
        if(r_socket > 0) {
            info("VM node was successfully restored.");
            COMM_send_message(NULL, MSG_REFRESH, a_socket);
        }
        sem_wait(&td->vm_lost);
    }

    debug("[jm_vm_healer] exiting");
    pthread_exit(NULL);
}

// Function responsible for the behavior of the job manager.
void job_manager(int argc, char *argv[], char *so, struct byte_array *final_result, struct jm_thread_data * td)
{
    enum message_type type;                                         // Type of received message.
    uint64_t socket_cl;                                             // Closing socket.
    int origin_socket;                                              // Socket that sent the request.
    int run_num;                                                    // Actual run number.
    char * v;                                                       // Used as auxiliary. 
    ssize_t n;                                                      // Used as auxiliary.
    int retries;                                                    // Auxiliary to establish connection with VM Task Manager.
    uint64_t buffer;                                                // buffer used for received uint64_t values 
    //struct timeval tv;                                            // Timeout for select. (not used, check jm_vm_healer).
    int i;                                                          // Iterator

    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc (sizeof(struct byte_array)); 
    byte_array_init(ba, 10);

    // Binary Array used to store the binary, the .so. 
    struct byte_array * ba_binary = (struct byte_array *) malloc (sizeof(struct byte_array));
    byte_array_init(ba_binary, 0);
    byte_array_pack_binary(ba_binary, so);
 
    // Binary Array used to store the hash of the binary
    struct byte_array * ba_hash = (struct byte_array *) malloc (sizeof(struct byte_array));
    byte_array_init(ba_hash, 0);
    byte_array_compute_hash(ba_hash, ba_binary);

    // Connected_ip array used to find VM info to send to Committer. Used for VM connection.
    struct connected_ip * vm_node;
    char port[10];                              // port of node.
    char ack[2];                                // ack message sent to monitor.
    
    //void *user_data = ctor((argc), (argv));

    // Information about the completed task.
    int tid, tm_id;

    // Used for id recovery.
    int rank_rcv, rank_original;

    //Journal related
    struct j_entry * entry;
    int j_id=0;

    // Refresh related
    struct connected_ip * ptr;

    if(CHECK_ARGS > 0) {
        if(argc < ARGC_C) {
            error("Wrong number of arguments in argc");
        }
        if(argv == NULL) {
            error("NULL argv");
        }
        if(final_result == NULL) {
            error("NULL final_result struct");
        }
    }

    if(JM_KEEP_JOURNAL > 0) {
        j_id = JOURNAL_get_id(td->dia, 'M');
    }

    while (1) {

        // Set timeout values for wait_request. // Not using timeout anymore, check jm_vm_healer function above.
        //tv.tv_sec = JM_WAIT_REQUEST_TIMEOUT_SEC;
        //tv.tv_usec = JM_WAIT_REQUEST_TIMEOUT_USEC;
        COMM_wait_request(&type, &origin_socket, ba, NULL, -1, NULL, NULL); 

        if(JM_KEEP_JOURNAL > 0) {
            entry = JOURNAL_new_entry(td->dia, j_id);
            entry->action = 'R';
            gettimeofday(&entry->start, NULL);
        }
        
        switch (type) {
            case MSG_READY:
                if(td->is_done_loading == 1) {
                    append_request(td, type, origin_socket);
                }
                else {
                    debug("Received task request from %d but shared data is still being load.", LIST_search_socket(COMM_ip_list, origin_socket));
                    COMM_send_message(NULL, MSG_NO_TASK, origin_socket);
                } 
                break;
            case MSG_DONE:;
                byte_array_unpack64(ba, &buffer);
                tid = (int)buffer;
                byte_array_unpack64(ba, &buffer);
                tm_id = (int)buffer;
                debug("TASK %d was completed by %d!", tid, tm_id);

                if(JM_KEEP_REGISTRY > 0) {
                    REGISTRY_add_completion_registry (td, tid, tm_id);	
                }
                
                remove_task(td, tid);
                LIST_update_tasks_info (COMM_ip_list,NULL,-1, tm_id, 0, 1);
                break;
            case MSG_NEW_CONNECTION:
                COMM_create_new_connection();
                break;
            case MSG_CLOSE_CONNECTION:
                _byte_array_unpack64(ba, &socket_cl);
                // Sidenote: COMM_close_connection returns 1 if it has closed a VM TM.
                if(COMM_close_connection((int)socket_cl) > 0) {
                // Try to warn healer.
                    if(JM_VM_RESTORE > 0) {
                        sem_post(&td->vm_lost);
                    }
                    else {
                        warning("VM disconnected. Can't try to reconnect because ANY_VM_TASK_MANAGER is set as <= 0");
                    }
                }
                break;
            case MSG_GET_COMMITTER:
                COMM_send_committer(origin_socket);
                break;
            case MSG_GET_PATH:
                v = lib_path;
                n = (size_t) (strlen(v)+1); 
                byte_array_init(ba, n);
                byte_array_pack8v(ba, v, n);
                COMM_send_message(ba, MSG_STRING, origin_socket);
                break;
            case MSG_GET_RUNNUM:
                byte_array_clear(ba);
                run_num = COMM_get_run_num();
                byte_array_pack64(ba, run_num);
                COMM_send_message(ba,MSG_GET_RUNNUM,origin_socket);
                break;
            case MSG_SET_COMMITTER:
                COMM_register_committer(origin_socket);
                break;
            case MSG_SET_MONITOR:
                COMM_register_monitor(origin_socket); 
                break;
            case MSG_SET_TASK_MANAGER_ID:
                byte_array_unpack64(ba, &buffer);
                rank_rcv = (int)buffer;
                byte_array_unpack64(ba, &buffer);
                rank_original = (int)buffer;
                COMM_ip_list = LIST_update_id(COMM_ip_list, rank_rcv, rank_original);
                break;
            case MSG_GET_ALIVE:
                COMM_send_int(origin_socket, COMM_alive);
                break;
            case MSG_GET_BINARY:
                COMM_send_message(ba_binary, MSG_GET_BINARY, origin_socket);
                break;
            case MSG_GET_HASH:
                COMM_send_message(ba_hash, MSG_GET_HASH, origin_socket);
                break;
            case MSG_KILL:
                if(origin_socket != socket_committer) {
                    error("Received kill from a worker, not allowed.");
                }
                else {
                    info("Computation stopped by the committer");
                    td->is_finished = 1;
                }
                break;
            case MSG_GET_STATUS:
                v = LIST_generate_info(COMM_ip_list,NULL);
                n = (size_t) (strlen(v)+1); 
                byte_array_init(ba, n);
                byte_array_pack8v(ba, v, n);
                COMM_send_message(ba, MSG_STRING, origin_socket);
                free(v);
                break;
            case MSG_GET_REGISTRY:
                if(JM_KEEP_REGISTRY > 0) {
                    v = REGISTRY_generate_info(td, NULL);
                    n = (size_t) (strlen(v)+1); 
                    byte_array_init(ba, n);
                    byte_array_pack8v(ba, v, n);
                    COMM_send_message(ba, MSG_STRING, origin_socket);
                    free(v);
                }
                else {
                    COMM_send_message(NULL, MSG_EMPTY, origin_socket);
                }
                break;
            case MSG_EMPTY:
                info("Nothing received: Timeout or problem in wait_request().");
                break;
            case MSG_NEW_VM_TASK_MANAGER:
                retries = 1;
                info("Received information about VM task manager waiting connection.");
                //COMM_send_message(ba, MSG_NEW_VM_TASK_MANAGER, socket_committer);
                if(COMM_connect_to_vm_task_manager(&retries, ba, NULL)<0) {
                    ack[0] = 'N';
                    info("Could not connect to VM Task Manager.");
                }
                else {
                    ack[0] = 'Y'; 
                }
                ack[1] = '\0';

                n = (size_t) 2; 
                byte_array_init(ba, n);
                byte_array_pack8v(ba, ack, n);  
                COMM_send_message(ba, MSG_STRING, origin_socket);
                break;
            case MSG_SEND_VM_TO_COMMITTER:
                info("MSG_SEND_VM_TO_COMMITTER request received!");
                // xxx.xxx.xxx.xxx|yyyyyyyy\0
                v = malloc(sizeof(char)*25);
                v[0] = '\0';
                vm_node = LIST_search_socket(COMM_ip_list, origin_socket); 

                // add address to string
                v = strcat(v, vm_node->address);
                v = strcat(v, "|");

                // cast ptr->port to string port and join with string.
                sprintf(port, "%d", vm_node->port);
                v = strcat(v, port);

                // find size of string and initialize the byte array.
                n = (size_t) (strlen(v)+1); 
                byte_array_init(ba, n);

                // packs the string and try to connect.
                byte_array_pack8v(ba, v, n);  
                info("MSG_SEND_VM_TO_COMMITTER was done by %s!", v);
                COMM_send_message(ba, MSG_NEW_VM_TASK_MANAGER, socket_committer);
                free(v);
                break;
            case MSG_GET_NUM_TASKS:
                // Make string with number of tasks and send it.
                v = malloc(sizeof(char)*9);
                v[0] = '\0';
                sprintf (v, "%d",td->num_tasks_total);
                n = (size_t) (strlen(v)+1);
                byte_array_init(ba,n);
                byte_array_pack8v(ba,v,n);

                COMM_send_message(ba,MSG_GET_NUM_TASKS,origin_socket); 
                free(v);
                break;
            case MSG_REFRESH:
                ptr = COMM_ip_list->list_pointer;

                while(ptr!=NULL) {
                    if(ptr->type == (int)VM_RESTORED) {
                        break;
                    }
                    ptr = ptr->next;
                } 

                if(ptr == NULL) {
                    error("MSG_REFRESH was received but couldn't find restored VM in ip list.");
                }
                else {
                    COMM_add_client_socket(ptr->socket);
                }
                
            default:
                break;
        }

        if(JM_KEEP_JOURNAL > 0) {
            gettimeofday(&entry->end, NULL);
        }
 
        // If computation is over, closes all sockets and exits. 
        if (td->is_finished==1){
            info("Sending kill to committer");
            COMM_connect_to_committer(NULL);
            COMM_send_message(NULL, MSG_KILL, socket_committer);

            //info("Fetching final result");
            //COMM_read_message(final_result, &type, socket_committer);
            //COMM_disconnect_from_committer();
            COMM_close_all_connections(); 
            
            break;
        }
    }

    // Send kill to all extra threads.
    for (i = 0; i < JM_SEND_THREADS; i++) {    
        append_request(td, MSG_KILL, 0);
    }

    // If exist, quit the gen_thread.
    for (i = 0; i < JM_GEN_THREADS; i++) {    
        td->gen_kill = 1; 
        sem_post(&td->gen_request);         // Release jm_gen_worker 
    }

    // Free memory allocated in byte arrays.
    byte_array_free(ba);
    byte_array_free(ba_binary);
    byte_array_free(ba_hash);
    free(ba);
    free(ba_binary);
    free(ba_hash);

    // And in user_data
    free(td->user_data);

    info("Terminating job manager");
}
