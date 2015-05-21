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

#include "list.h"

#include <stdio.h>
#include <string.h>   
#include <stdlib.h>
#include <unistd.h>
#include "comm.h"

// Add an element to a list. If it's not yet initialize, allocate memory and append the first term.
// If there is at least one hole in the ids number, put in the first hole it could find.
struct LIST_data * LIST_add_ip_address (struct LIST_data * data_pointer, char * adr, int prt, int socket,int type, int * rank) {
    struct connected_ip * ptr;
    struct connected_ip * iter;
    char * ip_hole;
    int holes_counter=0;
   
    // In case there is no one there or there ins't any zombie hole : create the node.
    if((data_pointer == NULL) || (data_pointer->holes == 0)) {
        ptr = (struct connected_ip *) malloc (sizeof(struct connected_ip));
        ptr->address = (char *) malloc (sizeof(char)*12);   // max size of ip : xxx.xxx.xxx\0
        (ptr->address)[0] = '\0';
        strcat(ptr->address, adr);
        ptr->port = prt;
        ptr->socket = socket;
        ptr->rcv_tasks = 0;
        ptr->done_tasks = 0;
        ptr->connected = 1;
        ptr->type = type;
    }
    
    if(data_pointer == NULL) {     // Initialize, add the job manager
        data_pointer = (struct LIST_data *) malloc (sizeof(struct LIST_data));
        
        ptr->id=0;
        ptr->next = NULL;
        ptr->type=0;
        
        data_pointer->id_counter = 0;
        data_pointer->holes = 0;
        data_pointer->list_pointer = ptr;
        
        ip_hole = (char *) malloc(sizeof(char)*2);
        ip_hole[0]='0'; ip_hole[1]='\0';
        
        data_pointer = LIST_add_ip_address (data_pointer, ip_hole, 0, 0, (int) COMMITTER, NULL);
        LIST_search_id(data_pointer, 1)->connected=0;
        return data_pointer;
    }
   
    else {
        if(data_pointer->holes == 0) {   // Add worker without holes
            ptr->next = data_pointer->list_pointer;
            data_pointer->id_counter++;
            ptr->id= data_pointer->id_counter;
            if (rank != NULL) {
                * rank = ptr->id;
            }
            
            data_pointer->list_pointer = ptr;
            return data_pointer;
        }
        else {                          // Add worker with holes
            holes_counter = data_pointer->holes;
            data_pointer->holes = data_pointer->holes - 1;  // Updates holes counter.
            iter = data_pointer->list_pointer;
            
            // Find the first (oldest) hole.
            while (holes_counter > 0) {
                if(iter->next == NULL) {
                    // If list got empty for some reason with holes_counter=0
                    error("Inconsistent list: couldn't find a hole but holes_counter>0");
                    data_pointer->holes = 0; 
                    return LIST_add_ip_address (data_pointer, adr, prt, socket, type, rank);
                }
                if(iter->type == -1) {
                    if(holes_counter > 1) {
                        iter = iter ->next;
                    }
                    holes_counter--;
                }
            
                else { 
                    iter = iter->next;
                }
            }

            // And put the new node in the hole.
            (iter->address)[0] = '\0';
            strcat(iter->address, adr);
            iter->port = prt;
            iter->socket = socket;
            iter->rcv_tasks = 0;
            iter->done_tasks = 0;
            iter->connected = 1;
            iter->type = type;

            if(rank != NULL) {
                * rank = iter->id;
            }
            
            return data_pointer;
        }
    }
}

// Mark node as disconnected in the list.
void LIST_disconnect_ip_adress(struct LIST_data * data_pointer, char * adr, int prt) {
    struct connected_ip * pointer;
    
    if(data_pointer == NULL) {
        error("Tried to disconnect ip from null list");
        return;
    }
    else {
        pointer = data_pointer->list_pointer;
        
        while(pointer != NULL) {
            if((strcmp((const char *)adr, (const char *)pointer->address)==0) && (prt == pointer->port)) {
                pointer->connected = 0;              
                pointer->socket = -1;
                return;
            }
            pointer = pointer->next;
        }
    } 
}

// Remove an element from the list using the ip and port.
struct LIST_data * LIST_remove_ip_address (struct LIST_data * data_pointer, char * adr, int prt) {
    struct connected_ip * ptr = data_pointer->list_pointer;

