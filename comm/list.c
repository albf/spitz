#include "list.h"

struct connected_ip * add_ip_address (struct connected_ip * pointer, char * adr, int prt) {
    struct connected_ip * ptr = (struct connected_ip *) malloc (sizeof(struct connected_ip));
    ptr->address = adr;
    ptr->port = prt;
    ptr->next = pointer;
    return ptr;
}

struct connected_ip * remove_ip_address (struct connected_ip * pointer, char * adr, int prt) {
    struct connected_ip * home = pointer;
    struct connected_ip * ptr;

    if(pointer == NULL)
        return pointer;
    
    if((strcmp((const char *)adr, (const char *)pointer->address)==0) && (prt == pointer->port)) { 
    	ptr = pointer->next;
        free(pointer);
        return ptr;
    }
    
    ptr = pointer->next;
    while(ptr!=NULL) {
        if((strcmp((const char *)adr, (const char *)ptr->address)==0) && (prt == ptr->port)) {
            pointer->next = ptr->next;
            free(ptr);
            ptr = NULL;
        }
        
        else {
                pointer = ptr;
                ptr = pointer->next;
        }
    }
    
    return home;    
}

struct connected_ip * search_ip_address (struct connected_ip * pointer, char * adr, int prt) {
    if(pointer == NULL)
        return NULL;

    else
        while(pointer != NULL) {
            if((strcmp((const char *)adr, (const char *)pointer->next)==0) && (prt == pointer->port)) {
                return pointer;              
            }
            pointer = pointer->next;
        }
    
    return NULL;
}

void print_all_ip (struct connected_ip * pointer) {
    int total = 0;
    
    while(pointer != NULL) {
        total++;
        printf("Connection: %d\n", total);
        printf("ip: %s\n", pointer->address);
        printf("port: %d\n\n", pointer->port);
        pointer = pointer->next;
    }
}

int get_total_workers (struct connected_ip * pointer) {
    int total = 0;

    while(pointer!=NULL) {
	pointer = pointer->next;
	total++;
    }

    return total;
}