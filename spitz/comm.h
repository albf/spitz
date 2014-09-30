/**
Alexandre L. B. F.      
*/

#ifndef COMM_H
#define	COMM_H

#include <stdio.h>
#include <string.h>   
#include <stdlib.h>
#include <unistd.h>   
#include <arpa/inet.h>    
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>           // Wait some seconds beetween requests.
#include "list.h"               // List of ips connected.
#include <barray.h>
#include <errno.h>
#include <fcntl.h>              // Used to set block/non-block sockets.
#include "barray.h"
#include "log.h"

#define PORT_MANAGER 8888
#define PORT_COMMITTER 9998
#define max_clients 30
#define max_pending_connections 3

// Enums of actor and message type
enum actor {
	JOB_MANAGER = 0,
	COMMITTER   = 1,
        TASK_MANAGER = 2,
};

// Enuns the types of possible messages.
enum message_type {
	MSG_READY,              // Task Manager to Job Manager
	MSG_TASK,
	MSG_RESULT,	
	MSG_KILL,	
	MSG_DONE,	
	MSG_GET_COMMITTER,	// Task Manager to Job Manager 
	MSG_GET_PATH,		// Task Manager to Job Manager
	MSG_GET_RUNNUM,		// Task Manager/Committer to Job Manager
        MSG_GET_ALIVE,          // Task Manager to JobManager
	MSG_SET_COMMITTER,	// Committer to Job Manager
        MSG_NEW_CONNECTION,     // Task Manager/Committer to Job Manager/Commiter
        MSG_CLOSE_CONNECTION,   // Task Manager/Committer to Job Manager/Commiter
        MSG_STRING,
        MSG_EMPTY
};

/* Functions based in Client/Server architecture */

// Client functions.
int COMM_connect_to_committer(int * retries);
int COMM_connect_to_job_manager(char ip_adr[], int * retries);

// Both, check by id what's the case.
int COMM_get_alive();
void COMM_close_all_connections();
int COMM_get_rank_id();
int COMM_get_run_num();

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

#endif	/* COMM_H */