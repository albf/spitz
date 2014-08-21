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
void connect_to_job_manager(char ip_adr[]);
void set_committer();
void get_committer();
int request_committer();
int get_rank_id();
int telnet_client(int argc, char *argv[]);

    // Job Manager Functions
int setup_job_manager_network(int argc , char *argv[]);
int wait_request();
void create_new_connection();
void close_connection(int sock);
    
// General Propose
int send_byte_array(int sock, unsigned char * array);       // with unknown size
unsigned char * read_byte_array(int sock);                  // with unknown size

#endif	/* COMM_H */

