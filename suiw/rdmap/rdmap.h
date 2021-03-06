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

#ifndef _RDMAP_H
#define _RDMAP_H

#include "ddp/buffer.h"
#include "ddp/ddp.h"
#include "rdmap/rdmap_structs.h"
#include "common.h"

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

#define SEND_QN 0
#define READ_QN 1
#define TERMINATE_QN 2

//! Init DDP Stream, Queue setup, recv/send thread start
struct rdmap_stream_context* rdmap_init_stream(struct rdmap_stream_init_attr* ctx);

//! Free ddp stream structures, kill threads
void rdmap_kill_stream(struct rdmap_stream_context* ctx);

//! Not overriding tagged buffer registration functions
//! Use rdmap_stream_context->ddp_ctx for those

/**
 * @brief RDMA Send op.
 * 
 * @param message 
 * @param len 
 * @param invalidate_stag 
 * @param flags 
 * @return int 
 */

int rdmap_send(struct rdmap_stream_context* ctx, struct send_wr& wr);
int rdmap_write(struct rdmap_stream_context* ctx, struct send_wr& wr);
int rdmap_read(struct rdmap_stream_context* ctx, struct send_wr& wr);

/**
 * @brief adds buffer to ddp queue `0` to receive sends from remote
 * 
 * @param buf 
 * @return int 
 */
int rdma_post_recv(struct rdmap_stream_context*, struct recv_wr& wr);

int rdma_register(struct rdmap_stream_context*, struct tagged_buffer* buf);
int rdma_invalidate(struct rdmap_stream_context*, struct stag_t* stag);

//! Printers
void print_rdma_message(struct rdma_message* message, char* buf);
#endif