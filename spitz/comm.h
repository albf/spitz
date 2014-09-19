/* 
 * File:   comm.h
 * Author: Alexandre
 *
 * Created on August 20, 2014, 9:39 PM
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
#include <sys/time.h>
#include "list.h"
#include <barray.h>
#include <errno.h>
#include <fcntl.h>

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

enum message_type {
	MSG_READY,
	MSG_TASK,
	MSG_RESULT,	
	MSG_KILL,	
	MSG_DONE,	
	MSG_GET_COMMITTER,	// Worker to Job Manager 
	MSG_GET_PATH,		// Worker to Job Manager
	MSG_GET_RUNNUM,		// Worker/Committer to Job Manager
        MSG_GET_ALIVE,          // Worker to JobManager
	MSG_SET_COMMITTER,	// Committer to Job Manager
        MSG_NEW_CONNECTION,     // Worker/Committer to Job Manager/Commiter
        MSG_CLOSE_CONNECTION    // Worker/Committer to Job Manager/Commiter
};

/* Functions */

// Worker and Committer Functions
void COMM_connect_to_job_manager(char ip_adr[]);
void COMM_connect_to_committer();
int COMM_setup_committer();
void COMM_set_committer();
void COMM_get_committer();
int COMM_get_rank_id();
int COMM_get_run_num();
char * COMM_get_path();
int COMM_get_alive();
void COMM_disconnect_from_job_manager();
void COMM_disconnect_from_committer();

// Job Manager Functions
int COMM_setup_job_manager_network();
struct byte_array * COMM_wait_request(enum message_type * type, int * origin_socket,struct byte_array * ba);
int COMM_register_committer();
void COMM_create_new_connection();
void COMM_close_connection(int sock);
void COMM_increment_run_num();
void COMM_set_path(char * file_path);
void COMM_send_committer();
void COMM_send_path();
void COMM_send_alive(int origin_socket);
void COMM_LIST_print_ip_list();

// General Propose
struct byte_array * COMM_read_message(struct byte_array *ba, enum message_type *type, int rcv_socket);
int COMM_send_bytes(int sock, void * bytes, int size);
void COMM_send_message(struct byte_array *ba, int type, int dest_socket);
void COMM_read_bytes(int sock, int * size, struct byte_array * ba);
int COMM_send_char_array(int sock, char * array);               // with unknown size
char * COMM_read_char_array(int sock);                          // with unknown size
int COMM_send_int(int sock, int value);
int COMM_read_int(int sock);

// Extern Variables
extern struct sockaddr_in addr_committer;               // address of committer node
extern char * lib_path;                                 // path of binary
extern int run_num;
extern socket_manager, socket_committer;
extern int my_rank;
extern int sd;

#endif	/* COMM_H */