#include "../include/network.h"
#include "../include/logging.h"
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <arpa/inet.h>

#define DEBUG 1

static char last_error[256] = {0};
static pthread_t heartbeat_thread;
static volatile bool heartbeat_running = false;

static void set_last_error(const char* msg) {
    strncpy(last_error, msg, sizeof(last_error) - 1);
    log_error("NETWORK", last_error);
}

const char* get_last_error(void) {
    return last_error;
}

void clear_last_error(void) {
    last_error[0] = '\0';
}

/*************************************************
* @Name: vLogNetwork
* @Def: Logs network events for debugging
* @Arg: In: psEvent = event description
*       In: psDetails = event details
*       In: nResult = operation result
* @Ret: None
*************************************************/
void vLogNetwork(const char* psEvent, const char* psDetails, int nResult) {
    if (DEBUG) {
        char* psMsg;
        asprintf(&psMsg, "NETWORK DEBUG - %s: %s (Result: %d)\n",
                psEvent, psDetails, nResult);
        write(STDOUT_FILENO, psMsg, strlen(psMsg));
        free(psMsg);
    }
}

/*************************************************
* @Name: nSetSocketTimeout
* @Def: Sets socket timeout options
* @Arg: In: nSockfd = socket file descriptor
*       In: nSeconds = timeout in seconds
* @Ret: 0 on success, -1 on failure
*************************************************/
int nSetSocketTimeout(int nSockfd, int nSeconds) {
    struct timeval tTimeout;
    tTimeout.tv_sec = nSeconds;
    tTimeout.tv_usec = 0;

    if (setsockopt(nSockfd, SOL_SOCKET, SO_RCVTIMEO, &tTimeout, sizeof(tTimeout)) < 0) {
        return -1;
    }
    if (setsockopt(nSockfd, SOL_SOCKET, SO_SNDTIMEO, &tTimeout, sizeof(tTimeout)) < 0) {
        return -1;
    }
    return 0;
}

/*************************************************
* @Name: create_server
* @Def: Creates a server socket
* @Arg: In: psIP = IP address to bind to
*       In: nPort = port to bind to
* @Ret: Connection pointer or NULL on failure
*************************************************/
Connection* create_server(const char *psIP, int nPort) {
    Connection *pConn = malloc(sizeof(Connection));
    if (!pConn) {
        vLogNetwork("CREATE_SERVER", "Memory allocation failed", -1);
        return NULL;
    }

    // Create socket
    pConn->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (pConn->fd < 0) {
        vLogNetwork("CREATE_SERVER", "Socket creation failed", pConn->fd);
        free(pConn);
        return NULL;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(pConn->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        vLogNetwork("CREATE_SERVER", "Setsockopt failed", -1);
        close(pConn->fd);
        free(pConn);
        return NULL;
    }

    // Configure address
    pConn->addr.sin_family = AF_INET;
    pConn->addr.sin_port = htons(nPort);
    pConn->addr.sin_addr.s_addr = inet_addr(psIP);

    char sDebug[100];
    snprintf(sDebug, sizeof(sDebug), "Configuring server on %s:%d", psIP, nPort);
    vLogNetwork("CREATE_SERVER", sDebug, 0);

    // Bind socket
    if (bind(pConn->fd, (struct sockaddr*)&pConn->addr, sizeof(pConn->addr)) < 0) {
        vLogNetwork("CREATE_SERVER", "Bind failed", -1);
        close(pConn->fd);
        free(pConn);
        return NULL;
    }

    // Listen for connections
    if (listen(pConn->fd, 5) < 0) {
        vLogNetwork("CREATE_SERVER", "Listen failed", -1);
        close(pConn->fd);
        free(pConn);
        return NULL;
    }

    return pConn;
}

/*************************************************
* @Name: connect_to_server
* @Def: Connects to a server
* @Arg: In: psIP = server IP address
*       In: nPort = server port
* @Ret: Connection pointer or NULL on failure
*************************************************/
Connection* connect_to_server(const char *psIP, int nPort) {
    Connection *pConn = malloc(sizeof(Connection));
    if (!pConn) {
        vLogNetwork("CONNECT", "Memory allocation failed", -1);
        return NULL;
    }

    // Create socket
    pConn->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (pConn->fd < 0) {
        vLogNetwork("CONNECT", "Socket creation failed", pConn->fd);
        free(pConn);
        return NULL;
    }

    // Set timeout
    if (nSetSocketTimeout(pConn->fd, SOCKET_TIMEOUT_SEC) < 0) {
        vLogNetwork("CONNECT", "Set timeout failed", -1);
        close(pConn->fd);
        free(pConn);
        return NULL;
    }

    // Configure address
    pConn->addr.sin_family = AF_INET;
    pConn->addr.sin_port = htons(nPort);
    pConn->addr.sin_addr.s_addr = inet_addr(psIP);

    // Connect to server
    char sDebug[100];
    snprintf(sDebug, sizeof(sDebug), "Connecting to %s:%d", psIP, nPort);
    if (connect(pConn->fd, (struct sockaddr*)&pConn->addr, sizeof(pConn->addr)) < 0) {
        vLogNetwork("CONNECT", sDebug, -1);
        close(pConn->fd);
        free(pConn);
        return NULL;
    }

    vLogNetwork("CONNECT", sDebug, pConn->fd);
    return pConn;
}

/*************************************************
* @Name: accept_connection
* @Def: Accepts a new connection
* @Arg: In: pServer = server connection
* @Ret: New socket file descriptor or -1 on failure
*************************************************/
int accept_connection(Connection *pServer) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int nClientFd = accept(pServer->fd, (struct sockaddr*)&client_addr, &addr_len);

    char sDebug[100];
    snprintf(sDebug, sizeof(sDebug), "Client IP: %s", inet_ntoa(client_addr.sin_addr));
    vLogNetwork("ACCEPT", sDebug, nClientFd);

    if (nClientFd >= 0) {
        nSetSocketTimeout(nClientFd, SOCKET_TIMEOUT_SEC);
    }

    return nClientFd;
}

