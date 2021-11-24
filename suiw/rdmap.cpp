#include "rdmap.h"

#include "lwlog.h"


//! Init DDP Stream, Queue setup, recv/send thread start
int rdmap_init_stream(struct rdmap_stream_context* ctx) {
    ctx->ddp_ctx = ddp_init_stream(ctx->sockfd, ctx->pd);

    //! Register untagged queues
}

//! Free ddp stream structures
void rdmap_kill_stream(struct rdmap_stream_context* ctx);

//! Not overriding tagged buffer registration functions
//! Use rdmap_stream_context->ddp_ctx for those


int rdmap_send(struct rdmap_stream_context*, void* message, __u32 len, stag_t* invalidate_stag, int flags);
int rdmap_write(struct rdmap_stream_context*, void* message, __u32 len, stag_t* stag, __u32 offset);
int rdmap_read(struct rdmap_stream_context*, __u32 len, stag_t* src_stag, __u32 src_offset, stag_t* sink_stag, __u32 sink_offset);
