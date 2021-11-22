/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2004, 2011-2012 Intel Corporation.  All rights reserved.
 * Copyright (c) 2005, 2006, 2007 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2005 PathScale, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
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
#include <dlfcn.h>

#include "verbs.h"
#include "softuiwarp.h"

/* Real library symbols; bound in `init` when this library is loaded. */
static struct ibv_device **(*real_get_device_list)(int*) = NULL;
static void (*real_free_device_list)(struct ibv_device**) = NULL;
static const char *(*real_get_device_name)(struct ibv_device*) = NULL;
static __be64 (*real_get_device_guid)(struct ibv_device*) = NULL;
static struct ibv_context *(*real_open_device)(struct ibv_device*) = NULL;
static int (*real_close_device)(struct ibv_context*) = NULL;
static int (*real_get_async_event)(struct ibv_context*, struct ibv_async_event*) = NULL;
static void (*real_ack_async_event)(struct ibv_async_event*) = NULL;
static int (*real_query_device)(struct ibv_context*, struct ibv_device_attr*) = NULL;
static int (*real_query_port)(struct ibv_context*, uint8_t, struct ibv_port_attr*) = NULL;
static int (*real_query_gid)(struct ibv_context*, uint8_t, int, union ibv_gid*) = NULL;
static int (*real_query_pkey)(struct ibv_context*, uint8_t, int, __be16*) = NULL;
static struct ibv_pd *(*real_alloc_pd)(struct ibv_context*) = NULL;
static int (*real_dealloc_pd)(struct ibv_pd*) = NULL;
static struct ibv_mr *(*real_reg_mr)(struct ibv_pd*, void*, size_t, int) = NULL;
static int (*real_rereg_mr)(struct ibv_mr*, int, struct ibv_pd*, void*, size_t, int) = NULL;
static int (*real_dereg_mr)(struct ibv_mr*) = NULL;
static struct ibv_comp_channel *(*real_create_comp_channel)(struct ibv_context *) = NULL;
static int (*real_destroy_comp_channel)(struct ibv_comp_channel*) = NULL;
static struct ibv_cq *(*real_create_cq)(struct ibv_context*, int, void*, struct ibv_comp_channel*, int) = NULL;
static int (*real_resize_cq)(struct ibv_cq*, int) = NULL;
static int (*real_destroy_cq)(struct ibv_cq*) = NULL;
static int (*real_get_cq_event)(struct ibv_comp_channel*, struct ibv_cq**, void**) = NULL;
static void (*real_ack_cq_events)(struct ibv_cq*, unsigned int) = NULL;
static struct ibv_srq *(*real_create_srq)(struct ibv_pd*, struct ibv_srq_init_attr*) = NULL;
static int (*real_modify_srq)(struct ibv_srq*, struct ibv_srq_attr*, int) = NULL;
static int (*real_query_srq)(struct ibv_srq*, struct ibv_srq_attr*) = NULL;
static int (*real_destroy_srq)(struct ibv_srq*) = NULL;
static struct ibv_qp *(*real_create_qp)(struct ibv_pd*, struct ibv_qp_init_attr*) = NULL;
static int (*real_modify_qp)(struct ibv_qp*, struct ibv_qp_attr*, int) = NULL;
static int (*real_query_qp)(struct ibv_qp*, struct ibv_qp_attr*, int, struct ibv_qp_init_attr*) = NULL;
static int (*real_destroy_qp)(struct ibv_qp*) = NULL;
static struct ibv_ah *(*real_create_ah)(struct ibv_pd*, struct ibv_ah_attr*) = NULL;
static int (*real_init_ah_from_wc)(struct ibv_context*, uint8_t, struct ibv_wc*, struct ibv_grh*, struct ibv_ah_attr*) = NULL;
static struct ibv_ah *(*real_create_ah_from_wc)(struct ibv_pd*, struct ibv_wc*, struct ibv_grh*, uint8_t) = NULL;
static int (*real_destroy_ah)(struct ibv_ah*) = NULL;
static int (*real_attach_mcast)(struct ibv_qp*, const union ibv_gid*, uint16_t) = NULL;
static int (*real_detach_mcast)(struct ibv_qp*, const union ibv_gid*, uint16_t) = NULL;
static int (*real_fork_init)(void) = NULL;
static const char *(*real_node_type_str)(enum ibv_node_type) = NULL;
static const char *(*real_port_state_str)(enum ibv_port_state) = NULL;
static const char *(*real_event_type_str)(enum ibv_event_type) = NULL;
static int (*real_resolve_eth_l2_from_gid)(struct ibv_context*, struct ibv_ah_attr*, uint8_t[], uint16_t*) = NULL;

