/*
 * Software Userspace iWARP device driver for Linux
 *
 * Authors: Saksham Goel <saksham@cs.utexas.edu>
 *
 * Copyright (c) 2021
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *   - Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   - Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *   - Neither the name of IBM nor the names of its contributors may be
 *     used to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/mman.h>

#include "common.h"
#include "mpa/mpa.h"
#include "ddp/ddp.h"
#include "rdmap/rdmap.h"
#include "lwlog.h"

//#define PERFTEST_TEST

#define DEFAULT_BUF_SIZE 1024
#define DEFAULT_ITERS 1024
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT "9999"

static char doc[] = "Fake rdma ping which implements iWARP";
static char log_buf[1024];

struct perftest_context {
    char* port; 
    char* ip;
    int buf_size;
    char *buf;
    int iters;
    bool is_client;
    int serverfd;
    int huge_shmid;
    struct rdmap_stream_context* ctx;
};

static int argparse(int argc, char **argv, struct perftest_context *c) {
    int opt;
    c->buf_size = DEFAULT_BUF_SIZE;
    c->iters = DEFAULT_ITERS;
    c->ip = (char*) DEFAULT_IP;
    c->port = (char*) DEFAULT_PORT;
    c->is_client = false;
    while ((opt = getopt(argc, argv, "p:s:b:i:c")) != -1) {
        switch (opt) {
          case 's':
            c->ip = optarg;
            break;
          case 'p':
            c->port = optarg;
            break;
          case 'b':
            c->buf_size = atoi(optarg);
            break;
          case 'i':
            c->iters = atoi(optarg);
            break;
          case 'c':
            c->is_client = true;
            break;
          case '?':
            if (optopt == 'c') {
                lwlog_err("Option -%c requires an argument.", optopt);
            } else if (isprint(optopt)) {
                lwlog_err("Unknown option '-%c'.", optopt);
            } else {
                lwlog_err("Unknown option character '\\x%x'.", optopt);
            }
            return 1;
          default:
            return 1;
        }
    } 
    return 0;
}

static inline uint64_t get_nanos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

/**
 * @brief Create a tcp connection object
 * 
 * @param config ip/port config
 * @return int 0 if success, < 0 if error
 */
int create_tcp_connection(struct perftest_context *config) {
    int ret;
    int sock;
    // Create the socket.
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        goto fail;
    }
    lwlog_notice("File descriptor %d", sock);
    // Convert the hostname to a sockaddr.
    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res;
    if (getaddrinfo(config->ip, config->port, &hints, &res)) {
        perror("getaddrinfo");
        goto fail;
    }
    if (config->is_client) {
        // Running the client, so connect to server.
        lwlog_notice("Connecting to server at %s:%s ...", config->ip, config->port);
        if (connect(sock, res->ai_addr, res->ai_addrlen) == -1) {
            perror("connect");
            goto fail_freeaddrinfo;
        }
    } else {
        // Running the server, so bind and accept a connection.
        config->serverfd = sock;
        if (bind(config->serverfd, res->ai_addr, res->ai_addrlen)) {
            perror("bind");
            goto fail_freeaddrinfo;
        }
        // Listen for connections.
        if (listen(config->serverfd, 1)) {
            perror("listen");
            goto fail_freeaddrinfo;
        }
        // Accept a connection;
        lwlog_notice("Waiting for client to connect on port %s ... ", config->port);
        sock = accept(config->serverfd, 0, 0);    
        if (sock == -1) {
            perror("accept");
            goto fail_freeaddrinfo;
        }
    }
    freeaddrinfo(res);
    return sock;
fail_freeaddrinfo:
    freeaddrinfo(res);
fail:
    close(sock);
    return -1;
}

#define DEFAULT_PAGE_SIZE 4096

