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
 
    // Binary Array used to store the binary, the .so. 
    struct byte_array * ba_hash = (struct byte_array *) malloc (sizeof(struct byte_array));
    byte_array_init(ba_hash, 0);
    byte_array_compute_hash(ba_hash, ba_binary);
    
    void *user_data = ctor((argc), (argv));
    size_t tid, task_id = 0;

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
                    debug("Sending generated task %d to %d", task_id, rank);
                    
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
            case MSG_DONE:
                byte_array_unpack64(ba, &tid);
                iter = home;
                prev = NULL;

                // search for the task that finished
                while (iter->id != tid) {
                    prev = iter;
                    iter = iter->next;
                }

                // if there is a previous in the list. 
                if (prev) {
                    prev->next = iter->next;
                }

                // if not, it's the home.
                else {
                    home = home->next;
                }

                // if it's the head, pick the previous one
                if(iter == head) {
                    head = prev;
                }
                
                debug("TASK %d is complete!", tid);
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
            case MSG_EMPTY:
                info("Message received incomplete or a problem occurred.");
                break;
            default:
                break;
        }
        
        // If there is only the JobManager and the Committer
        if (is_finished==1){
            info("Sending kill to committer");
            COMM_connect_to_committer(NULL);
            COMM_send_message(ba, MSG_KILL, socket_committer);

            info("Fetching final result");
            COMM_read_message(final_result, &type, socket_committer);
            //COMM_disconnect_from_committer();
            COMM_close_all_connections(); 
            
            break;
        }
    }

    // Free memory allocated in byte arrays.
    byte_array_free(ba);
    byte_array_free(ba_binary);
    byte_array_free(ba_hash);
    free(ba);
    free(ba_binary);
    free(ba_hash);
    info("Terminating job manager");
}