#undef ibv_query_port

/**
 * ibv_get_device_list - Get list of IB devices currently available
 * @num_devices: optional.  if non-NULL, set to the number of devices
 * returned in the array.
 *
 * Return a NULL-terminated array of IB devices.  The array can be
 * released with ibv_free_device_list().
 */
struct ibv_device **ibv_get_device_list(int *num_devices) {
	printf("ibv_get_device_list\n");
    int num;
    // Get the number of real IB devices.
    ibv_device **device_list = real_get_device_list(&num);
    // We'll return a copy of that list, with our fake device added.
    ibv_device **result = (ibv_device**) malloc(sizeof(struct ibv_device*)*(num+2));
    for (int i = 0; i < num; i++) {
        // We need to individually copy all the structs within the list, 
        // since they'll be invalid after we free the original list.
        ibv_device *dev = (ibv_device*) malloc(sizeof(struct ibv_device));
        memcpy(dev, device_list[i], sizeof(struct ibv_device));
        result[i] = dev;
    }
    // Add our device.
    result[num] = suiw_get_ibv_device();
    // Null terminate.
    result[num+1] = nullptr;
    ibv_free_device_list(device_list);
    // Set the number of devices, if necessary.
    if (num_devices) {
        *num_devices = num+1;
    }
	return result;
}

/**
 * ibv_free_device_list - Free list from ibv_get_device_list()
 *
 * Free an array of devices returned from ibv_get_device_list().  Once
 * the array is freed, pointers to devices that were not opened with
 * ibv_open_device() are no longer valid.  Client code must open all
 * devices it intends to use before calling ibv_free_device_list().
 */
void ibv_free_device_list(struct ibv_device **list) {
	printf("ibv_free_device_list\n");
    int i = 0;
    while (list[i]) {
        free(list[i++]);
    }
    free(list);
}

/**
 * ibv_get_device_name - Return kernel device name
 */
const char *ibv_get_device_name(struct ibv_device *device) {
	printf("ibv_get_device_name\n");
    if (suiw_is_device_softuiwarp(device)) {
        return device->dev_name;
    } else {
        return real_get_device_name(device);
    }
}

/**
 * ibv_get_device_guid - Return device's node GUID
 */
__be64 ibv_get_device_guid(struct ibv_device *device) {
	printf("ibv_get_device_guid\n");
    if (suiw_is_device_softuiwarp(device)) {
        return suiw_get_device_guid(device);
    } else {
        return real_get_device_guid(device);
    }
}

/**
 * ibv_open_device - Initialize device for use
 */
struct ibv_context *ibv_open_device(struct ibv_device *device) {
	printf("ibv_open_device\n");
    if (suiw_is_device_softuiwarp(device)) {
        return suiw_open_device(device);
    } else {
        return real_open_device(device);
    }
}

/**
 * ibv_close_device - Release device
 */
int ibv_close_device(struct ibv_context *context) {
	printf("ibv_close_device\n");
    if (suiw_is_device_softuiwarp(context->device)) {
        return suiw_close_device(context);
    } else {
        return real_close_device(context);
    }
}

