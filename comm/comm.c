/**
Based on Silver Moon (m00n.silv3r@gmail.com) example and being modeled to SPITZ.

Alexandre L. B. F.      
*/

#include "comm.h"

/* Global Variables */
char buffer[1025], * lib_path;      //data buffer of 1K
struct sockaddr_in committer;       // address of committer node
int new_socket, rank, run_num;

/* Job Manager only */
int master_socket, addrlen, new_socket, client_socket[max_clients], sd;
int max_sd, alive;
fd_set readfds; //set of socket descriptors
struct connected_ip * ip_list;

// Send N bytes pointer by bytes pointer.
int COMM_send_bytes(int sock, void * bytes, int size) {
    char size_c[20];
    int return_value;
    
    sprintf(size_c, "%d", size);
    strcat(size_c, "|\n");
    return_value = (int) send(sock, size_c, (strlen(size_c)-1), 0); // don't send '\n'
    return_value = (int) send(sock, bytes, size, 0);

    return 0;    
}

// Read unknown type and size from socket.
void * COMM_read_bytes(int sock) {
    char message_size[20], received_char='0';
    int total_rcv, offset=0, msg_size;
    char * bytes_rcv;

    while(received_char!='|') {
        total_rcv = read(sock, &received_char, 1);

        if(received_char!='|')
            message_size[offset]=received_char;
        else
            message_size[offset]='\n';

        offset++;
    }

    msg_size = atoi(message_size);
    bytes_rcv = malloc(msg_size);
    offset = 0;    
    
    while(offset < msg_size) {
        total_rcv = read(sock, (bytes_rcv+offset), (msg_size-offset));
        offset+=total_rcv;
    }
   
    return (void *) bytes_rcv;
}

// Send char array using send bytes function
int COMM_send_char_array(int sock, char * array) {
    return COMM_send_bytes(sock, (void *) array, strlen(array));
}

// Read char array using read bytes function
char * COMM_read_char_array(int sock) {
    return (char *) COMM_read_bytes(sock);
}

int COMM_send_int(int sock, int value) {
    return COMM_send_bytes(sock, (void *) &value, sizeof(int));
}

int COMM_read_int(int sock) {
    int result;
    int * rcv_int;
    
    rcv_int = (int *) COMM_read_bytes(sock);
    result = * rcv_int;
    free(rcv_int);
    return result;
}

void COMM_connect_to_job_manager(char ip_adr[]) {
    struct sockaddr_in address;
    char rcv_rank[10];
    int valread;
    
    //Create socket
    new_socket = socket(AF_INET , SOCK_STREAM , 0);
    if (new_socket == -1)
    {
        printf("Could not create socket");
    }
    //puts("Socket created");
     
    address.sin_addr.s_addr = inet_addr(ip_adr);
    address.sin_family = AF_INET;
    address.sin_port = htons( 8888 );
 
    //Connect to remote server
    if (connect(new_socket , (struct sockaddr *)&address , sizeof(address)) < 0) {
        printf("Could not connect to the Job Manager.\n");
    }
    else {
        puts("Connected Successfully to the Job Manager\n");
        if(valread = read(new_socket, rcv_rank, 10)>0) {
            rank = atoi(rcv_rank); 
        }
        else {
            printf("Problem getting the rank value\n");
        }
    }
}

int COMM_get_alive() {
    if(send(new_socket,"ga", 2) != 0) {
	printf("Get alive failed\n");
	return -1;
    }

    return COMM_read_int(new_socket);
}

void COMM_set_path(char * file_path) {
    lib_path = strcpy(malloc(sizeof(char)*strlen(file_path)), file_path);
}

char * COMM_get_path() {
   if(send(new_socket, "gp", 2, 0) < 0) {
       printf("Get path failed \n");
       return NULL;
   } 

   return COMM_read_char_array(new_socket);
} 

void COMM_increment_run_num() {
    if(rank==0)
        run_num++;
}

int COMM_get_run_num() {
    if(send(new_socket, "gn", 2, 0) < 0) {
        printf("Get run failed \n");
        return -1;
    }

    return COMM_read_int(new_socket); 
}

void COMM_set_committer() {
    // sent set committer request
    if (send(new_socket, "sc", 2, 0) < 0) {
        printf("Set committer failed\n");
        return;
    }
    
    printf("Set committer successfully\n");
}

void COMM_get_committer() {
    
    while(COMM_request_committer() != 1) {
	printf("Committer not found yet! \n");
        sleep(1);
    }
}

int COMM_request_committer() {
    int c_port, valread;
    char * token;
    
    if (send(new_socket, "gc", 2, 0) < 0) {
        printf("Get committer failed\n");
        return -1;
    }

    //Receive a reply from the server
    if (valread = read(new_socket, buffer, 1024) >= 0) {
        if (strlen(buffer) == 1) {              // Job manager doesn't know yet
            return -1;
        } else {
            // otherwise, get the ip and port values.
            token = strtok(buffer, "|\0");
            committer.sin_addr.s_addr = inet_addr(token);
            committer.sin_family = AF_INET;
            
            token = strtok(NULL, "|\0");
            c_port = atoi(strtok(token, "|"));
            committer.sin_port = htons(c_port);
            return 1;
            
        }
    }

    return -1;
}

// Rank id is now in a variable
int COMM_get_rank_id() {
    return rank;
}

// Old get_rank version.
/*int get_rank_id() {
    char recv_id[10];
    int valread;
        
    if (send(new_socket, "gr", 2, 0) < 0) {
        printf("Get rank failed\n");
        return -1;
    }

    if (valread = read(new_socket,recv_id ,10) >= 0) {
        recv_id[valread] = '\0'; 
        return atoi(recv_id);
    }

    return -1;
}*/

