#ifndef _DDP_H
#define _DDP_H
/*
 * Software Userspace iWARP device driver for Linux
 *
 * Authors: Saksham Goel <saksham@cs.utexas.edu>
 *
 * Copyright (c) 2021
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *   - Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   - Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *   - Neither the name of IBM nor the names of its contributors may be
 *     used to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include "lwlog.h"
#include "buffer.h"
#include "common/iwarp.h"
#include <linux/types.h>
#include <asm/byteorder.h>

//! C++ stdlib abomination
#include <unordered_map>

#define MAX_UNTAGGED_BUFFERS 5
#define DDP_CTRL_SIZE 1

struct ddp_stream_context {
    int sockfd;
    struct pd_t* pd;

    untagged_buffer_queue* queues;
    std::unordered_map<__u32, tagged_buffer> tagged_buffers;
};

struct ddp_hdr {
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

struct ddp_tagged_meta {
    __u8 rsvdULP1 = 0;
    __u32 tag;
    __u64 TO;
};

struct ddp_untagged_meta {
    __u8 rsvdULP1 = 0;
    __u32 rsvdULP2;
    __u32 qn;
    __u32 msn;
    __u32 mo = 0;
};

struct ddp_message {
    struct ddp_hdr* hdr;
    union {
        struct ddp_tagged_meta* tagged_metadata;
        struct ddp_untagged_meta* untagged_metadata;
    };
    union {
        struct untagged_buffer* untag_buf;
        struct tagged_buffer* tag_buf;
    };
    int len;
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
void ddp_post_recv(struct ddp_stream_context*, int qn, struct untagged_buffer* buf);

int ddp_recv(struct ddp_stream_context*, struct ddp_message*);

/**
 * @brief Registers a tagged memory region
 * 
 * The stag is filled by this function, and should be empty.
 * 
 * @return int 
 */
int register_tagged_buffer(struct ddp_stream_context*, struct tagged_buffer*);
void deregister_tagged_buffer(struct ddp_stream_context*, struct stag_t*);


#endif