#include "perftest.h"
#include "rdmap/rdmap.h"

struct sge read_sg;
struct send_wr read_wr;

struct sge *read_sgs;

int read_bw_init(perftest_context *perftest_ctx, uint32_t lstag, struct send_data *sd) {
    // Build read WR.
    read_sgs = (struct sge*) malloc(sizeof(struct sge)*perftest_ctx->max_reqs);
    for (int i = 0; i < perftest_ctx->max_reqs; i++) {
        read_sgs[i].addr = (uint64_t) perftest_ctx->buf;
        read_sgs[i].length = perftest_ctx->buf_size;
        read_sgs[i].lkey = lstag;
    }
    read_wr.wr_id = 2;
    read_wr.sg_list = read_sgs;
    read_wr.num_sge = 1;
    read_wr.opcode = RDMAP_RDMA_READ_REQ;
    read_wr.wr.rdma.rkey = sd->stag;
    read_wr.wr.rdma.remote_addr = sd->offset;
    // Fill the client's region with incrementing values.
    if (perftest_ctx->is_client) {
        for (int i = 0; i < perftest_ctx->buf_size; i++) {
            perftest_ctx->buf[i] = (char) i;
        }
    }
    return 0;
}

int read_bw_iter(perftest_context *perftest_ctx) {
    // Client does nothing.
    if (perftest_ctx->is_client) {
        return 0;
    }
    // Server issues reads to client.
    int ret;
    for (int i = 0; i < perftest_ctx->max_reqs; i++) {
        ret = rdmap_read(perftest_ctx->ctx, read_wr);
        read_wr.wr_id++;
        if (ret < 0) {
            lwlog_err("Failed to issue RDMA READ!");
            return -1;
        }
    }
    lwlog_debug("completed reads");
    struct work_completion wc;
    auto read_cq = perftest_ctx->ctx->send_q->cq->q;
    for (int i = 0; i < perftest_ctx->max_reqs; i++) {
        do { ret = read_cq->try_dequeue(wc); } while (!ret) ;
        lwlog_debug("received completion");
        if (wc.status != WC_SUCCESS) {
            lwlog_err("Received remote send with error");
            return -1;
        } else if (wc.opcode != WC_READ_REQUEST) {
            lwlog_err("Received wrong message type!");
            return -1;
        }
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

void read_bw_fini(perftest_context *perftest_ctx, float time_s) {
    uint64_t num_requests = perftest_ctx->max_reqs * perftest_ctx->iters;
    float Mpps = num_requests / (time_s * 1000.0 * 1000.0);
    float MBps = (num_requests * perftest_ctx->buf_size) / (time_s * 1000.0 * 1000.0);
    lwlog_notice("Message rate (Mp/s): %f", Mpps);
    lwlog_notice("Bandwidth (MB/s): %f", MBps);
    free(read_sgs);
}

int main(int argc, char **argv) {
    perftest_run(argc, argv, read_bw_init, read_bw_iter, read_bw_fini);
}
