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


/**
 DDP has been designed with the following high-level architectural
 goals:

    * Provide a buffer model that enables the Local Peer to Advertise
    a named buffer (i.e., a Tag for a buffer) to the Remote Peer,
    such that across the network the Remote Peer can Place data into
    the buffer at Remote-Peer-specified locations.  This is referred
    to as the Tagged Buffer Model.

    * Provide a second receive buffer model that preserves ULP message
    boundaries from the Remote Peer and keeps the Local Peer's
    buffers anonymous (i.e., Untagged).  This is referred to as the
    Untagged Buffer Model.

    * Provide reliable, in-order Delivery semantics for both Tagged
    and Untagged Buffer Models.

    * Provide segmentation and reassembly of ULP messages.

    * Enable the ULP Buffer to be used as a reassembly buffer, without
    a need for a copy, even if incoming DDP Segments arrive out of
    order.  This requires the protocol to separate Data Placement of
    ULP Payload contained in an incoming DDP Segment from Data
    Delivery of completed ULP Messages.

    * If the Lower Layer Protocol (LLP) supports multiple LLP Streams
    within an LLP Connection, provide the above capabilities
    independently on each LLP Stream and enable the capability to be
    exported on a per-LLP-Stream basis to the ULP.

Basically:
1. chops ULP data into DDP segments according to MULPDU
2. places data directly into receive buffers:
    - if tagged, then must have sent an STag before using ULP
    - if untagged, then must have registered a series of ordered buffers by ULP
 */
#include <linux/types.h>
#include <asm/byteorder.h>
#include <stdint.h>
#include <stdio.h>
//#include <map>
//#include <queue>
//#include <pair>
#define DDP_TAGGED_HDR_SIZE 14
#define DDP_UNTAGGED_HDR_SIZE 18
//#define MULPDU 1500 //const mulpdu for now
#define TAGGED_BUFFERS_NUM 5 //TODO: change for ULP to define number of buffer
#define UNTAGGED_BUFFERS_NUM 5 //TODO: change for ULP to define number of buffer
#define TAGGED_BUFFER_SIZE 1000000 //TODO: change for ULP to define number of buffer
#define UNTAGGED_BUFFER_SIZE 1000000 //TODO: change for ULP to define number of buffer
#define MOD32 = 4294967296
struct ddp_tagged_hdr {
    uint8_t reserved;
    uint8_t reservedULP; //is this requireed here or only in ULP?
    uint32_t stag;
    uint64_t to;
};

struct ddp_untagged_hdr {
    uint8_t reserved;
    uint8_t reservedULP;
    uint32_t reserved2;
    uint32_t qn;
    uint32_t msn;
    uint32_t mo;
};

struct ddp_hdr {
    uint8_t ctrl; //what's this?
    union {
        struct ddp_tagged_hdr* tagged;
        struct ddp_untagged_hdr* untagged;
    };
};

/* only for receiving */
struct ddp_packet {
    struct ddp_hdr* hdr;
    char* data;
};

//! TODO: Take hint from siw.h, and complete


struct pd {
    uint32_t pd_id;
};

struct ddp_stream_context {
    int sockfd;
    struct pd* pd_id;
};

struct stag_t {
    uint32_t id;
    struct pd* pd_id;
};

//map<uint32_t,uint32_t> tag_to_pd;

//queue<char*> tagged_buffer[TAGGED_BUFFERS_NUM];
//queue<char*> untagged_buffer[UNTAGGED_BUFFERS_NUM];

//map<uint8_t, pair<u_int32_t, pair<u_int32_t, u_int32_t> > > ULP_to_tagged_buffer; //stag and start and end (included) of the buffer for that ULP/message
//map<uint64_t, pair<u_int32_t, pair<u_int32_t, u_int32_t> > > ULP_to_tagged_buffer; //stag and start and end (included) of the buffer for that ULP/message

struct ddp_stream_context* ddp_init_stream(int sockfd, struct pd* pd_id);

void ddp_kill_stream(struct ddp_stream_context* ctx);

int register_stag(struct stag_t* tag);

void register_tagged_buffer();

void register_untagged_buffer();

//! Check validity (stag, etc.)
int ddp_tagged_recv(struct ddp_stream_context* ctx, struct ddp_packet* packet);

//! Fragementation + Send
int ddp_tagged_send(struct ddp_stream_context* ctx, struct stag_t* tag, uint32_t offset, void* data, uint32_t len, uint8_t rsrvdULP);

//! Register Queues
//void register_tagged_queue();
//void register_untagged_queue();
//! Untagged send/recv
int ddp_untagged_send(struct ddp_stream_context* ctx, struct stag_t* tag, void* data, 
                    uint32_t len, uint64_t reserved, uint32_t qn, uint32_t msn);
//! TODO: Test fake_ping_client by sending the next Send/Read Request data

#endif
