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

#include "comm.h"

#include <stdio.h>
#include <string.h>   
#include <stdlib.h>
#include <unistd.h>   
#include <arpa/inet.h>    
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>           // Wait some seconds beetween requests.
#include <barray.h>
#include <errno.h>
#include <fcntl.h>              // Used to set block/non-block sockets.
#include "log.h"
#include "taskmanager.h"

/* Global Variables */
struct sockaddr_in COMM_addr_committer; // address of committer node
int socket_manager, socket_committer;   // Important socket values
int COMM_my_rank, COMM_run_num;         // Rank and run_num variables
int COMM_loop_b;                        // Used to balance the requests
char * COMM_addr_manager;               // String of JM ip.
enum actor type;                        // Type of current node.

/* Job Manager/Committer (servers) only */
int COMM_master_socket;                 // socket used to accept connections
int COMM_addrlen;                       // len of socket addresses
int COMM_client_socket[max_clients];    // used to stock the sockets.
int COMM_committer_index;               // committer index in the structure.
int COMM_alive;                         // number of connected members
struct LIST_data * COMM_ip_list;        // list of ips connected to the manager


/* Functions Exclusive to this implementation, not used by upper level */

// Worker
int COMM_get_committer(int * retries);
int COMM_request_committer();

// Communication
int COMM_read_bytes(int sock, int * size, struct byte_array * ba, int null_terminator);
int COMM_send_bytes(int sock, void * bytes, int size);
int COMM_send_int(int sock, int value);
int COMM_read_int(int sock);


// Send message, used to make request between processes 
int COMM_send_message(struct byte_array *ba, int type, int dest_socket) {
    struct byte_array _ba;
    int return_value;
    
    if (!ba) {
      _ba.ptr = NULL;
      _ba.len = 0;
      ba = &_ba;
    }

    debug("Sending message size: %d\n", ba->len);

    return_value = COMM_send_int(dest_socket, type);
    if(return_value < 0) {
        error("Problem sending message type");
        return -1;
    }
    
    return_value = COMM_send_bytes(dest_socket, ba->ptr, (int)ba->len);
    if(return_value <0) {
        error("Problem sending message content");
        return -1;
    }
    
    return 0;
}

// Receive the message responsible for the communication between processes
// Returns 0 for sucess -1 and -2 if found any issue. In that case, message type is MSG_EMPTY.
int COMM_read_message(struct byte_array *ba, enum message_type *type, int rcv_socket) {
    int read_return;
    
    *type = (enum message_type) COMM_read_int(rcv_socket);
    
    // Received wrong type.
    if((*((int *)type)) == -1) {
        error("Problem reading message type.");
        *type = MSG_EMPTY;
        return -1;
    }

    if (ba==NULL) {
        error("Null byte array passed to read_message function.");
        *type = MSG_EMPTY;
        return -2;
    }
    
    read_return = COMM_read_bytes(rcv_socket, NULL, ba, 0);
    if(read_return < 0) {
        error("Problem reading message content.");
        *type = MSG_EMPTY;
    }

    debug("Reading message size: %d\n", ba->len);

    return read_return;
}

// Send N bytes pointer by bytes pointer.
int COMM_send_bytes(int sock, void * bytes, int size) {
    char size_c[20];
    int return_value;
    
    sprintf(size_c, "%d", size);
    debug("SENDBYTES_ MSG_SIZE: %d\n", size);
    strcat(size_c, "|\n");
    return_value = (int) send(sock, size_c, (strlen(size_c)-1), MSG_NOSIGNAL); // don't send '\n'

    if(return_value <= 0) {
        return -1;
    }
    
    if(size>0) {
    	return_value = (int) send(sock, bytes, size, MSG_NOSIGNAL);
        if(return_value <=0) {
            return -1;
        }
    }

    return 0;    
}

