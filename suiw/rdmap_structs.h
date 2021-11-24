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

#ifndef _RDMAP_S_H
#define _RDMAP_S_H

#include "ddp_new.h"
#include "common/iwarp.h"
#include "cq.h"

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

struct rdmap_stream_context {
    int sockfd;
    struct pd_t* pd;
    
    int connected = 0;

    struct wq* send_q;
    struct wq* recv_q;

    //! 
    struct ddp_stream_context* ddp_ctx;
};

#endif