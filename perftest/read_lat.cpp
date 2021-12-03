/*
 * Software Userspace iWARP device driver for Linux 
 *
 * MIT License
 * 
 * Copyright (c) 2021 Saksham Goel, Matthew Pabst
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "perftest.h"
#include "rdmap/rdmap.h"

struct sge read_sg;
struct send_wr read_wr;

int read_lat_init(perftest_context *perftest_ctx, uint32_t lstag, struct send_data *sd) {
    // Build read WR.
    read_sg.addr = (uint64_t) perftest_ctx->buf;
    read_sg.length = perftest_ctx->buf_size;
    read_sg.lkey = lstag;
    read_wr.wr_id = 2;
    read_wr.sg_list = &read_sg;
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

void read_lat_fini(perftest_context *perftest_ctx, float time_s) {}

int main(int argc, char **argv) {
    perftest_run(argc, argv, read_lat_init, read_lat_iter, read_lat_fini);
}