/*************************************************
* @Name: receive_data
* @Def: Receives data from socket
* @Arg: In: pConn = connection to receive from
*       Out: pvBuffer = buffer to store data
*       In: nLen = maximum length to receive
* @Ret: Number of bytes received or -1 on failure
*************************************************/
ssize_t receive_data(Connection *pConn, void *pvBuffer, size_t nLen) {
    ssize_t nTotal = 0;
    char *psBuffer = (char*)pvBuffer;

    while ((size_t)nTotal < nLen) {
        ssize_t nBytes = read(pConn->fd, psBuffer + nTotal, 1);
        if (nBytes <= 0) {
            if (nTotal == 0) return nBytes;
            break;
        }

        nTotal += nBytes;
        if (psBuffer[nTotal-1] == '\n') break;
    }

    if (nTotal > 0) {
        char sDebug[256];
        snprintf(sDebug, sizeof(sDebug), "Received: %.*s", (int)nTotal, (char*)pvBuffer);
        vLogNetwork("RECEIVE", sDebug, nTotal);
    }

    return nTotal;
}

/*************************************************
* @Name: send_data
* @Def: Sends data through socket
* @Arg: In: pConn = connection to send through
*       In: pvData = data to send
*       In: nLen = length of data
* @Ret: Number of bytes sent or -1 on failure
*************************************************/
ssize_t send_data(Connection *pConn, const void *pvData, size_t nLen) {
    char sDebug[256];
    snprintf(sDebug, sizeof(sDebug), "Sending %zu bytes: %.*s",
             nLen, (int)nLen, (char*)pvData);

    ssize_t nBytes = write(pConn->fd, pvData, nLen);
    vLogNetwork("SEND", sDebug, nBytes);

    return nBytes;
}

/*************************************************
* @Name: receive_data_timeout
* @Def: Receives data with timeout
* @Arg: In: pConn = connection to receive from
*       Out: pvBuffer = buffer to store data
*       In: nLen = maximum length to receive
*       In: nTimeout = timeout in seconds
* @Ret: Number of bytes received or -1 on failure
*************************************************/
ssize_t receive_data_timeout(Connection *pConn, void *pvBuffer, size_t nLen, int nTimeout) {
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(pConn->fd, &readfds);

    tv.tv_sec = nTimeout;
    tv.tv_usec = 0;

    char sDebug[100];
    snprintf(sDebug, sizeof(sDebug), "Waiting for data with %d second timeout", nTimeout);
    vLogNetwork("TIMEOUT_WAIT", sDebug, 0);

    int nResult = select(pConn->fd + 1, &readfds, NULL, NULL, &tv);
    if (nResult > 0) {
        return receive_data(pConn, pvBuffer, nLen);
    }

    vLogNetwork("TIMEOUT_WAIT", "Timeout or error occurred", nResult);
    return -1;
}