/**
 * ibv_get_async_event - Get next async event
 * @event: Pointer to use to return async event
 *
 * All async events returned by ibv_get_async_event() must eventually
 * be acknowledged with ibv_ack_async_event().
 */
int ibv_get_async_event(struct ibv_context *context,
			struct ibv_async_event *event) {
	printf("ibv_get_async_event\n");
	return -1;
}

/**
 * ibv_ack_async_event - Acknowledge an async event
 * @event: Event to be acknowledged.
 *
 * All async events which are returned by ibv_get_async_event() must
 * be acknowledged.  To avoid races, destroying an object (CQ, SRQ or
 * QP) will wait for all affiliated events to be acknowledged, so
 * there should be a one-to-one correspondence between acks and
 * successful gets.
 */
void ibv_ack_async_event(struct ibv_async_event *event) {
	printf("ibv_ack_async_event\n");
}

/**
 * ibv_query_device - Get device properties
 */
int ibv_query_device(struct ibv_context *context,
		     struct ibv_device_attr *device_attr) {
	printf("ibv_query_device\n");
    if (suiw_is_device_softuiwarp(context->device)) {
        return suiw_query_device(context, device_attr);
    } else {
        return real_query_device(context, device_attr);
    }
}

/**
 * ibv_query_port - Get port properties
 */
int ibv_query_port(struct ibv_context *context, uint8_t port_num,
		   struct ibv_port_attr *port_attr) {
	printf("ibv_query_port\n");
	return -1;
}

/**
 * ibv_query_gid - Get a GID table entry
 */
int ibv_query_gid(struct ibv_context *context, uint8_t port_num,
		  int index, union ibv_gid *gid) {
	printf("ibv_query_gid\n");
	return -1;
}

/**
 * ibv_query_pkey - Get a P_Key table entry
 */
int ibv_query_pkey(struct ibv_context *context, uint8_t port_num,
		   int index, __be16 *pkey) {
	printf("ibv_query_gid\n");
	return -1;
}

/**
 * ibv_alloc_pd - Allocate a protection domain
 */
struct ibv_pd *ibv_alloc_pd(struct ibv_context *context) {
	printf("ibv_alloc_pd\n");
	return NULL;
}

/**
 * ibv_dealloc_pd - Free a protection domain
 */
int ibv_dealloc_pd(struct ibv_pd *pd) {
	printf("ibv_dealloc_pd\n");
	return -1;
}

/**
 * ibv_reg_mr - Register a memory region
 */
struct ibv_mr *ibv_reg_mr(struct ibv_pd *pd, void *addr,
			  size_t length, int access) {
	printf("ibv_reg_mr\n");
	return NULL;
}

/**
 * ibv_rereg_mr - Re-Register a memory region
 */
int ibv_rereg_mr(struct ibv_mr *mr, int flags,
		 struct ibv_pd *pd, void *addr,
		 size_t length, int access) {
	printf("ibv_rereg_mr\n");
	return -1;
}
/**
 * ibv_dereg_mr - Deregister a memory region
 */
int ibv_dereg_mr(struct ibv_mr *mr) {
	printf("ibv_dereg_mr\n");
	return -1;
}

/**
 * ibv_create_comp_channel - Create a completion event channel
 */
struct ibv_comp_channel *ibv_create_comp_channel(struct ibv_context *context) {
	printf("ibv_create_comp_channel\n");
	return NULL;
}

/**
 * ibv_destroy_comp_channel - Destroy a completion event channel
 */
int ibv_destroy_comp_channel(struct ibv_comp_channel *channel) {
	printf("ibv_destroy_comp_channel\n");
	return -1;
}

/**
 * ibv_create_cq - Create a completion queue
 * @context - Context CQ will be attached to
 * @cqe - Minimum number of entries required for CQ
 * @cq_context - Consumer-supplied context returned for completion events
 * @channel - Completion channel where completion events will be queued.
 *     May be NULL if completion events will not be used.
 * @comp_vector - Completion vector used to signal completion events.
 *     Must be >= 0 and < context->num_comp_vectors.
 */
