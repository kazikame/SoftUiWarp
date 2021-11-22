
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <map>

#include "softuiwarp.h"
#include "libsuiw_internal.h"

/* Internal state. */

static ibv_device *suiw_ibv_device;     // keep around the fake struct to avoid rebuilding it
static int dfd = -1;                    // socket used to communicate with the daemon process

/* Internal functions. */

static const char *kernel_dev_name = "enp94s0f0"; // just hard-code this for now...
static const char *dev_name = "softuiwarp0";
static const char *dev_path = "/dev/zero";
static const char *ibdev_path = "/dev/zero";
static const uint64_t dev_guid = 0xdeadbeef;

static ibv_device *create_suiw_device() {
    ibv_device *fake_device = (ibv_device*) malloc(sizeof(struct ibv_device));
    memset(fake_device, 0, sizeof(struct ibv_device));
    fake_device->transport_type = IBV_TRANSPORT_IWARP;
    strcpy(fake_device->name, kernel_dev_name);
    strcpy(fake_device->dev_name, dev_name);
    strcpy(fake_device->dev_path, dev_path);
    strcpy(fake_device->ibdev_path, ibdev_path);
    return fake_device;
}

static int connect_to_daemon() {
    int ret = 0;
    struct sockaddr_in servaddr;
    // Create the socket.
    dfd = socket(AF_INET, SOCK_STREAM, 0)   ;
    if (dfd == -1) {
        perror("socket");
        return -1;
    }
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
    // Connect to the server.
    ret = connect(dfd, res->ai_addr, res->ai_addrlen);
    if (ret != 0) {
        perror("connect");
        return ret;
    }
    return ret;
}

/* Implementation of public API. */

ibv_device *suiw_get_ibv_device() {
    ibv_device *dev = (ibv_device*) malloc(sizeof(struct ibv_device));
    memcpy(dev, suiw_ibv_device, sizeof(struct ibv_device));
    return dev;
}

bool suiw_is_device_softuiwarp(ibv_device* device) {
    return device != nullptr && strcmp(device->dev_name, dev_name) == 0;
}

ibv_context *suiw_open_device(ibv_device *device) {
    if (device == nullptr)
        return nullptr;
    ibv_context *context = (ibv_context*) malloc(sizeof(ibv_context));
    memset(context, 0, sizeof(ibv_context));
    context->device = device;
    // TODO there may be some more initialization to do here
    return context;
}

int suiw_close_device(ibv_context *context) {
    if (context == nullptr)
        return 0;
    free(context);
    return 0;
}

__be64 suiw_get_device_guid(ibv_device *device) {
    return htobe64(dev_guid);
}

int suiw_query_device(struct ibv_context *context, struct ibv_device_attr *device_attr) {
    return -1;
}

rdma_event_channel *suiw_create_event_channel() {
    // TODO need daemon to do this
    return nullptr;
}

void suiw_destroy_event_channel(rdma_event_channel *channel) {
    // TODO
}

int suiw_create_id(struct rdma_event_channel *channel,
		   struct rdma_cm_id **id, void *context,
		   enum rdma_port_space ps) {
    // TODO need daemon to do this
    return -1;
}

int suiw_destroy_id(struct rdma_cm_id *id) {
    // TODO
    return -1;
}

int suiw_resolve_addr(struct rdma_cm_id *id, struct sockaddr *src_addr,
		    struct sockaddr *dst_addr, int timeout_ms) {
    // TODO need daemon to do this
    return -1;
}

int suiw_get_cm_event(struct rdma_event_channel *channel,
            struct rdma_cm_event **event) {
    // TODO
    /*
    rdma_cm_event *recv_event = (rdma_cm_event*) malloc(sizeof(rdma_cm_event));
    *event = recv_event;
    int ret = read(channel->fd, recv_event, sizeof(rdma_cm_event));
    return ret == sizeof(rdma_cm_event) ? 0 : -1;
    */
    return -1;
}

int suiw_ack_cm_event(struct rdma_cm_event *event) {
    // TODO
    return -1;
}

int suiw_resolve_route(struct rdma_cm_id *id, int timeout_ms) {
    // TODO need daemon to do this
    return -1;
}

/* Runs when the library is loaded. */
__attribute__((constructor)) static void init(void) {
    printf("Loading libsuiw ...\n");
    suiw_ibv_device = create_suiw_device();
    connect_to_daemon();
}

/* Runs when the library is unloaded. */
__attribute__((destructor)) static void fini(void) {
    printf("Unloading libsuiw ...\n");
    free(suiw_ibv_device);
}
