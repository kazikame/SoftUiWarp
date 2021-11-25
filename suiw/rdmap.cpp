#include "buffer.h"
#include "rdmap.h"
#include "ddp_new.h"
#include "lwlog.h"


//! Init DDP Stream, DDP Queue setup, recv/send thread start
struct rdmap_stream_context* rdmap_init_stream(struct rdmap_stream_init_attr* attr) {

    struct rdmap_stream_context* ctx = (struct rdmap_stream_context*) malloc(sizeof(struct rdmap_stream_context));

    //! Init DDP Stream
    ctx->ddp_ctx = ddp_init_stream(attr->sockfd, attr->pd);

    

}

//! Free ddp stream structures, kill threads, 
void rdmap_kill_stream(struct rdmap_stream_context* ctx);

//! Not overriding tagged buffer registration functions
//! Use rdmap_stream_context->ddp_ctx for those


int rdmap_send(struct rdmap_stream_context*, void* message, __u32 len, stag_t* invalidate_stag, int flags);
int rdmap_write(struct rdmap_stream_context*, void* message, __u32 len, stag_t* stag, __u32 offset);
int rdmap_read(struct rdmap_stream_context*, __u32 len, stag_t* src_stag, __u32 src_offset, stag_t* sink_stag, __u32 sink_offset);

int rdma_post_recv(struct rdmap_stream_context*, struct untagged_buffer* buf);