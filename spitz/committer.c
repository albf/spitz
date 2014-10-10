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


void committer(int argc, char *argv[], void *handle)
{
    int is_finished=0;                                              // Indicate if it's finished.
    int origin_socket=0;                                            // Socket of the requester, return by COMM_wait_request.
    enum message_type type;                                         // Type of received message.
    uint64_t socket_cl;                                             // Closing socket, packed by the COMM_wait_request.
   
    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc(sizeof(struct byte_array));
    byte_array_init(ba, 100);

    // Changed: Indexing by task id, 0 for not committed and 1 for already committed.
    // Once a new task arrive, if it's more them actual cap, realloc using it's id*2
    size_t cap = 10;                                                // Initial capacity.
    size_t i, old_cap;                                              // Auxiliary to capacity
    size_t *committed = malloc(sizeof(size_t) * cap);               // List indexed by the task id. 
                                                                    // 1 for committed, 0 for not yet.
    size_t task_id;                                                 // Task id of received result.
    
    // Loads the user functions.
    void * (*setup) (int, char **);
    void (*commit_pit) (void *, struct byte_array *);
    void (*commit_job) (void *, struct byte_array *);

    *(void **)(&setup)      = dlsym(handle, "spits_setup_commit");  // Loads the user functions.
    *(void **)(&commit_pit) = dlsym(handle, "spits_commit_pit");
    *(void **)(&commit_job) = dlsym(handle, "spits_commit_job");
    //setup_free = dlsym("spits_setup_free_commit");
    void *user_data = setup(argc, argv);

    for(i=0; i<cap; i++) {                                          // initialize the task array
        committed[i]=0;
    }
        
    info("Starting committer main loop");
    while (1) {
        COMM_wait_request(&type, &origin_socket, ba);
        
        switch (type) {
            case MSG_RESULT:
                byte_array_unpack64(ba, &task_id);
                debug("Got a RESULT message for task %d", task_id);
                
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
                    commit_pit(user_data, ba);
                    byte_array_clear(ba);
                    byte_array_pack64(ba, task_id);
                    info("Sending task %d to Job Manager.", task_id);
                    COMM_send_message(ba, MSG_DONE, socket_manager);
                }

                break;
            case MSG_KILL:                                          // Received kill from Job Manager.
                info("Got a KILL message, committing job");
                byte_array_clear(ba);
                if (commit_job) {
                    commit_job(user_data, ba);
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

    free(user_data);
    free(committed);
    free(ba);
    info("Terminating committer");
    byte_array_free(ba);
}