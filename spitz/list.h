/*
 * Copyright 2014 Alexandre Luiz Brisighello Filho <albf.unicamp@gmail.com>
 *
 * This file is part of spitz.
 *
 * spitz is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * spitz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with spitz.  If not, see <http://www.gnu.org/licenses/>.
 */

 // Simple linked list used to store the ip and port from works and committers.

#ifndef LIST_H
#define	LIST_H

// Represent a list of connected ips.
struct connected_ip {
    struct connected_ip * next;
    char * address;
    int port;
    int type;
    int id;
    int connected;
    int socket;
    unsigned long rcv_tasks;
    unsigned long done_tasks;
};

// A list and it's data.
struct LIST_data {
    int id_counter;
    int holes;
    struct connected_ip * list_pointer; 
};

// List manipulation 
struct LIST_data * LIST_add_ip_address (struct LIST_data * data_pointer, char * adr, int prt, int socket, int type, int * rank);
void LIST_disconnect_ip_adress(struct LIST_data * data_pointer, char * adr, int prt);
struct LIST_data * LIST_remove_ip_address (struct LIST_data * data_pointer, char * adr, int prt);
struct LIST_data * LIST_remove_id(struct LIST_data * data_pointer, int id);
struct connected_ip * LIST_search_id(struct LIST_data * data_pointer, int rank_id);
struct connected_ip * LIST_search_ip_address (struct LIST_data * data_pointer, char * adr, int prt);
struct connected_ip * LIST_search_socket(struct LIST_data * data_pointer, int socket);
struct LIST_data * LIST_register_committer(struct LIST_data * data_pointer, char * adr, int prt, int new_prt);
struct LIST_data * LIST_update_id(struct LIST_data * data_pointer, int sock, int new_id);
void LIST_update_tasks_info (struct LIST_data * data_pointer,char * adr, int prt, int rank_id, int n_rcv, int n_done);
void LIST_free_data (struct LIST_data * data_pointer);

// Other requests
int check_VM_nodes(struct LIST_data * data_pointer);
int LIST_get_id (struct LIST_data * data_pointer, char * adr, int prt);
int LIST_get_total_nodes (struct LIST_data * data_pointer);
int LIST_get_socket (struct LIST_data * data_pointer, int rank_id);

// Debug & GUI Information
char * LIST_get_monitor_info(struct LIST_data * data_pointer);
void LIST_print_all_ip (struct LIST_data * data_pointer);
int LIST_print_all_ip_ordered (struct LIST_data * data_pointer);

// Auxiliary
void LIST_free_list (struct connected_ip * pointer);
int LIST_get_socket_list (struct connected_ip * pointer, int rank_id);

#endif	/* LIST_H */