    if(data_pointer == NULL) {
        error("Removing ip address from null data_pointer");
        return data_pointer;
    }
   
    ptr = data_pointer->list_pointer;
   
    // Search for nodes matching the data 
    ptr = data_pointer->list_pointer->next;
    while(ptr!=NULL) {
        if((strcmp((const char *)adr, (const char *)ptr->address)==0) && (prt == ptr->port)) {
            ptr->socket = -1;
            ptr->type = -1;
            ptr = NULL;
            data_pointer->holes++;
        }
        else {
            ptr = ptr->next;
        }
    }
    
    return data_pointer;
}

// Remove an element from the list using the ip and port.
struct LIST_data * LIST_remove_id(struct LIST_data * data_pointer, int id) {
    struct connected_ip * ptr;

    if(data_pointer == NULL) {
        error("Removing id from null data_pointer");
        return data_pointer;
    }
   
    // Check for the rest.
    ptr = data_pointer->list_pointer->next;
    while(ptr!=NULL) {
        if(ptr->id == id) {
            ptr->socket = -1;
            ptr->type = -1;
            ptr = NULL;
            data_pointer->holes++;
        }
        else {
            ptr = ptr->next;
        }
    }
    
    return data_pointer;
}

// Search for a given address and port.
struct connected_ip * LIST_search_ip_address (struct LIST_data * data_pointer, char * adr, int prt) {
    struct connected_ip * pointer;
    
    if(data_pointer == NULL) {
        return NULL;
    }
    else {
        pointer = data_pointer->list_pointer;
        
        while(pointer != NULL) {
            if((strcmp((const char *)adr, (const char *)pointer->address)==0) && (prt == pointer->port)) {
                return pointer;              
            }
            pointer = pointer->next;
        }
    }
    
    return NULL;
}

// Search for a given id rank.
struct connected_ip * LIST_search_id(struct LIST_data * data_pointer, int rank_id) {
    struct connected_ip * pointer;
    
    if(data_pointer == NULL) {
        return NULL;
    }
    else {
        pointer = data_pointer->list_pointer;
        
        while(pointer != NULL) {
            if(pointer->id == rank_id){
                return pointer;              
            }
            pointer = pointer->next;
        }
    }
    
    return NULL; 
}

// Upgrade a member to the committer position (id = (int) COMMITTER). Removes the duplicate.
struct LIST_data * LIST_register_committer(struct LIST_data * data_pointer, char * adr, int prt, int new_prt) {
    int committer_socket = LIST_get_socket(data_pointer, LIST_get_id(data_pointer, adr, prt));
    struct connected_ip * ptr;

    data_pointer = LIST_remove_ip_address(data_pointer, adr, prt);

    if(data_pointer == NULL) {
        return data_pointer;
    }
    
    ptr = data_pointer->list_pointer;
    while(ptr!=NULL) {
        if(ptr->id == (int) COMMITTER){
            if(ptr->address != NULL) {
                free(ptr->address);
            }

            ptr->address = (char *) malloc (sizeof(char)*12);   // max size of ip : xxx.xxx.xxx\0
            (ptr->address)[0] = '\0';
            strcat(ptr->address, adr);
            ptr->port = new_prt;
            ptr->socket = committer_socket;
            ptr->type = 1;
            ptr->connected = 1;
            ptr = NULL;
        }
        else {
            ptr = ptr->next;
        }
    }
    
    return data_pointer;    
}

// Changed id from a node. Do it to reuse id and info for previously disconnected node. 
// Due to the logic of the close_connection function, it will only have one duplicate.
struct LIST_data * LIST_update_id(struct LIST_data * data_pointer, int current_id, int original_id) {
    struct connected_ip * ptr_c = LIST_search_id(data_pointer, current_id); 
    struct connected_ip * ptr_o= LIST_search_id(data_pointer, original_id); 
    
    if((ptr_c == NULL)||(ptr_o == NULL)) {
        return data_pointer;
    }

    ptr_o->connected = 1;
    ptr_o->socket = ptr_c->socket;
    ptr_o->port = ptr_c->port;
   
