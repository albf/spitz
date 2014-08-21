/**
Based on Silver Moon (m00n.silv3r@gmail.com) example and being modeled to SPITZ.

Alexandre L. B. F.      
*/

#include "comm.h"

/* Global Variables */
char buffer[1025];  //data buffer of 1K
struct sockaddr_in committer;    // address of committer node
int new_socket;

/* Job Manager only */
int master_socket, addrlen, new_socket, client_socket[max_clients], sd;
int max_sd;
int rank;
fd_set readfds; //set of socket descriptors
struct connected_ip * ip_list;

int send_byte_array(int sock, unsigned char * array) {
    int length = strlen(array) + strlen(array) + 1; // Send format : size|array
    char size[20];

    sprintf(size, "%d", length);
    strcat(size, "|");
    send(sock, size, strlen(size), 0);
    send(sock, array, strlen(array), 0);
    
    return 0;
}

void * read_byte_array(int sock) {
    char receive_buffer[20];
    char * first_part;
    unsigned char * bytes;
    unsigned char * bytes_ptr;
    int total_size, received=0, i=0;
    
    received+=read(sock, receive_buffer, 20);    
    total_size = atoi(strtok(receive_buffer,"|")); 
    
    bytes = (unsigned char *) malloc (total_size*sizeof(unsigned char));
    first_part = strtok(NULL, "|");
  
    for(i=0; i<strlen(first_part); i++) {
      bytes[i]=first_part[0];
    }

    bytes_ptr = bytes+(strlen(first_part));
    
    while(received < total_size) {
        received+=read(sock, bytes_ptr, 1024); 
        bytes_ptr+received;
    }

    return bytes;
}

void connect_to_job_manager(char ip_adr[]) {
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

// connect first

void set_committer() {
    // sent set committer request
    if (send(new_socket, "sc", 2, 0) < 0) {
        printf("Set committer failed\n");
        return;
    }
    
    printf("Set committer successfully\n");
}

void get_committer() {
    
    while(request_committer() != 1) {
	printf("Committer not found yet! \n");
        sleep(1);
    }
}

int request_committer() {
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

int get_rank_id() {
    return rank;
}

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

int telnet_client(int argc, char *argv[]) {
    connect_to_job_manager("127.0.0.1");
     
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
         
        //Receive a reply from the server
        if( recv(new_socket , buffer , 1024 , 0) < 0)
        {
            puts("recv failed");
            break;
        }
         
        puts("Server reply :");
        puts(buffer);
    }
     
    close(new_socket);
    return 0;
}

int setup_job_manager_network(int argc , char *argv[])
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
    ip_list = add_ip_address(ip_list, "127.0.0.0", PORT );
    rank = 0;
    
    return 1;
}

// Job Manager function
int wait_request() {
    int i, valread, activity;
    struct sockaddr_in sender_credentials;
 
    puts("Waiting for request \n");
     
    //clear the socket set
    FD_ZERO(&readfds);

    //add master socket to set
    FD_SET(master_socket, &readfds);
    max_sd = master_socket;
     
    //add child sockets to set
    for ( i = 0 ; i < max_clients ; i++) 
    {
        //socket descriptor
        sd = client_socket[i];
         
        //if valid socket descriptor then add to read list
        if(sd > 0)
            FD_SET( sd , &readfds);
         
        //highest file descriptor number, need it for the select function
        if(sd > max_sd)
            max_sd = sd;
    }

    //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
    activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

    if (activity < 0) 
    {
        printf("select error");
    }
      
    //If something happened on the master socket , then its an incoming connection
    if (FD_ISSET(master_socket, &readfds)) {
            create_new_connection();
    }
      
    // I/O Operation
    for (i = 0; i < max_clients; i++) 
    {
        sd = client_socket[i];
          
        if (FD_ISSET( sd , &readfds)) 
        {
            // Someone is closing
            if ((valread = read( sd , buffer, 1024)) == 0)
            {
                close_connection(sd);
                client_socket[i] = 0;
            }
              
            // Other message
            else
            {
                buffer[valread] = '\0';
                printf("buffer: %s, valread: %d\n", buffer, valread);
                
                // get rank
                if (strncmp(buffer, "gr", 2)==0) {
                    char rank_id[10];
                    getpeername(sd, (struct sockaddr*)&sender_credentials, (socklen_t*)&addrlen);
                    sprintf(rank_id, "%d", get_rank_id(ip_list, inet_ntoa(committer.sin_addr), ntohs(committer.sin_port)));
                    send(sd, rank_id, strlen(rank_id), 0); 
                }
                
                // set committer 
                else if (strncmp(buffer, "sc", 2)==0) {
                    getpeername(sd , (struct sockaddr*)&committer , (socklen_t*)&addrlen);
                    printf("Set committer, ip %s, port %d", inet_ntoa(committer.sin_addr) , ntohs(committer.sin_port));
                }
                
                // get committer
                else if(strncmp(buffer, "gc", 2)==0) {
                    if((strcmp((const char *) inet_ntoa(committer.sin_addr), "0.0.0.0")==0) && (ntohs(committer.sin_port) == 0))
                        send(sd, "0", 1, 0);
                    else {
                        char committer_send[25] = "";
                        strcat (committer_send,  inet_ntoa(committer.sin_addr));
                        
                        char port_str[6];	// used to represent a short int 
		                sprintf (port_str, "%d", (int) ntohs(committer.sin_port));
                        
                        strcat (committer_send, "|");
                        strcat (committer_send, port_str);
                        
                        // message format: ip|porta
                        send(sd, committer_send, strlen(committer_send),0);
                    }
                }
                else
                    return atoi(buffer);
            }
        }
    }
      
    return 0;
}

void create_new_connection() {
    int i;
    char id_send[10];
    struct sockaddr_in address;
    
    if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
	    printf("Problem accepting connection");
    }
  
    //inform user of socket number - used in send and receive commands
    printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
    
    // Add new connection to the list, assign rank id and send to the user.
    ip_list = add_ip_address(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port)); 
    sprintf(id_send, "%d", get_id(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port)));
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

void close_connection(int sock) {
    struct sockaddr_in address;
    
    //Somebody disconnected , get his details and print
    getpeername(sock , (struct sockaddr*)&address , (socklen_t*)&addrlen);
    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
    ip_list = remove_ip_address(ip_list, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    
    //Close the socket 
    close( sd );
}

void print_ip_list() {
    print_all_ip(ip_list);        
}