#if !defined(__FreeBSD__)
int alloc_hugepage_region(struct perftest_context *c) {
    // Allocate regions, fingers crossed we get hugepages :]
    c->buf = (char*) mmap(NULL, c->buf_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (c->buf == MAP_FAILED) {
        return -1;
    }
    // Fill the client's region with incrementing values.
    if (c->is_client) {
        for (int i = 0; i < c->buf_size; i++) {
            c->buf[i] = (char) i;
        }
    }
    return 0;
}
#endif

int rdmap_send_data(struct perftest_context *perftest_ctx, void *data, size_t data_len, struct send_wr &wr) {
    wr.sg_list->addr = (uint64_t) data;
    wr.sg_list->length = data_len;
    int ret = rdmap_send(perftest_ctx->ctx, wr);
    // ACK the associated event.
    struct work_completion wc;
    auto cqq = perftest_ctx->ctx->send_q->cq->q;
    int suc;
    do { suc = cqq->try_dequeue(wc); } while (!suc) ;
    return ret;
}

int rdmap_recv_data(struct perftest_context *perftest_ctx, void *buf, size_t buf_len, struct recv_wr &wr) {
    // Prepare the WR.
    wr.sg_list->addr = (uint64_t) buf;
    wr.sg_list->length = buf_len;
    rdma_post_recv(perftest_ctx->ctx, wr);
    // Poll the receive completion queue.
    int ret;
    struct work_completion wc;
    auto cqq = perftest_ctx->ctx->recv_q->cq->q;
    do { ret = cqq->try_dequeue(wc); } while (!ret) ;
    lwlog_debug("Received send data with id %lu", wc.wr_id);
    if (wc.status != WC_SUCCESS) {
        lwlog_err("Received remote send with error");
    } else if (wc.opcode != WC_SEND) {
        lwlog_err("Received wrong message type!");
    }
    return ret;
}

struct send_data {
    uint64_t offset;
    uint64_t stag;
    uint64_t size;
};

int main(int argc, char **argv)
{
    //! Parse arguments.
    struct perftest_context perftest_ctx;
    if (argparse(argc, argv, &perftest_ctx)) {
        lwlog_err("Failed to parse arguments!");
        return 1;
    }

    // Alloc hugepage buffer.
    if (alloc_hugepage_region(&perftest_ctx) != 0) {
        lwlog_err("Failed to allocate hugepage buffer!");
        return 1;
    }

    //! Protection Domain
    struct pd_t pd;
    pd.pd_id = 1;

    //! Create CQ
    struct cq* cq = create_cq(NULL, 10);

    //! Create SQ/RQ
    struct wq_init_attr wq_attr;
    wq_attr.wq_type = wq_type::WQT_SQ;
    wq_attr.max_wr = 10;
    wq_attr.max_sge = 10;
    wq_attr.pd = &pd;
    wq_attr.cq = cq;

    struct wq* sq = create_wq(NULL, &wq_attr);

    wq_attr.wq_type = wq_type::WQT_RQ;
    struct wq* rq = create_wq(NULL, &wq_attr);

    //! RDMAP Init Attributes
    struct rdmap_stream_init_attr attr;
    attr.send_q = sq;
    attr.recv_q = rq;

    //! TCP Connection
    int sockfd = create_tcp_connection(&perftest_ctx);
    if (sockfd < 0) {
        lwlog_err("cannot create socket");
        return 1;
    }
    attr.sockfd = sockfd;
    attr.max_pending_read_requests = 10;

    // MPA connection.
    if (perftest_ctx.is_client) {
        mpa_client_connect(sockfd, NULL, 0, NULL);
    } else {
        mpa_server_accept(sockfd, NULL, 0, NULL);
    }

    //! Register Buffers
    struct rdmap_stream_context* ctx = rdmap_init_stream(&attr);

    //! Register context with CQ/SQ/RQ
    cq->ctx = ctx;
    sq->context = ctx;
    rq->context = ctx;

    // Create Tagged Buffer for read
    tagged_buffer tg_buf;
    tg_buf.data = (char*)perftest_ctx.buf;
    tg_buf.len = sizeof(perftest_ctx.buf);

    register_tagged_buffer(ctx->ddp_ctx, &tg_buf);
    __u32 stag = tg_buf.stag.tag;

    // Build send WR.
    struct sge send_sg;
    struct send_wr send_wr;
    send_wr.wr_id = 1;
    send_wr.sg_list = &send_sg;
    send_wr.num_sge = 1;
    send_wr.opcode = RDMAP_SEND;

    // Build recv WR.
    struct sge recv_sg;
    struct recv_wr recv_wr;
    recv_wr.wr_id = 1901;
    recv_wr.sg_list = &recv_sg;
    recv_wr.num_sge = 1;

    // Build read WR.
    struct sge read_sg;
    read_sg.addr = ntohll((uint64_t) perftest_ctx.buf);
    read_sg.length = ntohl(perftest_ctx.buf_size);
    read_sg.lkey = ntohl(stag);
    struct send_wr read_wr;
    read_wr.wr_id = 2;
    read_wr.sg_list = &read_sg;
    read_wr.num_sge = 1;
    read_wr.opcode = RDMAP_RDMA_READ_REQ;

    perftest_ctx.ctx = ctx;

    int ret;
    if (perftest_ctx.is_client) {
        struct send_data sd;
        sd.offset = (uint64_t) perftest_ctx.buf;
        sd.stag = stag;
        sd.size = perftest_ctx.buf_size;

        if (rdmap_send_data(&perftest_ctx, (void*) &sd, sizeof(struct send_data), send_wr) < 0) {
            lwlog_err("failed to send client info!");
        }
    } else {
        // Receive remote_addr and rkey from client.
        struct send_data sd;
        if (rdmap_recv_data(&perftest_ctx, (void*) &sd, sizeof(struct send_data), recv_wr) < 0) {
            lwlog_err("failed to recv client info!");
        }
        lwlog_debug("Received addr %p stag %lu and size %lu from client.", (void*)sd.offset, sd.stag, sd.size);
        read_wr.wr.rdma.rkey = ntohl(sd.stag);
        read_wr.wr.rdma.remote_addr = ntohll(sd.offset);
    }

    /* BEGIN BENCHMARKING */
    uint64_t start_ns, end_ns, total_ns;
    auto read_cq = perftest_ctx.ctx->send_q->cq->q;
    if (!perftest_ctx.is_client) {
        start_ns = get_nanos();
        for (int iter = 0; iter < perftest_ctx.iters; iter++) {
            lwlog_debug("iter: %d", iter);
            ret = rdmap_read(ctx, read_wr);
            if (ret < 0) {
                lwlog_err("Failed to issue RDMA READ!");
                break;
            }
            lwlog_debug("completed read");
            struct work_completion wc;
            do { ret = read_cq->try_dequeue(wc); } while (!ret) ;
            lwlog_debug("received completion");
            if (wc.status != WC_SUCCESS) {
                lwlog_err("Received remote send with error");
            } else if (wc.opcode != WC_READ_REQUEST) {
                lwlog_err("Received wrong message type!");
            }
#ifdef PERFTEST_TEST
            for (int i = 0; i < perftest_ctx.buf_size; i++) {
                if (perftest_ctx.buf[i] != ((char) i)) {
                    lwlog_err("Received incorrect value 0x%hhx for index %d!", perftest_ctx.buf[i], i);
                    break;
                }
            }
            memset(perftest_ctx.buf, 0, perftest_ctx.buf_size);
#endif // PERFTEST_TEST
        }
        end_ns = get_nanos();
        total_ns = end_ns - start_ns;
    }
    
    if (perftest_ctx.is_client) {
        lwlog_info("Waiting for server finished notification ...");
        rdmap_recv_data(&perftest_ctx, (void*)&total_ns, sizeof(uint64_t), recv_wr);
    } else {
        send_wr.wr_id = 234;
        lwlog_info("Finished benchmarking, notifying client.");
        rdmap_send_data(&perftest_ctx, (void*)&total_ns, sizeof(uint64_t), send_wr);
    }

    // Print results.
    lwlog_notice("Completed test!");
    lwlog_notice("Total Time (s): %f", (total_ns/(1000.0 * 1000.0 * 1000.0)));
    lwlog_notice("Time per iteration (us): %f", (total_ns/1000.0/perftest_ctx.iters));

    // Cleanup.
    rdmap_kill_stream(perftest_ctx.ctx);
    close(sockfd);
    munmap(perftest_ctx.buf, perftest_ctx.buf_size);
    if (!perftest_ctx.is_client) {
        int flags = 1;
        setsockopt(perftest_ctx.serverfd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(int));
        close(perftest_ctx.serverfd);
    }
}
