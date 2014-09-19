/**
Based on Silver Moon (m00n.silv3r@gmail.com) example and being modeled to SPITZ.

Alexandre L. B. F.      
*/

#include "comm.h"

/* Global Variables */
char buffer[1025], * lib_path;      	// data buffer of 1K
struct sockaddr_in addr_committer;      // address of committer node
int socket_manager, socket_committer;   // Important socket values
int my_rank, run_num;                   // Rank and run_num variables
int loop_b;                             // Used to balance the requests

/* Job Manager only */
int master_socket, addrlen, client_socket[max_clients], sd;
int alive;                              // number of connected members
fd_set readfds;                         // set of socket descriptors
struct connected_ip * ip_list;          // list of ips connected to the manager

/* Functions Exclusive to this implementation */
// Worker
int COMM_telnet_client(int argc, char *argv[]);
int COMM_request_committer();

// Send message, used to make request between processes 
void COMM_send_message(struct byte_array *ba, int type, int dest_socket) {
    struct byte_array _ba;
    if (!ba) {
      _ba.ptr = NULL;
      _ba.len = 0;
      ba = &_ba;
    }

    COMM_send_int(dest_socket, type);   
    COMM_send_bytes(dest_socket, ba->ptr, (int)ba->len);
}

// Receive the message responsible for the communication between processes
struct byte_array * COMM_read_message(struct byte_array *ba, enum message_type *type, int rcv_socket) {
    *type = (enum message_type) COMM_read_int(rcv_socket);
    if((*((int *)type)) == -1)
        return ba;

    if (ba==NULL) {
        ba = (struct byte_array *) malloc (sizeof(struct byte_array));
        ba->ptr = NULL;
        ba->len = 0;
    } else {
    
    COMM_read_bytes(rcv_socket, NULL, ba);
    
    return ba;
    }
}

// Send N bytes pointer by bytes pointer.
int COMM_send_bytes(int sock, void * bytes, int size) {
    char size_c[20];
    int return_value;
    
    sprintf(size_c, "%d", size);
    strcat(size_c, "|\n");
    return_value = (int) send(sock, size_c, (strlen(size_c)-1), 0); // don't send '\n'
    if(size>0)
    	return_value = (int) send(sock, bytes, size, 0);

    return 0;    
}

// Read unknown type and size from socket.
void COMM_read_bytes(int sock, int * size, struct byte_array * ba) {
    char message_size[20], received_char='0';
    int total_rcv, offset=0, msg_size;

    while(received_char!='|') {
        total_rcv = read(sock, &received_char, 1);

        if(total_rcv == 0) {
            *size = -1;
            return;
        }
        
        if(received_char!='|')
            message_size[offset]=received_char;
        else
            message_size[offset]='\n';

        offset++;
    }

    msg_size = atoi(message_size);
    offset = 0;
    byte_array_resize(ba, msg_size);
    ba->len = msg_size, ba->iptr = ba->ptr;
    
    if(size != NULL) {
    	* size = msg_size;    
    }
        
    while(offset < msg_size) {		// if zero, doesn't come in
        total_rcv = read(sock, (ba->ptr+offset), (msg_size-offset));
        offset+=total_rcv;
    }
}

// Send char array using send bytes function
int COMM_send_char_array(int sock, char * array) {
    return COMM_send_bytes(sock, (void *) array, strlen(array)+1);      // send the \n
}

// Read char array using read bytes function
char * COMM_read_char_array(int sock) {
    struct byte_array ba;
    byte_array_init(&ba, 0);
    COMM_read_bytes(sock, NULL, &ba);
    return (char *) ba.ptr; 
}

// Send int using send bytes function
int COMM_send_int(int sock, int value) {
    return COMM_send_bytes(sock, (void *) &value, sizeof(int));
}

// Read int using read bytes function
int COMM_read_int(int sock) {
    struct byte_array ba;
    int * result, size;
    
    byte_array_init(&ba, 0);
    COMM_read_bytes(sock, &size, &ba);

    if(size == -1) {
        return -1; 
    }
    
    result = (int *) ba.ptr;
    return * result;
}