    LIST_remove_id(data_pointer, current_id);
    return data_pointer;    
}

// Get the id of a given address and port values.
int LIST_get_id (struct LIST_data * data_pointer, char * adr, int prt) {
    return (LIST_search_ip_address(data_pointer, adr, prt))->id;
}

// Print all ip in inversed ordered. (FILO))
void LIST_print_all_ip (struct LIST_data * data_pointer) {
    int total = 0;
    struct connected_ip * pointer;

    if(data_pointer == NULL) {
        return;
    }
    
    pointer = data_pointer->list_pointer;
    
    while(pointer != NULL) {
        total++;
        debug("Connection: %d\n", total);
        debug("ip: %s\n", pointer->address);
        debug("port: %d\n", pointer->port);
        debug("id: %d\n\n", pointer->id);
        pointer = pointer->next;
    }
}

// Print all ip in crescenting order. (FIFO)
int LIST_print_all_ip_ordered (struct LIST_data * data_pointer) {
    struct connected_ip * pointer;
    struct LIST_data next_data;
    
    if(data_pointer!=NULL) {
        pointer = data_pointer->list_pointer;
        if(pointer != NULL) {
            next_data.list_pointer = pointer->next;
            int num=LIST_print_all_ip_ordered (&next_data);
            debug("Connection: %d\n", num);
            debug("ip: %s\n", pointer->address);
            debug("port: %d\n", pointer->port);
            debug("id: %d\n\n", pointer->id);
            return (num+1); 
        }
    }
    return 1;
}

// Get the number of total nodes registered in the list.
int LIST_get_total_nodes (struct LIST_data * data_pointer) {
    int total = 0;
    struct connected_ip * pointer;
    
    if(data_pointer == NULL) {
        return total;
    }

    pointer = data_pointer->list_pointer;
    
    while(pointer!=NULL) {
        pointer = pointer->next;
        total++;
    }

    return total;
}

// Free the data of the structure.
void LIST_free_data (struct LIST_data * data_pointer) {
    if(data_pointer==NULL) {
        error("Trying to free NULL data_pointer");
        return;
    }

    LIST_free_list(data_pointer->list_pointer);
    free(data_pointer);
}

// Free the data of a given list of ips.
void LIST_free_list (struct connected_ip * pointer) {
    if(pointer!=NULL) {
        LIST_free_list(pointer->next);
        free(pointer->address);
        free(pointer);
    }    
}

// Get a socket for a given rank id value, base in a list data structure.
int LIST_get_socket (struct LIST_data * data_pointer, int rank_id) {
    struct connected_ip * ptr;

    if(data_pointer == NULL) {
        error("NULL pointer passed to LIST_get_socket");
        return -1;
    }

    else { 
        ptr = data_pointer->list_pointer;
        while(ptr!=NULL) {
            if(ptr->id == rank_id) {
                return ptr->socket;
            }
            ptr = ptr->next;
        }

        error("Can't find id provided in get_socket");
        return -1;
    }
}

// Get node for a given socket, based in list of ips.
struct connected_ip * LIST_search_socket(struct LIST_data * data_pointer, int socket) {
    struct connected_ip * pointer = data_pointer-> list_pointer; 
    
    if (pointer == NULL) {
        return NULL;
    }
   
    while (pointer != NULL) {
        if (pointer->socket == socket) {
            return pointer;
        }
        pointer = pointer->next;
    }
    return NULL;
}

// Get type using socket.
int LIST_get_type_with_socket(struct LIST_data * data_pointer, int socket) {
    struct connected_ip * pointer = data_pointer-> list_pointer; 
    
    if (pointer == NULL) {
        return -1;
    }
   
    while (pointer != NULL) {
        if (pointer->socket == socket) {
            return pointer->type;
        }
        pointer = pointer->next;
    }
    return -1;
}

// Updates the list using the adr (if it's not null) or the rank_id.
// Sum done_tasks and rcv_tasks with provided values. 
void LIST_update_tasks_info (struct LIST_data * data_pointer,char * adr, int prt, int rank_id, int n_rcv, int n_done) {
    struct connected_ip * pointer;
   
    if(adr!=NULL) {
        pointer = LIST_search_ip_address(data_pointer,adr, prt);  
    } 
    else {
        pointer = LIST_search_id(data_pointer, rank_id);  
    }

    if(pointer != NULL) {
        pointer->done_tasks = pointer->done_tasks + n_done;
        pointer->rcv_tasks = pointer->rcv_tasks + n_rcv;
    }
}

