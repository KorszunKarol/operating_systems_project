#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int fd;
    struct sockaddr_in addr;
} Connection;

Connection* create_server(const char *ip, int port);
int accept_connection(Connection *server);
Connection* connect_to_server(const char *ip, int port);
void close_connection(Connection *conn);
ssize_t send_data(Connection *conn, const void *data, size_t len);
ssize_t receive_data(Connection *conn, void *buffer, size_t len);

#endif