/*
 * Software Userspace iWARP device driver for Linux 
 *
 * MIT License
 * 
 * Copyright (c) 2021 Saksham Goel, Matthew Pabst
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

#ifndef _RDMAP_S_H
#define _RDMAP_S_H

#include "ddp/ddp.h"
#include "common.h"
#include "cq.h"
#include <pthread.h>

struct __attribute__((packed)) rdmap_ctrl {
    __u8 bits;
};

struct rdmap_message {
    struct rdmap_ctrl* ctrl;

    //! RDMAP fields from DDP header
    union {
        struct rdmap_tagged_hdr* tag_hdr;
        struct rdmap_untagged_hdr* untag_hdr;
    };

    //! RDMAP fields from DDP payload (ONLY read request and terminate)
    union {
        struct rdmap_read_req_hdr* read_req_hdr;
        struct rdmap_terminate_hdr* terminate_hdr;
    };
    void* payload;
};

/**
 * According to RFC 5040.
 * 
 * For Read Request, Send, and Terminate
 * 
 */
struct __attribute__((packed)) rdmap_untagged_hdr {
    __u32 tag;
    __u32 qn;
    __u32 msn;
    __u32 mo;
};
struct __attribute__((packed)) rdmap_read_req_fields {
    __u32 sink_tag;
    __u64 sink_TO;
    __u32 rdma_rd_sz;
    __u32 src_tag;
    __u64 src_TO;
};

/**
 * According to RFC 5040.
 * 
 * For Write, Read Response
 * 
 */
struct __attribute__((packed)) rdmap_tagged_hdr {
    __u32 tag;
    __u64 TO;
};

struct __attribute__((packed)) rdmap_terminate_hdr {
    __u32 ctrl_bits;
};

static inline __u8 get_rdmap_op(__u8 bits)
{
    return bits & 0xF;
}

struct rdmap_stream_context {
    struct ddp_stream_context* ddp_ctx;
    int connected = 0;

    struct wq* send_q;
    struct wq* recv_q;

    pthread_t recv_thread;
    pthread_t send_thread;
};

struct rdmap_stream_init_attr {
    int sockfd;
    struct pd_t* pd;
    
    struct wq* send_q;
    struct wq* recv_q;

    int max_pending_read_requests;
};

#endif