/* 
 * File:   list.h
 * Author: alexandre
 *
 * Created on August 20, 2014, 12:38 AM
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
};

struct connected_ip * add_ip_address (struct connected_ip * pointer, char * adr, int prt);
struct connected_ip * remove_ip_address (struct connected_ip * pointer, char * adr, int prt);
struct connected_ip * search_ip_address (struct connected_ip * pointer, char * adr, int prt);
void print_all_ip (struct connected_ip * pointer);
int get_total_workers (struct connected_ip * pointer);

#endif	/* LIST_H */

