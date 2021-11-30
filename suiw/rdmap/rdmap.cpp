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

#include "ddp/buffer.h"
#include "rdmap/rdmap.h"
#include "ddp/ddp.h"
#include "lwlog.h"
#include "pthread.h"
#include <arpa/inet.h>

//! Main receive loop run in a separate thread
//! TODO:
void* rnic_recv(void* ctx_ptr)
{
    struct rdmap_stream_context* ctx = (struct rdmap_stream_context*)ctx_ptr;
    struct ddp_message ddp_message;
    struct rdmap_message message;

    moodycamel::ConcurrentQueue<recv_wr>* rq = ctx->recv_q->recv_q;
    moodycamel::ConcurrentQueue<work_completion>* recv_cq = ctx->recv_q->cq->q;
    moodycamel::ConcurrentQueue<work_completion>* cq = ctx->send_q->cq->q;
    moodycamel::ConcurrentQueue<work_completion>* pending_cq = ctx->send_q->cq->pending_q;

    struct recv_wr wr;
    struct work_completion wce;
    
    //! Only for read responses
    moodycamel::ConcurrentQueue<send_wr>* sq = ctx->send_q->send_q;
    struct send_wr read_resp;
    struct sge sg;
    struct rdmap_read_req_fields* read_req;

    read_resp.opcode = rdma_opcode::RDMAP_RDMA_READ_RESP;
    read_resp.wr_id = 1001;
    read_resp.sg_list = &sg;
    read_resp.num_sge = 1;
    while(ctx->connected)
    {
        int ret = ddp_recv(ctx->ddp_ctx, &ddp_message);
        if (unlikely(ret < 0))
        {
            lwlog_err("ddp_recv error, exiting");
            return NULL;
        }
        message.ctrl = (rdmap_ctrl*)((char *)&ddp_message.hdr + DDP_CTRL_SIZE);
        __u8 opcode = get_rdmap_op(message.ctrl->bits);
        switch(opcode)
        {
            case rdma_opcode::RDMAP_RDMA_WRITE: {
                assert(ddp_is_tagged(ddp_message.hdr.bits));
                lwlog_info("Someone wrote data at %p", ddp_message.tag_buf.data);
                //! do nothing :)
                break;
            }
            case rdma_opcode::RDMAP_RDMA_READ_REQ: {
                assert(!ddp_is_tagged(ddp_message.hdr.bits));
                // handle w/o letting the client know
                read_req = (struct rdmap_read_req_fields*) ddp_message.untag_buf.data;

                //! Get the corresponding tagged buffer
                lwlog_debug("Read Req Fields %u 0x%lx %u %u 0x%lx", ntohl(read_req->sink_tag),
                                                                ntohll(read_req->sink_TO),
                                                                ntohl(read_req->rdma_rd_sz),
                                                                ntohl(read_req->src_tag),
                                                                ntohll(read_req->src_TO));
                auto it = ctx->ddp_ctx->tagged_buffers.find(ntohl(read_req->src_tag));
                if (unlikely(it == ctx->ddp_ctx->tagged_buffers.end()))
                {
                    lwlog_err("Read request for stag %u not found", ntohl(read_req->src_tag));
                }
                sg.addr = ntohll(read_req->src_TO);
                sg.lkey = ntohl(read_req->src_tag);
                sg.length = ntohl(read_req->rdma_rd_sz);

                read_resp.wr.rdma.rkey = ntohl(read_req->sink_tag);
                read_resp.wr.rdma.remote_addr = ntohll(read_req->sink_TO);
                
                sq->enqueue(read_resp);
                read_resp.wr_id++;

                //! Replenish untagged buffer in queue 1
                ddp_post_recv(ctx->ddp_ctx, READ_QN, &ddp_message.untag_buf, 1);
                break;
            }
            case rdma_opcode::RDMAP_RDMA_READ_RESP: {
                assert(ddp_is_tagged(ddp_message.hdr.bits));
                // send wce from pending
                int found = pending_cq->try_dequeue(wce);
                if (unlikely(!found))
                {
                    lwlog_err("read response received but pending_cq is empty");
                    break;
                }

                //! Check if wce matches with work that was done
                wce.byte_len = ret;
                cq->enqueue(wce);
                break;
            }
            case rdma_opcode::RDMAP_SEND_SE_INVAL:
            case rdma_opcode::RDMAP_SEND_INVAL: {
                //! TODO: Handle Invalidation
                struct stag_t stag;
                stag.tag = ntohl(ddp_message.tagged_metadata.rsvdULP1);
                int erased = rdma_invalidate(ctx, &stag);
                if (erased <= 0)
                {
                    lwlog_err("Invalid STag in send w/ invalidate; terminating");
                    return NULL;
                }
                //! fallover to SEND
            }
            case rdma_opcode::RDMAP_SEND_SE: //! fallover to SEND
            case rdma_opcode::RDMAP_SEND: {
                assert(!ddp_is_tagged(ddp_message.hdr.bits));
                int found = rq->try_dequeue(wr);
                if (unlikely(!found))
                {
                    lwlog_err("no entry in receive request queue");
                    break;
                }
                wce.opcode = (enum wc_opcode) opcode;
                wce.byte_len = ret;
                wce.status = WC_SUCCESS;
                wce.wr_id = wr.wr_id;
                cq->enqueue(wce);
                break;
            }
            case rdma_opcode::RDMAP_TERMINATE: {
                assert(!ddp_is_tagged(ddp_message.hdr.bits));
                ctx->connected = 0;
                lwlog_err("Terminate received");
                break;
            }
        }
    }

    //! Graceful exit
    lwlog_info("Recv loop thread exiting");
    return NULL;
}

