#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <poll.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/types.h>

#include <vector>

#include <rdma/rdma_cma.h>

#include "libsuiw_internal.h"

// Timeout for workloop polling.
static const int poll_timeout_ms = 1000;

/* Internal state. */

static int server_fd = -1;
static std::vector<int> ec_fds;
static std::vector<int> cq_fds;

/*
 * Binds server_fd to the server address and port.
 *  returns: 0 on success, -1 on failure.
 */
static int bind_server() {
    int ret;
    // Convert the hostname to a sockaddr.
    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res;
    ret = getaddrinfo(daemon_hostname, daemon_port, &hints, &res);
    if (ret != 0) {
        perror("getaddrinfo");
        return ret;
    }
    // Bind the server.
    ret = bind(server_fd, res->ai_addr, res->ai_addrlen);
    if (ret) {
        perror("bind");
        return ret;
    }
    freeaddrinfo(res);
    // Listen for connections.
    ret = listen(server_fd, SOMAXCONN); 
    if (ret == -1) {
        perror("listen");
        return ret;
    }
    return ret;
}

/*
 * Initiates a connection with the library.
 *  returns: a fd for the connection with the server, -1 if failed.
 */
static int wait_for_client_connect() {
    // Accept a connection.
    printf("waiting for connection from libsuiw at %s:%s ... ", daemon_hostname, daemon_port);
    int ret = accept(server_fd, 0, 0);
    printf("done!\n");
    return ret;
}

static void create_event_channel() {
    printf("creating event channel\n");
    int ec_fd = wait_for_client_connect();
    ec_fds.push_back(ec_fd);
}

static void create_completion_queue() {
    printf("creating completion queue\n");
    int cq_fd = wait_for_client_connect();
    cq_fds.push_back(cq_fd);
}

static void create_queue_pair() {
    printf("creating queue pair\n");
}

static int lib_handle_recv(int fd) {
    printf("handling lib msg ...\n");
    struct daemon_msg msg;
    int ret = recv(fd, &msg, sizeof(msg), MSG_WAITALL);
    if (ret == -1) {
        perror("recv");
        return -1;
    } else if (ret == 0) {
        // TCP socket closed by peer.
        return 0;
    } else if (ret < sizeof(msg)) {
        printf("unexpectedly received %d bytes from lib\n", ret);
        return -1;
    }
    // Handle the message.
    switch (msg.type) {
        case DAEMON_CREATE_EC:
            create_event_channel();
            break;
        case DAEMON_CREATE_CQ:
            create_completion_queue();
            break;
        case DAEMON_CREATE_QP:
            create_queue_pair();
            break;
    }
    return ret;
}

static int dpdk_handle_recv(int fd) {
    printf("handling dpdk packet ...\n");
}

static int ec_handle_recv(int fd, size_t idx) {
    int ret = 0;
    rdma_cm_event event;
    ret = recv(fd, &event, sizeof(event), 0);
    printf("handling ec packet for fd %lu of event type %s\n", idx, rdma_event_str(event.event));
    if (ret == -1) {
        perror("recv");
        return -1;
    } else if (ret == 0) {
        // TCP socket closed by peer.
        return 0;
    } else if (ret < sizeof(event)) {
        printf("unexpectedly received %d bytes from event channel\n", ret);
        return -1;
    }
    // TODO respond to event if necessary
    ret = send(fd, &event, sizeof(event), 0);
    if (ret == -1) {
        perror("send");
        return -1;
    } else if (ret == 0) {
        // TCP socket closed by peer.
        return 0;
    } else if (ret < sizeof(event)) {
        printf("failed to send full event channel message\n");
        return -1;
    }
    return ret;
}

static int cq_handle_recv(int fd, size_t idx) {
    printf("handling cq packet %lu\n", idx);
}

static int workloop(int lib_fd, int dpdk_fd) {
    // Build poll fds.
    short flags = POLLIN | POLLERR;
    size_t num_fds = 2;
    struct pollfd *fds = (pollfd*) malloc(sizeof(pollfd)*num_fds);
    bzero(fds, num_fds*sizeof(pollfd));
    fds[0].fd = lib_fd;
    fds[0].events = flags;
    fds[1].fd = dpdk_fd;
    fds[1].events = flags;
    // Main workloop.
    int ret = 0;
    while (1) {
        // If we have new FDs to poll, re-build the pollfd structure.
        size_t ec_fd_start = 2;
        size_t cq_fd_start = ec_fd_start + ec_fds.size();
        size_t new_num_fds = cq_fd_start + cq_fds.size();
        if (num_fds != new_num_fds) {
            printf("adding file descriptor to poll list\n");
            num_fds = new_num_fds;
            fds = (pollfd*) realloc(fds, sizeof(pollfd)*num_fds);
            bzero(fds, num_fds*sizeof(pollfd));
            size_t num_fds = 2 + ec_fds.size() + cq_fds.size();
            for (size_t i = 0; i < num_fds; i++) {
                if (i == 0) {
                    fds[i].fd = lib_fd;
                } else if (i == 1) {
                    fds[i].fd = dpdk_fd;
                } else if ((i-ec_fd_start) < ec_fds.size()) {
                    fds[i].fd = ec_fds[i-ec_fd_start];
                } else if ((i-cq_fd_start) < cq_fds.size()) {
                    fds[i].fd = cq_fds[i-cq_fd_start];
                }
                fds[i].events = flags;
            }
        }
        // Do the actual poll.
        int pollret = poll(fds, num_fds, poll_timeout_ms);
        if (pollret == -1) {
            perror("poll");
            return -1;
        } else if (pollret == 0) {
            printf("poll timeout.\n");
            continue;
        }
        size_t i = 0;
        while (pollret > 0) {
            if (fds[i].revents != 0) {
                int error_mask = POLLERR | POLLNVAL;
                if (fds[i].revents & POLLHUP) {
                    printf("fd %lu unexpectedly closed connection\n", i);
                    return -1;
                } else if (fds[i].revents & (POLLERR | POLLNVAL)) {
                    printf("fd %lu returned error!\n", i);
                    return -1;
                }
                assert(fds[i].revents & POLLIN);
                if (i == 0) {
                    ret = lib_handle_recv(fds[i].fd);
                    if (ret == 0) {
                        printf("library closed connection.\n");
                        return 0;
                    }
                } else if (i == 1) {
                    ret = dpdk_handle_recv(fds[i].fd);
                } else if ((i-ec_fd_start) < ec_fds.size()) {
                    ret = ec_handle_recv(fds[i].fd, i-ec_fd_start);
                } else if ((i-cq_fd_start) < cq_fds.size()) {
                    ret = cq_handle_recv(fds[i].fd, i-cq_fd_start);
                }
                if (ret == -1) {
                    printf("something went wrong handling packets for fd %lu!\n", i);
                    return -1;
                }
                pollret--;
            }
            i++;
        }
    }
    return 0;
}

/* Initialize everything and start the workloop thread. */
int main(int argc, char **argv) {
    printf("libsuiw daemon process\n");
    // Create the socket.
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return -1;
    }
    // Bind the socket.
    if (bind_server()) {
        return -1;
    }
    // Listen for connection from libsuiw.
    int lib_fd = wait_for_client_connect();
    if (lib_fd == -1) {
        printf("failed to connect to libsuiw!\n");
        return -1;
    } 
    printf("connected to libsuiw successfully!\n");
    // Run the workloop.
    int dpdk_fd = -1;
    int ret = workloop(lib_fd, dpdk_fd);
    // Clean up.
    int flags = 1;
    close(lib_fd);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(int));
    close(server_fd);
    return ret;
}