// Read unknown type and size from socket.
// Sock: Sending Socket         ;       size: If not null, returns the size of received message.
// ba : Destination.            ;       null_terminator: if 1, put a null terminator in the end.
int COMM_read_bytes(int sock, int * size, struct byte_array * ba, int null_terminator) {
    char message_size[20], received_char='0';
    int total_rcv, offset=0, msg_size;

    while(received_char!='|') {
        total_rcv = read(sock, &received_char, 1);

        if(total_rcv <= 0) {            // check if received zero bytes or an error.
            if(size != NULL) {
                *size = -1;
            }
            return -1;
        }
        
        if(received_char!='|') {
            message_size[offset]=received_char;
        }
        else {
            message_size[offset]='\0';
        }

        offset++;
    }

    msg_size = atoi(message_size);
    debug("READBYTES _ MSG_SIZE: %d\n", msg_size);
    offset = 0;
    
    if(null_terminator > 0) {           // Check if should reserve space for null terminator. 
        byte_array_resize(ba, msg_size+1);
        (ba->ptr) [msg_size] = '\0';
    }
    else {
        byte_array_resize(ba, msg_size);
    }
    
    if(size != NULL) {
    	* size = msg_size;    
    }
        
    ba->len = 0;
    while(offset < msg_size) {		// if zero, doesn't come in
        total_rcv = read(sock, (ba->ptr+offset), (msg_size-offset));
        ba->len += ((size_t)total_rcv);
        debug("READBYTES_DURING_DIF : %d\n", (msg_size-offset));
        debug("READBYTES_DURING_RCV : %d\n", total_rcv);
        debug("READBYTES_DURING_TOT : %d\n", (int)ba->len);

        if(total_rcv <= 0) {            // check if received zero bytes or an error.
            if(size != NULL) {
                *size = -1;
            }
            return -1;
        }
        
        offset+=total_rcv;
    }
    return 0;
}

// Send int using send bytes function
int COMM_send_int(int sock, int value) {
    char * msg = (char *) malloc (20*sizeof(char));
    int ret;
    
    sprintf(msg, "%d", value);
    ret =  COMM_send_bytes(sock, (void *)msg, strlen(msg));     // Don't send \0.
    free (msg);                                                 // Reason: avoid sending 1 byte and python issues with \0
    return ret;
}

// Read int using read bytes function
int COMM_read_int(int sock) {
    struct byte_array ba;
    int result, size; 
    /*int i;                    // Option for not sending \0.
    char * convert; */
    
    byte_array_init(&ba, 0);
    COMM_read_bytes(sock, &size, &ba, 1);
    /*convert = (char *) malloc ((size+1)*sizeof(char));

    for(i=0; i<size; i++) {
        convert[i] = ((char *)ba.ptr)[i];
    }    
    convert[size] = '\0'; */

    if(size == -1) {
        byte_array_free(&ba);
        return -1; 
    }
    
    result = atoi((char *) ba.ptr);
    byte_array_free(&ba);
    return result;
}

// Establish connection with job manager, used by any Task Manager.
int COMM_connect_to_job_manager(char ip_adr[], int * retries) {
    int con_ret;
    struct byte_array * ba; 
    int old_id = COMM_get_rank_id();
    
    if ((type == TASK_MANAGER) || (type == COMMITTER) || (type == MONITOR)) {
        con_ret = COMM_connect_to_job_manager_local(ip_adr, retries);
    }
    else if(type==VM_TASK_MANAGER) {
        con_ret = COMM_vm_connection(1,0); 
    }

    // If it is a task manager, connection was okay and received_one = 1, try to restore id.
    if (((type == TASK_MANAGER) || (type == VM_TASK_MANAGER)) && (con_ret == 0) && (received_one > 0)) {
        ba = (struct byte_array *) malloc (sizeof(struct byte_array));        
        byte_array_init(ba, 16);

        _byte_array_pack64(ba, (uint64_t) COMM_get_rank_id());          // Pack current_id 
        _byte_array_pack64(ba, (uint64_t) old_id);                      // Pack old_id 
        
        con_ret = COMM_send_message(ba, MSG_SET_TASK_MANAGER_ID, socket_manager);
        if(con_ret < 0) {
            error("Error sending request for restoring ID to Job Manager.");
            COMM_close_connection(socket_manager);
        }
        
        byte_array_free(ba);
        free(ba);
    }
    
    return con_ret;
}

