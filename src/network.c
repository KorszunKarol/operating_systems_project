#include "network.h"
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>

#define DEBUG 1
#define SOCKET_TIMEOUT_SEC 3

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
    int nOpt = 1;
    if (setsockopt(pConn->fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &nOpt, sizeof(nOpt))) {
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

    while (nTotal < nLen) {
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
uint16_t calculate_checksum(Frame* frame) {
    uint32_t sum = 0;

    // Add type (1 byte)
    sum += frame->type;

    // Add data length (2 bytes)
    sum += (frame->data_length & 0xFF);
    sum += ((frame->data_length >> 8) & 0xFF);

    // Add data bytes
    for (int i = 0; i < frame->data_length; i++) {
        sum += (uint8_t)frame->data[i];
    }

    // Add timestamp (4 bytes)
    sum += (frame->timestamp & 0xFF);
    sum += ((frame->timestamp >> 8) & 0xFF);
    sum += ((frame->timestamp >> 16) & 0xFF);
    sum += ((frame->timestamp >> 24) & 0xFF);

    // Calculate final checksum (mod 2^16)
    return (uint16_t)(sum % 65536);
}

/*************************************************
* @Name: send_frame
* @Def: Sends a frame
* @Arg: In: conn = connection to send through
*       In: frame = frame to send
* @Ret: 0 on success, -1 on failure
*************************************************/
int send_frame(Connection* conn, Frame* frame) {
    if (!conn || !frame) return -1;

    frame->timestamp = time(NULL);
    frame->checksum = calculate_checksum(frame);

    ssize_t bytes = write(conn->fd, frame, FRAME_SIZE);

    char debug[512];
    snprintf(debug, sizeof(debug),
             "Sending frame - Type: 0x%02X, Length: %d, Checksum: 0x%04X",
             frame->type, frame->data_length, frame->checksum);
    vLogNetwork("SEND", debug, bytes);

    return (bytes == FRAME_SIZE) ? 0 : -1;
}

/*************************************************
* @Name: receive_frame
* @Def: Receives a frame
* @Arg: In: conn = connection to receive from
* @Ret: Received frame or NULL on failure
*************************************************/
Frame* receive_frame(Connection* conn) {
    if (!conn) return NULL;

    Frame* frame = malloc(sizeof(Frame));
    if (!frame) return NULL;

    ssize_t bytes = read(conn->fd, frame, FRAME_SIZE);
    if (bytes != FRAME_SIZE) {
        vLogNetwork("RECV", "Failed to read complete frame", bytes);
        free(frame);
        return NULL;
    }

    char debug[512];
    snprintf(debug, sizeof(debug),
             "Received frame: Type=0x%02X, Length=%d, Checksum=0x%04X",
             frame->type, frame->data_length, frame->checksum);
    vLogNetwork("RECV_FRAME", debug, bytes);

    // Validate checksum
    uint16_t received_checksum = frame->checksum;
    frame->checksum = 0;
    uint16_t calculated_checksum = calculate_checksum(frame);
    frame->checksum = received_checksum;

    if (received_checksum != calculated_checksum) {
        vLogNetwork("VALIDATE", "Checksum mismatch", -1);
        Frame* error = create_frame(FRAME_ERROR, NULL, 0);
        send_frame(conn, error);
        free_frame(error);
        free_frame(frame);
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
int validate_frame(Frame* frame) {
    if (!frame) return 0;

    // Log frame details
    char debug[256];
    snprintf(debug, sizeof(debug),
             "Validating frame - Type: 0x%02X, Length: %d, Data: %.*s",
             frame->type, frame->data_length,
             frame->data_length, frame->data);
    vLogNetwork("VALIDATE", debug, 0);

    // Save received checksum
    uint16_t received_checksum = frame->checksum;

    // Calculate checksum
    frame->checksum = 0;  // Zero out for calculation
    uint16_t calculated_checksum = calculate_checksum(frame);

    // Restore original checksum
    frame->checksum = received_checksum;

    // Log detailed checksum comparison
    snprintf(debug, sizeof(debug),
             "Checksum comparison - Received: 0x%04X, Calculated: 0x%04X, Match: %s",
             received_checksum, calculated_checksum,
             (received_checksum == calculated_checksum) ? "Yes" : "No");
    vLogNetwork("VALIDATE_CHECKSUM", debug, received_checksum == calculated_checksum);

    // If checksums don't match, log frame contents for debugging
    if (received_checksum != calculated_checksum) {
        snprintf(debug, sizeof(debug),
                "Frame contents - Type: 0x%02X, Length: %d, Data: '%.*s', "
                "Timestamp: %u",
                frame->type, frame->data_length,
                frame->data_length, frame->data,
                frame->timestamp);
        vLogNetwork("FRAME_DETAIL", debug, 0);
    }

    return received_checksum == calculated_checksum;
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

/*************************************************
* @Name: log_frame
* @Def: Logs frame details for debugging
* @Arg: In: prefix = log prefix
*       In: frame = frame to log
* @Ret: None
*************************************************/
void log_frame(const char* prefix, Frame* frame) {
    if (DEBUG) {
        char* msg;
        asprintf(&msg, "FRAME %s - Type: 0x%02X, Length: %d, Checksum: 0x%04X\n",
                prefix, frame->type, frame->data_length, frame->checksum);
        write(STDOUT_FILENO, msg, strlen(msg));
        free(msg);
    }
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
int is_connected(Connection* conn) {
    if (!conn || conn->fd < 0) {
        return 0;
    }

    // Use poll() to check connection status
    struct pollfd pfd;
    pfd.fd = conn->fd;
    pfd.events = POLLIN | POLLHUP;
    pfd.revents = 0;

    int result = poll(&pfd, 1, 0);  // Non-blocking poll

    if (result < 0) {
        // Error occurred
        return 0;
    }

    // Check if connection was closed
    if (pfd.revents & POLLHUP) {
        return 0;
    }

    // Connection is still alive
    return 1;
}

void* vHeartbeatThread(void* pvArg) {
    Connection* pConn = (Connection*)pvArg;

    while (1) {
        // Send heartbeat frame (type 0x12)
        Frame* heartbeat = create_frame(FRAME_HEARTBEAT, NULL, 0);
        if (send_frame(pConn, heartbeat) != 0) {
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
    if (!conn) return NULL;

    // Set up select() for timeout
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(conn->fd, &readfds);

    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    // Wait for data with timeout
    int ready = select(conn->fd + 1, &readfds, NULL, NULL, &tv);

    if (ready < 0) {
        // Error occurred
        vLogNetwork("TIMEOUT", "Select error", -1);
        return NULL;
    }

    if (ready == 0) {
        // Timeout occurred
        vLogNetwork("TIMEOUT", "No data received within timeout", 0);
        return NULL;
    }

    // Data is available, receive frame
    return receive_frame(conn);
}