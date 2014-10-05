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
    
    int holes_counter=0;
    struct connected_ip * backup;
    
    if(data_pointer == NULL) {     // Initialize, add the job manager
        data_pointer = (struct LIST_data *) malloc (sizeof(struct LIST_data));
        
        ptr->id=0;
        ptr->next = NULL;
        
        data_pointer->id_counter = 0;
        data_pointer->holes = 0;
        data_pointer->list_pointer = ptr;
        
        return LIST_add_ip_address (data_pointer, NULL, 0, 0, NULL);
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

// Search for a give address and port.
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
int LIST_get_total_workers (struct LIST_data * data_pointer) {
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