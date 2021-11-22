#ifndef SOFTUIWARP_H
#define SOFTUIWARP_H

#include <stdlib.h>

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

ibv_device *suiw_get_ibv_device();

bool suiw_is_device_softuiwarp(ibv_device *device);

ibv_context *suiw_open_device(ibv_device *device);

int suiw_close_device(ibv_context *context);

__be64 suiw_get_device_guid(ibv_device *device);

int suiw_query_device(struct ibv_context *context, struct ibv_device_attr *device_attr);

rdma_event_channel *suiw_create_event_channel();

void suiw_destroy_event_channel(struct rdma_event_channel*);

int suiw_create_id(struct rdma_event_channel *channel,
                   struct rdma_cm_id **id, void *context,
                   enum rdma_port_space ps);

int suiw_destroy_id(struct rdma_cm_id *id);

int suiw_resolve_addr(struct rdma_cm_id *id, struct sockaddr *src_addr,
                      struct sockaddr *dst_addr, int timeout_ms);
    
int suiw_get_cm_event(struct rdma_event_channel *channel,
                      struct rdma_cm_event **event);

int suiw_ack_cm_event(struct rdma_cm_event *event);

int suiw_resolve_route(struct rdma_cm_id *id, int timeout_ms);

#endif // SOFTUIWARP_H
