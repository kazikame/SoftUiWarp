/*
 * Software Userspace iWARP device driver for Linux 
 *
 * MIT License
 * 
 * Copyright (c) 2021 Saksham Goel
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _DDP_H
#define _DDP_H

#include <stdlib.h>
#include "lwlog.h"
#include "buffer.h"
#include "common.h"
#include <linux/types.h>
#include <asm/byteorder.h>

//! C++ stdlib abomination
#include <unordered_map>

#define MAX_UNTAGGED_BUFFERS 3
#define DDP_CTRL_SIZE 1
#define DDP_TAGGED_HDR_SIZE sizeof(struct ddp_tagged_meta)
#define DDP_UNTAGGED_HDR_SIZE sizeof(struct ddp_untagged_meta)

struct ddp_stream_context {
    int sockfd;
    struct pd_t* pd;

    struct untagged_buffer_queue* queues;
    std::unordered_map<__u32, tagged_buffer> tagged_buffers;
};

struct __attribute__((__packed__)) ddp_hdr {
    __u8 bits = 0;
};

enum {
    // T|L| Rsvd  | DV|
    DDP_HDR_T = 1 << 7,
    DDP_HDR_L = 1 << 6,
    DDP_HDR_RSVD = 0xF << 2,
    DDP_HDR_DV = 0x3,
};

inline __u8 ddp_set_tagged(__u8 bits) {
    return bits;
}

inline int ddp_is_tagged(__u8 bits)
{
    return DDP_HDR_T & bits;
}

//! TODO: Bad Alignment. Can cause performance problems.
struct __attribute__((__packed__)) ddp_tagged_meta {
    __u8 rsvdULP1;
    __u32 tag;
    __u64 TO;
};

//! TODO: Bad Alignment. Can cause performance problems.
struct __attribute__((__packed__)) ddp_untagged_meta {
    __u8 rsvdULP1;
    __u32 rsvdULP2;
    __u32 qn;
    __u32 msn;
    __u32 mo;
};

struct ddp_message {
    struct ddp_hdr hdr;
    union  {
        struct ddp_tagged_meta tagged_metadata;
        struct ddp_untagged_meta untagged_metadata;
    };
    union {
        struct untagged_buffer untag_buf;
        struct tagged_buffer tag_buf;
    };
    int len;
};

struct __attribute__((__packed__)) ddp_hdr_packed {
    struct ddp_hdr hdr;
    union {
        struct ddp_tagged_meta tagged_metadata;
        struct ddp_untagged_meta untagged_metadata;
    };
};

/**
 * @brief Inits a DDP stream
 * 
 * makes the queues as well
 * TODO: take input a new struct ddp_init_attr
 * 
 * @param sockfd MPA connected socket
 * @param pd protection domain
 * @return struct ddp_stream_context* 
 */
struct ddp_stream_context* ddp_init_stream(int sockfd, struct pd_t* pd);
void ddp_kill_stream(struct ddp_stream_context* ctx);

int ddp_send_tagged(struct ddp_stream_context*, struct ddp_tagged_meta*, sge* sge_list, int num_sge);
int ddp_send_untagged(struct ddp_stream_context*, struct ddp_untagged_meta*, sge* sge_list, int num_sge);

/**
 * @brief Registers a buffer in queue `qn` for an untagged message
 *        Buffers are consumed in a FIFO manner
 * 
 * @param qn queue number in which the buffer is added
 * @param buf buffer
 */
void ddp_post_recv(struct ddp_stream_context*, int qn, struct untagged_buffer* buf, int num_bufs);
inline struct untagged_buffer_queue* ddp_check_untagged_hdr(struct ddp_stream_context* ctx, struct ddp_tagged_meta* hdr);

/**
 * @brief Receives a DDP message. Blocking
 * 
 * `msg` must be allocated.
 * 
 * @param ctx 
 * @param msg 
 * @return int 
 */
int ddp_recv(struct ddp_stream_context* ctx, struct ddp_message* msg);

/**
 * @brief Registers a tagged memory region
 * 
 * The stag is filled by this function, and should be empty.
 * 
 * @return int 
 */
int register_tagged_buffer(struct ddp_stream_context*, struct tagged_buffer*);
int deregister_tagged_buffer(struct ddp_stream_context*, struct stag_t*);

/**
 * @brief checks if the stag and offset are valid wrt to `ctx`
 *        and returns the buffer. Returns NULL on error
 * 
 * @param ctx 
 * @param hdr 
 * @return int 
 */
inline struct tagged_buffer* ddp_check_stag(struct ddp_stream_context* ctx, struct ddp_tagged_meta* hdr);

#endif