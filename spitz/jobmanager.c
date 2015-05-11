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
#include "comm.h"
#include "log.h"
#include "barray.h"
#include "spitz.h"


// Adds an registry of task being sent to an Task Manager.
void add_registry(struct jm_thread_data *td, size_t task_id, int tm_id) {
    int changed = 0;
    int initial_size, i;
    struct task_registry * ptr;
    struct task_registry * next;

    pthread_mutex_lock(&td->registry_lock);

    initial_size = td->registry_capacity;
    while(task_id >= (td->registry_capacity)) {
        changed = 1;
        td->registry_capacity = td->registry_capacity*2;
    }
    
    if(changed > 0) {
        td->registry = (struct task_registry **) realloc(td->registry, td->registry_capacity*sizeof(struct task_registry *));
        for(i=initial_size; i < td->registry_capacity; i++) {
            td->registry[i] = NULL;
        }
    }

    ptr = (struct task_registry *) malloc (sizeof(struct task_registry)); 
    
    ptr->tm_id = tm_id; 
    ptr->task_id = task_id;

    // Save time information 
    ptr->send_time = (struct timeval *) malloc(sizeof(struct timeval));
    gettimeofday(&ptr->send_time);
    ptr->completed_time = NULL;

    if(td->registry[task_id] == NULL) {
        td->registry[task_id] = ptr;
    }
    else {
        next = td->registry[task_id];
        while(next->next != NULL) {
            next = next->next;
        }
        next->next = ptr;
    }

    pthread_mutex_unlock(&td->registry_lock);
}

// Indicate if task_id was already sent to tm_id. 1 if yes, 0 if no.
int check_registry(struct jm_thread_data * td, size_t task_id, int tm_id) {
    struct task_registry * ptr;
    pthread_mutex_lock(&td->registry_lock);

    if(task_id >= (td->registry_capacity)) {
        error("Registry check of unregistered task (not allocated space) : %d", task_id);
        pthread_mutex_unlock(&td->registry_lock);
        return 0;
    }
    ptr = td->registry[task_id];
    if(ptr == NULL) {
        error("Registry check of unregistered task (NULL pointer) : %d", task_id);
        pthread_mutex_unlock(&td->registry_lock);
        return 0;
    }

    for(; ((ptr!=NULL)&&(ptr->tm_id != tm_id)); ptr=ptr->next);
    if(ptr == NULL) {
        pthread_mutex_unlock(&td->registry_lock);
        return 0;
    } 
    else {
        pthread_mutex_unlock(&td->registry_lock);
        return 1;
    } 
}

void add_completion_registry (struct jm_thread_data *td, size_t task_id, int tm_id) {
    struct task_registry * ptr;
    pthread_mutex_lock(&td->registry_lock);
    ptr = td->registry[task_id];
    while((ptr != NULL)&&(ptr->tm_id != tm_id)) {
        ptr = ptr->next;
    }

    if(ptr == NULL) {
        error("Error in registry update. Couldn't found Task Manager %d in Registry[%d]", tm_id, (int) task_id);
        pthread_mutex_unlock(&td->registry_lock);
        return;
    }

    ptr->completed_time = (struct timeval *) malloc(sizeof(struct timeval));
    gettimeofday(&ptr->completed_time);
    pthread_mutex_unlock(&td->registry_lock);
}

