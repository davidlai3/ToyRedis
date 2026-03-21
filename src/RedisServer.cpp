#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h> // read(), write(), close()

#include "../include/RedisServer.h"

#define MAX 80 
#define SA struct sockaddr 

// Function designed for chat between client and server. 
void RedisServer::receive_message(int connfd) 
{ 
    char buff[MAX]; 
    int n; 
    // infinite loop for chat 
    for (;;) { 
        memset(buff, 0, MAX); 
  
        // read the message from client and copy it in buffer 
        read(connfd, buff, sizeof(buff)); 
        // print buffer which contains the client contents 
        printf("From client: %sTo client: ", buff); 
        memset(buff, 0, MAX); 
        n = 0; 
        // copy server message in the buffer 
        while ((buff[n++] = getchar()) != '\n') 
            ; 
  
        // and send that buffer to client 
        write(connfd, buff, sizeof(buff)); 
  
        // if msg contains "Exit" then server exit and chat ended. 
        if (strncmp("exit", buff, 4) == 0) { 
            printf("Server Exit...\n"); 
            break; 
        } 
    } 
}

void RedisServer::start_server() {
    int sockfd, connfd;
    socklen_t len;
    struct sockaddr_in servaddr, cli; 

  
    // socket create and verification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } 

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) < 0) {
        printf("setsockopt failed...\n");
    }

    memset(&servaddr, 0, sizeof(servaddr));
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(PORT); 
  
    // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
        printf("socket bind failed...\n"); 
        exit(0); 
    } 
  
    // Now server is ready to listen and verification 
    if ((listen(sockfd, 5)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else {
        printf("Server listening on port %d...\n", PORT);
    }

    len = sizeof(cli); 
    // Accept the data packet from client and verification 
    connfd = accept(sockfd, (SA*)&cli, &len); 
    if (connfd < 0) { 
        printf("server accept failed...\n"); 
        exit(0); 
    } 
  
    // Function for chatting between client and server 
    receive_message(connfd); 
  
    // After chatting close the socket 
    close(sockfd); 
}
  