/*************************************************
* @Name: close_connection
* @Def: Closes a connection
* @Arg: In: pConn = connection to close
* @Ret: None
*************************************************/
void close_connection(Connection *pConn) {
    if (pConn) {
        vLogNetwork("CLOSE", "Closing connection", pConn->fd);
        close(pConn->fd);
        free(pConn);
    }
}

/*************************************************
* @Name: create_frame
* @Def: Creates a new frame
* @Arg: In: type = frame type
*       In: data = frame data
*       In: data_length = length of data
* @Ret: New frame or NULL on failure
*************************************************/
Frame* create_frame(uint8_t type, const char* data, uint16_t data_length) {
    Frame* frame = calloc(1, sizeof(Frame));  // Use calloc to zero-initialize
    if (!frame) return NULL;

    frame->type = type;
    frame->data_length = data_length;
    frame->timestamp = time(NULL);

    // Copy data if provided
    if (data && data_length > 0) {
        memcpy(frame->data, data, data_length);
    }

    frame->checksum = calculate_checksum(frame);

    char debug[256];
    snprintf(debug, sizeof(debug),
             "Created frame - Type: 0x%02X, Length: %d, Checksum: 0x%04X",
             frame->type, frame->data_length, frame->checksum);
    vLogNetwork("CREATE", debug, 0);

    return frame;
}

/*************************************************
* @Name: calculate_checksum
* @Def: Calculates frame checksum
* @Arg: In: frame = frame to calculate checksum for
* @Ret: Calculated checksum
*************************************************/
uint16_t calculate_checksum(const Frame* frame) {
    if (!frame) return 0;

    uint32_t sum = 0;

    // Add type
    sum += frame->type;

    // Add data length
    sum += (frame->data_length & 0xFF);
    sum += ((frame->data_length >> 8) & 0xFF);

    // Add data
    for (size_t i = 0; i < frame->data_length; i++) {
        sum += (uint8_t)frame->data[i];
    }

    // Add timestamp
    const uint8_t* ts = (const uint8_t*)&frame->timestamp;
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
        sum += ts[i];
    }

    return (uint16_t)(sum % 65536);
}

/*************************************************
* @Name: send_frame
* @Def: Sends a frame
* @Arg: In: conn = connection to send through
*       In: frame = frame to send
* @Ret: 0 on success, -1 on failure
*************************************************/
bool send_frame(Connection* conn, const Frame* frame) {
    if (!conn || !frame) {
        set_last_error("Invalid parameters");
        return false;
    }

    Frame temp = *frame;
    temp.timestamp = time(NULL);
    temp.checksum = calculate_checksum(&temp);

    ssize_t sent = write(conn->fd, &temp, sizeof(Frame));
    if (sent != sizeof(Frame)) {
        set_last_error("Failed to send complete frame");
        return false;
    }

    return true;
}

/*************************************************
* @Name: receive_frame
* @Def: Receives a frame
* @Arg: In: conn = connection to receive from
* @Ret: Received frame or NULL on failure
*************************************************/
Frame* receive_frame(Connection* conn) {
    if (!conn) {
        set_last_error("Invalid connection");
        return NULL;
    }

    Frame* frame = malloc(sizeof(Frame));
    if (!frame) {
        set_last_error("Memory allocation failed");
        return NULL;
    }

    if (receive_data(conn, frame, sizeof(Frame)) != sizeof(Frame)) {
        set_last_error("Failed to receive complete frame");
        free(frame);
        return NULL;
    }

    if (!validate_frame(frame)) {
        set_last_error("Frame validation failed");
        free(frame);
        return NULL;
    }

    return frame;
}

/*************************************************
* @Name: validate_frame
* @Def: Validates a received frame
* @Arg: In: frame = frame to validate
* @Ret: 1 if valid, 0 if invalid
*************************************************/
bool validate_frame(const Frame* frame) {
    if (!frame) return false;

    uint16_t received = frame->checksum;
    ((Frame*)frame)->checksum = 0;  // Temporarily clear for calculation
    uint16_t calculated = calculate_checksum(frame);
    ((Frame*)frame)->checksum = received;  // Restore

    return received == calculated;
}

/*************************************************
* @Name: free_frame
* @Def: Frees a frame
* @Arg: In: frame = frame to free
* @Ret: None
*************************************************/
void free_frame(Frame* frame) {
    free(frame);
}

void vHandleErrorFrame(Connection* pConn, const char* psError) {
    Frame* frame = create_frame(FRAME_ERROR, psError, strlen(psError));
    send_frame(pConn, frame);
    free_frame(frame);
}

