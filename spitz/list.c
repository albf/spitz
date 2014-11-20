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
#include "comm.h"

// Add an element to a list. If it's not yet initialize, allocate memory and append the first term.
// If there is at least one hole in the ids number, put in the same hole it could find.
struct LIST_data * LIST_add_ip_address (struct LIST_data * data_pointer, char * adr, int prt, int socket, int * rank) {
    struct connected_ip * ptr = (struct connected_ip *) malloc (sizeof(struct connected_ip));
    struct connected_ip * iter;
    ptr->address = adr;
    ptr->port = prt;
    ptr->socket = socket;
    ptr->rcv_tasks = 0;
    ptr->done_tasks = 0;
    ptr->connected = 1;
    ptr->type = 2;
    char * ip_hole;
    
    int holes_counter=0;
    struct connected_ip * backup;
    
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
        
        data_pointer = LIST_add_ip_address (data_pointer, ip_hole, 0, 0, NULL);
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
        else {                  // Add worker with holes
            holes_counter = data_pointer->holes;
            iter = backup;
            
            while (holes_counter > 0) {
                if(iter->id != ((iter->next->id)+1)) {
                    if(holes_counter > 1) {
                        iter = iter ->next;
                    }
                    
                    holes_counter--;
                }
            
                else { 
                    iter = iter->next;
                }
            }

            ptr->next = iter->next;
            iter->next = ptr;
            ptr->id = iter->id - 1;
            
            if(rank != NULL) {
                * rank = ptr->id;
            }
            
            return data_pointer;
        }
    }
}

// Mark node as disconnected in the list.
void LIST_disconnect_ip_adress(struct LIST_data * data_pointer, char * adr, int prt) {
    struct connected_ip * pointer;
    
    if(data_pointer == NULL) {
        return;
    }
    else {
        pointer = data_pointer->list_pointer;
        
        while(pointer != NULL) {
            if((strcmp((const char *)adr, (const char *)pointer->address)==0) && (prt == pointer->port)) {
                pointer->connected = 0;              
                return;
            }
            pointer = pointer->next;
        }
    } 
}

// Remove an element from the list using the ip and port.
struct LIST_data * LIST_remove_ip_address (struct LIST_data * data_pointer, char * adr, int prt) {
    struct connected_ip * prev = data_pointer->list_pointer;
    struct connected_ip * ptr;

    if(data_pointer == NULL) {
        return data_pointer;
    }
    
    if((strcmp((const char *)adr, (const char *)data_pointer->list_pointer->address)==0)&&(prt == data_pointer->list_pointer->port)){ 
    	ptr = data_pointer->list_pointer->next;
        free(data_pointer->list_pointer);
        data_pointer->id_counter--;
        data_pointer->list_pointer = ptr;
        return data_pointer;
    }
    
    ptr = data_pointer->list_pointer->next;
    while(ptr!=NULL) {
        if((strcmp((const char *)adr, (const char *)ptr->address)==0) && (prt == ptr->port)) {
            prev->next = ptr->next;
            free(ptr);
            ptr = NULL;
            data_pointer->holes++;
        }
        
        else {
            prev = ptr;
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

            ptr->address = adr;
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
        printf("Connection: %d\n", total);
        printf("ip: %s\n", pointer->address);
        printf("port: %d\n", pointer->port);
        printf("id: %d\n\n", pointer->id);
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
            printf("Connection: %d\n", num);
            printf("ip: %s\n", pointer->address);
            printf("port: %d\n", pointer->port);
            printf("id: %d\n\n", pointer->id);
            return (num+1); 
        }
    }
    return 1;
}

// Get the number of total workers registered in the list.
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
        return;
    }

    LIST_free_list(data_pointer->list_pointer);
    free(data_pointer);
}

// Free the data of a given list of ips.
void LIST_free_list (struct connected_ip * pointer) {
    if(pointer!=NULL) {
        LIST_free_list(pointer->next);
        free(pointer);
    }    
}

// Get a socket for a given rank id value, base in a list data structure.
int LIST_get_socket (struct LIST_data * data_pointer, int rank_id) {
    if(data_pointer == NULL) {
        return -1;
    }

    else { 
        return LIST_get_socket_list(data_pointer->list_pointer, rank_id);       
    }
}

// Get a socket for a given rank id value, based in list of ips.
int LIST_get_socket_list (struct connected_ip * pointer, int rank_id) {
    if (pointer == NULL) {
        return -1;
    }
    
    else if (pointer->id == rank_id) {
        return pointer->socket;
    }

    else {
        return LIST_get_socket_list(pointer->next, rank_id);
    }
}

// Get rank for a given socket, based in list of ips.
int LIST_get_rank_id_with_socket(struct LIST_data * data_pointer, int socket) {
    struct connected_ip * pointer = data_pointer-> list_pointer; 
    
    if (pointer == NULL) {
        return -1;
    }
   
    while (pointer != NULL) {
        if (pointer->socket == socket) {
            return pointer->id;
        }
        pointer = pointer->next;
    }
    return -1;
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

// Format : id(max 999 999)|ip(ip v6)|port(int)|type(0,1,2 or 3)|connected(1 or 0)|rcv_tasks|done_tasks; [Another entry here]
// Assume rcv_tasks/done_tasks max 999 999 999 (12 chars)
// Ip max : 999.999.999.999 = 15 chars
// Port Range : 0...65535 = 5 chars 
// Max size of string = n* (9+1 (id) +15+1 (ip) + 5+1 (port) 1+1 (type) + 1+1 + 12+1 + 12+1 + 1(;) ) = n * 53. 
char * LIST_get_monitor_info(struct LIST_data * data_pointer) {
    char * info;
    char buffer[15];
    int total_nodes = LIST_get_total_nodes(data_pointer);
    struct connected_ip * pointer = data_pointer->list_pointer;
    
    info = (char *) malloc(sizeof(char)*53*total_nodes + 1);     // \n addition
    info[0] = '\0';
    
    while (pointer != NULL) {
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
        strcat(info, ";");

        pointer = pointer->next;
    }
    strcat(info, "\0");
    return info;
}