// Waits for Job Manager and/or Committer connection. Requests help for making committer connection.
// Will always return 0 for now, as it waits forever.
int COMM_vm_connection(int waitJM, char waitCM) {
    int is_there_jm=0;                                              // Indicates if the job manager is set.
    int is_there_cm=0;                                              // Indicates if committer is set.
    enum message_type type;                                         // Type of received message.
    uint64_t socket_cl;                                             // Closing socket.
    int origin_socket;                                              // Socket that sent the request.
    char * v;                                                       // Used as auxiliary. 
    int64_t bufferr;                                                // Buffer used to receive id. 
   
    // May be looking only for JM or for CM
    if(waitJM == 0) {
        is_there_jm = 1;
    }

    if(waitCM == 0) {
        is_there_cm = 1;
    }

    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc (sizeof(struct byte_array));
    byte_array_init(ba, 10);

    // If only looking for committer, try to request connection help from Job Manager.
    if((is_there_jm == 1) && (is_there_cm == 0)) {
         if( COMM_send_message(NULL, MSG_SEND_VM_TO_COMMITTER, socket_manager) < 0 ) {
            error("Problem requesting Committer connection to Job Manager.");
            close(socket_manager);
            is_there_jm = 0;
        }
        else {
            debug("Successfully requested Committer connection to Job Manager.");
        }
    }

    // Will only stop waiting when both connections are alive.
    while ((is_there_jm == 0) || (is_there_cm ==0)) {
        COMM_wait_request(&type, &origin_socket, ba, NULL); 
         
        switch (type) {
            case MSG_SET_JOB_MANAGER:
                socket_manager = origin_socket;
                byte_array_unpack64(ba, &bufferr);
                COMM_my_rank = (int)bufferr;
                if(COMM_my_rank < 0) {
                    error("Problem getting the rank id. Disconnected from Job Manager.");
                    close(socket_manager);
                }
                else {
                    debug("Successfully received a rank id from Job Manager.");

                    // If there isn't committer, send request. 
                    if(is_there_cm == 0) {
                        if( COMM_send_message(NULL, MSG_SEND_VM_TO_COMMITTER, socket_manager) < 0 ) {
                            error("Problem requesting Committer connection to Job Manager.");
                            close(socket_manager);
                        }
                        else {
                            debug("Successfully requested Committer connection to Job Manager.");
                            is_there_jm = 1;               
                        }
                    }

                    // If there is committer, just accept job manager.
                    else {
                        is_there_jm = 1;
                    }
                }
                break;
            case MSG_SET_COMMITTER:
                socket_committer = origin_socket;
                is_there_cm = 1;
                break;
            case MSG_EMPTY:
                info("Message received incomplete or a problem occurred.");
                break;
            case MSG_NEW_CONNECTION:
                COMM_create_new_connection();
                break;
            case MSG_CLOSE_CONNECTION:
                _byte_array_unpack64(ba, &socket_cl);

                if(socket_cl == socket_committer) {
                    is_there_cm = 0;
                }
                if(socket_cl == socket_manager) {
                    is_there_jm = 0;
                }                
                
                COMM_close_connection((int)socket_cl);
                break;
            default:
                break;
        }
        
        // If both are set. 
        if ((is_there_jm==1)&&(is_there_cm==1)){
            break;
        }
    }
    // Free memory allocated in byte arrays.
    byte_array_free(ba);
    free(ba);
    
    return 0;
} 

// Establishes a connection with the job manager. 
// If retries == NULL -> Retries indefinitely. If retries > 0, try "retries" times. Else, don't try.
// Returns 0 if everything is okay, < 0 otherwise. Works only for local nodes. 
int COMM_connect_to_job_manager_local(char ip_adr[], int * retries) {
    struct sockaddr_in address;
    int is_connected = 0;
    int retries_left;
    int rank_rcv;
    int msg_ret;
    struct byte_array * ba;
        
    // Verify if retries is valid.
    if(retries != NULL) {
        if(*retries <= 0) {
            error("Retries value not valid in connect_to_job_manager function.");
            return -1;
        }
         
        retries_left = *retries;
    }
    
    //Create socket
    socket_manager = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_manager == -1)
    {
        error("Could not create socket");
        return -2;
    }
     
    address.sin_addr.s_addr = inet_addr(ip_adr);
    address.sin_family = AF_INET;
    address.sin_port = htons( PORT_MANAGER );
 
    //Connect to remote server
    while(is_connected == 0) {
        if(retries != NULL) {
            if(retries_left == 0) {
                error("Failed to connect to Job Manager, no retries left.");
                return -3;
            }
            retries_left --;
        }
        
        if (connect(socket_manager , (struct sockaddr *)&address , sizeof(address)) < 0) {
            error("Could not connect to the Job Manager. Trying again. \n");
            sleep(1);
        }
        else {
            debug("Connected Successfully to the Job Manager\n");
            rank_rcv = COMM_read_int(socket_manager);
            if(rank_rcv < 0) {
                error("Problem getting the rank id. Disconnected from Job Manager.");
                close(socket_manager);
                sleep(1);
            }
            else { 
                debug("Successfully received a rank id from Job Manager.");

                if(received_one==0) {
                    COMM_my_rank = rank_rcv;
                    is_connected = 1;
                }
                else {
                    ba = (struct byte_array *) malloc (sizeof(struct byte_array));
                    byte_array_init(ba, 16);
                    byte_array_pack64(ba, rank_rcv);
                    byte_array_pack64(ba, COMM_my_rank);
                    
                    if(COMM_send_message(ba, MSG_SET_TASK_MANAGER_ID, socket_manager) == 0) {
                        debug("Successfully sent request to change ID.");
                        is_connected = 1;
                    }
                    else {
                        error("Problem sending request to change ID.");
                        close(socket_manager);
                        sleep(1);
                    }
                    
                    byte_array_free(ba);
                    free(ba);
                }
                
            }
        }
    }

    return 0;
}

