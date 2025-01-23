#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <poll.h>

#define FRAME_SIZE 256
#define DATA_SIZE 247  // 256 - (1 + 2 + 2 + 4)
#define SOCKET_TIMEOUT_SEC 10  // Increase from 3 to 10 seconds

// Frame structure (256 bytes total)
typedef struct __attribute__((packed)) {
    uint8_t type;           // 1 byte
    uint16_t data_length;   // 2 bytes
    char data[DATA_SIZE];   // 247 bytes (256 - 9)
    uint16_t checksum;      // 2 bytes
    uint32_t timestamp;     // 4 bytes
} Frame;                    // Total: 256 bytes

// Frame types as defined in protocol
#define FRAME_CONNECT_REQ      0x01  // Fleck -> Gotham
#define FRAME_WORKER_REG       0x02  // Worker -> Gotham
#define FRAME_WORKER_CONNECT   0x03  // Fleck -> Worker
#define FRAME_FILE_INFO        0x04  // Worker -> Fleck
#define FRAME_FILE_DATA        0x05  // Both ways
#define FRAME_MD5_CHECK        0x06  // Both ways
#define FRAME_DISCONNECT       0x07  // Any -> Any
#define FRAME_NEW_MAIN        0x08  // Gotham -> Worker
#define FRAME_ERROR           0x09  // Any -> Any
#define FRAME_DISTORT_REQ     0x10  // Fleck -> Gotham
#define FRAME_RESUME_REQ      0x11  // Fleck -> Gotham
#define FRAME_HEARTBEAT       0x12  // Any -> Any

// Connection structure
typedef struct {
    int fd;
    struct sockaddr_in addr;
} Connection;

// Frame functions
Frame* create_frame(uint8_t type, const char* data, uint16_t data_length);
void free_frame(Frame* frame);
int send_frame(Connection* conn, Frame* frame);
Frame* receive_frame(Connection* conn);
uint16_t calculate_checksum(Frame* frame);
int validate_frame(Frame* frame);
void log_frame(const char* prefix, Frame* frame);
Frame* receive_frame_timeout(Connection* conn, int timeout_sec);

// Connection functions
Connection* create_server(const char* ip, int port);
Connection* connect_to_server(const char* ip, int port);
int accept_connection(Connection* server);
void close_connection(Connection* conn);
int is_connected(Connection* conn);

// Data transfer functions
ssize_t send_data(Connection* conn, const void* data, size_t len);
ssize_t receive_data(Connection* conn, void* buffer, size_t len);
ssize_t receive_data_timeout(Connection* conn, void* buffer, size_t len, int timeout);

#endif