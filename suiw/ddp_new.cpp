#include "ddp_new.h"
#include "mpa/mpa.h"

struct ddp_stream_context* ddp_init_stream(int sockfd, struct pd_t* pd)
{
    struct ddp_stream_context* ctx = new ddp_stream_context();
    ctx->sockfd = sockfd;
    ctx->pd = pd;

    ctx->queues = new untagged_buffer_queue[MAX_UNTAGGED_BUFFERS];
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

    for (int i = 0; i < num_sge; i++)
    {
        int last_sge = i == num_sge;

        //! Break each sge into MPA segments
        int remaining_bytes = sge_list[i].length;

        while (remaining_bytes > 0)
        {
            int packet_len = remaining_bytes > EMSS ? EMSS : remaining_bytes;
            mpa_sge_list[sg_num].addr = (uint64_t) (sge_list[i].addr + (sge_list[i].length - remaining_bytes));
            mpa_sge_list[sg_num].length = packet_len;
            int ret = mpa_send(ctx->sockfd, mpa_sge_list, sg_num+1, 0);
            if (ret < 0)
            {
                lwlog_err("send failed");
                return ret;
            }
            //! Increase offset
            hdr2.TO += packet_len;
            remaining_bytes -= packet_len;
        }
    }
}

//! TODO: This can be made more efficient by merging multiple sge into a single MPA packet
int ddp_send_untagged(struct ddp_stream_context* ctx, struct ddp_untagged_meta* next_hdr, sge* sge_list, int num_sge)
{
    struct ddp_hdr hdr;
    hdr.bits |= DDP_HDR_T;
    hdr.bits |= 1;
    
    struct ddp_untagged_meta hdr2;
    hdr2 = *next_hdr;

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
        int last_sge = i == num_sge;

        //! Break each sge into MPA segments
        int remaining_bytes = sge_list[i].length;

        while (remaining_bytes > 0)
        {
            int packet_len = remaining_bytes > EMSS ? EMSS : remaining_bytes;
            mpa_sge_list[sg_num].addr = (uint64_t) (sge_list[i].addr + (sge_list[i].length - remaining_bytes));
            mpa_sge_list[sg_num].length = packet_len;
            int ret = mpa_send(ctx->sockfd, mpa_sge_list, sg_num+1, 0);
            if (ret < 0)
            {
                lwlog_err("send failed");
                return ret;
            }
            //! Increase offset
            hdr2.mo += packet_len;
            remaining_bytes -= packet_len;
        }
    }
}

void ddp_post_recv(struct ddp_stream_context* ctx, int qn, struct untagged_buffer* buf)
{
    if (qn < 0 || qn >= MAX_UNTAGGED_BUFFERS)
    {
        lwlog_err("qn is invalid %d", qn);
        return;
    } 

    ctx->queues[qn].q.enqueue(*buf);
}

int ddp_recv(struct ddp_stream_context*, struct ddp_message*)
{
    return -1;
}

int register_tagged_buffer(struct ddp_stream_context* ctx, struct tagged_buffer* buf) 
{
    buf->stag.tag = (__u32)((__u64)buf->data);
    ctx->tagged_buffers[buf->stag.tag] = (*buf);
    return 0;
}

void deregister_tagged_buffer(struct ddp_stream_context* ctx, struct stag_t* stag)
{
    ctx->tagged_buffers.erase(stag->tag);
}