//! TODO: Add terminate
/* Main RNIC Send loop */
void* rnic_send(void* ctx_ptr)
{
    struct rdmap_stream_context* ctx = (struct rdmap_stream_context*)ctx_ptr;
    assert(ctx->send_q->wq_type == WQT_SQ);
    moodycamel::ConcurrentQueue<send_wr>* q = ctx->send_q->send_q;
    moodycamel::ConcurrentQueue<work_completion>* cq = ctx->send_q->cq->q;
    moodycamel::ConcurrentQueue<work_completion>* pending_cq = ctx->send_q->cq->pending_q;

    struct send_wr req;

    __u8 rdma_hdr = 1 << 6;
    struct work_completion wce;
    wce.src_qp = ctx->send_q->wq_num;
    while (ctx->connected)
    {
        int found = q->try_dequeue(req);
        if (!found) continue;
        
        rdma_hdr = (1 << 6) | req.opcode;
        switch(req.opcode)
        {
            case rdma_opcode::RDMAP_SEND_SE:
        	case rdma_opcode::RDMAP_SEND: {
                lwlog_info("Sending RDMAP Send Message");
                struct ddp_untagged_meta ddp_hdr;
                static __u32 msn = 1;

                ddp_hdr.rsvdULP1 = rdma_hdr;
                ddp_hdr.qn = htonl(SEND_QN);
                ddp_hdr.msn = htonl(msn++);

                int ret = ddp_send_untagged(ctx->ddp_ctx, &ddp_hdr, req.sg_list, req.num_sge);
                
                send_wr_to_wce(&req, &wce);
                wce.byte_len = ret;
                //! Notify completion queue
                if (unlikely(ret < 0))
                {
                    lwlog_err("RDMA Send failed %d", ret);
                    wce.status = WC_FATAL_ERR;
                }
                else 
                {
                    wce.status = WC_SUCCESS;
                }
                cq->enqueue(wce);
                break;
            }
            case rdma_opcode::RDMAP_SEND_SE_INVAL: 
            case rdma_opcode::RDMAP_SEND_INVAL: {
                struct ddp_untagged_meta ddp_hdr;
                static __u32 msn = 1;
                
                ddp_hdr.rsvdULP1 = rdma_hdr;
                ddp_hdr.rsvdULP2 = htonl(req.invalidate_rkey);
                ddp_hdr.qn = htonl(SEND_QN);
                ddp_hdr.msn = htonl(msn++);
                int ret = ddp_send_untagged(ctx->ddp_ctx, &ddp_hdr, req.sg_list, req.num_sge);
                
                send_wr_to_wce(&req, &wce);
                wce.byte_len = ret;
                //! Notify completion queue
                if (unlikely(ret < 0))
                {
                    wce.status = WC_FATAL_ERR;
                }
                else 
                {
                    wce.status = WC_SUCCESS;
                }
                cq->enqueue(wce);
                break;
            }
        	case rdma_opcode::RDMAP_RDMA_READ_REQ: {
                struct ddp_untagged_meta ddp_hdr;
                static __u32 msn = 1;
                ddp_hdr.rsvdULP1 = rdma_hdr;
                ddp_hdr.qn = htonl(READ_QN);
                ddp_hdr.msn = htonl(msn++);
                if (req.num_sge != 1)
                {
                    lwlog_err("Number of sge not 1 in read req (%d)", req.num_sge);
                    break;
                }

                //! For reads, sge_list contains the read lkey (stag)
                struct rdmap_read_req_fields fields;
                fields.sink_tag = req.sg_list[0].lkey;
                fields.sink_TO = req.sg_list[0].addr;
                fields.rdma_rd_sz = req.sg_list[0].length;
                fields.src_tag = req.wr.rdma.rkey;
                fields.src_TO = req.wr.rdma.remote_addr;
                struct sge message;
                message.addr = (uint64_t)&fields;
                message.length = sizeof(struct rdmap_read_req_fields);
                int ret = ddp_send_untagged(ctx->ddp_ctx, &ddp_hdr, &message, 1);
                
                send_wr_to_wce(&req, &wce);
                wce.byte_len = ret;
                //! Notify completion queue
                if (unlikely(ret < 0))
                {
                    wce.status = WC_FATAL_ERR;
                    cq->enqueue(wce);
                }
                else 
                {
                    wce.status = WC_SUCCESS;
                    //! Must be enqueued in `pending`
                    //! Shifted from `pending` to `cq` when read response is received
                    pending_cq->enqueue(wce);
                }
                break;
            }
	        case rdma_opcode::RDMAP_RDMA_WRITE: {
                struct ddp_tagged_meta ddp_hdr;
                ddp_hdr.rsvdULP1 = rdma_hdr;
                ddp_hdr.tag = htonl(req.wr.rdma.rkey);
                ddp_hdr.TO = htonll(req.wr.rdma.remote_addr);
                int ret = ddp_send_tagged(ctx->ddp_ctx, &ddp_hdr, req.sg_list, req.num_sge);
                
                send_wr_to_wce(&req, &wce);
                wce.byte_len = ret;
                //! Notify completion queue
                if (unlikely(ret < 0))
                {
                    lwlog_err("Write Request %lu failed", req.wr_id);
                    wce.status = WC_FATAL_ERR;
                }
                else 
                {
                    wce.status = WC_SUCCESS;
                }
                cq->enqueue(wce);
                break;
            }
            case rdma_opcode::RDMAP_RDMA_READ_RESP: {
                //! This is from a read request, and NOT from the user
                struct ddp_tagged_meta ddp_hdr;
                ddp_hdr.rsvdULP1 = rdma_hdr;
                ddp_hdr.tag = htonl(req.wr.rdma.rkey);
                ddp_hdr.TO = htonll(req.wr.rdma.remote_addr);
                int ret = ddp_send_tagged(ctx->ddp_ctx, &ddp_hdr, req.sg_list, req.num_sge);
                if (unlikely(ret < 0))
                {
                    lwlog_err("Read Response for request %lu failed", req.wr_id);
                }
                //! No work completion to be generated
                break;
            }
            default: {
                lwlog_err("Invalid request opcode: %d", req.opcode);
            }
        }
    }

    lwlog_info("Send loop thread exiting");
    return NULL;
}


