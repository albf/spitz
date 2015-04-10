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

#ifndef COMM_H
#define	COMM_H

#include "barray.h"
#include "list.h"               // List of ips connected.
#include "spitz.h"

// Enums of actor and message type
enum actor {
    JOB_MANAGER  = 0,
    COMMITTER    = 1,
    TASK_MANAGER = 2,
    MONITOR = 3,
    VM_TASK_MANAGER = 4
};

// Enums of possible messages. Keep this equal to the python comm.py header. 
enum message_type {
    MSG_READY                   = 0,    // Task Manager to Job Manager
    MSG_TASK                    = 1,    // A task, Job Manager to Task Manager.
    MSG_RESULT                  = 2,    // A result, Task Manager to Committer.	
    MSG_KILL 	                = 3,    // Kill message, when there is nothing to send.
    MSG_DONE                    = 4,    // Indicates some task has finished.
    MSG_GET_COMMITTER   	= 5,    // Task Manager to Job Manager 
    MSG_GET_PATH                = 6,    // Task Manager to Job Manager
    MSG_GET_RUNNUM              = 7,    // Task Manager/Committer to Job Manager
    MSG_GET_ALIVE               = 8,    // Task Manager to JobManager
    MSG_SET_COMMITTER           = 9,    // Committer to Job Manager and Committer to VM Task Manager
    MSG_NEW_CONNECTION          = 10,   // Task Manager/Committer to Job Manager/Commiter
    MSG_CLOSE_CONNECTION        = 11,   // Task Manager/Committer to Job Manager/Commiter
    MSG_STRING                  = 12,   // Send a string, char *, from one node to another.
    MSG_EMPTY                   = 13,   // When nothing was received, used for error proposes.
    MSG_GET_BINARY              = 14,   // Used to request and get the binary
    MSG_GET_HASH                = 15,   // Used to request the md5 hash of the binary.
    MSG_GET_STATUS              = 16,   // Used to request status of all connected nodes.
    MSG_GET_NUM_TASKS           = 17,   // Used to request the number of tasks registered for in the JM.
    MSG_SET_MONITOR             = 18,   // Monitor to JobManager
    MSG_SET_JOB_MANAGER         = 19,   // Job Manager to VM Task Manager
    MSG_SET_TASK_MANAGER_ID     = 20,   // Task Manager to Job Manager, to restore ID. 
    MSG_NEW_VM_TASK_MANAGER     = 21,   // Monitor to JobManager, JobManager to Committer.
    MSG_SEND_VM_TO_COMMITTER    = 22,   // VM request to JobManager help to connect to Committer.
    MSG_NO_TASK                 = 23    // JM sends to TM when there is no task to send but computation is not over..
};

enum status_type {
    STATUS_GENERAL              = 1     // Get current state from all nodes (connected or not). 
};

/* Functions based in Client/Server architecture */

// Client functions.
int COMM_connect_to_committer(int * retries);
int COMM_connect_to_committer_local(int * retries);
int COMM_connect_to_job_manager(char ip_adr[], int * retries);
int COMM_connect_to_job_manager_local(char ip_adr[], int * retries);
int COMM_connect_to_vm_task_manager(int * retries, struct byte_array * ba);
int COMM_vm_connection(int waitJM, char waitCM);
int COMM_setup_vm_network();

// Both, check by id what's the case.
int COMM_get_alive();
void COMM_close_all_connections();
enum actor COMM_get_actor_type();
int COMM_get_rank_id();
int COMM_get_run_num();
void COMM_set_actor_type(char * value);

// 1. Server functions : connection oriented.
void COMM_close_connection(int sock);
void COMM_create_new_connection();
int COMM_setup_job_manager_network();
int COMM_setup_committer_network();
int COMM_wait_request(enum message_type * type, int * origin_socket, struct byte_array * ba, struct timeval * tv);

// 2. Server functions : variable oriented.
void COMM_increment_run_num();
int COMM_register_committer(int sock);
int COMM_register_monitor(int sock);
void COMM_send_committer(int sock);

// Debug
void COMM_LIST_print_ip_list();

// General Propose (sending and receiving data from sockets)
int COMM_read_message(struct byte_array *ba, enum message_type *type, int rcv_socket);
int COMM_send_message(struct byte_array *ba, int type, int dest_socket);

// Extern Variables
extern int socket_manager, socket_committer;                // socket of servers (job manager and committer)
extern int COMM_alive;
extern char * COMM_addr_manager;
extern int COMM_my_rank;
extern struct LIST_data * COMM_ip_list;
extern enum actor type;

// Debug
extern int COMM_client_socket[max_clients];    // used to stock the sockets.

#endif	/* COMM_H */
