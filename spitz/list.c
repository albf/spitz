#include "list.h"

int LIST_id_counter;
int LIST_holes;

struct connected_ip * LIST_add_ip_address (struct connected_ip * pointer, char * adr, int prt, int socket, int * rank) {
    struct connected_ip * ptr = (struct connected_ip *) malloc (sizeof(struct connected_ip));
    ptr->address = adr;
    ptr->port = prt;
    ptr->socket = socket;
    
    int holes_counter=0;
    struct connected_ip * backup;
    
    if(ptr->next == NULL) {     // Initialize, add the job manager
        ptr->id=0;
        LIST_id_counter = 0;
        LIST_holes = 0;
        return LIST_add_ip_address (pointer, NULL, 0, 0);
    }
    
    else {
        if(LIST_holes == 0) {   // Add worker without holes
            ptr->next = pointer;
            LIST_id_counter++;
            ptr->id= LIST_id_counter;
            if (rank != NULL)
                * rank = LIST_id_counter;
            return ptr;
        }
        else {                  // Add worker with holes
            holes_counter = LIST_holes;
            backup = pointer;
            
            while (holes_counter > 0) {
                if(pointer->id != ((pointer->next->id)+1)) {
                    if(holes_counter > 1)
                        pointer = pointer->next;
                    
                    holes_counter--;
                }
            
                else 
                    pointer = pointer->next;
            }

            ptr->next = pointer->next;
            pointer->next = ptr;
            ptr->id = pointer->id - 1;
            
            if(rank != NULL)
                * rank = LIST_id_counter;
            
            return backup;
        }
    }
}

struct connected_ip * LIST_remove_ip_address (struct connected_ip * pointer, char * adr, int prt) {
    struct connected_ip * home = pointer;
    struct connected_ip * ptr;

    if(pointer == NULL)
        return pointer;
    
    if((strcmp((const char *)adr, (const char *)pointer->address)==0) && (prt == pointer->port)) { 
    	ptr = pointer->next;
        free(pointer);
        LIST_id_counter--;
        return ptr;
    }
    
    ptr = pointer->next;
    while(ptr!=NULL) {
        if((strcmp((const char *)adr, (const char *)ptr->address)==0) && (prt == ptr->port)) {
            pointer->next = ptr->next;
            free(ptr);
            ptr = NULL;
            LIST_holes++; 
        }
        
        else {
            pointer = ptr;
            ptr = pointer->next;
        }
    }
    
    return home;    
}

struct connected_ip * LIST_search_ip_address (struct connected_ip * pointer, char * adr, int prt) {
    if(pointer == NULL)
        return NULL;

    else
        while(pointer != NULL) {
            if((strcmp((const char *)adr, (const char *)pointer->address)==0) && (prt == pointer->port)) {
                return pointer;              
            }
            pointer = pointer->next;
        }
    
    return NULL;
}

struct connected_ip * LIST_register_committer(struct connected_ip * pointer, char * adr, int prt) {
    struct connected_ip * home = LIST_remove_ip_address(pointer, adr, prt);
    struct connected_ip * ptr;

    if(home == NULL)
        return home;
    
    ptr = home;
    while(ptr!=NULL) {
        if(ptr->id == (int) COMMITTER){
            if(ptr->address != NULL)
                free(ptr->address);

            ptr->address == adr;
            ptr->port = ptr;
            ptr = NULL;
            
        }
        else {
            pointer = ptr;
            ptr = pointer->next;
        }
    }
    
    return home;    
}

int LIST_get_id (struct connected_ip * pointer, char * adr, int prt) {
    return (LIST_search_ip_address(pointer, adr, prt))->id;
}

void LIST_print_all_ip (struct connected_ip * pointer) {
    int total = 0;
    
    while(pointer != NULL) {
        total++;
        printf("Connection: %d\n", total);
        printf("ip: %s\n", pointer->address);
        printf("port: %d\n", pointer->port);
        printf("id: %d\n\n", pointer->id);
        pointer = pointer->next;
    }
}

int LIST_print_all_ip_ordered (struct connected_ip * pointer) {
    if(pointer!=NULL) {
        int num=LIST_print_all_ip_ordered (pointer->next);
        printf("Connection: %d\n", num);
        printf("ip: %s\n", pointer->address);
        printf("port: %d\n", pointer->port);
        printf("id: %d\n\n", pointer->id);
        return (num+1);    
    }
    return 1;
}

int LIST_get_total_workers (struct connected_ip * pointer) {
    int total = 0;

    while(pointer!=NULL) {
	pointer = pointer->next;
	total++;
    }

    return total;
}

void LIST_free_list (struct connected_ip * pointer) {
    if(pointer!=NULL) {
        LIST_free_list(pointer->next);
        free(pointer);
    }    
}

int LIST_get_socket (struct connected_ip * pointer, int rank_id) {
    if (pointer == NULL)
        return -1;
    
    else if (pointer->id == rank_id)
        return pointer->socket;

    else
        return LIST_get_socket(pointer->next, rank_id);
}