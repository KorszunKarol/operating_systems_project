#ifndef __NETWORK_H__
#define __NETWORK_H__

#include "protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <logging.h>
#define SOCKET_TIMEOUT_SEC 10

typedef struct {
    int fd;
    struct sockaddr_in addr;
    bool is_server;
} Connection;



Connection* create_server(const char* ip, int port);
Connection* connect_to_server(const char* ip, int port);
Connection* accept_client(Connection* server);
void close_connection(Connection* conn);
bool is_connected(Connection* conn);

bool send_frame(Connection* conn, const Frame* frame);
Frame* receive_frame(Connection* conn);
Frame* receive_frame_timeout(Connection* conn, int timeout_sec);

const char* get_last_error(void);
void clear_last_error(void);

bool start_heartbeat_monitor(Connection* conn);
void stop_heartbeat_monitor(void);

void log_error(const char* module, const char* message);
void log_message(LogLevel level, const char* module, const char* format, ...);
void vLogNetwork(const char* psEvent, const char* psDetails, int nResult);
int accept_connection(Connection *pServer);
ssize_t send_data(Connection *pConn, const void *pvData, size_t nLen);
ssize_t receive_data(Connection *pConn, void *pvBuffer, size_t nLen);

#endif