struct ibv_cq *ibv_create_cq(struct ibv_context *context, int cqe,
			     void *cq_context,
			     struct ibv_comp_channel *channel,
			     int comp_vector) {
	printf("ibv_create_cq\n");
	return NULL;
}

/**
 * ibv_resize_cq - Modifies the capacity of the CQ.
 * @cq: The CQ to resize.
 * @cqe: The minimum size of the CQ.
 *
 * Users can examine the cq structure to determine the actual CQ size.
 */
int ibv_resize_cq(struct ibv_cq *cq, int cqe) {
	printf("ibv_resize_cq\n");
	return -1;
}

/**
 * ibv_destroy_cq - Destroy a completion queue
 */
int ibv_destroy_cq(struct ibv_cq *cq) {
	printf("ibv_destroy_cq\n");
	return -1;
}

/**
 * ibv_get_cq_event - Read next CQ event
 * @channel: Channel to get next event from.
 * @cq: Used to return pointer to CQ.
 * @cq_context: Used to return consumer-supplied CQ context.
 *
 * All completion events returned by ibv_get_cq_event() must
 * eventually be acknowledged with ibv_ack_cq_events().
 */
int ibv_get_cq_event(struct ibv_comp_channel *channel,
		     struct ibv_cq **cq, void **cq_context) {
	printf("ibv_get_cq_event\n");
	return -1;
}

/**
 * ibv_ack_cq_events - Acknowledge CQ completion events
 * @cq: CQ to acknowledge events for
 * @nevents: Number of events to acknowledge.
 *
 * All completion events which are returned by ibv_get_cq_event() must
 * be acknowledged.  To avoid races, ibv_destroy_cq() will wait for
 * all completion events to be acknowledged, so there should be a
 * one-to-one correspondence between acks and successful gets.  An
 * application may accumulate multiple completion events and
 * acknowledge them in a single call to ibv_ack_cq_events() by passing
 * the number of events to ack in @nevents.
 */
void ibv_ack_cq_events(struct ibv_cq *cq, unsigned int nevents) {
	printf("ibv_ack_cq_events\n");
}

/**
 * ibv_create_srq - Creates a SRQ associated with the specified protection
 *   domain.
 * @pd: The protection domain associated with the SRQ.
 * @srq_init_attr: A list of initial attributes required to create the SRQ.
 *
 * srq_attr->max_wr and srq_attr->max_sge are read the determine the
 * requested size of the SRQ, and set to the actual values allocated
 * on return.  If ibv_create_srq() succeeds, then max_wr and max_sge
 * will always be at least as large as the requested values.
 */
struct ibv_srq *ibv_create_srq(struct ibv_pd *pd,
			       struct ibv_srq_init_attr *srq_init_attr) {
	printf("ibv_create_srq\n");
	return NULL;
}

/**
 * ibv_modify_srq - Modifies the attributes for the specified SRQ.
 * @srq: The SRQ to modify.
 * @srq_attr: On input, specifies the SRQ attributes to modify.  On output,
 *   the current values of selected SRQ attributes are returned.
 * @srq_attr_mask: A bit-mask used to specify which attributes of the SRQ
 *   are being modified.
 *
 * The mask may contain IBV_SRQ_MAX_WR to resize the SRQ and/or
 * IBV_SRQ_LIMIT to set the SRQ's limit and request notification when
 * the number of receives queued drops below the limit.
 */
int ibv_modify_srq(struct ibv_srq *srq,
		   struct ibv_srq_attr *srq_attr,
		   int srq_attr_mask) {
	printf("ibv_modify_srq\n");
	return -1;
}

