/*
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

#include "monitor.h"

#include "comm.h"
#include "barray.h"
#include "log.h"
#include "spitz.h"
#include <string.h>   

void monitor(int argc, char *argv[])
{
    int is_finished=0;                                              // Indicate if it's finished.
    int command;                                                    // Command code red by stdin.
    enum message_type type;                                         // Type of received message.
    int comm_return=0;                                              // Return values from send and read.
    int retries;                                                    // Number of retries used to send a VM.

    // Used to parse and send a ip from a vm task manager.
    size_t n;
    char * v; 
    char vm_send[26] = "";
    
    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc(sizeof(struct byte_array));
    byte_array_init(ba, 100);

    COMM_send_message(NULL, MSG_SET_MONITOR, socket_manager);       // set as a committer with manager
    debug("Set monitor successfully.");

    info("Starting monitor main loop.");
    while (1) {
        scanf("%d", &command);
        error("Read Command: %d", command);
        // Quit Command
        if(command == 0) {
            printf("[STATUS #00] Finishing monitor.\n");                
            break;
        }
        // Get status command. 
        else if(command == 1) {
            type = MSG_EMPTY;
            while (type == MSG_EMPTY) {
                comm_return = COMM_send_message(NULL, MSG_GET_STATUS, socket_manager);
                if(comm_return < 0) {
                    error("Problem found to send message to Job Manager");
                    type = MSG_EMPTY;
                }

                else {
                    comm_return = COMM_read_message(ba, &type, socket_manager);
                    if(comm_return < 0) {
                        error("Problem found to read message from Job Manager");
                        type = MSG_EMPTY;
                    } 
                }
            }
            printf("[STATUS #01] %s \n", ba->ptr);                
        }
        // Connect to VM Task Manager command.
        else if(command == 2) {
            // format : ip|port\n
            error("inside command 2");
            scanf("%s", vm_send);
            error("VM IP: %s", vm_send);
            
            v = vm_send;
            n = (size_t) strlen(vm_send);
            
            byte_array_init(ba, n);
            byte_array_pack8v(ba, v, n);
            
            type = MSG_EMPTY;
            retries = 3;
            while (type == MSG_EMPTY) {
                comm_return = COMM_send_message(ba, MSG_NEW_VM_TASK_MANAGER, socket_manager);
                if(comm_return < 0) {
                    error("Problem found to send message to Job Manager");
                    type = MSG_EMPTY;
                    retries = retries - 1;
                    if(retries == 0) {
                        break;
                        printf("[STATUS #02] Failed \n");                
                    }
                }

                else {
                    error("VM IP and PORT sent successfully to Job Manager");
                    break;
                }
            }
            printf("[STATUS #02] Success \n");                
        }
    }

    byte_array_free(ba);
    free(ba);
    info("Terminating Monitor.");
}