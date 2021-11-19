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

#include "iwarp.h"

#include "lwlog.h"

#define ULPDU_MAX_SIZE 1 << 16
#define CRC_SIZE 4
#define FPDU_MAX_SIZE ULPDU_MAX_SIZE + 2 + CRC_SIZE

static char doc[] = "Fake rdma ping which implements iWARP";

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
        break;
    }

    return sockfd;
}

/**
 * @brief sends an MPA request/reply
 * 
 * @param sockfd TCP connected socket
 * @param pdata private data to be sent
 * @param pd_len length of the private data
 * @param req 0 if response, else request
 * @return int number of total bytes sent, negative if failed
 */
int send_mpa_rr(int sockfd, const void* pdata, __u8 pd_len, int req)
{
    //! Make MPA request header
    struct mpa_rr hdr;
    memset(&hdr, 0, sizeof hdr);

    strncpy(hdr.key, req ? MPA_KEY_REQ : MPA_KEY_REP, MPA_RR_KEY_LEN);
    int version = MPA_REVISION_2;
    __mpa_rr_set_revision(&hdr.params.bits, version);

    hdr.params.pd_len = __cpu_to_be16(pd_len);
    
    int mpa_len = 0;
    
    //! Make sendmsg() msg struct
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    
    struct iovec iov[3];
    int iovec_num = 0;

    iov[iovec_num].iov_base = &hdr;
    iov[iovec_num].iov_len = sizeof(hdr);
    mpa_len += sizeof(hdr);

    if (pd_len)
    {
        iovec_num++;
        iov[iovec_num].iov_base = (char *)pdata;
		iov[iovec_num].iov_len = pd_len;
		mpa_len += pd_len;
    }

    msg.msg_iov = iov;
    msg.msg_iovlen = 3;

    int ret = sendmsg(sockfd, &msg, 0);
    return ret;
}

/**
 * @brief receives an MPA request/reply
 * 
 * @param sockfd TCP connection socket
 * @param info request/reply struct in which data is received. Must be allocated
 * @return int number of bytes received, < 0 if error
 */
int recv_mpa_rr(int sockfd, struct siw_mpa_info* info)
{
    int bytes_rcvd = 0;

    struct mpa_rr* hdr = &info->hdr;

    //! Get header
    int rcvd = recv(sockfd, hdr, sizeof(struct mpa_rr), 0);

    if (rcvd < sizeof(struct mpa_rr))
    {
        lwlog_err("recv_mpa_rr: didn't receive enough bytes");
        return -1;
    }

    __u16 pd_len = __be16_to_cpu(hdr->params.pd_len);

    //! private data length is 0
    if (!pd_len)
    {
        //! ensure that no redundant data has been sent!
        int garbage;
        rcvd = recv(sockfd, (char *)&garbage, sizeof(garbage), MSG_DONTWAIT);

        //! No data on socket, the peer is protocol compliant :)
        if (rcvd == -EAGAIN) return 0;

        if (rcvd > 0)
        {
            lwlog_err("recv_mpa_rr: peer sent extra data after req/resp");
            return rcvd;
        }

        if (rcvd < 0)
        {
            lwlog_err("recv_mpa_rr: connection error after header received");
            return rcvd;
        }

        if (rcvd == 0)
        {
            lwlog_err("recv_mpa_rr: peer EOF");
			return -EPIPE;
        }

        //! unreachable
    }

    //! private data length is nonzero. Must allocate private data.
    if (!info->pdata)
    {
        info->pdata = malloc(pd_len + 4);
        if (!info->pdata)
        {
            lwlog_err("recv_mpa_rr: out of memory");
            return -ENOMEM;
        }
    }

    rcvd = recv(sockfd, info->pdata, pd_len + 4, MSG_DONTWAIT);

    if (rcvd < 0)
    {
        lwlog_err("recv_mpa_rr: connection error after trying to get private data");
        return rcvd;
    }
    if (rcvd > pd_len)
    {
        lwlog_err("recv_mpa_rr: received more than private data length");
        return -EPROTO;
    }

    lwlog_info("MPA req/rep received pd_len: %d, pdata: %d", pd_len, info->pdata);
    return rcvd;
}

int main(int argc, char **argv)
{
    //! TODO: Use a better argparse library
    struct Config c = argparse(argc, argv);

    int sockfd = create_tcp_connection(&c);

    if (sockfd < 0)
    {
        lwlog_err("cannot create socket");
        return 1;
    }

    __u32 num = 65537;
    send_mpa_rr(sockfd, (char *)&num, sizeof(__u32), 1);

    close(sockfd);
}