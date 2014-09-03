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
int master_socket, addrlen, socket_manager, client_socket[max_clients], sd;
int max_sd, alive;
fd_set readfds; //set of socket descriptors
struct connected_ip * ip_list;

int COMM_get_socket_committer() {
    return socket_committer;
}

int COMM_get_socket_manager() {
    return socket_manager;
}

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

char * COMM_read_char_array(int sock) {
    struct byte_array ba;
    byte_array_init(&ba, 0);
    COMM_read_bytes(sock, NULL, &ba);
    return (char *) ba.ptr; 
}

int COMM_send_int(int sock, int value) {
    return COMM_send_bytes(sock, (void *) &value, sizeof(int));
}

int COMM_read_int(int sock) {
    struct byte_array ba;
    int * result;
    
    byte_array_init(&ba, 0);
    COMM_read_bytes(sock, NULL, &ba);
    
    result = (int *) ba.ptr;
    return * result;
}

void COMM_connect_to_job_manager(char ip_adr[]) {
    struct sockaddr_in address;
    char rcv_rank[10];
    int valread;
    
    //Create socket
    socket_manager = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_manager == -1)
    {
        printf("Could not create socket");
    }
     
    address.sin_addr.s_addr = inet_addr(ip_adr);
    address.sin_family = AF_INET;
    address.sin_port = htons( 8888 );
 
    //Connect to remote server
    if (connect(socket_manager , (struct sockaddr *)&address , sizeof(address)) < 0) {
        printf("Could not connect to the Job Manager.\n");
    }
    else {
        puts("Connected Successfully to the Job Manager\n");
        my_rank = COMM_read_int(socket_manager); 
    }
}

// Get committer with the job manager and estabilish a connection.
void COMM_connect_to_committer() {
    COMM_get_committer();
    
    //Create socket
    socket_committer = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_committer == -1)
        printf("Could not create socket");
     
    //Connect to remote server
    if (connect(socket_committer , (struct sockaddr *)&addr_committer , sizeof(addr_committer)) < 0) 
        printf("Could not connect to the Committer.\n");
    
    else 
        puts("Connected Successfully to the Committer\n");
        
}

void COMM_send_alive(int origin_socket) {
    COMM_send_int(origin_socket, alive);
}

int COMM_get_alive() {
    if(my_rank==0)
        return alive;
   
    COMM_send_message(NULL, MSG_GET_ALIVE, socket_manager);
    return COMM_read_int(socket_manager);
}

void COMM_set_path(char * file_path) {
    lib_path = strcpy(malloc(sizeof(char)*strlen(file_path)), file_path);
}

char * COMM_get_path() {
    COMM_send_message(NULL, MSG_GET_PATH, socket_manager);
    return COMM_read_char_array(socket_manager);
} 

void COMM_send_path() {
    if(lib_path != NULL)
	    COMM_send_char_array(sd, lib_path);
	else
	    COMM_send_char_array(sd, "NULL\n");
}

void COMM_increment_run_num() {
    if(my_rank==0)
        run_num++;
}

int COMM_get_run_num() {
    if(send(socket_manager, "gn", 2, 0) < 0) {
        printf("Get run failed \n");
        return -1;
    }
    return COMM_read_int(socket_manager); 
}

void COMM_set_committer() {
    // sent set committer request
    COMM_send_message(NULL, MSG_SET_COMMITTER, socket_manager);      // set as a committer with manager
    printf("Set committer successfully\n");
}

void COMM_get_committer() {
    while(COMM_request_committer() != 1) {
        printf("Committer not found yet! \n");
        sleep(1);
    }
}

int COMM_request_committer() {
    int c_port;
    char * token, * rcv_msg;
    
    COMM_send_message(NULL, MSG_GET_COMMITTER, socket_manager);
    
    if ((rcv_msg = COMM_read_char_array(socket_manager)) != NULL) {     //receive a reply from the server
        if (strlen(buffer) == 1) {                                      // Job manager doesn't know yet
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

void COMM_set_rank_id(int new_value) {
    my_rank = new_value;
}

// Rank id is now in a variable
int COMM_get_rank_id() {
    return my_rank;
}

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
            puts("Send failed");
            return 1;
        }
        //Receive a reply from the server
        if( recv(socket_manager , buffer , 1024 , 0) < 0)
        {
            puts("recv failed");
            break;
        }
         
        puts("Server reply :");
        puts(buffer);
    }
     
    close(socket_manager);
    return 0;
}

