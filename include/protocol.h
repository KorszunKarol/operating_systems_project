#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

// Frame types
#define FRAME_CONNECT_REQ      0x01
#define FRAME_WORKER_REG       0x02
#define FRAME_WORKER_CONNECT   0x03
#define FRAME_FILE_INFO        0x04
#define FRAME_FILE_DATA        0x05
#define FRAME_MD5_CHECK        0x06
#define FRAME_DISCONNECT       0x07
#define FRAME_NEW_MAIN        0x08
#define FRAME_ERROR           0x09
#define FRAME_DISTORT_REQ     0x10
#define FRAME_RESUME_REQ      0x11
#define FRAME_HEARTBEAT       0x12

#define DATA_SIZE 247
#pragma pack(push, 1)
typedef struct {
    uint8_t type;
    uint16_t data_length;
    char data[247];
    uint16_t checksum;
    uint32_t timestamp;
} Frame;
#pragma pack(pop)

// Core frame operations
Frame* create_frame(uint8_t type, const char* data, uint16_t data_len);
void free_frame(Frame* frame);
bool validate_frame(const Frame* frame);
uint16_t calculate_checksum(const Frame* frame);

// Frame type creation helpers
Frame* create_connect_request(const char* username, const char* ip, uint16_t port);
Frame* create_worker_registration(const char* worker_type, const char* ip, uint16_t port);
Frame* create_distort_request(const char* media_type, const char* filename);
Frame* create_file_info(size_t filesize, const char* md5sum);
Frame* create_file_data(const char* data, uint16_t length);
Frame* create_file_check_response(bool success);
Frame* create_disconnect_frame(const char* id);
Frame* create_error_frame(const char* error_msg);
Frame* create_heartbeat_frame(void);

// Data parsing utilities
bool parse_connect_request(const Frame* frame, char* username, char* ip, uint16_t* port);
bool parse_worker_registration(const Frame* frame, char* worker_type, char* ip, uint16_t* port);
bool parse_distort_request(const Frame* frame, char* media_type, char* filename);
bool parse_file_info(const Frame* frame, size_t* filesize, char* md5sum);
bool parse_file_data(const Frame* frame, char* data, uint16_t* length);
bool parse_disconnect(const Frame* frame, char* id);
bool parse_error(const Frame* frame, char* error_msg);

#endif