// Establishes a connection with the job manager
void COMM_connect_to_job_manager(char ip_adr[]) {
    struct sockaddr_in address;
    int isConnected = 0;
    
    //Create socket
    socket_manager = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_manager == -1)
    {
        error("Could not create socket");
    }
     
    address.sin_addr.s_addr = inet_addr(ip_adr);
    address.sin_family = AF_INET;
    address.sin_port = htons( 8888 );
 
    //Connect to remote server
    while(isConnected == 0 ) {
        if (connect(socket_manager , (struct sockaddr *)&address , sizeof(address)) < 0) {
            error("Could not connect to the Job Manager. Trying again. \n");
            sleep(1);
        }
        else {
            debug("Connected Successfully to the Job Manager\n");
            my_rank = COMM_read_int(socket_manager);
            isConnected = 1; 
        }
    }
}

// Get committer with the job manager and establish a connection.
void COMM_connect_to_committer() {
    COMM_get_committer();
    int isConnected = 0;
    
    //Create socket
    socket_committer = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_committer == -1)
        error("Could not create socket");
     
    //Connect to remote server
    while(isConnected == 0 ) {
        if (connect(socket_committer , (struct sockaddr *)&addr_committer , sizeof(addr_committer)) < 0) { 
            error("Could not connect to the Committer. Trying again.\n");
            sleep(1); 
        }
        else { 
            debug("Connected Successfully to the Committer\n");
            isConnected = 1; 
        }
    }    
}

// Send number of alive members.
void COMM_send_alive(int origin_socket) {
    COMM_send_int(origin_socket, alive);
}

// Request and receive number of alive members.
int COMM_get_alive() {
    if((my_rank==(int)JOB_MANAGER)||(my_rank==(int)COMMITTER))
        return alive;
   
    COMM_send_message(NULL, MSG_GET_ALIVE, socket_manager);
    return COMM_read_int(socket_manager);
}

// Set the path variable
void COMM_set_path(char * file_path) {
    lib_path = strcpy(malloc(sizeof(char)*strlen(file_path)), file_path);
}

// Request and receive the path from the manager
char * COMM_get_path() {
    COMM_send_message(NULL, MSG_GET_PATH, socket_manager);
    return COMM_read_char_array(socket_manager);
} 

// Send the path due a request
void COMM_send_path() {
    if(lib_path != NULL)
	    COMM_send_char_array(sd, lib_path);
	else
	    COMM_send_char_array(sd, "NULL\n");
}

// Increment the run_num variable
void COMM_increment_run_num() {
    if(my_rank==0)
        run_num++;
}

// Request and return the run_num variable from the job_manager
int COMM_get_run_num() {
     if(my_rank==0)
        return run_num;

    COMM_send_message(NULL, MSG_GET_RUNNUM, socket_manager);
    return COMM_read_int(socket_manager); 
}

// Send request to be committer to the job_manager
void COMM_set_committer() {
    COMM_send_message(NULL, MSG_SET_COMMITTER, socket_manager);      // set as a committer with manager
    debug("Set committer successfully\n");
}

// Request and return the committer from the job_manager
void COMM_get_committer() {
    if(my_rank==0)
        return;
    
    while(COMM_request_committer() != 1) {
        info("Committer not found yet! \n");
        sleep(1);
    }
}

// Do the request for the committer value. May receive a failed message.
int COMM_request_committer() {
    int c_port;
    char * token, * rcv_msg;

    COMM_send_message(NULL, MSG_GET_COMMITTER, socket_manager);
    
    if ((rcv_msg = COMM_read_char_array(socket_manager)) != NULL) {     //receive a reply from the server
        if (strlen(rcv_msg) <= 1) {                                     // Job manager doesn't know yet
            return -1;
        } else {
            // otherwise, get the ip and port values.
            token = strtok(rcv_msg, "|\0");
            addr_committer.sin_addr.s_addr = inet_addr(token);
            addr_committer.sin_family = AF_INET;
            
            token = strtok(NULL, "|\0");
            c_port = atoi(token);
            //c_port = atoi(strtok(token, "|"));
            addr_committer.sin_port = htons(c_port);
            return 1;
        }
    }
    return -1;
}

// Rank id is now in a variable
int COMM_get_rank_id() {
    return my_rank;
}