/**
 * ibv_query_srq - Returns the attribute list and current values for the
 *   specified SRQ.
 * @srq: The SRQ to query.
 * @srq_attr: The attributes of the specified SRQ.
 */
int ibv_query_srq(struct ibv_srq *srq, struct ibv_srq_attr *srq_attr) {
	printf("ibv_query_srq\n");
	return -1;
}

/**
 * ibv_destroy_srq - Destroys the specified SRQ.
 * @srq: The SRQ to destroy.
 */
int ibv_destroy_srq(struct ibv_srq *srq) {
	printf("ibv_destroy_srq\n");
	return -1;
}

/**
 * ibv_create_qp - Create a queue pair.
 */
struct ibv_qp *ibv_create_qp(struct ibv_pd *pd,
			     struct ibv_qp_init_attr *qp_init_attr) {
	printf("ibv_create_qp\n");
	return NULL;
}

/**
 * ibv_modify_qp - Modify a queue pair.
 */
int ibv_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		  int attr_mask) {
	printf("ibv_modify_qp\n");
	return -1;
}

/**
 * ibv_query_qp - Returns the attribute list and current values for the
 *   specified QP.
 * @qp: The QP to query.
 * @attr: The attributes of the specified QP.
 * @attr_mask: A bit-mask used to select specific attributes to query.
 * @init_attr: Additional attributes of the selected QP.
 *
 * The qp_attr_mask may be used to limit the query to gathering only the
 * selected attributes.
 */
int ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		 int attr_mask,
		 struct ibv_qp_init_attr *init_attr) {
	printf("ibv_query_qp\n");
	return -1;
}

/**
 * ibv_destroy_qp - Destroy a queue pair.
 */
int ibv_destroy_qp(struct ibv_qp *qp) {
	printf("ibv_destroy_qp\n");
	return -1;
}

/**
 * ibv_create_ah - Create an address handle.
 */
struct ibv_ah *ibv_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr) {
	printf("ibv_create_ah\n");
	return NULL;
}

/**
 * ibv_init_ah_from_wc - Initializes address handle attributes from a
 *   work completion.
 * @context: Device context on which the received message arrived.
 * @port_num: Port on which the received message arrived.
 * @wc: Work completion associated with the received message.
 * @grh: References the received global route header.  This parameter is
 *   ignored unless the work completion indicates that the GRH is valid.
 * @ah_attr: Returned attributes that can be used when creating an address
 *   handle for replying to the message.
 */
int ibv_init_ah_from_wc(struct ibv_context *context, uint8_t port_num,
			struct ibv_wc *wc, struct ibv_grh *grh,
			struct ibv_ah_attr *ah_attr) {
	printf("ibv_init_ah_from_wc\n");
	return -1;
}

/**
 * ibv_create_ah_from_wc - Creates an address handle associated with the
 *   sender of the specified work completion.
 * @pd: The protection domain associated with the address handle.
 * @wc: Work completion information associated with a received message.
 * @grh: References the received global route header.  This parameter is
 *   ignored unless the work completion indicates that the GRH is valid.
 * @port_num: The outbound port number to associate with the address.
 *
 * The address handle is used to reference a local or global destination
 * in all UD QP post sends.
 */
struct ibv_ah *ibv_create_ah_from_wc(struct ibv_pd *pd, struct ibv_wc *wc,
				     struct ibv_grh *grh, uint8_t port_num) {
	printf("ibv_create_ah_from_wc\n");
	return NULL;
}

/**
 * ibv_destroy_ah - Destroy an address handle.
 */
int ibv_destroy_ah(struct ibv_ah *ah) {
	printf("ibv_destroy_ah\n");
	return -1;
}

/**
 * ibv_attach_mcast - Attaches the specified QP to a multicast group.
 * @qp: QP to attach to the multicast group.  The QP must be a UD QP.
 * @gid: Multicast group GID.
 * @lid: Multicast group LID in host byte order.
 *
 * In order to route multicast packets correctly, subnet
 * administration must have created the multicast group and configured
 * the fabric appropriately.  The port associated with the specified
 * QP must also be a member of the multicast group.
 */
