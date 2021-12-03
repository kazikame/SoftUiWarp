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
#include <arpa/inet.h>
#include "ddp/ddp.h"
#include "mpa/mpa.h"

struct ddp_stream_context* ddp_init_stream(int sockfd, struct pd_t* pd)
{
    struct ddp_stream_context* ctx = new ddp_stream_context();
    ctx->sockfd = sockfd;
    ctx->pd = pd;

    ctx->queues = new untagged_buffer_queue[MAX_UNTAGGED_BUFFERS];
    for (int i = 0; i < MAX_UNTAGGED_BUFFERS; i++)
    {
        ctx->queues[i].q = new moodycamel::ConcurrentQueue<struct untagged_buffer>();
    }
    return ctx;
}

void ddp_kill_stream(struct ddp_stream_context* ctx) 
{
    delete[] ctx->queues;
    delete ctx;

    return;
}

//! TODO: This can be made more efficient by merging multiple sge into a single MPA packet
int ddp_send_tagged(struct ddp_stream_context* ctx, struct ddp_tagged_meta* next_hdr, sge* sge_list, int num_sge)
{
    struct ddp_hdr hdr;
    hdr.bits |= DDP_HDR_T;
    hdr.bits |= 1;
    
    struct ddp_tagged_meta hdr2;
    hdr2 = *next_hdr;

    sge mpa_sge_list[2 + num_sge];
    int sg_num = 0;

    mpa_sge_list[sg_num].addr = (uint64_t)&hdr;
    mpa_sge_list[sg_num].length = sizeof(hdr);
    sg_num++;

    mpa_sge_list[sg_num].addr = (uint64_t)&hdr2;
    mpa_sge_list[sg_num].length = sizeof(hdr2);
    sg_num++;

    __u64 ho_TO = ntohll(hdr2.TO);
    for (int i = 0; i < num_sge; i++)
    {
        int last_sge = i == (num_sge-1);

        //! Break each sge into MPA segments
        int remaining_bytes = sge_list[i].length;

        while (remaining_bytes > 0)
        {
            int packet_len = remaining_bytes > MULPDU ? MULPDU : remaining_bytes;

            if (last_sge && packet_len == remaining_bytes)
            {
                hdr.bits |= DDP_FLAG_LAST;
            }
            mpa_sge_list[sg_num].addr = (uint64_t) (sge_list[i].addr + (sge_list[i].length - remaining_bytes));
            mpa_sge_list[sg_num].length = packet_len;
            int ret = mpa_send(ctx->sockfd, mpa_sge_list, sg_num+1, 0);
            if (unlikely(ret < 0))
            {
                lwlog_err("send failed");
                return ret;
            }
            //! Increase offset
            ho_TO += packet_len;
            hdr2.TO = htonll(ho_TO);
            remaining_bytes -= packet_len;
        }
    }

    return 0;
}

//! TODO: This can be made more efficient by merging multiple sge into a single MPA packet
int ddp_send_untagged(struct ddp_stream_context* ctx, struct ddp_untagged_meta* next_hdr, sge* sge_list, int num_sge)
{
    lwlog_info("Sending DDP Untagged Message");
    struct ddp_hdr hdr;
    hdr.bits |= 1;
    
    struct ddp_untagged_meta hdr2;
    hdr2 = *next_hdr;
    hdr2.mo = 0;

    sge mpa_sge_list[2 + num_sge];
    int sg_num = 0;

    mpa_sge_list[sg_num].addr = (uint64_t)&hdr;
    mpa_sge_list[sg_num].length = sizeof(hdr);
    sg_num++;

    mpa_sge_list[sg_num].addr = (uint64_t)&hdr2;
    mpa_sge_list[sg_num].length = sizeof(hdr2);
    sg_num++;

    for (int i = 0; i < num_sge; i++)
    {
        int last_sge = i == (num_sge-1);

        //! Break each sge into MPA segments
        int remaining_bytes = sge_list[i].length;
        while (remaining_bytes > 0)
        {
            int packet_len = remaining_bytes > MULPDU ? MULPDU : remaining_bytes;
            if (last_sge && packet_len == remaining_bytes)
            {
                hdr.bits |= DDP_FLAG_LAST;
            }
            
            mpa_sge_list[sg_num].addr = (uint64_t) (sge_list[i].addr + (sge_list[i].length - remaining_bytes));
            mpa_sge_list[sg_num].length = packet_len;
            int ret = mpa_send(ctx->sockfd, mpa_sge_list, sg_num+1, 0);
            if (ret < 0)
            {
                lwlog_err("send failed: %d", ret);
                return ret;
            }
            //! Increase offset
            hdr2.mo += packet_len;
            remaining_bytes -= packet_len;
        }
    }

    return 0;
}

