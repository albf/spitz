/* 
 * File:   main.c
 * Author: alexandre
 *
 * Created on August 20, 2014, 10:29 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include "comm.h"
#include "list.h"

/*
 * 
 */
int main(int argc, char*argv[]) {
    if(argc == 1) {
        COMM_setup_job_manager_network(argc, argv);
        while(1) {
            COMM_wait_request();
            COMM_LIST_print_ip_list();        
        }
    }
    else if(argc == 2 ) {
	COMM_connect_to_job_manager("127.0.0.1");
	COMM_get_committer();
    }
    else { 
        COMM_telnet_client(argc,argv);
    }
}