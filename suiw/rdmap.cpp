#include "buffer.h"
#include "rdmap.h"
#include "ddp_new.h"
#include "lwlog.h"

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
            case rdmap_opcode::WRITE: {
                assert(ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdmap_opcode::READ_REQUEST: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdmap_opcode::READ_RESPONSE: {
                assert(ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdmap_opcode::SEND: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdmap_opcode::SEND_INVALIDATE: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdmap_opcode::SEND_SOLICIT: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdmap_opcode::SEND_SOLICIT_INVALIDATE: {
                assert(!ddp_is_tagged(ddp_message.hdr->bits));
                break;
            }
            case rdmap_opcode::TERMINATE: {
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

        rdma_hdr |= req.opcode;
        switch(req.opcode)
        {
            case SEND_SOLICIT:
        	case SEND: {
                struct ddp_untagged_meta ddp_hdr;
                
                ddp_hdr.rsvdULP1 = rdma_hdr;
                ddp_hdr.qn = SEND_QN;
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
            case SEND_SOLICIT_INVALIDATE: 
            case SEND_INVALIDATE: {
                struct ddp_untagged_meta ddp_hdr;
                
                ddp_hdr.rsvdULP1 = rdma_hdr;
                ddp_hdr.rsvdULP2 = req.invalidate_rkey;
                ddp_hdr.qn = SEND_QN;
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
        	case READ_REQUEST: {
                break;
            }
	        case WRITE: {
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
}

//! Free ddp stream structures, kill threads, 
void rdmap_kill_stream(struct rdmap_stream_context* ctx) {
    ddp_kill_stream(ctx->ddp_ctx);

    //! Join with threads
    ctx->connected = 0;

    pthread_join(ctx->recv_thread, NULL);
    pthread_join(ctx->send_thread, NULL);
}

//! Not overriding tagged buffer registration functions
//! Use rdmap_stream_context->ddp_ctx for those


int rdmap_send(struct rdmap_stream_context*, void* message, __u32 len, stag_t* invalidate_stag, int flags);
int rdmap_write(struct rdmap_stream_context*, void* message, __u32 len, stag_t* stag, __u32 offset);
int rdmap_read(struct rdmap_stream_context*, __u32 len, stag_t* src_stag, __u32 src_offset, stag_t* sink_stag, __u32 sink_offset);

int rdma_post_recv(struct rdmap_stream_context*, struct untagged_buffer* buf);