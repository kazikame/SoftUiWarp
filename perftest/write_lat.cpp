#include "perftest.h"
#include "rdmap/rdmap.h"

#include <x86intrin.h>

struct sge write_sg;
struct send_wr write_wr;

int write_lat_init(perftest_context *perftest_ctx, uint32_t lstag, struct send_data *sd) {
    // Build read WR.
    write_sg.addr = (uint64_t) perftest_ctx->buf;
    write_sg.length = perftest_ctx->buf_size;
    write_sg.lkey = lstag;
    write_wr.wr_id = 2;
    write_wr.sg_list = &write_sg;
    write_wr.num_sge = 1;
    write_wr.opcode = RDMAP_RDMA_WRITE;
    write_wr.wr.rdma.rkey = sd->stag;
    write_wr.wr.rdma.remote_addr = sd->offset;
    if (!perftest_ctx->is_client) {
        // Fill the server's region with incrementing values.
        for (int i = 0; i < perftest_ctx->buf_size; i++) {
            perftest_ctx->buf[i] = (char) i;
        }
    }
    return 0;
}

int write_lat_iter(perftest_context *perftest_ctx) {
    int ret;
    // Client clears buffer and waits to receive write.
    if (perftest_ctx->is_client) {
#ifdef PERFTEST_TEST
        // Clear the whole buffer if we're testing.
        memset(perftest_ctx->buf, 0, perftest_ctx->buf_size);
#else
        // Only clear a byte if we're benchmarking.
        perftest_ctx->buf[1] = (char) 0;
#endif
        lwlog_debug("waiting for write at ...");
        do { _mm_clflush(&perftest_ctx->buf); } while (perftest_ctx->buf[1] == 0) ;
        lwlog_debug("found write!");
#ifdef PERFTEST_TEST
        for (int i = 0; i < perftest_ctx->buf_size; i++) {
            if (perftest_ctx->buf[i] != ((char) i)) {
                lwlog_err("Received incorrect value 0x%hhx for index %d!", perftest_ctx->buf[i], i);
                return -1;
            }
        }
#endif // PERFTEST_TEST
    }

    // Write the buffer to the remote.
    lwlog_debug("issuing write ...");
    ret = rdmap_write(perftest_ctx->ctx, write_wr);
    if (ret < 0) {
        lwlog_err("Failed to issue RDMA Write!");
        return -1;
    }
    lwlog_debug("completed write");
    struct work_completion wc;
    auto write_cq = perftest_ctx->ctx->send_q->cq->q;
    do { ret = write_cq->try_dequeue(wc); } while (!ret) ;
    lwlog_debug("received completion");
    if (wc.status != WC_SUCCESS) {
        lwlog_err("Received remote send with error");
        return -1;
    } else if (wc.opcode != WC_WRITE) {
        lwlog_err("Received wrong message type!");
        return -1;
    }

    // Server clears buffer and waits to receive write.
    if (!perftest_ctx->is_client) {
#ifdef PERFTEST_TEST
        // Clear the whole buffer if we're testing.
        memset(perftest_ctx->buf, 0, perftest_ctx->buf_size);
#else
        // Only clear a byte if we're benchmarking.
        perftest_ctx->buf[1] = (char) 0;
#endif
        lwlog_debug("waiting for write ...");
        do { _mm_clflush(&perftest_ctx->buf); } while (perftest_ctx->buf[1] == 0) ;
        lwlog_debug("found write!");
#ifdef PERFTEST_TEST
        for (int i = 0; i < perftest_ctx->buf_size; i++) {
            if (perftest_ctx->buf[i] != ((char) i)) {
                lwlog_err("Received incorrect value 0x%hhx for index %d!", perftest_ctx->buf[i], i);
                return -1;
            }
        }
#endif // PERFTEST_TEST
    }
    return 0;
}

void write_lat_fini(perftest_context *ctx, float time_s) {}

int main(int argc, char **argv) {
    perftest_run(argc, argv, write_lat_init, write_lat_iter, write_lat_fini);
}
