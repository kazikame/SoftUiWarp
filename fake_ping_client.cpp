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

    //! Make MPA Connection
    int ret = mpa_client_connect(sockfd, NULL, 0, NULL);

    if (ret < 0)
    {
        lwlog_err("closing connection");
        close(sockfd);
    }
    sleep(10);
    std::cout<<"dfdg\n";
    //! Send some stuff
    int garbage = 5;
    ret = mpa_send(sockfd, &garbage, sizeof(int), 0);
    struct pd* pd1 = new pd;
    pd1->pd_id = 12; 
    std::cout<<"print\n";
    struct ddp_stream_context* ctx = ddp_init_stream(sockfd, pd1);
    std::cout<<"print 2\n";
    struct stag_t* stag = new stag_t;
    stag->pd_id = pd1;
    stag->id = 1;
    register_stag(stag);
    std::cout<<"tag dd\n";
    register_tagged_buffer();
    char data[10] = "Tswhat";
    for(int i = 0;i<10;i++){
        data[i] = 't';
    }
    printf("before send");
    ddp_tagged_send(ctx, stag, 0, data, 10, 1);
    printf("done\n");
    sleep(10);
}