char * LIST_join_info(struct connected_ip * pointer, char * info) {
    char buffer[16];

    if(pointer->next != NULL) {
        info = LIST_join_info(pointer->next, info);
    }

    sprintf(buffer, "%d", pointer->id);
    strcat(info, buffer);
    strcat(info, "|");
    
    strcat(info, pointer->address);
    strcat(info, "|");

    sprintf(buffer, "%d", pointer->port);
    strcat(info, buffer);
    strcat(info, "|");

    sprintf(buffer, "%d", pointer->type);
    strcat(info, buffer);
    strcat(info, "|");

    sprintf(buffer, "%d", pointer->connected);
    strcat(info, buffer);
    strcat(info, "|");
    
    sprintf(buffer, "%lu", pointer->rcv_tasks);
    strcat(info, buffer);
    strcat(info, "|");

    sprintf(buffer, "%lu", pointer->done_tasks);
    strcat(info, buffer);
    strcat(info, ";\n");

    return info;
}

// Format : id(max 999 999)|ip(ip v6)|port(int)|type(0,1,2 or 3)|connected(1 or 0)|rcv_tasks|done_tasks; [Another entry here]
// Assume rcv_tasks/done_tasks max 999 999 999 (12 chars)
// Ip max : 999.999.999.999 = 15 chars
// Port Range : 0...65535 = 5 chars 
// Max size of string = n* (9+1 (id) +15+1 (ip) + 5+1 (port) 1+1 (type) + 1+1 + 12+1 + 12+1 + 1(;) + 1(\n ) <= 55. 
char * LIST_generate_info(struct LIST_data * data_pointer, char * filename) {
    char * info;
    int total_nodes = LIST_get_total_nodes(data_pointer);
    struct connected_ip * pointer = data_pointer->list_pointer;
    FILE *f;
    
    info = (char *) malloc(sizeof(char)*55*total_nodes + 1);     // \0 addition
    info[0] = '\0';
    
    info = LIST_join_info(pointer, info);

    if(filename) {
        // Remove file if exist.
        if( access(filename, F_OK) != -1 ) {
            remove(filename);
        }
        // Create file with info string.
        f = fopen(filename, "w");
        fprintf(f, "%s", info);
        fclose(f);
        return info;
    }

    return info;
}

// Search for all disconnected VM nodes and try to connect with them. 
// Returns the number of nodes reconnected successfully.
int check_VM_nodes(struct LIST_data * data_pointer) {
    // Iteration struct.
    struct connected_ip * ptr = data_pointer->list_pointer;
    // Used to convert ip/port to format expected by COMM_connect_to_vm_task_manager
    struct byte_array * ba = (struct byte_array *) malloc (sizeof(struct byte_array));

                                                // xxx.xxx.xxx.xxx|yyyyyyyy\0
    char *v = malloc(sizeof(char)*25);          // string of address in format above.
    size_t n;                                   // string length
    char port[10];                              // port of node.
    int retries = 1;                            // retries for each VM node.
    int total = 0;                              // number of nodes reconnected.
    v[0] = '\0';

    while (ptr!= NULL) {
        if((ptr->type==(int)VM_TASK_MANAGER) && (ptr->connected == 0)) {
            // add address to string
            v = strcat(v, ptr->address);
            v = strcat(v, "|");

            // cast ptr->port to string port and join with string.
            sprintf(port, "%d", ptr->port);
            v = strcat(v, port);

            // find size of string and initialize the byte array.
            n = (size_t) (strlen(v)+1); 
            byte_array_init(ba, n);

            // packs the string and try to connect.
            byte_array_pack8v(ba, v, n); 
            if(COMM_connect_to_vm_task_manager(&retries, ba) == 0 ) {
                debug("Reconnected successfully with VM node with id %d.", ptr->id);
                total ++;
            }

            // free memory used in byte_array
            byte_array_free(ba);
        }
        ptr = ptr->next;
    }

    // Free memory used.
    free(v);
    free(ba);

    return total;
}