int COMM_telnet_client(int argc, char *argv[]) {
    COMM_connect_to_job_manager("127.0.0.1");
     
    //keep communicating with server
    while(1)
    {
        printf("Enter message : ");
        scanf("%s" , buffer);
         
        //Send some data
        if( send(new_socket , buffer , strlen(buffer) , 0) < 0)
        {
            puts("Send failed");
            return 1;
        }
        
        char * return_array = COMM_read_char_array(new_socket); 
        
        //Receive a reply from the server
/*        if( recv(new_socket , buffer , 1024 , 0) < 0)
        {
            puts("recv failed");
            break;
        }
*/         
        puts("Server reply :");
        puts(buffer);
    }
     
    close(new_socket);
    return 0;
}

int COMM_setup_job_manager_network(int argc , char *argv[])
{
    int i, opt = 1;
    struct sockaddr_in address;
    
    ip_list = NULL;
    committer.sin_addr.s_addr = 0;
    committer.sin_port = 0;
      
    for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }
      
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
    address.sin_port = htons( PORT );
      
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        printf("bind failed\n");
        return -1;
    }
    printf("Listener on port %d \n", PORT);
     
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0) {
        return -1;
    }
      
    addrlen = sizeof(address);
    ip_list = LIST_add_ip_adress(ip_list, "127.0.0.0", PORT );
    rank = 0;
    run_num = 0;
    lib_path = NULL;
    alive = 0;
    
    return 1;
}

// Job Manager function
int COMM_wait_request() {
    int i, valread, activity;
 
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
	alive++;
        COMM_create_new_connection();
    }
      
    // I/O Operation
    for (i = 0; i < max_clients; i++) 
    {
        sd = client_socket[i];
          
        if (FD_ISSET( sd , &readfds)) 
        {
            if ((valread = read( sd , buffer, 1024)) == 0)      // Someone is closing
            {
		alive--;
                COMM_close_connection(sd);
                client_socket[i] = 0;
            }
              
            else                                                // Other request
            {
                buffer[valread] = '\0';
                printf("buffer: %s, valread: %d\n", buffer, valread);
                return COMM_reply_request();
            }
        }
    }
      
    return 0;
}

int COMM_reply_request() {
   // get alive number
    if (strncmp(buffer, "ga", 2) == 0) {
	COMM_send_int(sd, alive);
    }
    
    // get committer
    else if (strncmp(buffer, "gc", 2) == 0) {
        if ((strcmp((const char *) inet_ntoa(committer.sin_addr), "0.0.0.0") == 0) && (ntohs(committer.sin_port) == 0))
            send(sd, "0", 1, 0);
        else {
            char committer_send[25] = "";
            strcat(committer_send, inet_ntoa(committer.sin_addr));

            char port_str[6]; // used to represent a short int 
            sprintf(port_str, "%d", (int) ntohs(committer.sin_port));

            strcat(committer_send, "|");
            strcat(committer_send, port_str);

            // message format: ip|porta
            COMM_send_char_array(sd, committer_send);
        }
    }
    
    // get path
    else if (strncmp(buffer, "gr", 2) == 0) {
        if(lib_path != NULL)
	    COMM_send_char_array(sd, lib_path);
	else
	    COMM_send_char_array(sd, "NULL\n");
    }
        
    // get rank
    else if (strncmp(buffer, "gr", 2) == 0) {
        struct sockaddr_in sender_credentials;
        getpeername(sd, (struct sockaddr*) &sender_credentials, (socklen_t*) & addrlen);
        COMM_send_int(sd, COMM_get_rank_id(ip_list, inet_ntoa(sender_credentials.sin_addr), ntohs(sender_credentials.sin_port)));
    }
    
    // get run
    else if (strncmp(buffer, "gn", 2) == 0) {
        COMM_send_int(sd, run_num);

    } 
    
    // set committer 
    else if (strncmp(buffer, "sc", 2) == 0) {
        getpeername(sd, (struct sockaddr*) &committer, (socklen_t*) & addrlen);
        printf("Set committer, ip %s, port %d", inet_ntoa(committer.sin_addr), ntohs(committer.sin_port));
    }
    
    else
        return atoi(buffer);

    return 0;
}

void COMM_create_new_connection() {
    int i;
    char id_send[10];
    struct sockaddr_in address;
    
    if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
	    printf("Problem accepting connection");
    }
  
    //inform user of socket number - used in send and receive commands
    printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
    
    // Add new connection to the list, assign rank id and send to the user.
    ip_list = LIST_add_ip_adress(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port)); 
    sprintf(id_send, "%d", LIST_get_id(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port)));
    send(new_socket,id_send, 10,0);
    
    //add new socket to array of sockets
    for (i = 0; i < max_clients; i++) 
    {
	//if position is empty
        if( client_socket[i] == 0 )
        {
            client_socket[i] = new_socket;
            printf("Adding to list of sockets as %d\n" , i);
                 
            break;
        }
    }
}

void COMM_close_connection(int sock) {
    struct sockaddr_in address;
    
    //Somebody disconnected , get his details and print
    getpeername(sock , (struct sockaddr*)&address , (socklen_t*)&addrlen);
    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
    ip_list = LIST_remove_ip_adress(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    
    //Close the socket 
    close( sd );
}

void COMM_LIST_print_ip_list() {
    LIST_print_all_ip(ip_list);        
}