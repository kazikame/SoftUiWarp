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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "common.h"
#include "mpa/mpa.h"
#include "ddp/ddp.h"
#include "rdmap/rdmap.h"
#include "lwlog.h"

#define CRC_SIZE 4
#define RPING_MSG_FMT           "rdma-ping-%d: "
#define BUF_LEN                 64

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

struct ping_context {
    struct rdmap_stream_context* ctx;
    sem_t sem;
    pthread_t cqt;
};

void* cq_thread(void* data)
{
    lwlog_info("CQ thread started");
    struct ping_context* ping_ctx = (struct ping_context*) data;
    auto cq = ping_ctx->ctx->recv_q->cq->q;
    struct work_completion wc;

    char garbage_buffer[1000];

    //! Receive WR
    struct recv_wr wr;
    wr.wr_id = 1901;

    struct sge sg;
    sg.addr = (uint64_t)garbage_buffer;
    sg.length = 1000;
    wr.sg_list = &sg;
    wr.num_sge = 1;

    rdma_post_recv(ping_ctx->ctx, wr);
    while(ping_ctx->ctx->connected)
    {
        int ret = cq->try_dequeue(wc);
        if (!ret) continue;

        if (wc.opcode != WC_SEND || wc.wr_id != wr.wr_id) {
            continue;
        }
        if (wc.status != WC_SUCCESS)
        {
            lwlog_err("Received remote send with error");
            ping_ctx->ctx->connected = 0;
            break;
        }

        lwlog_info("Remote send recvd: %lu", wc.wr_id);
        rdma_post_recv(ping_ctx->ctx, wr);
        sem_post(&ping_ctx->sem);
    }
}

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
    attr.max_pending_read_requests = 10;
    
    //! Make MPA Connection
    int ret = mpa_client_connect(sockfd, NULL, 0, NULL);
    if (ret < 0)
    {
        lwlog_err("closing connection");
        close(sockfd);
    }
    
    //! Register Buffers
    struct rdmap_stream_context* ctx = rdmap_init_stream(&attr);

    //! Register context with CQ/SQ/RQ
    cq->ctx = ctx;
    sq->context = ctx;
    rq->context = ctx;

    //! Create Tagged Buffer for read
    char buf[BUF_LEN];
    tagged_buffer tg_buf;
    tg_buf.data = (char*)buf;
    tg_buf.len = sizeof(buf);

    register_tagged_buffer(ctx->ddp_ctx, &tg_buf);
    __u32 stag = tg_buf.stag.tag;

    //! Make Send Work Request: scatter/gather
    struct send_wr req;
    req.wr_id = 1;

    struct send_data d;
    d.offset = htonll((uint64_t)(char*)buf);
    d.stag = htonl(stag);
    d.size = htonl(sizeof(buf));

    struct sge sg;
    sg.addr = (uint64_t)&d;
    sg.length = sizeof(d);

    req.sg_list = &sg;
    req.num_sge = 1;
    req.opcode = RDMAP_SEND;

    /*
     * rping "ping/pong" loop:
     * 	client sends source rkey/addr/len
     *	server receives source rkey/add/len
     *	server rdma reads "ping" data from source
     * 	server sends "go ahead" on rdma read completion
     *	client sends sink rkey/addr/len
     * 	server receives sink rkey/addr/len
     * 	server rdma writes "pong" data to sink
     * 	server sends "go ahead" on rdma write completion
     * 	<repeat loop>
     */
    struct ping_context ping_ctx;
    ping_ctx.ctx = ctx;
    
    //! Semaphore
    sem_init(&ping_ctx.sem, 0, 0);

    //! Completion Queue Thread
    pthread_create(&ping_ctx.cqt, 0, cq_thread, &ping_ctx);
    //! Main Client Loop
    int start = 65;
    ret = 0;
    int cc = 0;
    for (int ping = 0; ctx->connected; ping++) {

		/* Put some ascii text in the buffer. */
		cc = snprintf(buf, sizeof(buf), RPING_MSG_FMT, ping);
		for (int i = cc, c = start; i < sizeof(buf); i++) {
			buf[i] = c;
			c++;
			if (c > 122)
				c = 65;
		}
		start++;
		if (start > 122)
			start = 65;
		buf[sizeof(buf) - 1] = 0;
        lwlog_info("buffer before read: %s", buf);

        ret = rdmap_send(ctx, req);
		if (ret < 0) {
			lwlog_err("post send error %d", ret);
			break;
		}

		/* Wait for server to ACK */
		sem_wait(&ping_ctx.sem);

		ret = rdmap_send(ctx, req);
		if (ret < 0) {
			lwlog_err("post send error %d", ret);
			break;
		}

		/* Wait for the server to say the RDMA Write is complete. */
		sem_wait(&ping_ctx.sem);
        lwlog_info("buffer after write: %s", buf);
	}
}