// Connect to VM task manager with address provided by job manager. 
// Returns 0 if success and < 0 otherwise. 
int COMM_connect_to_vm_task_manager(int * retries, struct byte_array * ba) {
    int c_port;
    char * token;
    struct sockaddr_in node_address;
    int socket_node;
    int is_connected = 0;
    int retries_left;
    int id_send; 
    int i;

    // Non-Blocking socket.
    struct sockaddr_in addr; 
    long arg; 
    fd_set myset; 
    struct timeval tv; 
    int valopt; 
    socklen_t lon; 
    
    // Verify if retries is valid.
    if(retries != NULL) {
        if(*retries <= 0) {
            error("Retries value not valid in connect_to_vm_task_manager function.");
            return -1;
        }
         
        retries_left = *retries;
    }
    
    // Get the ip and port values.
    token = strtok((char *) ba->ptr, "|\0");
    node_address.sin_addr.s_addr = inet_addr(token);
    node_address.sin_family = AF_INET;
    
    token = strtok(NULL, "|\0");
    c_port = atoi(token);
    //c_port = atoi(strtok(token, "|"));
    node_address.sin_port = htons(c_port);

    //Create socket
    socket_node = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_node == -1) {
        error("Could not create socket");
        return -2;
    }

    // Set, for now, as a non-blocking socket
    if((arg = fcntl(socket_node, F_GETFL, NULL)) < 0) { 
        error("Error in fcntl(soc, F_GETFL,NULL).");
        return -4;
    } 
    arg |= O_NONBLOCK; 
    if( fcntl(socket_node, F_SETFL, arg) < 0) { 
        error("Error in fcntl(soc, F_SETFL,NULL).");
        return -5;
    } 
    
    //Connect to remote server
    while(is_connected == 0 ) {
        if(retries != NULL) {
            if(retries_left == 0) {
                error("Failed to connect to Vm Task Manager, no retries left.");
                return -3;
            }
            retries_left --;
        }
        
        // Using non-blocking socket, try to connect.
        if (connect(socket_node, (struct sockaddr *)&node_address, sizeof(node_address)) < 0) { 
            if(errno == EINPROGRESS) {
                // Use select with timeout
                tv.tv_sec = 5;
                tv.tv_usec = 0;
                FD_ZERO(&myset); 
                FD_SET(socket_node, &myset); 
                if (select(socket_node+1, NULL, &myset, NULL, &tv) > 0) { 
                    lon = sizeof(int); 
                    getsockopt(socket_node, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon); 
                    // valopt should be zero if connection was successful.
                    if (!valopt) { 
                        is_connected = 1;
                    } 
                } 
            }
        }
        // Instant connection? Shouldn't occur.
        else { 
            is_connected = 1; 
        }

        if(is_connected == 0) {
            close(socket_node);
            if(retries_left == 0) {
                error("Could not connect to the Vm Task Manager. No retries left.\n");
            }
            if(retries_left != 0) {
                error("Could not connect to the Vm Task Manager. Trying again.\n");
                sleep(1); 
            }
        }
        else {
            debug("Connected Successfully to the VM Task Manager\n");
        }
    }   

    if(is_connected == 1) {
        // Set socket to blocking mode.
        arg = fcntl(socket_node, F_GETFL, NULL); 
        arg &= (~O_NONBLOCK); 
        fcntl(socket_node, F_SETFL, arg);
        
        // Add new connection to the list, assign rank id and send to the client (if the manager).
        if(COMM_my_rank == (int)JOB_MANAGER) {
            COMM_ip_list = LIST_add_ip_address(COMM_ip_list, inet_ntoa(node_address.sin_addr), ntohs(node_address.sin_port), socket_node, VM_TASK_MANAGER, &id_send); 
            byte_array_clear(ba);
            byte_array_pack64(ba, id_send);
            COMM_send_message(ba,MSG_SET_JOB_MANAGER,socket_node);
        }
        else if(COMM_my_rank == (int) COMMITTER) {
            COMM_send_message(NULL,MSG_SET_COMMITTER,socket_node);
        }

        //add new socket to array of sockets
        for (i = 0; i < max_clients; i++) 
        {
              //if position is empty
            if( COMM_client_socket[i] == 0 )
            {
                COMM_client_socket[i] = socket_node;
                
                if(COMM_my_rank==0) {
                    info("Adding to list of sockets as %d\n" , i);
                    COMM_LIST_print_ip_list(); 
                }

                break;
            }
        }
        COMM_alive++;
    }
    return 0;
}

