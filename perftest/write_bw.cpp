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

#include <x86intrin.h>

struct sge write_sg;
struct send_wr write_wr;

char byte_end = (char) 0x42;
struct sge write_sg_end;
struct send_wr write_wr_end;

int write_bw_init(perftest_context *perftest_ctx, uint32_t lstag, struct send_data *sd) {
    // Build write WR.
    write_sg.addr = (uint64_t) perftest_ctx->buf;
    write_sg.length = perftest_ctx->buf_size;
    write_sg.lkey = lstag;
    write_wr.wr_id = 2;
    write_wr.sg_list = &write_sg;
    write_wr.num_sge = 1;
    write_wr.opcode = RDMAP_RDMA_WRITE;
    write_wr.wr.rdma.rkey = sd->stag;
    write_wr.wr.rdma.remote_addr = sd->offset;
    // Build ending write WR.
    memcpy(&write_sg_end, &write_sg, sizeof(struct sge));
    memcpy(&write_wr_end, &write_wr, sizeof(struct send_wr));
    write_sg_end.addr = (uint64_t) &byte_end;
    write_sg_end.length = 1;
    write_wr_end.wr_id = 0xffffffff;
    write_wr.sg_list = &write_sg_end;
    if (!perftest_ctx->is_client) {
        // Fill the server's region with incrementing values.
        for (int i = 0; i < perftest_ctx->buf_size; i++) {
            perftest_ctx->buf[i] = (char) i;
        }
    }
    return 0;
}

int write_bw_iter(perftest_context *perftest_ctx) {
    // Client clears buffer and waits for writes.
    if (perftest_ctx->is_client) {
        perftest_ctx->buf[0] = (char) 0;
        lwlog_debug("waiting for write ...");
        do { _mm_clflush(&perftest_ctx->buf); } while (perftest_ctx->buf[0] == 0) ;
        perftest_ctx->buf[0] = 0;
        lwlog_debug("found write!");
    }

    // Issue all the large writes.
    int ret;
    for (int i = 0; i < perftest_ctx->max_reqs; i++) {
        ret = rdmap_write(perftest_ctx->ctx, write_wr);
        write_wr.wr_id++;
        if (ret < 0) {
            lwlog_err("Failed to issue RDMA WRITE!");
            return -1;
        }
    }
    lwlog_debug("completed writes");
    struct work_completion wc;
    auto write_cq = perftest_ctx->ctx->send_q->cq->q;
    for (int i = 0; i < perftest_ctx->max_reqs; i++) {
        do { ret = write_cq->try_dequeue(wc); } while (!ret) ;
        lwlog_debug("received completion");
        if (wc.status != WC_SUCCESS) {
            lwlog_err("Received remote send with error");
            return -1;
        } else if (wc.opcode != WC_WRITE) {
            lwlog_err("Received wrong message type!");
            return -1;
        }
    }
    // Issue the last write that indicates completion.
    write_sg.length = 1;
    ret = rdmap_write(perftest_ctx->ctx, write_wr_end);
    do { ret = write_cq->try_dequeue(wc); } while (!ret) ;
    
    // Server clears buffer and waits for writes.
    if (!perftest_ctx->is_client) {
        perftest_ctx->buf[0] = (char) 0;
        lwlog_debug("waiting for write ...");
        do { _mm_clflush(&perftest_ctx->buf); } while (perftest_ctx->buf[0] == 0) ;
        lwlog_debug("found write!");
    }
    return 0;
}

void write_bw_fini(perftest_context *perftest_ctx, float time_s) {
    uint64_t num_requests = perftest_ctx->max_reqs * perftest_ctx->iters;
    float Mpps = num_requests / (time_s * 1000.0 * 1000.0);
    float MBps = (num_requests * perftest_ctx->buf_size) / (time_s * 1000.0 * 1000.0);
    lwlog_notice("Message rate (Mp/s): %f", Mpps);
    lwlog_notice("Bandwidth (MB/s): %f", MBps);
}

int main(int argc, char **argv) {
    perftest_run(argc, argv, write_bw_init, write_bw_iter, write_bw_fini);
}