int ibv_attach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid) {
	printf("ibv_attach_mcast\n");
	return -1;
}

/**
 * ibv_detach_mcast - Detaches the specified QP from a multicast group.
 * @qp: QP to detach from the multicast group.
 * @gid: Multicast group GID.
 * @lid: Multicast group LID in host byte order.
 */
int ibv_detach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid) {
	printf("ibv_detach_mcast\n");
	return -1;
}

/**
 * ibv_fork_init - Prepare data structures so that fork() may be used
 * safely.  If this function is not called or returns a non-zero
 * status, then libibverbs data structures are not fork()-safe and the
 * effect of an application calling fork() is undefined.
 */
int ibv_fork_init(void) {
	printf("ibv_fork_init\n");
	return -1;
}

int ibv_resolve_eth_l2_from_gid(struct ibv_context *context,
				struct ibv_ah_attr *attr,
				uint8_t eth_mac[ETHERNET_LL_SIZE],
				uint16_t *vid) {
	printf("ibv_resolve_eth_l2_from_gid\n");
	return -1;
}

/* 
 * Stringificatiion functions, that we can just passthrough.
 */
const char *ibv_node_type_str(enum ibv_node_type node_type) {
    return real_node_type_str(node_type);
}

const char *ibv_port_state_str(enum ibv_port_state port_state) {
	return real_port_state_str(port_state);
}

const char *ibv_event_type_str(enum ibv_event_type event) {
	return real_event_type_str(event);
}

/* Binds a symbol to the real library's symbol. */
static void *bind_symbol(const char *sym) {
    void *handle = dlopen("/users/sg99/softiwarp-user-for-linux-rdma/build/lib/libibverbs.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "RDMA lib interpose: dlopen failed\n");
        abort();
    }
    void *ptr;
    if ((ptr = dlsym(handle, sym)) == NULL) { 
        fprintf(stderr, "RDMA lib interpose: dlsym failed (%s)\n", sym);
        abort();
    }
    return ptr;
}

