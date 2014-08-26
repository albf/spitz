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

#define PORT 8888
#define max_clients 30

/* Functions */

// Worker and Committer Functions
void COMM_connect_to_job_manager(char ip_adr[]);
void COMM_set_committer();
void COMM_get_committer();
int COMM_request_committer();
int COMM_get_rank_id();
int COMM_telnet_client(int argc, char *argv[]);

    // Job Manager Functions
int COMM_setup_job_manager_network(int argc , char *argv[]);
int COMM_wait_request();
void COMM_create_new_connection();
void COMM_close_connection(int sock);
    
// General Propose
int COMM_send_bytes(int sock, void * bytes, int size);
void * COMM_read_bytes(int sock);
int COMM_send_char_array(int sock, char * array);      // with unknown size
char* COMM_read_char_array(int sock);                  // with unknown size

#endif	/* COMM_H */

