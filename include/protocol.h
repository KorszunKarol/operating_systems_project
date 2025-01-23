#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

// Frame types
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

// Frame structure (256 bytes total)
typedef struct {
    uint8_t type;           // 1 byte
    uint16_t data_length;   // 2 bytes
    char data[247];         // 247 bytes
    uint16_t checksum;      // 2 bytes
    uint32_t timestamp;     // 4 bytes
} Frame;

// Frame functions
Frame* create_frame(uint8_t type, const char* data);
void free_frame(Frame* frame);
int send_frame(int fd, Frame* frame);
Frame* receive_frame(int fd);
uint16_t calculate_checksum(Frame* frame);

#endif