__attribute__((constructor)) static void init(void) {
    printf("Loading libibverbs interposition...\n");
    // Bind the real libibverbs functions.
    real_get_device_list = (ibv_device** (*)(int*)) bind_symbol("ibv_get_device_list");
    real_free_device_list = (void (*)(ibv_device**)) bind_symbol("ibv_free_device_list");
    real_get_device_name = (const char* (*)(ibv_device*)) bind_symbol("ibv_get_device_name");
    real_get_device_guid = (__be64 (*)(ibv_device*)) bind_symbol("ibv_get_device_guid");
    real_open_device = (ibv_context* (*)(ibv_device*)) bind_symbol("ibv_open_device");
    real_close_device = (int (*)(ibv_context*)) bind_symbol("ibv_close_device");
    real_get_async_event = (int (*)(ibv_context*, ibv_async_event*)) bind_symbol("ibv_get_async_event");
    real_ack_async_event = (void (*)(ibv_async_event*)) bind_symbol("ibv_ack_async_event");
    real_query_device = (int (*)(ibv_context*, ibv_device_attr*)) bind_symbol("ibv_query_device");
    real_query_port = (int (*)(ibv_context*, uint8_t, ibv_port_attr*)) bind_symbol("ibv_query_port");
    real_query_gid = (int (*)(ibv_context*, uint8_t, int, ibv_gid*)) bind_symbol("ibv_query_gid");
    real_query_pkey = (int (*)(ibv_context*, uint8_t, int, __be16*)) bind_symbol("ibv_query_pkey");
    real_alloc_pd = (ibv_pd* (*)(ibv_context*)) bind_symbol("ibv_alloc_pd");
    real_dealloc_pd = (int (*)(ibv_pd*)) bind_symbol("ibv_dealloc_pd");
    real_reg_mr = (ibv_mr* (*)(ibv_pd*, void*, size_t, int)) bind_symbol("ibv_reg_mr");
    real_rereg_mr = (int (*)(ibv_mr*, int, ibv_pd*, void*, size_t, int)) bind_symbol("ibv_rereg_mr");
    real_dereg_mr = (int (*)(ibv_mr*)) bind_symbol("ibv_dereg_mr");
    real_create_comp_channel = (ibv_comp_channel* (*)(ibv_context*)) bind_symbol("ibv_create_comp_channel");
    real_create_cq = (ibv_cq* (*)(ibv_context*, int, void*, ibv_comp_channel*, int)) bind_symbol("ibv_create_cq");
    real_resize_cq = (int (*)(ibv_cq*, int)) bind_symbol("ibv_resize_cq");
    real_destroy_cq = (int (*)(ibv_cq*)) bind_symbol("ibv_destroy_cq");
    real_get_cq_event = (int (*)(ibv_comp_channel*, ibv_cq**, void**)) bind_symbol("ibv_get_cq_event");
    real_ack_cq_events = (void (*)(ibv_cq*, unsigned int)) bind_symbol("ibv_ack_cq_events");
    real_create_srq = (ibv_srq* (*)(ibv_pd*, ibv_srq_init_attr*)) bind_symbol("ibv_create_srq");
    real_modify_srq = (int (*)(ibv_srq*, ibv_srq_attr*, int)) bind_symbol("ibv_modify_srq");
    real_query_srq = (int (*)(ibv_srq*, ibv_srq_attr*)) bind_symbol("ibv_query_srq");
    real_destroy_srq = (int (*)(ibv_srq*)) bind_symbol("ibv_destroy_srq");
    real_create_qp = (ibv_qp* (*)(ibv_pd*, ibv_qp_init_attr*)) bind_symbol("ibv_create_qp");
    real_modify_qp = (int (*)(ibv_qp*, ibv_qp_attr*, int)) bind_symbol("ibv_modify_qp");
    real_query_qp = (int (*)(ibv_qp*, ibv_qp_attr*, int, ibv_qp_init_attr*)) bind_symbol("ibv_query_qp");
    real_destroy_qp = (int (*)(ibv_qp*)) bind_symbol("ibv_destroy_qp");
    real_create_ah = (ibv_ah* (*)(ibv_pd*, ibv_ah_attr*)) bind_symbol("ibv_create_ah");
    real_init_ah_from_wc = (int (*)(ibv_context*, uint8_t, ibv_wc*, ibv_grh*, ibv_ah_attr*)) bind_symbol("ibv_init_ah_from_wc");
    real_create_ah_from_wc = (ibv_ah* (*)(ibv_pd*, ibv_wc*, ibv_grh*, uint8_t)) bind_symbol("ibv_create_ah_from_wc");
    real_destroy_ah = (int (*)(ibv_ah*)) bind_symbol("ibv_destroy_ah");
    real_attach_mcast = (int (*)(ibv_qp*, const ibv_gid*, uint16_t)) bind_symbol("ibv_attach_mcast");
    real_detach_mcast = (int (*)(ibv_qp*, const ibv_gid*, uint16_t)) bind_symbol("ibv_detach_mcast");
    real_fork_init = (int (*)()) bind_symbol("ibv_fork_init");
    real_node_type_str = (const char* (*)(ibv_node_type)) bind_symbol("ibv_node_type_str");
    real_port_state_str = (const char* (*)(ibv_port_state)) bind_symbol("ibv_port_state_str");
    real_event_type_str = (const char* (*)(ibv_event_type)) bind_symbol("ibv_event_type_str");
    real_resolve_eth_l2_from_gid = (int (*)(ibv_context*, ibv_ah_attr*, uint8_t*, uint16_t*)) bind_symbol("ibv_resolve_eth_l2_from_gid");
}