// Push a task into the thread FIFO.
void append_request(struct jm_thread_data * td, struct byte_array * ba, enum message_type type, int socket) {
    struct request_elem * new_elem = (struct request_elem *) malloc (sizeof(struct request_elem));
    struct request_FIFO * FIFO = td->request_list;
   
    // Allocate new element.
    new_elem->ba = ba;
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
void * add_task(struct jm_thread_data *td, struct task *node) {
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
        if (td->task_counter >= td->num_tasks_total) {
            td->is_finished = 1;
        }
        ret = NULL;
    }

    else {
        if(KEEP_REGISTRY > 0) {
            // Get the next new one for the current node and updates list. 
            ret = td->tasks->home;
            aux = NULL;
            while((ret != NULL)&&(check_registry(td, ret->id, rank) == 1)) {
                aux = ret;
                ret = ret->next;
            }

            if(ret != NULL) {
                if(aux != NULL) {
                    aux->next = ret->next;
                    td->tasks->head->next = ret;
                    td->tasks->head = ret;
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
            clean = td->tasks->home; 
            td->tasks->home = td->tasks->home->next;
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

    // if all tasks were generated or computation is over, just stops.
    if((td->task_counter >= td->num_tasks_total)||(td->all_generated > 0)) {
        ret = -1;
    }
    // if there are still tasks to generate. 
    else {
        ret = td->task_counter;
        td->task_counter++;
    }
    
    pthread_mutex_unlock(&td->tc_lock);
    
    return ret;
}

// Function responsible for the generating worker.
void * jm_gen_worker(void * ptr) {
    struct jm_thread_data * td = ptr;
    spitz_tgen_t tgen = dlsym(td ->handle, "spits_job_manager_next_task");
    int tid;

    while(1) {
        // Wait for requests.
        pthread_mutex_lock(&td->jm_gen_lock);

        // Check if gen_ba is null.
        if(td->gen_ba == NULL) {
            break;
        }

        // Try to generate task.
        tid = * (td->gen_tid); 
        debug("[jm_gen_worker] Received task %d to generate.", tid);
        
        if(!(tgen(td->user_data, td->gen_ba))) {
            * (td->gen_tid) = -1;
            td->all_generated = 1;
        }
        
        // Release waiting task.
        pthread_mutex_unlock(&td->gen_ready_lock);
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
    int rank;

    int tid;
    int task_generated;

    if(GEN_PARALLEL != 0) {
        tgen = dlsym(td ->handle, "spits_job_manager_next_task");
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
            task_generated = 0;

            client = LIST_search_socket(COMM_ip_list, my_request->socket);

            if(client == NULL) {
                error("Couldn't find client with socket provided by main thread. Ignoring request.");
            } 

            else {
                rank = client->id;
                tid = next_task_num(td);
                byte_array_clear(my_request->ba); 

                debug("Trying to generate task : %d for TM %d", tid, rank);

                if (tid >= 0) {
                    // Generate task, if possible in parallel. 
                    if(GEN_PARALLEL > 0) {
                        byte_array_pack64(my_request->ba, tid);

                        // Update current_gen value
                        pthread_mutex_lock(&td->current_gen_lock);
                        td->current_gen += 1;
                        pthread_mutex_unlock(&td->current_gen_lock);
                        
                        // try to generate task.
                        if(tgen(td->user_data, my_request->ba)) {
                            task_generated = 1;
                        }
                        else {
                            td->all_generated = 1;
                        }

                        // Update current_gen value
                        pthread_mutex_lock(&td->current_gen_lock);
                        td->current_gen -= 1;
                        pthread_mutex_unlock(&td->current_gen_lock);
                    }

                    // Can't generate in parallel.
                    else {
                        byte_array_pack64(my_request->ba, tid);
                        
                        // Lock task generation region.
                        pthread_mutex_lock(&td->gen_region_lock);

                        // Pack task info to be generated.
                        td->gen_tid = &tid;
                        td->gen_ba = my_request->ba;

                        // Release jm_gen_worker/
                        pthread_mutex_unlock(&td->jm_gen_lock);

                        // Wait for it to be ready.
                        pthread_mutex_lock(&td->gen_ready_lock);

                        pthread_mutex_unlock(&td->gen_region_lock);

                        if(tid >= 0 ) {
                            task_generated = 1;
                        }
                    }
                }

                // If task was generated, add it to queue and send it.
                if(task_generated == 1) {
                    node = (struct task *) malloc (sizeof(struct task));
                    node->id = tid;
                    node->data = my_request->ba;
                    add_task(td, node);
                    
                    debug("Sending generated task %d to %d", tid, rank);
                    LIST_update_tasks_info (COMM_ip_list, NULL, -1, rank, 1, 0);
                    if(KEEP_REGISTRY>0) {
                        add_registry(td, tid,rank);
                    }
                    COMM_send_message(my_request->ba, MSG_TASK, my_request->socket);
                }
                else {
                    // Can't generate, assume this is the end.
                    pthread_mutex_lock(&td->tc_lock);
                    td->num_tasks_total = td->task_counter;
                    pthread_mutex_unlock(&td->tc_lock);

                    // If couldn't generate, try to send a repeated one.
                    while(task_generated == 0) {
                        node = next_task(td, rank);
                        if (node == NULL) {
                            // Couldn't find a task because computation is finished.
                            // Logic: If it's finished or there is no one generating anything, why couldn't I find anything to send?
                            if((td->is_finished > 0)||(GEN_PARALLEL == 0)||(td->current_gen == 0)) {
                                debug("Sending kill message to rank %d and killing worker, nothing to be done",rank);
                                COMM_send_message(NULL, MSG_KILL, my_request->socket);
                                break;
                            }
                            // Couldn't find a task because the computation isn't finished. Wait?
                            else {
                                sleep(1);
                            }
                        }
                        // Found a task, will replicate to another node.
                        else {
                            debug("Replicating task %d to rank %d",node->id,rank);
                            LIST_update_tasks_info (COMM_ip_list, NULL, -1, rank, 1, 0);
                            if(KEEP_REGISTRY>0) {
                                add_registry(td, tid,rank);
                            }
                            COMM_send_message(node->data, MSG_TASK, my_request->socket);
                            break;
                        }
                    }
                }

            }
        }

        else {
            error("Unexpected type of message for jobmanager worker.");
        }
    
        free(my_request);
    }

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
    uint64_t aux64;                                                 // Auxiliary, used to cast variables. 
    int64_t bufferr;                                                // buffer used for received int64_t values 
    struct timeval tv;                                              // Timeout for select.
    int i;                                                          // Iterator

    // VM restore management.
    int counter=0;                                                  // Counts requests to restore VMs Task Managers.
    int is_there_any_vm=0;                                          // Indicate if there is, at least, one VM connection.
    int total_restores=0;                                           // Amount of nodes restored.
    
    //struct task *home = NULL, *mark = NULL, *head = NULL;           // Pointer to represent the FIFO.
    
    void * ptr = td->handle;                                        // Open the binary file.
    
    //spitz_ctor_t ctor = dlsym(ptr, "spits_job_manager_new");        // Loads the user functions.
    spitz_tgen_t tgen = dlsym(ptr, "spits_job_manager_next_task");

    // Data structure to exchange message between processes. 
    struct byte_array * ba; 

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
    
    //void *user_data = ctor((argc), (argv));

    // Information about the completed task.
    int tid; 
    int tm_id;

    // Used for id recovery.
    int rank_rcv;
    int rank_original;

    while (1) {
        ba = (struct byte_array *) malloc (sizeof(struct byte_array));
        byte_array_init(ba, 10);

        // Set timeout values for wait_request.
        tv.tv_sec = WAIT_REQUEST_TIMEOUT_SEC;
        tv.tv_usec = WAIT_REQUEST_TIMEOUT_USEC;
        COMM_wait_request(&type, &origin_socket, ba, &tv); 
        
        switch (type) {
            case MSG_READY:
		if(td->is_done_loading == 1) {
			byte_array_clear(ba);
			append_request(td, ba, type, origin_socket);
		}
		else {
			debug("Received task request from %d but shared data is still being load.", LIST_search_socket(COMM_ip_list, origin_socket));
			COMM_send_message(NULL, MSG_NO_TASK, origin_socket);
		}
               
                break;
            case MSG_DONE:;
                byte_array_unpack64(ba, &bufferr);
                tid = (int)bufferr;
                byte_array_unpack64(ba, &bufferr);
                tm_id = (int)bufferr;
                debug("TASK %d was completed by %d!", tid, tm_id);

                //if(KEEP_REGISTRY > 0) {
		//	add_completion_registry (td, tid, tm_id);	
                //}
                
                remove_task(td, tid);
                LIST_update_tasks_info (COMM_ip_list,NULL,-1, tm_id, 0, 1);
                break;
            case MSG_NEW_CONNECTION:
                COMM_create_new_connection();
                break;
            case MSG_CLOSE_CONNECTION:
                _byte_array_unpack64(ba, &socket_cl);
                COMM_close_connection((int)socket_cl);
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
                byte_array_unpack64(ba, &bufferr);
                rank_rcv = (int)bufferr;
                byte_array_unpack64(ba, &bufferr);
                rank_original = (int)bufferr;
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
                v = LIST_get_monitor_info(COMM_ip_list);
                n = (size_t) (strlen(v)+1); 
                byte_array_init(ba, n);
                byte_array_pack8v(ba, v, n);
                COMM_send_message(ba, MSG_STRING, origin_socket);
                free(v);
                break;
            case MSG_EMPTY:
                info("Nothing received: Timeout or problem in wait_request().");
                break;
            case MSG_NEW_VM_TASK_MANAGER:
                if(is_there_any_vm == 0) {
                    is_there_any_vm = 1;
                }

                retries = 3;
                info("Received information about VM task manager waiting connection.");
                //COMM_send_message(ba, MSG_NEW_VM_TASK_MANAGER, socket_committer);
                COMM_connect_to_vm_task_manager(&retries, ba);
                break;
            case MSG_SEND_VM_TO_COMMITTER:
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
            default:
                break;
        }

        if(type != MSG_READY) {
            free(ba);
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
        else {
            if (is_there_any_vm == 1){
                counter++;
                if(counter>=RESTORE_RATE) {
                    total_restores = check_VM_nodes(COMM_ip_list); 
                    counter=0;
                }
            }
        }
    }

    // Send kill to all extra threads.
    for (i = 0; i < JM_EXTRA_THREADS; i++) {    
        append_request(td, NULL, MSG_KILL, 0);
    }

    // If exist, quit the gen_thread.
    if(GEN_PARALLEL == 0 ) {
        pthread_mutex_lock(&td->gen_region_lock);       // Lock task generation region.

        td->gen_ba = NULL; 
        pthread_mutex_unlock(&td->jm_gen_lock);         // Release jm_gen_worker 

        pthread_mutex_unlock(&td->gen_region_lock);
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
