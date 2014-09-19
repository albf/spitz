/* 
 * File:   list.h
 * Author: alexandre
 * Simple linked list used to store the ip and port from works and committers.
 */

#ifndef LIST_H
#define	LIST_H

#include <stdio.h>
#include <string.h>   
#include <stdlib.h>
#include "comm.h"

// Represent a list of connected ips.
struct connected_ip {
    struct connected_ip * next;
    char * address;
    int port;
    int id;
    int socket;
};

// A list and it's data.
struct LIST_data {
    int id_counter;
    int holes;
    struct connected_ip * list_pointer; 
};

// List manipulation 
struct LIST_data * LIST_add_ip_address (struct LIST_data * data_pointer, char * adr, int prt, int socket, int * rank);
struct LIST_data * LIST_remove_ip_address (struct LIST_data * data_pointer, char * adr, int prt);
struct connected_ip * LIST_search_ip_address (struct LIST_data * data_pointer, char * adr, int prt);
struct LIST_data * LIST_register_committer(struct LIST_data * data_pointer, char * adr, int prt, int new_prt);
void LIST_free_data (struct LIST_data * data_pointer);

// Other requests
int LIST_get_id (struct LIST_data * data_pointer, char * adr, int prt);
int LIST_get_total_workers (struct LIST_data * data_pointer);
int LIST_get_socket (struct LIST_data * data_pointer, int rank_id);

// Debug & GUI Information
void LIST_print_all_ip (struct LIST_data * data_pointer);
int LIST_print_all_ip_ordered (struct LIST_data * data_pointer);

// Auxiliary
void LIST_free_list (struct connected_ip * pointer);
int LIST_get_socket_list (struct connected_ip * pointer, int rank_id);

#endif	/* LIST_H */

