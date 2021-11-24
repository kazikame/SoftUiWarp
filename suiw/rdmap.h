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

#ifndef _RDMAP_H
#define _RDMAP_H

#include "ddp_new.h"
#include "common/iwarp.h"

/**
 * Operations:
 * 
 * 
 * int rdmap_init()
 *  - Set untagged queues
 *  - start new thread, run rdma_recv_loop()
 * 
 * 1. Send
 * 2. Send w/ Invalidate
 * 3. Send w/ SE
 * 4. Send w/ SE & Invalidate
 * 
 * int rdmap_send(void* message, __u32 len, stag_t* stag, int flags)
 * int rdmap_recv(...)
 * 
 * How do events work? EventFD?
 * 
 * 5. RDMA Write
 * 
 * int rdmap_write_send(void* message, __u32 len, stag_t* stag, __u32 offset)
 * 
 * 6. RDMA Read
 * 7. Terminate
 * 
 * 
 * Memory Registration
 * 
 * struct rdmap_mr *rdmap_reg_mr(struct pd_t *pd, void *addr, size_t length, enum rdmap_access_flags access);
 * 
 * Internal Functions
 * void rdma_recv_loop() -->
 * 
 * int parse_ddp_message(struct ddp_message* ddp, struct rdmap_message* message);
 * int rdmap_handle_send()
 * int rdmap_handle_read()
 */

/**
 *    *  RDMAP uses three queues for Untagged Buffers:

      *  Queue Number 0 (used by RDMAP for Send, Send with Invalidate,
         Send with Solicited Event, and Send with Solicited Event and
         Invalidate operations).

      *  Queue Number 1 (used by RDMAP for RDMA Read operations).

      *  Queue Number 2 (used by RDMAP for Terminate operations).
 * 
 * 
 * Read response handling?
 * 
 * 
 */

struct rdmap_ctrl {
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

struct rdmap_tagged_hdr {
    __u32 tag;
    __u32 qn;
    __u32 msn;
    __u32 mo;
};
struct rdmap_read_req_fields {
    __u32 sink_tag;
    __u32 sink_TO;
    __u32 rdma_rd_sz;
    __u32 src_tag;
    __u32 src_TO;
};

struct rdmap_untagged_hdr {
    __u32 tag;
    __u64 TO;
};

struct rdmap_terminate_hdr {
    __u32 ctrl_bits;
};


/* RFC 5040 */
enum rdmap_opcode {
    WRITE = 0x0000B,
    READ_REQUEST = 0x0001B,
    READ_RESPONSE = 0x0010B,
    SEND = 0x0011B,
    SEND_SOLICIT = 0x0101B,
    SEND_INVALIDATE = 0x0100B,
    SEND_SOLICIT_INVALIDATE = 0x0110B,
    TERMINATE = 0x0111B
};

//! Strict subset of ddp_stream_context
struct rdmap_stream_context {
    struct ddp_stream_context* ddp_ctx;

    int terminated = 0;
    //! TODO: Add completion queue stuff?
};

//! Init DDP Stream, Queue setup, thread start
struct rdmap_stream_context* rdmap_init_stream(int sockfd, struct pd_t* pd);

//! Free ddp stream structures
void rdmap_kill_stream(struct rdmap_stream_context* ctx);

//! Not overriding tagged buffer registration functions
//! Use rdmap_stream_context for those


int rdmap_send(struct rdmap_stream_context*, void* message, __u32 len, stag_t* invalidate_stag, int flags);
int rdmap_write(struct rdmap_stream_context*, void* message, __u32 len, stag_t* stag, __u32 offset);
int rdmap_read(struct rdmap_stream_context*, __u32 len, stag_t* src_stag, __u32 src_offset, stag_t* sink_stag, __u32 sink_offset);

#endif