void ddp_post_recv(struct ddp_stream_context* ctx, int qn, struct untagged_buffer* bufs, int num_bufs)
{
    if (unlikely(qn < 0 || qn >= MAX_UNTAGGED_BUFFERS))
    {
        lwlog_err("qn is invalid %d", qn);
        return;
    } 

    ctx->queues[qn].q->enqueue_bulk(bufs, num_bufs);
}

struct untagged_buffer_queue* ddp_check_untagged_hdr(struct ddp_stream_context* ctx, struct ddp_untagged_meta* hdr)
{
    uint32_t qn = hdr->qn;
    if (unlikely(qn >= MAX_UNTAGGED_BUFFERS))
    {
        lwlog_err("Invalid queue detected %d", qn);
        return NULL;
    }

    //! UT represent!
    struct untagged_buffer_queue* ut_q = &ctx->queues[qn];

    uint32_t msn = hdr->msn;
    if (unlikely(ut_q->msn > msn))
    {
        lwlog_err("Invalid MSN detected %d (current %d)", msn, ut_q->msn);
        return NULL;
    }

    //! TODO: Add MO check

    return ut_q;
}

//! TODO: receive should timeout after a certain threshold
//! TODO: support gather receives
int ddp_recv(struct ddp_stream_context* ctx, struct ddp_message* msg)
{
    int ddp_payload_len = 0;

    siw_mpa_packet mpa_packet;
    mpa_packet.ulpdu = (char*) &msg->hdr.bits;

    //! Get DDP Control Header
    int ret = mpa_recv(ctx->sockfd, &mpa_packet, DDP_CTRL_SIZE);
    if (unlikely(ret < DDP_CTRL_SIZE))
    {
        lwlog_err("ddp recv failed %d", ret);
        return ret;
    }
    
    if (msg->hdr.bits & DDP_HDR_T)
    {
        //! Tagged
        lwlog_info("DDP Tagged Header detected");
        
        int mpa_payload_len = mpa_packet.ulpdu_len - (DDP_CTRL_SIZE + DDP_TAGGED_HDR_SIZE);
        //! Check if remaining length has data
        if (unlikely(mpa_payload_len < 0))
        {
            lwlog_err("mpa packet is smaller than ddp tagged header");
            return -1;
        }

        //! Get remaining header
        mpa_packet.ulpdu = (char*) &msg->tagged_metadata;

        ret = mpa_recv(ctx->sockfd, &mpa_packet, DDP_TAGGED_HDR_SIZE);
        if (unlikely(ret < 0))
        {
            lwlog_err("ddp recv failed %d", ret);
            return ret;
        }

        //! Check stag validity and get the tagged buffer
        msg->tagged_metadata.tag = ntohl(msg->tagged_metadata.tag);
        msg->tagged_metadata.TO = ntohll(msg->tagged_metadata.TO);

        struct tagged_buffer* ptr = ddp_check_stag(ctx, &msg->tagged_metadata);
        if (unlikely(ptr == NULL))
        {
            return -1;
        }
        msg->tag_buf = *ptr;

        //! Get entire payload
        uint32_t orig_stag = msg->tagged_metadata.tag;
        uint64_t orig_TO = msg->tagged_metadata.TO;

        struct ddp_hdr_packed packed_hdr;
        packed_hdr.hdr.bits = msg->hdr.bits;
        packed_hdr.tagged_metadata.TO = msg->tagged_metadata.TO;
        while(true)
        {
            //! Copy current payload to the right path
            mpa_packet.ulpdu = (char *) packed_hdr.tagged_metadata.TO;
            ret = mpa_recv(ctx->sockfd, &mpa_packet, mpa_payload_len);
            ddp_payload_len += mpa_payload_len;
            
            if (unlikely(ret < 0)) return -1;
            
            //! Check if this was the last
            if (packed_hdr.hdr.bits & DDP_FLAG_LAST) {
                break;
            }

            //! Get next header
            mpa_packet.bytes_rcvd = 0;
            mpa_packet.ulpdu = (char*) &packed_hdr;
            ret = mpa_recv(ctx->sockfd, &mpa_packet, DDP_CTRL_SIZE + DDP_TAGGED_HDR_SIZE);

            if (unlikely(ret < 0)) return -1;

            mpa_payload_len = mpa_packet.ulpdu_len - (DDP_CTRL_SIZE + DDP_TAGGED_HDR_SIZE);
            packed_hdr.tagged_metadata.TO = ntohll(packed_hdr.tagged_metadata.TO);
            //! TODO: Check TO/Stag validity
        }

        msg->tagged_metadata.TO = orig_TO;
    }
    else
    {
        //! Untagged
        lwlog_debug("DDP Untagged Header detected w/ len %d", mpa_packet.ulpdu_len);

        int mpa_payload_len = mpa_packet.ulpdu_len - (DDP_CTRL_SIZE + DDP_UNTAGGED_HDR_SIZE);
        //! Check if remaining length has data
        if (unlikely(mpa_payload_len < 0))
        {
            lwlog_err("mpa packet is smaller than ddp untagged header");
            return -1;
        }

        //! Get remaining header
        mpa_packet.ulpdu = (char*) &msg->untagged_metadata;

        ret = mpa_recv(ctx->sockfd, &mpa_packet, DDP_UNTAGGED_HDR_SIZE);
        if (unlikely(ret < 0))
        {
            lwlog_err("ddp recv failed %d", ret);
            return ret;
        }

        //! NTOHL :(
        msg->untagged_metadata.mo = ntohl(msg->untagged_metadata.mo);
        msg->untagged_metadata.qn = ntohl(msg->untagged_metadata.qn);
        msg->untagged_metadata.msn = ntohl(msg->untagged_metadata.msn);

        lwlog_debug("MO %u QN %u MSN %u", msg->untagged_metadata.mo, msg->untagged_metadata.qn, msg->untagged_metadata.msn);
        //! Check untagged header validty
        struct untagged_buffer_queue* ut_q = ddp_check_untagged_hdr(ctx, &msg->untagged_metadata);
        if (unlikely(ut_q == NULL))
        {
            return -1;
        }

        int found = ut_q->q->try_dequeue(msg->untag_buf);
        if (unlikely(!found))
        {
            lwlog_err("untagged buffer queue is empty");
        }

        struct ddp_hdr_packed packed_hdr;
        packed_hdr.hdr.bits = msg->hdr.bits;
        packed_hdr.untagged_metadata.mo = msg->untagged_metadata.mo;
        //! Get entire payload
        while(true)
        {
            //! Copy current payload to the right path
            mpa_packet.ulpdu = (char *) ((uint64_t)msg->untag_buf.data + packed_hdr.untagged_metadata.mo);
            ret = mpa_recv(ctx->sockfd, &mpa_packet, mpa_payload_len);
            ddp_payload_len += mpa_payload_len;
            if (unlikely(ret < 0)) return -1;
            
            //! Check if this was the last
            if (packed_hdr.hdr.bits & DDP_FLAG_LAST) break;

            //! Get next header
            mpa_packet.bytes_rcvd = 0;
            mpa_packet.ulpdu = (char *) &packed_hdr;
            ret = mpa_recv(ctx->sockfd, &mpa_packet, DDP_CTRL_SIZE + DDP_UNTAGGED_HDR_SIZE);

            if (unlikely(ret < 0)) return -1;
            mpa_payload_len = mpa_packet.ulpdu_len - (DDP_CTRL_SIZE + DDP_UNTAGGED_HDR_SIZE);

            packed_hdr.untagged_metadata.mo = ntohl(packed_hdr.untagged_metadata.mo);
            //! TODO: Check QN/MSN/MO validity
        }

        msg->untagged_metadata.mo = 0;
    }

    msg->len = ddp_payload_len;
    return 0;
}

