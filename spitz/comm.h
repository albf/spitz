/*
 * Copyright 2014 Alexandre Luiz Brisighello Filho <albf.unicamp@gmail.com>
 */

#ifndef COMM_H
#define	COMM_H

#define PORT_MANAGER 8896
#define PORT_COMMITTER 10004
#define max_clients 30
#define max_pending_connections 3

#include "barray.h"
#include "list.h"               // List of ips connected.

// Enums of actor and message type
enum actor {
    JOB_MANAGER  = 0,
    COMMITTER    = 1,
    TASK_MANAGER = 2,
    MONITOR = 3 
};

// Enuns the types of possible messages.
enum message_type {
    MSG_READY,              // Task Manager to Job Manager
    MSG_TASK,               // A task, Job Manager to Task Manager.
    MSG_RESULT,             // A result, Task Manager to Committer.	
    MSG_KILL,	            // Kill message, when there is nothing to send.
    MSG_DONE,
    MSG_GET_COMMITTER,	    // Task Manager to Job Manager 
    MSG_GET_PATH,	    // Task Manager to Job Manager
    MSG_GET_RUNNUM,         // Task Manager/Committer to Job Manager
    MSG_GET_ALIVE,          // Task Manager to JobManager
    MSG_SET_COMMITTER,	    // Committer to Job Manager
    MSG_NEW_CONNECTION,     // Task Manager/Committer to Job Manager/Commiter
    MSG_CLOSE_CONNECTION,   // Task Manager/Committer to Job Manager/Commiter
    MSG_STRING,             // Send a string, char *, from one node to another.
    MSG_EMPTY,              // When nothing was received, used for error proposes.
    MSG_GET_BINARY,         // Used to request and get the binary
    MSG_GET_HASH,           // Used to request the md5 hash of the binary.
    MSG_GET_STATUS          // Used to request status of all connected nodes.
};

/* Functions based in Client/Server architecture */

// Client functions.
int COMM_connect_to_committer(int * retries);
int COMM_connect_to_job_manager(char ip_adr[], int * retries);

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
int COMM_wait_request(enum message_type * type, int * origin_socket,struct byte_array * ba);

// 2. Server functions : variable oriented.
void COMM_increment_run_num();
int COMM_register_committer(int sock);
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

#endif	/* COMM_H */