int vValidateFrame(Frame* pFrame, Connection* pConn) {
    if (!pFrame) {
        vHandleErrorFrame(pConn, "Invalid frame");
        return 0;
    }

    uint16_t nReceivedChecksum = pFrame->checksum;
    pFrame->checksum = 0;
    uint16_t nCalculatedChecksum = calculate_checksum(pFrame);
    pFrame->checksum = nReceivedChecksum;

    if (nReceivedChecksum != nCalculatedChecksum) {
        vHandleErrorFrame(pConn, "Checksum mismatch");
        return 0;
    }

    return 1;
}

/*************************************************
* @Name: is_connected
* @Def: Checks if connection is still active
* @Arg: In: conn = Connection to check
* @Ret: 1 if connected, 0 if not
*************************************************/
bool is_connected(Connection* conn) {
    if (!conn || conn->fd < 0) {
        return false;
    }

    struct pollfd pfd = {
        .fd = conn->fd,
        .events = POLLIN | POLLHUP,
        .revents = 0
    };

    if (poll(&pfd, 1, 0) < 0) {
        return false;
    }

    return !(pfd.revents & (POLLHUP | POLLERR));
}

void* vHeartbeatThread(void* pvArg) {
    Connection* pConn = (Connection*)pvArg;

    while (1) {
        // Send heartbeat frame (type 0x12)
        Frame* heartbeat = create_frame(FRAME_HEARTBEAT, NULL, 0);
        if (!send_frame(pConn, heartbeat)) {
            free_frame(heartbeat);
            break;
        }
        free_frame(heartbeat);

        // Wait for response with timeout
        Frame* response = receive_frame_timeout(pConn, SOCKET_TIMEOUT_SEC);
        if (!response || response->type != FRAME_HEARTBEAT) {
            if (response) free_frame(response);
            break;
        }
        free_frame(response);

        sleep(SOCKET_TIMEOUT_SEC);
    }

    return NULL;
}

/*************************************************
* @Name: receive_frame_timeout
* @Def: Receives a frame with timeout
* @Arg: In: conn = connection to receive from
*       In: timeout_sec = timeout in seconds
* @Ret: Received frame or NULL on timeout/failure
*************************************************/
Frame* receive_frame_timeout(Connection* conn, int timeout_sec) {
    if (!conn) {
        set_last_error("Invalid connection");
        return NULL;
    }

    Frame* frame = malloc(sizeof(Frame));
    if (!frame) {
        set_last_error("Memory allocation failed");
        return NULL;
    }

    if (receive_data_timeout(conn, frame, sizeof(Frame), timeout_sec) != sizeof(Frame)) {
        set_last_error("Failed to receive frame within timeout");
        free(frame);
        return NULL;
    }

    if (!validate_frame(frame)) {
        set_last_error("Frame validation failed");
        free(frame);
        return NULL;
    }

    return frame;
}

int send_frame_conn(Connection* conn, const Frame* frame) {
    if (!conn || !frame) return -1;
    return send_data(conn, frame, sizeof(Frame)) == sizeof(Frame) ? 0 : -1;
}

Frame* receive_frame_conn(Connection* conn) {
    if (!conn) return NULL;

    Frame* frame = malloc(sizeof(Frame));
    if (!frame) return NULL;

    if (receive_data(conn, frame, sizeof(Frame)) != sizeof(Frame) || !validate_frame(frame)) {
        free(frame);
        return NULL;
    }

    return frame;
}

Frame* receive_frame_timeout_conn(Connection* conn, int timeout_sec) {
    if (!conn) return NULL;

    Frame* frame = malloc(sizeof(Frame));
    if (!frame) return NULL;

    if (receive_data_timeout(conn, frame, sizeof(Frame), timeout_sec) != sizeof(Frame) || !validate_frame(frame)) {
        free(frame);
        return NULL;
    }

    return frame;
}

bool start_heartbeat_monitor(Connection* conn) {
    if (heartbeat_running) {
        return false;
    }

    heartbeat_running = true;
    if (pthread_create(&heartbeat_thread, NULL, vHeartbeatThread, conn) != 0) {
        heartbeat_running = false;
        set_last_error("Failed to create heartbeat thread");
        return false;
    }

    return true;
}

void stop_heartbeat_monitor(void) {
    if (heartbeat_running) {
        heartbeat_running = false;
        pthread_join(heartbeat_thread, NULL);
    }
}