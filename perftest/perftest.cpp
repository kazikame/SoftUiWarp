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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "common.h"
#include "mpa/mpa.h"
#include "ddp/ddp.h"
#include "rdmap/rdmap.h"
#include "perftest.h"

static int argparse(int argc, char **argv, struct perftest_context *c) {
    int opt;
    c->buf_size = DEFAULT_BUF_SIZE;
    c->iters = DEFAULT_ITERS;
    c->ip = (char*) DEFAULT_IP;
    c->port = (char*) DEFAULT_PORT;
    c->is_client = false;
    c->max_reqs = DEFAULT_MAX_REQS;
    while ((opt = getopt(argc, argv, "p:s:b:i:r:c")) != -1) {
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
          case 'r':
            c->max_reqs = atoi(optarg);
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
static int create_tcp_connection(struct perftest_context *config) {
    int ret;
    int sock;
    // Create the socket.
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        goto fail;
    }
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
        if (config->serverfd == -1) {
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

static int alloc_buf(struct perftest_context *c) {
    // Allocate regions.
    c->buf = (char*) mmap(NULL, c->buf_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (c->buf == MAP_FAILED) {
        return -1;
    }
    return 0;
}

static int rdmap_send_data(struct perftest_context *perftest_ctx, void *data, size_t data_len, struct send_wr &wr) {
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

static int rdmap_recv_issue(struct perftest_context *perftest_ctx, void *buf, size_t buf_len, struct recv_wr &wr) {
    // Prepare the WR.
    wr.sg_list->addr = (uint64_t) buf;
    wr.sg_list->length = buf_len;
    return rdma_post_recv(perftest_ctx->ctx, wr);
}

static int rdmap_recv_ack(struct perftest_context *perftest_ctx) {
    // Poll the receive completion queue.
    int ret;
    struct work_completion wc;
    auto cqq = perftest_ctx->ctx->recv_q->cq->q;
    do { ret = cqq->try_dequeue(wc); } while (!ret) ;
    lwlog_debug("Received send data with id %lu", wc.wr_id);
    if (wc.status != WC_SUCCESS) {
        lwlog_err("Received remote send with error");
        return -1;
    } else if (wc.opcode != WC_SEND) {
        lwlog_err("Received wrong message type!");
        return -1;
    }
    return 0;
}

static void sync_with_remote(struct perftest_context *perftest_ctx, int sync_sock) {
    uint64_t dummy = 0;
    send(sync_sock, &dummy, sizeof(dummy), 0);
    recv(sync_sock, &dummy, sizeof(dummy), 0);
}

void perftest_run(int argc, char **argv, 
                  int (*test_init)(perftest_context*, uint32_t, struct send_data*),
                  int (*test_iter)(perftest_context*),
                  void (*test_fini)(perftest_context*, float))
{
    //! Parse arguments.
    struct perftest_context perftest_ctx;
    perftest_ctx.serverfd = -1;
    if (argparse(argc, argv, &perftest_ctx)) {
        lwlog_err("Failed to parse arguments!");
        return;
    }

    // Alloc hugepage buffer.
    if (alloc_buf(&perftest_ctx) != 0) {
        lwlog_err("Failed to allocate hugepage buffer!");
        return;
    }

    // Timing information.
    uint64_t start_ns, end_ns, total_ns;
    total_ns = 0;

    //! Protection Domain
    struct pd_t pd;
    pd.pd_id = 1;

    //! Create CQ
    struct cq* cq = create_cq(NULL, perftest_ctx.max_reqs+2);

    //! Create SQ/RQ
    struct wq_init_attr wq_attr;
    wq_attr.wq_type = wq_type::WQT_SQ;
    wq_attr.max_wr = perftest_ctx.max_reqs + 2;
    wq_attr.max_sge = perftest_ctx.max_reqs + 2;
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
        return;
    }
    int flag = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    attr.sockfd = sockfd;
    attr.max_pending_read_requests = perftest_ctx.max_reqs + 2;

    int sync_sock = create_tcp_connection(&perftest_ctx);
    if (sync_sock < 0) {
        lwlog_err("cannot create synchronization socket!");
        return;
    }

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
    tg_buf.len = perftest_ctx.buf_size;

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

    perftest_ctx.ctx = ctx;

    int ret;
    struct send_data my_sd;
    my_sd.offset = (uint64_t) perftest_ctx.buf;
    my_sd.stag = stag;
    my_sd.size = perftest_ctx.buf_size;
    struct send_data their_sd;
    if (perftest_ctx.is_client) {
        // Receive remote_addr and rkey from server.
        if (rdmap_recv_issue(&perftest_ctx, (void*) &their_sd, sizeof(struct send_data), recv_wr) < 0) {
            lwlog_err("failed to issue recv for server info!");
        }
        sync_with_remote(&perftest_ctx, sync_sock);
        // Send remote_addr and rkey to server.
        if (rdmap_send_data(&perftest_ctx, (void*) &my_sd, sizeof(struct send_data), send_wr) < 0) {
            lwlog_err("failed to send client info!");
        }
        // Complete recv.
        if (rdmap_recv_ack(&perftest_ctx)) {
            lwlog_err("failed to recv server info!");
        }
        lwlog_debug("Received addr %p stag %lu and size %lu from server.", (void*)their_sd.offset, their_sd.stag, their_sd.size);
    } else {
        // Receive remote_addr and rkey from client.
        if (rdmap_recv_issue(&perftest_ctx, (void*) &their_sd, sizeof(struct send_data), recv_wr) < 0) {
            lwlog_err("failed to issue recv for client info!");
        }
        sync_with_remote(&perftest_ctx, sync_sock);
        // Send remote_addr and rkey to server.
        if (rdmap_send_data(&perftest_ctx, (void*) &my_sd, sizeof(struct send_data), send_wr) < 0) {
            lwlog_err("failed to send server info!");
        }
        // Complete recv.
        if (rdmap_recv_ack(&perftest_ctx)) {
            lwlog_err("failed to recv client info!");
        }
        lwlog_debug("Received addr %p stag %lu and size %lu from client.", (void*)their_sd.offset, their_sd.stag, their_sd.size);
    }

    /* BEGIN BENCHMARKING */
    if (test_init(&perftest_ctx, stag, &their_sd)) {
        lwlog_err("Test initialization failed!");
        goto cleanup;
    }
    for (int iter = 0; iter < perftest_ctx.iters; iter++) {
        sync_with_remote(&perftest_ctx, sync_sock);
        start_ns = get_nanos();
        lwlog_debug("iter: %d", iter);
        if (test_iter(&perftest_ctx)) {
            lwlog_err("Test iteration failed!");
            goto cleanup;
        }
        end_ns = get_nanos();
        lwlog_notice("iter time: %lu", end_ns-start_ns);
        total_ns += (end_ns - start_ns);
    }

    if (perftest_ctx.is_client) {
        lwlog_info("Waiting for server finished notification ...");
        if (rdmap_recv_issue(&perftest_ctx, (void*)&total_ns, sizeof(uint64_t), recv_wr)) {
            lwlog_err("failed to issue recv for server completion!");
        }
        sync_with_remote(&perftest_ctx, sync_sock);
        rdmap_recv_ack(&perftest_ctx);
    } else {
        send_wr.wr_id = 234;
        lwlog_info("Finished benchmarking, notifying client.");
        sync_with_remote(&perftest_ctx, sync_sock);
        rdmap_send_data(&perftest_ctx, (void*)&total_ns, sizeof(uint64_t), send_wr);
    }

    // Print results.
    float time_s;
    lwlog_notice("Completed test!");
    time_s = (total_ns/(1000.0 * 1000.0 * 1000.0));
    lwlog_notice("Total Time (s): %f", time_s);
    lwlog_notice("Time per iteration (us): %f", (total_ns/1000.0/perftest_ctx.iters));
    lwlog_notice("One-way latency (us): %f", (total_ns/1000.0/perftest_ctx.iters/2));

    test_fini(&perftest_ctx, time_s);

    // Cleanup.
cleanup:
    rdmap_kill_stream(perftest_ctx.ctx);
    close(sockfd);
    close(sync_sock);
    munmap(perftest_ctx.buf, perftest_ctx.buf_size);
    if (!perftest_ctx.is_client) {
        int flags = 1;
        setsockopt(perftest_ctx.serverfd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(int));
        close(perftest_ctx.serverfd);
    }
}
