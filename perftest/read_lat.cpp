#include "perftest.h"
#include "rdmap/rdmap.h"

struct sge read_sg;
struct send_wr read_wr;

int read_lat_init(perftest_context *perftest_ctx, uint32_t lstag, struct send_data *sd) {
    // Build read WR.
    read_sg.addr = ntohll((uint64_t) perftest_ctx->buf);
    read_sg.length = ntohl(perftest_ctx->buf_size);
    read_sg.lkey = ntohl(lstag);
    read_wr.wr_id = 2;
    read_wr.sg_list = &read_sg;
    read_wr.num_sge = 1;
    read_wr.opcode = RDMAP_RDMA_READ_REQ;
    read_wr.wr.rdma.rkey = ntohl(sd->stag);
    read_wr.wr.rdma.remote_addr = ntohll(sd->offset);
    return 0;
}

int read_lat_iter(perftest_context *perftest_ctx) {
    // Client does nothing.
    if (perftest_ctx->is_client) {
        return 0;
    }
    // Server issues reads to client.
    int ret;
    ret = rdmap_read(perftest_ctx->ctx, read_wr);
    if (ret < 0) {
        lwlog_err("Failed to issue RDMA READ!");
        return -1;
    }
    lwlog_debug("completed read");
    struct work_completion wc;
    auto read_cq = perftest_ctx->ctx->send_q->cq->q;
    do { ret = read_cq->try_dequeue(wc); } while (!ret) ;
    lwlog_debug("received completion");
    if (wc.status != WC_SUCCESS) {
        lwlog_err("Received remote send with error");
        return -1;
    } else if (wc.opcode != WC_READ_REQUEST) {
        lwlog_err("Received wrong message type!");
        return -1;
    }
#ifdef PERFTEST_TEST
    for (int i = 0; i < perftest_ctx->buf_size; i++) {
        if (perftest_ctx->buf[i] != ((char) i)) {
            lwlog_err("Received incorrect value 0x%hhx for index %d!", perftest_ctx->buf[i], i);
            return -1;
        }
    }
    memset(perftest_ctx->buf, 0, perftest_ctx->buf_size);
#endif // PERFTEST_TEST
    return 0;
}

int main(int argc, char **argv) {
    perftest_run(argc, argv, read_lat_init, read_lat_iter);
}
