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

struct connected_ip {
    struct connected_ip * next;
    char * address;
    int port;
    int id;
};

// List manipulation 
struct connected_ip * LIST_add_ip_adress(struct connected_ip * pointer, char * adr, int prt);
struct connected_ip * LIST_remove_ip_adress(struct connected_ip * pointer, char * adr, int prt);
struct connected_ip * LIST_search_ip_adress(struct connected_ip * pointer, char * adr, int prt);
void LIST_free_list(struct connected_ip * pointer);

// Other requests
int LIST_get_id(struct connected_ip * pointer, char * adr, int prt);
int LIST_get_total_workers(struct connected_ip * pointer);

// Debug Information
void LIST_print_all_ip(struct connected_ip * pointer);
int LIST_print_all_ip_ordered(struct connected_ip * pointer);
void LIST_print_ip_list();

#endif	/* LIST_H */

