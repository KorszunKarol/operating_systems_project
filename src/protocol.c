#include "../include/protocol.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stddef.h>

Frame* create_frame(uint8_t type, const char* data, uint16_t data_len) {
    Frame* frame = malloc(sizeof(Frame));
    if (!frame) return NULL;

    frame->type = type;
    frame->data_length = data_len;
    frame->timestamp = (uint32_t)time(NULL);
    
    if (data && data_len > 0) {
        memcpy(frame->data, data, data_len < 247 ? data_len : 247);
    }

    frame->checksum = calculate_checksum(frame);
    return frame;
}

void free_frame(Frame* frame) {
    free(frame);
}

uint16_t calculate_checksum(const Frame* frame) {
    if (!frame) return 0;

    uint32_t sum = 0;
    const uint8_t* bytes = (const uint8_t*)frame;

    for (size_t i = 0; i < offsetof(Frame, checksum); i++) {
        sum += bytes[i];
    }

    return sum % 65536;
}

int validate_frame(const Frame* frame) {
    if (!frame) return 0;

    uint16_t calculated = calculate_checksum(frame);
    return calculated == frame->checksum;
}

int send_frame(int fd, const Frame* frame) {
    if (fd < 0 || !frame) return -1;

    ssize_t bytes = write(fd, frame, sizeof(Frame));
    return bytes == sizeof(Frame) ? 0 : -1;
}

Frame* receive_frame(int fd) {
    if (fd < 0) return NULL;

    Frame* frame = malloc(sizeof(Frame));
    if (!frame) return NULL;

    ssize_t bytes = read(fd, frame, sizeof(Frame));
    if (bytes != sizeof(Frame) || !validate_frame(frame)) {
        free(frame);
        return NULL;
    }

    return frame;
}

Frame* receive_frame_timeout(int fd, int timeout_sec) {
    if (fd < 0) return NULL;

    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    int ready = select(fd + 1, &readfds, NULL, NULL, &tv);
    if (ready <= 0) return NULL;

    return receive_frame(fd);
}

Frame* create_connect_request(const char* username, const char* ip, uint16_t port) {
    char data[247];
    int len = snprintf(data, sizeof(data), "%s&%s&%u", username, ip, port);
    return create_frame(FRAME_CONNECT_REQ, data, len);
}

Frame* create_worker_registration(const char* worker_type, const char* ip, uint16_t port) {
    char data[247];
    int len = snprintf(data, sizeof(data), "%s&%s&%u", worker_type, ip, port);
    return create_frame(FRAME_WORKER_REG, data, len);
}

Frame* create_distort_request(const char* media_type, const char* filename) {
    char data[247];
    int len = snprintf(data, sizeof(data), "%s&%s", media_type, filename);
    return create_frame(FRAME_DISTORT_REQ, data, len);
}

Frame* create_file_info(size_t filesize, const char* md5sum) {
    char data[247];
    int len = snprintf(data, sizeof(data), "%zu&%s", filesize, md5sum);
    return create_frame(FRAME_FILE_INFO, data, len);
}

Frame* create_file_data(const char* data, uint16_t length) {
    return create_frame(FRAME_FILE_DATA, data, length);
}

Frame* create_file_check_response(bool success) {
    const char* response = success ? "CHECK_OK" : "CHECK_KO";
    return create_frame(FRAME_MD5_CHECK, response, strlen(response));
}

Frame* create_disconnect_frame(const char* id) {
    return create_frame(FRAME_DISCONNECT, id, strlen(id));
}

Frame* create_error_frame(void) {
    return create_frame(FRAME_ERROR, NULL, 0);
}

Frame* create_heartbeat_frame(void) {
    return create_frame(FRAME_HEARTBEAT, NULL, 0);
}