//! Init DDP Stream, DDP Queue setup, recv/send thread start
struct rdmap_stream_context* rdmap_init_stream(struct rdmap_stream_init_attr* attr) {

    struct rdmap_stream_context* ctx = (struct rdmap_stream_context*) malloc(sizeof(struct rdmap_stream_context));

    //! Init DDP Stream
    ctx->ddp_ctx = ddp_init_stream(attr->sockfd, attr->pd);

    //! Fill context fields
    ctx->send_q = attr->send_q;
    ctx->recv_q = attr->recv_q;
    ctx->connected = 1;

    //! Init Read Untagged Buffers
    for (int i = 0; i < attr->max_pending_read_requests; i++)
    {
        struct untagged_buffer buf;
        buf.data = (char*) malloc(sizeof(struct rdmap_read_req_fields));
        buf.len = sizeof(struct rdmap_read_req_fields);
        ddp_post_recv(ctx->ddp_ctx, READ_QN, &buf, 1);
    }

    //! Receive Thread
    int ret = pthread_create(&ctx->recv_thread, NULL, rnic_recv, ctx);
    if (ret != 0)
    {
        lwlog_err("Couldn't create receive thread (%d)", ret);
        return NULL;
    }

    //! Send Thread
    ret = pthread_create(&ctx->send_thread, NULL, rnic_send, ctx);
    if (ret != 0)
    {
        lwlog_err("Couldn't create send thread (%d)", ret);
        return NULL;
    }