// Connect with committer using any task manager.
int COMM_connect_to_committer(int * retries) {
    if ((type == TASK_MANAGER) || (type == COMMITTER) || (type == MONITOR)) {
        COMM_connect_to_committer_local(retries);
    }
    else if(type==VM_TASK_MANAGER) {
        COMM_vm_connection(0,1); 
    }
}

// Get committer with the job manager and establish a connection. Local (non-VM) version.
int COMM_connect_to_committer_local(int * retries) {
    int is_connected = 0;
    int retries_left;
    
    if(COMM_get_committer(retries)<0) {
        error("Job Manager doesn't know who is the committer yet.");
        return -4;
    }

    // Verify if retries is valid.
    if(retries != NULL) {
        if(*retries <= 0) {
            error("Retries value not valid in connect_to_committer function.");
            return -1;
        }
         
        retries_left = *retries;
    }
    
    //Create socket
    socket_committer = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_committer == -1) {
        error("Could not create socket");
        return -2;
    }

    //Connect to remote server
    while(is_connected == 0 ) {
        if(retries != NULL) {
            if(retries_left == 0) {
                error("Failed to connect to Committer, no retries left.");
                return -3;
            }
            retries_left --;
        }
        
        if (connect(socket_committer , (struct sockaddr *)&COMM_addr_committer , sizeof(COMM_addr_committer)) < 0) { 
            error("Could not connect to the Committer. Trying again.\n");
            sleep(1); 
        }
        else { 
            debug("Connected Successfully to the Committer\n");
            is_connected = 1; 
        }
    }    
}

// Request and receive number of alive members.
int COMM_get_alive() {
    if((COMM_my_rank==(int)JOB_MANAGER)||(COMM_my_rank==(int)COMMITTER)) {
        return COMM_alive;
    }
   
    COMM_send_message(NULL, MSG_GET_ALIVE, socket_manager);
    return COMM_read_int(socket_manager);
}

// Increment the run_num variable
void COMM_increment_run_num() {
    if(COMM_my_rank==0) {
        COMM_run_num++;
    }
}

// Request and return the run_num variable from the job_manager
int COMM_get_run_num() {
     if(COMM_my_rank==(int)JOB_MANAGER) {
        return COMM_run_num;
     }

    COMM_send_message(NULL, MSG_GET_RUNNUM, socket_manager);
    return COMM_read_int(socket_manager); 
}

// Request and return the committer from the job_manager
int COMM_get_committer(int * retries) {
    int retries_left = 0;
    
    if(retries != NULL) {
        retries_left = *retries;
    }
    
    if(COMM_my_rank==0) {
        return 0;
    }
    
    while(COMM_request_committer() != 1) {
        info("Committer not found yet! \n");
        if(retries != NULL) {
            retries_left = retries_left - 1;
            if(retries_left == 0) {
                return -1;
            }
        }
        sleep(1);
    }
    return 0;
}

// Do the request for the committer value. May receive a failed message.
int COMM_request_committer() {
    int c_port;
    char * token;

    enum message_type type;                                             // Type of received message.
    struct byte_array * ba = (struct byte_array *) malloc(sizeof(struct byte_array));
    byte_array_init(ba, 0);
    
    COMM_send_message(NULL, MSG_GET_COMMITTER, socket_manager);
    COMM_read_message(ba, &type, socket_manager);
    
    if (ba->len <= 2) {                                                 // Job manager doesn't know yet
        byte_array_free(ba);
        free(ba);
        return -1;
    } 
    else {
        // otherwise, get the ip and port values.
        token = strtok((char *) ba->ptr, "|\0");
        COMM_addr_committer.sin_addr.s_addr = inet_addr(token);
        COMM_addr_committer.sin_family = AF_INET;
        
        token = strtok(NULL, "|\0");
        c_port = atoi(token);
        //c_port = atoi(strtok(token, "|"));
        COMM_addr_committer.sin_port = htons(c_port);

        byte_array_free(ba);
        free(ba);
        return 1;
    }

    byte_array_free(ba);
    free(ba);
    return -1;
}

// Rank id is now in a variable
int COMM_get_rank_id() {
    return COMM_my_rank;
}

