#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/epoll.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <vector>
#include <iostream>

#include "../include/RedisServer.h"

#define BUFFER_SIZE 4096
#define MAX_EVENTS 64
#define SA struct sockaddr 

int RedisServer::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void RedisServer::close_client(int fd, int epfd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
    if (connections.find(fd) == connections.end()) {
        printf("Tried to delete connection but doesn't exist...\n");
    }
    connections.erase(fd);
    close(fd);
}

void RedisServer::add_connection(int sockfd, int epfd) {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        // Try to accept connection
        int client_fd = accept(
            sockfd,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_len
        );

        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // no more pending connections
            }
            printf("accept failed...\n");
            break;
        }

        // Set client fd as nonblocking
        if (set_nonblocking(client_fd) == -1) {
            printf("set_nonblocking(client_fd) failed...\n");
            close(client_fd);
            continue;
        }

        epoll_event client_event{};
        client_event.events = EPOLLIN | EPOLLRDHUP;
        client_event.data.fd = client_fd;

        // Add client connection to epoll instance
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_event) == -1) {
            printf("epoll_ctl(ADD client_fd) failed...\n");
            close(client_fd);
            continue;
        }

        // Register client to server
        if (connections.find(client_fd) != connections.end()) {
            printf("Client already registered...\n");
            close(client_fd);
            break;
        }
        ClientConnection client{client_fd};
        connections[client_fd] = client;

        // Print ip of client
        char ipbuf[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));

        std::cout << "Accepted fd=" << client_fd
            << " from " << ipbuf
            << ":" << ntohs(client_addr.sin_port) << "\n";
    }
}

void RedisServer::receive_command(int fd, int epfd, char* buff) {
    while (true) {
        ClientConnection& client = connections[fd];
        ssize_t n = recv(fd, buff, BUFFER_SIZE, 0);

        if (n > 0) {
            client.buff.insert(client.buff.end(), buff, buff+n);
            std::cout << "fd " << fd << " sent " << n << " bytes: ";
            std::cout.write(buff, n);
            std::cout << "\n";
        } else if (n == 0) {
            // Peer performed an orderly shutdown
            std::cout << "Peer closed fd=" << fd << "\n";
            close_client(fd, epfd);
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // no more data for now
            }
            printf("recv failed...\n");
            close_client(fd, epfd);
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
        exit(-1); 
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

    // Set fd as nonblocking
    if (set_nonblocking(sockfd) == -1) {
        printf("fcntl failed...\n"); 
        exit(-1); 
    }
  
    // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
        printf("socket bind failed...\n"); 
        exit(-1); 
    } 
  
    // Now server is ready to listen and verification 
    if ((listen(sockfd, 5)) != 0) { 
        printf("listen failed...\n"); 
        exit(-1);
    } 
    else {
        printf("Server listening on port %d...\n", PORT);
    }

    // Create epoll instance
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        printf("epoll_create1 failed...\n");
        exit(-1);
    }

    // Register listening socket
    epoll_event listen_event{};
    listen_event.events = EPOLLIN;
    listen_event.data.fd = sockfd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &listen_event) == -1) {
        printf("epoll_ctl failed...\n");
        close(sockfd);
        exit(-1);
    }

    std::vector<epoll_event> events(MAX_EVENTS);
    char buff[BUFFER_SIZE];

    // Event loop
    while (true) {
        int ready = epoll_wait(epfd, events.data(), static_cast<int>(events.size()), -1);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;  // interrupted by signal, retry
            }
            printf("epoll_wait failed...\n");
            break;
        }

        for (int i = 0; i < ready; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            // New incoming connections on the listener
            if (fd == sockfd) {
                add_connection(sockfd, epfd);
                continue;
            }

            // Error / hangup / peer closed write side
            if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                std::cout << "Closing fd=" << fd << "\n";
                close_client(epfd, fd);
                continue;
            }

            // Readable client socket
            if (ev & EPOLLIN) {
                receive_command(fd, epfd, buff);
                continue;
            }
        }
    }

    close(epfd);
    close(sockfd);
}