    return ctx;
}

//! Free ddp stream structures, kill threads, 
void rdmap_kill_stream(struct rdmap_stream_context* ctx) {
    ddp_kill_stream(ctx->ddp_ctx);

    //! Join with threads
    ctx->connected = 0;

    pthread_join(ctx->recv_thread, NULL);
    pthread_join(ctx->send_thread, NULL);

    //! TODO: Free read request buffers
}

//! Not overriding tagged buffer registration functions
//! Use rdmap_stream_context->ddp_ctx for those


int rdmap_send(struct rdmap_stream_context* ctx, struct send_wr& wr)
{
    return ctx->send_q->send_q->enqueue(wr);
}

int rdmap_write(struct rdmap_stream_context* ctx, struct send_wr& wr)
{
    assert(wr.opcode == rdma_opcode::RDMAP_RDMA_WRITE);
    return ctx->send_q->send_q->enqueue(wr);
}

int rdmap_read(struct rdmap_stream_context* ctx, struct send_wr& wr)
{
    assert(wr.opcode == rdma_opcode::RDMAP_RDMA_READ_REQ);
    return ctx->send_q->send_q->enqueue(wr);
}

/**
 * @brief 
 * 
 * @param ctx 
 * @param buf 
 * @return int 
 */
int rdma_post_recv(struct rdmap_stream_context* ctx, struct recv_wr& wr)
{
    //! Add to untagged buffer
    struct untagged_buffer buf[wr.num_sge];
    for (int i = 0; i < wr.num_sge; i++)
    {
        buf[i].data = (char *)wr.sg_list[i].addr;
        buf[i].len = wr.sg_list[i].length;
    }

    ddp_post_recv(ctx->ddp_ctx, SEND_QN, buf, wr.num_sge);

    //! Add to queue
    ctx->recv_q->recv_q->enqueue(std::move(wr));

    return 0;
}

int rdma_register(struct rdmap_stream_context* ctx, struct tagged_buffer* buf)
{
    return register_tagged_buffer(ctx->ddp_ctx, buf);
}
int rdma_invalidate(struct rdmap_stream_context* ctx, struct stag_t* stag)
{
    return deregister_tagged_buffer(ctx->ddp_ctx, stag);
}