// Setup committer, to receive incoming connections.
int COMM_setup_committer_network() {
    int i, flags, opt = 1;
    struct sockaddr_in address;
    
    COMM_send_message(NULL, MSG_SET_COMMITTER, socket_manager);      // set as a committer with manager
    debug("Set committer successfully\n");
    
    for (i = 0; i < max_clients; i++) {
        COMM_client_socket[i] = 0;
    }

    COMM_client_socket[0] = socket_manager;
      
    //  create a master socket
    if( (COMM_master_socket= socket(AF_INET , SOCK_STREAM , 0)) == 0) {
        error("socket failed\n");
        return -1;
    }
  
    //set master socket to allow multiple connections
    if( setsockopt(COMM_master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        error("error in setsockopt function\n");
        return -1;
    }

    /* Set socket to non-blocking */ 
    if ((flags = fcntl(COMM_master_socket, F_GETFL, 0)) < 0) 
    { 
        error("Error getting the flags of master socket.");
    } 

    if (fcntl(COMM_master_socket, F_SETFL, flags | O_NONBLOCK) < 0) 
    { 
        error("Error setting new flags to the mater socket.\n");
    } 
  
    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT_COMMITTER );
      
    //bind the socket to localhost port PORT_COMMITTER
    if (bind(COMM_master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        error("bind in setup_committer failed\n");
        return -1;
    }
    debug("Listener on port %d \n", PORT_COMMITTER);
     
    //try to specify maximum of max_pending_connections pending connections for the master socket
    if (listen(COMM_master_socket, max_pending_connections) < 0) {
        return -1;
    }
    
    // Start list of variables
    COMM_addrlen = sizeof(address);   
    COMM_loop_b = 0;
    COMM_my_rank = (int) COMMITTER;
    COMM_alive = 1;                         // The committer is the only alive for now.
    COMM_committer_index = -1;              // Don't check for committer in wait_request.
    
    return 0;
}

// VM_Client setup client. 
int COMM_setup_vm_network() {
    int i,flags, opt = 1;
    struct sockaddr_in address;
      
    for (i = 0; i < max_clients; i++) {
        COMM_client_socket[i] = 0;
    }
      
    //  create a master socket
    if( (COMM_master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) {
        error("socket creation in setup_job_manager failed\n");
        return -1;
    }
  
    //set master socket to allow multiple connections
    if( setsockopt(COMM_master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        error("error in setsockopt function\n");
        return -1;
    }
  
    /* Set socket to non-blocking */ 
    if ((flags = fcntl(COMM_master_socket, F_GETFL, 0)) < 0) 
    { 
        error("Error getting the flags of master socket.");
    } 

    if (fcntl(COMM_master_socket, F_SETFL, flags | O_NONBLOCK) < 0) 
    { 
        error("Error setting new flags to the mater socket.\n");
    } 

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT_VM );
    
    //bind the socket to localhost port PORT_VM
    if (bind(COMM_master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        error("Bind in setup_vm_network failed\n");
        return -1;
    }
    debug("Listener on port %d \n", PORT_VM);
     
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(COMM_master_socket, max_pending_connections) < 0) {
        return -1;
    }
    
    // Start list of variables
    COMM_addrlen = sizeof(address);                                      
    COMM_my_rank = (int) VM_TASK_MANAGER;
    COMM_loop_b = 0;
    COMM_committer_index = -1;
    
    return 0;
}

// Setup the job_manager network, to accept connections and other variables
int COMM_setup_job_manager_network() {
    int i,flags, opt = 1;
    struct sockaddr_in address;
    
    COMM_ip_list = NULL;
    COMM_addr_committer.sin_addr.s_addr = 0;
    COMM_addr_committer.sin_port = 0;
      
    for (i = 0; i < max_clients; i++) {
        COMM_client_socket[i] = 0;
    }
      
    //  create a master socket
    if( (COMM_master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) {
        error("socket creation in setup_job_manager failed\n");
        return -1;
    }
  
    //set master socket to allow multiple connections
    if( setsockopt(COMM_master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        error("error in setsockopt function\n");
        return -1;
    }
  
    /* Set socket to non-blocking */ 
    if ((flags = fcntl(COMM_master_socket, F_GETFL, 0)) < 0) 
    { 
        error("Error getting the flags of master socket.");
    } 

    if (fcntl(COMM_master_socket, F_SETFL, flags | O_NONBLOCK) < 0) 
    { 
        error("Error setting new flags to the mater socket.\n");
    } 

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT_MANAGER );
    
    //bind the socket to localhost port PORT_MANAGER
    if (bind(COMM_master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        error("Bind in setup_job_manager failed\n");
        return -1;
    }
    debug("Listener on port %d \n", PORT_MANAGER);
     
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(COMM_master_socket, max_pending_connections) < 0) {
        return -1;
    }
    
    // Start list of variables
    COMM_addrlen = sizeof(address);                                      
    COMM_ip_list = LIST_add_ip_address(COMM_ip_list, "127.0.0.1", PORT_MANAGER, -1, (int) JOB_MANAGER, NULL);
    COMM_my_rank = (int)JOB_MANAGER;
    COMM_run_num = 0;
    COMM_alive = 1;
    COMM_loop_b = 0;
    COMM_committer_index = -1;
    
    return 0;
}

// Job Manager function, returns _ in origin socket variable _ the socket that have a request to do, or -1 if doesn't have I/O
// Returns 0 if ok, -1 if got out the select for no reason and -2 if ba is null. 
int COMM_wait_request(enum message_type * type, int * origin_socket, struct byte_array * ba, struct timeval * tv) {
    int i, activity, valid_request=0;               // Indicates if a valid request was made. 
    int max_sd;                                     // Auxiliary value to select.
    int socket_found=0;                             // Indicates if the socket was found.
    int sd;                                         // last socket used
    fd_set readfds;                                 // set of socket descriptors
   
    if (ba==NULL) {
        error("Null byte array passed to wait_request function.");
        *type = MSG_EMPTY;
        return -2;
    }
    
    debug("Waiting for request \n");
     
    while(valid_request == 0) {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(COMM_master_socket, &readfds);
        max_sd = COMM_master_socket;
         
        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++) 
        {
            sd = COMM_client_socket[i];             //socket descriptor
                     
            if(sd > 0) {                            //if valid socket descriptor then add to read list
                FD_SET( sd , &readfds);
            }
             
            if(sd > max_sd) {                       //highest file descriptor number, need it for the select function
                max_sd = sd;
            }
        }
            
        // wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , tv);

        // check for timeout
        if((tv!= NULL) && (activity == 0)) {
            debug("Timeout occurred! Returning to select.");
            *origin_socket = -1;
            *type = MSG_EMPTY;
            return 0;
        }

	debug("Received some request from select().");       
 
        // wait until get an activity without EINTR error.
        if ((activity < 0)&&(errno == EINTR)) { 
            error("Select error (EINTR), repeating select loop.");
            continue;
        }
        
        // after getting a valid select, go on.
        valid_request = 1;
    }
        
    //If something happened on the master socket , then its an incoming connection
    if (FD_ISSET(COMM_master_socket, &readfds)) {
        *type = MSG_NEW_CONNECTION;
        return 0;
    }

    // Check the committer first, for performance reasons.
    if (COMM_committer_index > 0) {
        sd = COMM_client_socket [COMM_committer_index];
        socket_found = 0;
        
        if(FD_ISSET(sd, &readfds)) {
            socket_found = 1;
        }
    }
    
    // I/O Operation
    while (socket_found == 0) {                     // Find the socket responsible.
        COMM_loop_b = (COMM_loop_b+1)%max_clients;  // update loop_b to be fair with workers.
        sd = COMM_client_socket[COMM_loop_b];
      
        if (FD_ISSET(sd , &readfds))                // Test the socket. 
        {
            socket_found=1;
        }
    }
    
    COMM_read_message(ba,type,sd);                  // Receive the request.
   
    if ((*(type)) == MSG_EMPTY)                     // Someone is closing
    {
        COMM_client_socket[COMM_loop_b] = 0;
        * type = MSG_CLOSE_CONNECTION;
        byte_array_clear(ba);
        byte_array_free(ba);
         _byte_array_pack64(ba, (uint64_t) sd);
         return 0;
    }
      
    else                                            // Other request
    {
        *origin_socket = sd;
        return 0;
    }
      
    return -1;
}

// Send the committer due a request
void COMM_send_committer(int sock) {
    struct byte_array * ba = (struct byte_array *) malloc (sizeof(struct byte_array));
    size_t n;
    char no_answer[2] = "\n\0";
    char * v; 
    
    //COMM_send_message(ba, MSG_STRING, sock);
    
    if ((strcmp((const char *) inet_ntoa(COMM_addr_committer.sin_addr), "0.0.0.0") == 0) && (ntohs(COMM_addr_committer.sin_port) == 0)) {
        v = no_answer;
        n = (size_t) strlen(no_answer);
    
        byte_array_init(ba, n);
        byte_array_pack8v(ba, v, n);
        COMM_send_message(ba, MSG_STRING, sock);
    }
    else {
        char committer_send[26] = "";
        strcat(committer_send, inet_ntoa(COMM_addr_committer.sin_addr));

        char port_str[6]; // used to represent a short int 
        sprintf(port_str, "%d", (int) ntohs(COMM_addr_committer.sin_port));

        strcat(committer_send, "|");
        strcat(committer_send, port_str);
        strcat(committer_send, "\0");
       
        v = committer_send;
        n = (size_t) (strlen(committer_send)+1);
        
        byte_array_init(ba, n);
        byte_array_pack8v(ba, v, n);
        
        // message format: ip|porta
        COMM_send_message(ba, MSG_STRING, sock);
    }

    byte_array_free(ba);
    free(ba);
}

// Register the committer, when the committer sets it
int COMM_register_committer(int sock) {
    int old_prt, i;
    
    for (i = 0; i < max_clients; i++) {
        if(COMM_client_socket[i] == sock) {
            COMM_committer_index = i;
            break;
        }
    }
   
    socket_committer = sock;
    getpeername(sock, (struct sockaddr*) &COMM_addr_committer, (socklen_t*) & COMM_addrlen);
    old_prt = ntohs(COMM_addr_committer.sin_port);
    
    COMM_addr_committer.sin_port = htons (PORT_COMMITTER);
    COMM_ip_list = LIST_register_committer(COMM_ip_list , inet_ntoa(COMM_addr_committer.sin_addr), old_prt, ntohs(COMM_addr_committer.sin_port));
    debug("Set committer, ip %s, port %d", inet_ntoa(COMM_addr_committer.sin_addr), htons(COMM_addr_committer.sin_port));

    COMM_LIST_print_ip_list();
    return 0;
}

// Register the monitor when it requests.
int COMM_register_monitor(int sock) {
    struct connected_ip * monitor_node;
    
    monitor_node = LIST_search_socket(COMM_ip_list, sock);
    monitor_node->type = 3; 

    return 0;
}

// Creates a new connection when the job manager or committer sees it.
void COMM_create_new_connection() {
    int i, rcv_socket, id_send;
    struct sockaddr_in address;
  
    if ((rcv_socket = accept(COMM_master_socket, (struct sockaddr *)&address, (socklen_t*)&COMM_addrlen))<0) {
	    error("Problem accepting connection, ERRNO: %s\n", strerror(errno));
            sleep(1);
        return;
    }
  
    //inform user of socket number - used in send and receive commands
    debug("New connection , socket fd is %d , ip is : %s , port : %d \n" , rcv_socket, inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
    
    // Add new connection to the list, assign rank id and send to the client (if the manager).
    if(COMM_my_rank == (int)JOB_MANAGER) {
        COMM_ip_list = LIST_add_ip_address(COMM_ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port), rcv_socket, (int)TASK_MANAGER, &id_send); 
        COMM_send_int(rcv_socket,id_send);
    }

    //add new socket to array of sockets
    for (i = 0; i < max_clients; i++) 
    {
	  //if position is empty
        if( COMM_client_socket[i] == 0 )
        {
            COMM_client_socket[i] = rcv_socket;
            
            if(COMM_my_rank==0) {
                info("Adding to list of sockets as %d\n" , i);
                COMM_LIST_print_ip_list(); 
            }

            break;
        }
    }

    COMM_alive++;
}

// Close the connection for committer and job manager.
void COMM_close_connection(int sock) {
    struct sockaddr_in address;
    enum actor node_role=-1;
    struct connected_ip * node;
    unsigned long node_rt;

    //Somebody disconnected , get his details and print
    getpeername(sock , (struct sockaddr*)&address , (socklen_t*)&COMM_addrlen);
    info("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

    if(COMM_my_rank==(int)JOB_MANAGER) {
        // Search for provided node using the socket.
        node = LIST_search_socket(COMM_ip_list, sock);

        if(node != NULL) {
            node_role = (enum actor) node->type; 
            node_rt = node->rcv_tasks; 
        }
       
        //If it's the monitor or a node that haven't worked, remove from the list. Mark as disconnected otherwise.
        if((node_role == MONITOR)||(node_rt == 0)) {
            COMM_ip_list = LIST_remove_ip_address(COMM_ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        }
        else {
            LIST_disconnect_ip_adress(COMM_ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        }
    }
    
    //Close the socket 
    close( sock );
    COMM_alive--;
}

// List all ips an the info of each one from the ip_list
void COMM_LIST_print_ip_list() {
    LIST_print_all_ip_ordered(COMM_ip_list);        
}

// Responsible for closing all the sockets. To be used just before exiting.
// Also free the ip_list memory if the job manager.
void COMM_close_all_connections() {
    int i, sd;

    if(COMM_my_rank==(int)JOB_MANAGER) {
        LIST_free_data(COMM_ip_list);
    }

    if(COMM_my_rank < 2) {
        for(i=0; i<max_clients; i++) {
                sd = COMM_client_socket[i];             //socket descriptor
            if(sd > 0) {
                close(sd);
            }
        }
    }

    if(COMM_master_socket > 0 ) {
        close(COMM_master_socket);
    }

    if(socket_manager > 0) {
        close (socket_manager);
    }

    if(socket_committer > 0) {
        close (socket_committer);
    }
}

// Get actor type, must be defined using the function below. 
enum actor COMM_get_actor_type(){
    return type;
}

// Set actor type.
void COMM_set_actor_type(char * value) {
    type = atoi(value);
}
