#include "buffer.h"
#include "rdmap.h"
#include "ddp.h"
#include "lwlog.h"
#include "pthread.h"
#include <arpa/inet.h>

//! Main receive loop run in a separate thread
void* rnic_recv(void* ctx_ptr)
{
    struct rdmap_stream_context* ctx = (struct rdmap_stream_context*)ctx_ptr;
    struct ddp_message ddp_message;
    struct rdmap_message message;
    while(ctx->connected)
    {
        int ret = ddp_recv(ctx->ddp_ctx, &ddp_message);
        if (ret < 0)
        {
            lwlog_err("ddp_recv error, exiting");
            return NULL;
        }
        message.ctrl = (rdmap_ctrl*)((char *)&ddp_message.hdr + DDP_CTRL_SIZE);
        
        switch(get_rdmap_op(message.ctrl->bits))
        {
            case rdma_opcode::RDMAP_RDMA_WRITE: {
                assert(ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdma_opcode::RDMAP_RDMA_READ_REQ: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdma_opcode::RDMAP_RDMA_READ_RESP: {
                assert(ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdma_opcode::RDMAP_SEND: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdma_opcode::RDMAP_SEND_INVAL: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdma_opcode::RDMAP_SEND_SE: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdma_opcode::RDMAP_SEND_SE_INVAL: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdma_opcode::RDMAP_TERMINATE: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
        }
    }

    //! Graceful exit
    return NULL;
}

/* Main RNIC Send loop */
void* rnic_send(void* ctx_ptr)
{
    struct rdmap_stream_context* ctx = (struct rdmap_stream_context*)ctx_ptr;
    assert(ctx->send_q->wq_type == WQT_SQ);
    moodycamel::ConcurrentQueue<send_wr>* q = ctx->send_q->send_q;
    moodycamel::ConcurrentQueue<work_completion>* cq = ctx->send_q->cq->q;
    send_wr req;

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
                if (ret < 0)
                {
                    wce.status = WC_SUCCESS;
                }
                else 
                {
                    wce.status = WC_FATAL_ERR;
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
                if (ret < 0)
                {
                    wce.status = WC_SUCCESS;
                }
                else 
                {
                    wce.status = WC_FATAL_ERR;
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
                fields.src_tag = req.wr.rdma.rkey;
                fields.src_TO = req.wr.rdma.remote_addr;
                struct sge message;
                message.addr = (uint64_t)&fields;
                message.length = sizeof(struct rdmap_read_req_fields);
                int ret = ddp_send_untagged(ctx->ddp_ctx, &ddp_hdr, &message, 1);
                
                send_wr_to_wce(&req, &wce);
                wce.byte_len = ret;
                //! Notify completion queue
                if (ret < 0)
                {
                    wce.status = WC_SUCCESS;
                }
                else 
                {
                    wce.status = WC_FATAL_ERR;
                }
                cq->enqueue(wce);
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
                if (ret < 0)
                {
                    wce.status = WC_SUCCESS;
                }
                else 
                {
                    wce.status = WC_FATAL_ERR;
                }
                cq->enqueue(wce);
                break;
            }
            default: {
                lwlog_err("Invalid request opcode: %d", req.opcode);
            }
        }
    }

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
        ddp_post_recv(ctx->ddp_ctx, READ_QN, &buf);
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


int rdmap_send(struct rdmap_stream_context* ctx, struct send_wr&& wr)
{
    return ctx->send_q->send_q->enqueue(std::move(wr));
}

int rdmap_write(struct rdmap_stream_context* ctx, struct send_wr&& wr)
{
    assert(wr.opcode == rdma_opcode::RDMAP_RDMA_WRITE);
    return ctx->send_q->send_q->enqueue(std::move(wr));
}

int rdmap_read(struct rdmap_stream_context* ctx, struct send_wr&& wr)
{
    assert(wr.opcode == rdma_opcode::RDMAP_RDMA_READ_REQ);
    return ctx->send_q->send_q->enqueue(std::move(wr));
}

int rdma_post_recv(struct rdmap_stream_context*, struct untagged_buffer* buf)
{
    return -1;
}