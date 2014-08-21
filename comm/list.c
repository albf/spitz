#include "list.h"

struct connected_ip * LIST_add_ip_adress (struct connected_ip * pointer, char * adr, int prt) {
    struct connected_ip * ptr = (struct connected_ip *) malloc (sizeof(struct connected_ip));
    ptr->address = adr;
    ptr->port = prt;
    ptr->next = pointer;
    
    if(ptr->next == NULL)
        ptr->id=0;
    else
        ptr->id=(ptr->next->id) + 1;
    
    return ptr;
}

struct connected_ip * LIST_remove_ip_adress (struct connected_ip * pointer, char * adr, int prt) {
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

struct connected_ip * LIST_search_ip_adress (struct connected_ip * pointer, char * adr, int prt) {
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

int LIST_get_id (struct connected_ip * pointer, char * adr, int prt) {
    return (LIST_search_ip_adress(pointer, adr, prt))->id;
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