// Simple telnet client, for debug reasons
int COMM_telnet_client(int argc, char *argv[]) {
    COMM_connect_to_job_manager("127.0.0.1");
     
    //keep communicating with server
    while(1)
    {
        printf("Enter message : ");
        scanf("%s" , buffer);
        //Send some data
        if( send(socket_manager , buffer , strlen(buffer) , 0) < 0)
        {
            error("Send failed");
            return 1;
        }
        //Receive a reply from the server
        if( recv(socket_manager , buffer , 1024 , 0) < 0)
        {
            error("recv failed");
            break;
        }
         
        printf("Server reply :");
        puts(buffer);
    }
     
    close(socket_manager);
    return 0;
}

// Setup committer, to receive incoming connections.
int COMM_setup_committer() {
    int i, flags, opt = 1;
    struct sockaddr_in address;
    
    COMM_set_committer();
    
    for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }
      
    //  create a master socket
    if( (master_socket= socket(AF_INET , SOCK_STREAM , 0)) == 0) {
        error("socket failed\n");
        return -1;
    }
  
    //set master socket to allow multiple connections
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        error("error in setsockopt function\n");
        return -1;
    }

    /* Set socket to non-blocking */ 
    if ((flags = fcntl(master_socket, F_GETFL, 0)) < 0) 
    { 
        error("Error getting the flags of master socket.");
    } 

    if (fcntl(master_socket, F_SETFL, flags | O_NONBLOCK) < 0) 
    { 
        error("Error setting new flags to the mater socket.\n");
    } 
  
    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT_COMMITTER );
      
    //bind the socket to localhost port PORT_COMMITTER
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        error("bind in setup_committer failed\n");
        return -1;
    }
    debug("Listener on port %d \n", PORT_COMMITTER);
     
    //try to specify maximum of max_pending_connections pending connections for the master socket
    if (listen(master_socket, max_pending_connections) < 0) {
        return -1;
    }
    
    // Start list of variables
    addrlen = sizeof(address);                                      
    lib_path = NULL;
    loop_b = 0;
    my_rank = (int) COMMITTER;
    alive = 1;
    
    return 0;
}

// Setup the job_manager network, to accept connections and other variables
int COMM_setup_job_manager_network() {
    int i,flags, opt = 1;
    struct sockaddr_in address;
    
    ip_list = NULL;
    addr_committer.sin_addr.s_addr = 0;
    addr_committer.sin_port = 0;
      
    for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }
      
    //  create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) {
        error("socket creation in setup_job_manager failed\n");
        return -1;
    }
  
    //set master socket to allow multiple connections
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        error("error in setsockopt function\n");
        return -1;
    }
  
    /* Set socket to non-blocking */ 
    if ((flags = fcntl(master_socket, F_GETFL, 0)) < 0) 
    { 
        error("Error getting the flags of master socket.");
    } 

    if (fcntl(master_socket, F_SETFL, flags | O_NONBLOCK) < 0) 
    { 
        error("Error setting new flags to the mater socket.\n");
    } 

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT_MANAGER );
    
    //bind the socket to localhost port PORT_MANAGER
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        error("Bind in setup_job_manager failed\n");
        return -1;
    }
    debug("Listener on port %d \n", PORT_MANAGER);
     
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, max_pending_connections) < 0) {
        return -1;
    }
    
    // Start list of variables
    addrlen = sizeof(address);                                      
    ip_list = LIST_add_ip_address(ip_list, "127.0.0.1", PORT_MANAGER, -1, NULL);
    my_rank = (int)JOB_MANAGER;
    run_num = 0;
    lib_path = NULL;
    alive = 1;
    loop_b = 0;
    
    return 0;
}

// Job Manager function, returns the socket that have a request to do, or -1 if doesn't have I/O
struct byte_array * COMM_wait_request(enum message_type * type, int * origin_socket, struct byte_array * ba) {
    int i, activity, valid_request=0; 
    int max_sd;                                                     // Auxiliar value to select.
    
    debug("Waiting for request \n");
     