// Setup committer, to receive incoming connections.
int COMM_setup_committer() {
    int i, opt = 1;
    struct sockaddr_in address;
    
    COMM_set_committer();
    
    for (i = 0; i < max_clients; i++)
        client_socket[i] = 0;
      
    //  create a master socket
    if( (socket_committer = socket(AF_INET , SOCK_STREAM , 0)) == 0) {
        printf("socket failed\n");
        return -1;
    }
  
    //set master socket to allow multiple connections
    if( setsockopt(socket_committer, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        printf("setsockopt\n");
        return -1;
    }
  
    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT_COMMITTER );
      
    //bind the socket to localhost port PORT_COMMITTER
    if (bind(socket_committer, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        printf("bind failed\n");
        return -1;
    }
    printf("Listener on port %d \n", PORT_COMMITTER);
     
    //try to specify maximum of max_pending_connections pending connections for the master socket
    if (listen(socket_committer, max_pending_connections) < 0) {
        return -1;
    }
    
    // Start list of variables
    addrlen = sizeof(address);                                      
    lib_path = NULL;
    loop_b = 0;
    
    return 0;
}

int COMM_setup_job_manager_network() {
    int i, opt = 1;
    struct sockaddr_in address;
    
    ip_list = NULL;
    addr_committer.sin_addr.s_addr = 0;
    addr_committer.sin_port = 0;
      
    for (i = 0; i < max_clients; i++) 
        client_socket[i] = 0;
      
    //  create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) {
        printf("socket failed\n");
        return -1;
    }
  
    //set master socket to allow multiple connections
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        printf("setsockopt\n");
        return -1;
    }
  
    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT_MANAGER );
      
    //bind the socket to localhost port PORT_MANAGER
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        printf("bind failed\n");
        return -1;
    }
    printf("Listener on port %d \n", PORT_MANAGER);
     
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, max_pending_connections) < 0) {
        return -1;
    }
    
    // Start list of variables
    addrlen = sizeof(address);                                      
    ip_list = LIST_add_ip_adress(ip_list, "127.0.0.1", PORT_MANAGER, -1);
    my_rank = 0;
    run_num = 0;
    lib_path = NULL;
    alive = 0;
    loop_b = 0;
    
    return 0;
}

// Job Manager function, returns the socket that have a request to do, or -1 if doesn't have I/O
struct byte_array * COMM_wait_request(enum message_type * type, int * origin_socket, struct byte_array * ba) {
    int i, activity;
    
    puts("Waiting for request \n");
     
    //clear the socket set
    FD_ZERO(&readfds);

    //add master socket to set
    FD_SET(master_socket, &readfds);
    max_sd = master_socket;
     
    //add child sockets to set
    for ( i = 0 ; i < max_clients ; i++) 
    {
        sd = client_socket[i];      //socket descriptor
                 
        if(sd > 0)                  //if valid socket descriptor then add to read list
            FD_SET( sd , &readfds);
         
        if(sd > max_sd)             //highest file descriptor number, need it for the select function
            max_sd = sd;
    }

    //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
    activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

    if (activity < 0) 
        printf("select error");
      
    //If something happened on the master socket , then its an incoming connection
    if (FD_ISSET(master_socket, &readfds)) {
        *type = MSG_NEW_CONNECTION;
        return ba;
    }
      
    // I/O Operation
    for (i = 0; i < max_clients; i++) 
    {
        sd = client_socket[loop_b];
          
        if (FD_ISSET( sd , &readfds)) 
        {
            ba = COMM_read_message(ba,type,sd);
           
            if ((*((int *)type)) == -1)      // Someone is closing
            {
                client_socket[i] = 0;
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
      
    return ba;
}

void COMM_send_committer() {
    char no_answer[2] = "0\n";
    
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

int COMM_register_committer() {
    getpeername(sd, (struct sockaddr*) &addr_committer, (socklen_t*) & addrlen);
    addr_committer.sin_port = (in_port_t) PORT_COMMITTER; 
    printf("Set committer, ip %s, port %d", inet_ntoa(addr_committer.sin_addr), ntohs(addr_committer.sin_port));
    return 0;
}

void COMM_create_new_connection() {
    int i, rcv_socket, id_send;
    struct sockaddr_in address;
  
    if ((rcv_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
	    printf("Problem accepting connection");
    }
  
    //inform user of socket number - used in send and receive commands
    printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , rcv_socket, inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
    
    // Add new connection to the list, assign rank id and send to the client (if the manager).
    if(my_rank == 0) {
        ip_list = LIST_add_ip_adress(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port), rcv_socket); 
        id_send = LIST_get_id(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        COMM_send_int(rcv_socket,id_send);
    }

    //add new socket to array of sockets
    for (i = 0; i < max_clients; i++) 
    {
	  //if position is empty
        if( client_socket[i] == 0 )
        {
            client_socket[i] = rcv_socket;
            printf("Adding to list of sockets as %d\n" , i);
            COMM_LIST_print_ip_list(); 
            break;
        }
    }

    alive++;
}

void COMM_close_connection(int sock) {
    struct sockaddr_in address;
    
    //Somebody disconnected , get his details and print
    getpeername(sock , (struct sockaddr*)&address , (socklen_t*)&addrlen);
    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

    if(my_rank==0)
        ip_list = LIST_remove_ip_adress(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    
    //Close the socket 
    close( sd );
    alive--;
}

void COMM_LIST_print_ip_list() {
    LIST_print_all_ip(ip_list);        
}