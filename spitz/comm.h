/* 
 * File:   comm.h
 * Author: alexandre
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

#define PORT 8888
#define max_clients 30

// Enums of actor and message type
enum actor {
	JOB_MANAGER = 0,
	COMMITTER   = 1,
};

enum message_type {
	MSG_READY,
	MSG_TASK,
	MSG_RESULT,	
	MSG_KILL,	
	MSG_DONE,	
	MSG_GET_ALIVE,
	MSG_GET_COMMITTER,
	MSG_GET_PATH,
	MSG_GET_RANK,
	MSG_GET_RUNNUM,
	MSG_SET_COMMITTER,	
};

/* Functions */

// Worker and Committer Functions
void COMM_connect_to_job_manager(char ip_adr[]);
void COMM_set_committer();
void COMM_get_committer();
int COMM_request_committer();
int COMM_get_rank_id();
int COMM_get_run_num();
char * COMM_get_path();
int COMM_telnet_client(int argc, char *argv[]);
int COMM_get_alive();

    // Job Manager Functions
int COMM_setup_job_manager_network(int argc , char *argv[]);
int COMM_wait_request();
int COMM_reply_request();
void COMM_create_new_connection();
void COMM_close_connection(int sock);
void COMM_increment_run_num();
void COMM_set_path(char * file_path);
    
// General Propose
int COMM_send_bytes(int sock, void * bytes, int size);
void * COMM_read_bytes(int sock, int * size);
int COMM_send_char_array(int sock, char * array);      // with unknown size
char* COMM_read_char_array(int sock);                  // with unknown size
int COMM_send_int(int sock, int value);
int COMM_read_int(int sock);
void COMM_send_message(struct byte_array *ba, int type, int dest_socket);
void COMM_get_message(struct byte_array *ba, enum message_type *type, int rcv_socket);

#endif	/* COMM_H */