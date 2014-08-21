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

struct connected_ip * add_ip_address (struct connected_ip * pointer, char * adr, int prt);
struct connected_ip * remove_ip_address (struct connected_ip * pointer, char * adr, int prt);
struct connected_ip * search_ip_address (struct connected_ip * pointer, char * adr, int prt);
int get_id (struct connected_ip * pointer, char * adr, int prt);
void print_all_ip (struct connected_ip * pointer);
int print_all_ip_ordered (struct connected_ip * pointer);
int get_total_workers (struct connected_ip * pointer);
void free_list (struct connected_ip * pointer);
void print_ip_list();

#endif	/* LIST_H */