/* All the enum to int conversion functions. Copy-pasted from the original implementation. */

int __attribute__((const)) ibv_rate_to_mult(enum ibv_rate rate)
{
        switch (rate) {
        case IBV_RATE_2_5_GBPS: return  1;
        case IBV_RATE_5_GBPS:   return  2;
        case IBV_RATE_10_GBPS:  return  4;
        case IBV_RATE_20_GBPS:  return  8;
        case IBV_RATE_30_GBPS:  return 12;
        case IBV_RATE_40_GBPS:  return 16;
        case IBV_RATE_60_GBPS:  return 24;
        case IBV_RATE_80_GBPS:  return 32;
        case IBV_RATE_120_GBPS: return 48;
        default:           return -1;
        }
}

enum ibv_rate __attribute__((const)) mult_to_ibv_rate(int mult)
{
        switch (mult) {
        case 1:  return IBV_RATE_2_5_GBPS;
        case 2:  return IBV_RATE_5_GBPS;
        case 4:  return IBV_RATE_10_GBPS;
        case 8:  return IBV_RATE_20_GBPS;
        case 12: return IBV_RATE_30_GBPS;
        case 16: return IBV_RATE_40_GBPS;
        case 24: return IBV_RATE_60_GBPS;
        case 32: return IBV_RATE_80_GBPS;
        case 48: return IBV_RATE_120_GBPS;
        default: return IBV_RATE_MAX;
        }
}

int  __attribute__((const)) ibv_rate_to_mbps(enum ibv_rate rate)
{
        switch (rate) {
        case IBV_RATE_2_5_GBPS: return 2500;
        case IBV_RATE_5_GBPS:   return 5000;
        case IBV_RATE_10_GBPS:  return 10000;
        case IBV_RATE_20_GBPS:  return 20000;
        case IBV_RATE_30_GBPS:  return 30000;
        case IBV_RATE_40_GBPS:  return 40000;
        case IBV_RATE_60_GBPS:  return 60000;
        case IBV_RATE_80_GBPS:  return 80000;
        case IBV_RATE_120_GBPS: return 120000;
        case IBV_RATE_14_GBPS:  return 14062;
        case IBV_RATE_56_GBPS:  return 56250;
        case IBV_RATE_112_GBPS: return 112500;
        case IBV_RATE_168_GBPS: return 168750;
        case IBV_RATE_25_GBPS:  return 25781;
        case IBV_RATE_100_GBPS: return 103125;
        case IBV_RATE_200_GBPS: return 206250;
        case IBV_RATE_300_GBPS: return 309375;
        default:               return -1;
        }
}

enum ibv_rate __attribute__((const)) mbps_to_ibv_rate(int mbps)
{
        switch (mbps) {
        case 2500:   return IBV_RATE_2_5_GBPS;
        case 5000:   return IBV_RATE_5_GBPS;
        case 10000:  return IBV_RATE_10_GBPS;
        case 20000:  return IBV_RATE_20_GBPS;
        case 30000:  return IBV_RATE_30_GBPS;
        case 40000:  return IBV_RATE_40_GBPS;
        case 60000:  return IBV_RATE_60_GBPS;
        case 80000:  return IBV_RATE_80_GBPS;
        case 120000: return IBV_RATE_120_GBPS;
        case 14062:  return IBV_RATE_14_GBPS;
        case 56250:  return IBV_RATE_56_GBPS;
        case 112500: return IBV_RATE_112_GBPS;
        case 168750: return IBV_RATE_168_GBPS;
        case 25781:  return IBV_RATE_25_GBPS;
        case 103125: return IBV_RATE_100_GBPS;
        case 206250: return IBV_RATE_200_GBPS;
        case 309375: return IBV_RATE_300_GBPS;
        default:     return IBV_RATE_MAX;
        }
}

