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

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "common/iwarp.h"
#include "mpa/mpa.h"
#include "suiw/ddp.h"
#include "suiw/rdmap.h"
#include "lwlog.h"

#define ULPDU_MAX_SIZE 1 << 16
#define CRC_SIZE 4
#define FPDU_MAX_SIZE ULPDU_MAX_SIZE + 2 + CRC_SIZE

static char doc[] = "Fake rdma ping which implements iWARP";
static char log_buf[1024];

struct Config
{
    char* ip;
    char* port;
};

struct Config argparse(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: ./fake_ping_client <ip> <port>\n");
        exit(1);
    }

    struct Config c;
    c.ip = argv[1];
    c.port = argv[2];
    return c;
}

/**
 * @brief Create a tcp connection object
 * 
 * @param config ip/port config
 * @return int 0 if success, < 0 if error
 */
int create_tcp_connection(struct Config* config)
{
    int sockfd, numbytes;
    char buf[FPDU_MAX_SIZE];

    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(config->ip, config->port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            lwlog_err("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            lwlog_err("client: connect");
            continue;
        }
        else
        {
            //! Disables Nagle's algorithm
            //! TODO: Check if this is really required.
            int flag = 1;
            setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
        }
        break;
    }

    return sockfd;
}

struct send_data {
    __u64 offset;
    __u32 stag;
    __u32 size;
};

int main(int argc, char **argv)
{
    //! TODO: Use a better argparse library
    struct Config c = argparse(argc, argv);

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
    int sockfd = create_tcp_connection(&c);
    if (sockfd < 0)
    {
        lwlog_err("cannot create socket");
        return 1;
    }
    attr.sockfd = sockfd;

    //! Register Buffers
    struct rdmap_stream_context* ctx = rdmap_init_stream(&attr);

    //! Register context with CQ/SQ/RQ
    cq->ctx = ctx;
    sq->context = ctx;
    rq->context = ctx;

    //! Create Tagged Buffer for read
    char buf[] = "rdma-ping-0: ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqr";
    tagged_buffer tg_buf;
    tg_buf.data = buf;
    tg_buf.len = 1000;

    register_tagged_buffer(ctx->ddp_ctx, &tg_buf);
    __u32 stag = tg_buf.stag.tag;

    //! Make MPA Connection
    int ret = mpa_client_connect(sockfd, NULL, 0, NULL);
    if (ret < 0)
    {
        lwlog_err("closing connection");
        close(sockfd);
    }

    //! Make Send Work Request
    struct send_wr req;
    req.wr_id = 1;

    //! Make Scatter/Gather req
    struct send_data d;
    d.offset = htonll((uint64_t)buf);
    d.stag = htonl(stag);
    d.size = htonl(sizeof(buf));

    struct sge sg;
    sg.addr = (uint64_t)&d;
    sg.length = sizeof(d);

    req.sg_list = &sg;
    req.num_sge = 1;
    req.opcode = RDMAP_SEND;

    lwlog_info("Sending RDMAP: %lld %u %u", d.offset, d.stag, d.size);
    rdmap_send(ctx, std::move(req));

    sleep(10);
}
