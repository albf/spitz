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
#include "comm.h"
#include "log.h"
#include "barray.h"
#include "spitz.h"

typedef void * (*spitz_ctor_t) (int, char **);
typedef int    (*spitz_tgen_t) (void *, struct byte_array *);

struct task {
    size_t id;
    struct byte_array data;
    struct task *next;
};

// Push a task into the thread FIFO.
void push_task(struct jm_thread_data * td, struct byte_array * ba, enum message_type type) {
    struct request_elem * new_elem = (struct request_elem *) malloc (sizeof(struct request_elem));
    struct request_FIFO * FIFO = td->request_list;
   
    // Allocate new element.
    new_elem->ba = ba;
    new_elem->type = type;
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
struct request_elem * pop_task(struct jm_thread_data * td) {
    struct request_elem * ret;
    
    // Wait for new functions;
    sem_wait(&td->num_requests);
    pthread_mutex_lock(&td->lock);
  
    // Get task and remove the first one.
    ret = td->request_list->first;
    td->request_list->first = td->request_list->first->next;
    
    // Let the FIFO go.
    pthread_mutex_unlock(&td->lock);
    return ret;
}

// Function responsible for the behavior of the job manager.
void job_manager(int argc, char *argv[], char *so, struct byte_array *final_result)
{
    int is_finished=0, rank=(int)TASK_MANAGER;                      // Indicates if the work is finished.
    enum message_type type;                                         // Type of received message.
    uint64_t socket_cl;                                             // Closing socket.
    int origin_socket;                                              // Socket that sent the request.
    int run_num;                                                    // Actual run number.
    char * v;                                                       // Used as auxiliary. 
    ssize_t n;                                                      // Used as auxiliary.
    int retries;                                                    // Auxiliary to establish connection with VM Task Manager.
    int aux; uint64_t aux64;                                        // Auxiliary, used to cast variables. 
    int64_t bufferr;                                                // buffer used for received int64_t values 

    // VM restore management.
    int counter=0;                                                  // Counts requests to restore VMs Task Managers.
    int is_there_any_vm=0;                                          // Indicate if there is, at least, one VM connection.
    int total_restores=0;                                           // Amount of nodes restored.
    
    struct task *clean;                                             // Auxiliary pointer used to free memory. 
    struct task *iter, *prev;                                       // Pointers to iterate through FIFO. 
    struct task *home = NULL, *mark = NULL, *head = NULL;           // Pointer to represent the FIFO.
    struct task *node;                                              // Pointer of new task.
    
    void *ptr = dlopen(so, RTLD_LAZY);                              // Open the binary file.
    if (!ptr) {
        error("Could not open %s", so);
        return;
    }
    
    spitz_ctor_t ctor = dlsym(ptr, "spits_job_manager_new");        // Loads the user functions.
    spitz_tgen_t tgen = dlsym(ptr, "spits_job_manager_next_task");

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
    
    void *user_data = ctor((argc), (argv));

    // Information about the completed task.
    int tid, task_id = 0;
    int tm_id;

    // Used for id recovery.
    int rank_rcv;
    int rank_original;

    while (1) {
        COMM_wait_request(&type, &origin_socket, ba); 
        
        switch (type) {
            case MSG_READY:
                byte_array_clear(ba);
                byte_array_pack64(ba, task_id);
                if (tgen(user_data, ba)) {
                    node = malloc(sizeof(*node));
                    node->id = task_id;
                    byte_array_init(&node->data, ba->len);
                    byte_array_pack8v(&node->data, ba->ptr, ba->len);
                    rank = (LIST_search_socket(COMM_ip_list, origin_socket))->id; 
                    debug("Sending generated task %d to %d", task_id, rank);
                    LIST_update_tasks_info (COMM_ip_list,NULL,-1, rank, 1, 0);
                    
                    // node has a new task
                    if(home == NULL) {
                        home = node;
                        head = node;
                        mark = node;
                    }
                    
                    else {
                    // update the head
                        head->next = node;
                        head = node;
                    }
                    
                    // points to nothing
                    node->next = NULL;
                    
                    COMM_send_message(ba, MSG_TASK, origin_socket);
                    task_id++;
                } else if (mark != NULL) {
                    COMM_send_message(&mark->data, MSG_TASK, origin_socket);
                    debug("Replicating task %d", mark->id);
                    mark = mark ->next;
                    if (!mark) {
                        mark = home;
                    }
                } 
                // will enter here if ended at least once
                else {        
                    debug("Sending KILL to rank %d", rank);
                    COMM_send_message(ba, MSG_KILL, origin_socket);
                    is_finished=1;
                }
                break;
            case MSG_DONE:;
                byte_array_unpack64(ba, &bufferr);
                tid = (int)bufferr;
                byte_array_unpack64(ba, &bufferr);
                tm_id = (int)bufferr;
                
                iter = home;
                prev = NULL;

                // search for the task that finished
                while (iter->id != tid) {
                    prev = iter;
                    iter = iter->next;
                }

                // if there is a previous in the list. 
                if (prev) {
                    clean = iter;
                    prev->next = iter->next;
                }

                // if not, it's the home.
                else {
                    clean = home;
                    home = home->next;
                }

                // if it's the head, pick the previous one
                if(iter == head) {
                    head = prev;
                }
                
                free(clean);
                debug("TASK %d is complete by %d!", tid, tm_id);
                LIST_update_tasks_info (COMM_ip_list,NULL,-1, tm_id, 0, 1);
                byte_array_free(&iter->data);
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
                    is_finished=1;
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
                info("Message received incomplete or a problem occurred.");
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
                break;
            case MSG_GET_NUM_TASKS:
                aux = atoi(argv[0]);
                aux64 = (uint64_t) aux;
                byte_array_clear(ba);
                byte_array_pack64(ba, aux64);
                COMM_send_message(ba,MSG_GET_NUM_TASKS,origin_socket); 
                break;
            default:
                break;
        }
        
        // If computation is over, closes all sockets and exits. 
        if (is_finished==1){
            info("Sending kill to committer");
            COMM_connect_to_committer(NULL);
            COMM_send_message(ba, MSG_KILL, socket_committer);

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

    // Free memory allocated in byte arrays.
    byte_array_free(ba);
    byte_array_free(ba_binary);
    byte_array_free(ba_hash);
    free(ba);
    free(ba_binary);
    free(ba_hash);

    // And in user_data
    free(user_data);

    info("Terminating job manager");
}