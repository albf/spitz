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

void committer(int argc, char *argv[])
{
    int is_finished=0;                                              // Indicate if it's finished.
    enum message_type type;                                         // Type of received message.
   
    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc(sizeof(struct byte_array));
    byte_array_init(ba, 100);

    info("Starting monitor main loop.");
    while (1) {
        switch (type) {
            case MSG_RESULT:
                break;
            case MSG_KILL:
            default:
                break;
        }

        // If he is the only member alive and the work is finished.
        if (is_finished==1){
            info("Work is done, time to die.");
            COMM_close_all_connections(); 
            break;
        }
    }

    byte_array_free(ba);
    free(ba);
    info("Terminating Monitor.");
}
