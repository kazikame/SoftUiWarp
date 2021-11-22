#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include "libsuiw_internal.h"

/* Internal state. */

int server_fd = -1;
int libfd = -1;

/* Configuration for communication with libsuiw. */

static int listen_for_connection() {
    int ret = 0;
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
    // Create the socket.
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return server_fd;
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
    if (ret) {
        perror("listen");
    }
    // Accept a connection (there should only be one maximum, since libsuiw is shared).
    printf("waiting for connection from libsuiw at %s:%s ...\n", daemon_hostname, daemon_port);
    libfd = accept(server_fd, 0, 0);
    return ret;
}

/* Initialize everything and start the workloop thread. */
int main(int argc, char **argv) {
    printf("libsuiw daemon process\n");
    int ret = listen_for_connection();
    if (ret != 0) {
        printf("failed to connect to libsuiw!\n");
        exit(1);
    } 
    printf("connected to libsuiw successfully!\n");
    // TODO start a workloop thread
}