int register_tagged_buffer(struct ddp_stream_context* ctx, struct tagged_buffer* buf) 
{
    buf->stag.tag = (__u32)((__u64)buf->data);
    lwlog_info("Registering tagged buffer with stag %u (pointer %u)", buf->stag.tag, (__u32)((__u64)buf->data));
    ctx->tagged_buffers[buf->stag.tag] = (*buf);
    return 0;
}

int deregister_tagged_buffer(struct ddp_stream_context* ctx, struct stag_t* stag)
{
    int ret = ctx->tagged_buffers.erase(stag->tag);
    return ret;
}

//! C++ function, bad
struct tagged_buffer* ddp_check_stag(struct ddp_stream_context* ctx, struct ddp_tagged_meta* hdr)
{
    auto it = ctx->tagged_buffers.find(hdr->tag);
    if (unlikely(it == ctx->tagged_buffers.end()))
    {
        lwlog_err("ddp recv found invalid tag");
        return NULL;
    }

    if (unlikely((uint64_t)it->second.data > hdr->TO || (uint64_t)it->second.data + it->second.len < hdr->TO))
    {
        lwlog_err("invalid offset TO: %llu, data: %lu, len: %lu", hdr->TO, (uint64_t)it->second.data, it->second.len);
        return NULL;
    }
    //! TODO: Do access control
    return &it->second;
}
