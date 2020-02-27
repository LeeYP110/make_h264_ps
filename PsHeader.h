#pragma once
#include <iostream>

#define RTP_HDR_LEN 12
#define RTP_MAX_PACKET_BUFF 1460

#define PS_HDR_LEN  14
#define SYS_HDR_LEN 18
#define PSM_HDR_LEN 24
#define PES_HDR_LEN 19

#define PS_PES_PAYLOAD_SIZE 0xFE00

typedef struct FrameInfo {
    typedef std::shared_ptr<FrameInfo> Ptr;

    bool is_i_frame;
    uint32_t scr;
    int stream_id;
    uint64_t pts;
    uint64_t dts;
    FrameInfo() {
        is_i_frame = false;
        scr        = 0;
        stream_id = 0xE0;
        pts = 0;
        dts = 0;
    }
};

typedef struct bits_buffer_t {
    int    i_size;
    int    i_data;
    unsigned char i_mask;
    unsigned char *p_data;

    bits_buffer_t() {
        i_size = 0;
        i_data = 0;
        i_mask = 0x80;
        p_data = nullptr;
    }
} bits_buffer_t, bits_buffer_s;

static inline int bits_initwrite(bits_buffer_t *p_buffer, int i_size, unsigned char* p_data) {
    p_buffer->i_size = i_size;
    p_buffer->i_data = 0;
    p_buffer->i_mask = 0x80;
    p_buffer->p_data = p_data;
    p_buffer->p_data[0] = 0;
    if (!p_buffer->p_data) {
        if (!(p_buffer->p_data = (unsigned char*)malloc(i_size))) {
            return -1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

static inline void bits_write(bits_buffer_t *p_buffer, int i_count, unsigned long  i_bits) {
    if (p_buffer == nullptr || p_buffer->p_data == nullptr) {
        return;
    }

    while (i_count > 0) {
        i_count--;

        if ((i_bits >> i_count) & 0x01) {
            p_buffer->p_data[p_buffer->i_data] |= p_buffer->i_mask; // 写入1
        } else {
            p_buffer->p_data[p_buffer->i_data] &= ~p_buffer->i_mask; // 写入0
        }
        p_buffer->i_mask >>= 1;

        if (p_buffer->i_mask == 0) { // 下一个字节
            p_buffer->i_data++;
            p_buffer->i_mask = 0x80;
        }
    }
}