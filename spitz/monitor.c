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

void monitor(int argc, char *argv[])
{
    int is_finished=0;                                              // Indicate if it's finished.
    int command;                                                    // Command code red by stdin.
    enum message_type type;                                         // Type of received message.
    int comm_return=0;                                              // Return values from send and read.
   
    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc(sizeof(struct byte_array));
    byte_array_init(ba, 100);

    COMM_send_message(NULL, MSG_SET_MONITOR, socket_manager);       // set as a committer with manager
    debug("Set monitor successfully.");

    info("Starting monitor main loop.");
    while (1) {
        scanf("%d", &command);
        error("Read Command: %d", command);

        if(command == 0) {
            printf("[STATUS #00] Finishing monitor.\n");                
            break;
        }
        
        if(command == 1) {
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
    }

    byte_array_free(ba);
    free(ba);
    info("Terminating Monitor.");
}