    while(valid_request == 0) {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;
         
        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++) 
        {
            sd = client_socket[i];          //socket descriptor
                     
            if(sd > 0) {                    //if valid socket descriptor then add to read list
                FD_SET( sd , &readfds);
            }
             
            if(sd > max_sd) {               //highest file descriptor number, need it for the select function
                max_sd = sd;
            }
        }
            
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0)&&(errno == EINTR)) { 
            error("Select error (EINTR), repeating select loop.");
            continue;
        }
        
        valid_request = 1;
        
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) {
            *type = MSG_NEW_CONNECTION;
            return ba;
        }
          
        // I/O Operation
        for (i = 0; i < max_clients; i++) 
        {
            sd = client_socket[loop_b];
              
            if (FD_ISSET(sd , &readfds)) 
            {
                ba = COMM_read_message(ba,type,sd);
               
                if ((*((int *)type)) == -1)      // Someone is closing
                {
                    client_socket[loop_b] = 0;
                    * type = MSG_CLOSE_CONNECTION;
                    byte_array_clear(ba);
                     _byte_array_pack64(ba, (uint64_t) sd);
                    return ba;
                }
                  
                else                            // Other request
                {
                    *origin_socket = sd;
                    return ba;
                }
            }
        
            loop_b = (loop_b+1)%max_clients;
        }
    }
      
    return ba;
}

// Send the committer due a request
void COMM_send_committer() {
    char no_answer[2] = "\0\n";
    
    if ((strcmp((const char *) inet_ntoa(addr_committer.sin_addr), "0.0.0.0") == 0) && (ntohs(addr_committer.sin_port) == 0))
        COMM_send_char_array(sd, no_answer);
    else {
        char committer_send[25] = "";
        strcat(committer_send, inet_ntoa(addr_committer.sin_addr));

        char port_str[6]; // used to represent a short int 
        sprintf(port_str, "%d", (int) ntohs(addr_committer.sin_port));

        strcat(committer_send, "|");
        strcat(committer_send, port_str);

        // message format: ip|porta
        COMM_send_char_array(sd, committer_send);
    }
}

// Register the committer, when the committer sets it
int COMM_register_committer() {
    int old_prt;
    
    getpeername(sd, (struct sockaddr*) &addr_committer, (socklen_t*) & addrlen);
    old_prt = ntohs(addr_committer.sin_port);
    
    addr_committer.sin_port = htons (PORT_COMMITTER);
    ip_list = LIST_register_committer(ip_list , inet_ntoa(addr_committer.sin_addr), old_prt, ntohs(addr_committer.sin_port));
    debug("Set committer, ip %s, port %d", inet_ntoa(addr_committer.sin_addr), htons(addr_committer.sin_port));

    COMM_LIST_print_ip_list();
    return 0;
}

// Creates a new connection when the job manager or committer sees it.
void COMM_create_new_connection() {
    int i, rcv_socket, id_send;
    struct sockaddr_in address;
  
    if ((rcv_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
	    error("Problem accepting connection, ERRNO: %s\n", strerror(errno));
        return;
    }
  
    //inform user of socket number - used in send and receive commands
    debug("New connection , socket fd is %d , ip is : %s , port : %d \n" , rcv_socket, inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
    
    // Add new connection to the list, assign rank id and send to the client (if the manager).
    if(my_rank == (int)JOB_MANAGER) {
        ip_list = LIST_add_ip_address(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port), rcv_socket, &id_send); 
        COMM_send_int(rcv_socket,id_send);
    }

    //add new socket to array of sockets
    for (i = 0; i < max_clients; i++) 
    {
	  //if position is empty
        if( client_socket[i] == 0 )
        {
            client_socket[i] = rcv_socket;
            
            if(my_rank==0) {
                info("Adding to list of sockets as %d\n" , i);
                COMM_LIST_print_ip_list(); 
            }

            break;
        }
    }

    alive++;
}

// Close the connection for committer and job manager.
void COMM_close_connection(int sock) {
    struct sockaddr_in address;
    
    //Somebody disconnected , get his details and print
    getpeername(sock , (struct sockaddr*)&address , (socklen_t*)&addrlen);
    info("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

    if(my_rank==(int)JOB_MANAGER) {
        ip_list = LIST_remove_ip_address(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    }
    
    //Close the socket 
    close( sock );
    alive--;
}

// List all ips an the info of each one from the ip_list
void COMM_LIST_print_ip_list() {
    LIST_print_all_ip(ip_list);        
}

// Disconnect from job manager node.
void COMM_disconnect_from_job_manager(){
    close(socket_manager);
}

// Disconnect from committer node
void COMM_disconnect_from_committer() {
    if(my_rank==(int)JOB_MANAGER)
        LIST_free_list(ip_list);
    
    close(socket_committer);
}