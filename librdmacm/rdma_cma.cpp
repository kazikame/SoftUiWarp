/*
 * Copyright (c) 2005 Voltaire Inc.  All rights reserved.
 * Copyright (c) 2005-2014 Intel Corporation.  All rights reserved.
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

#include "rdma_cma.h"

/* Real library symbols; bound in `init` when this library is loaded. */
static struct rdma_event_channel *(*real_create_event_channel)(void) = NULL;
static void (*real_destroy_event_channel)(struct rdma_event_channel*) = NULL;
static int (*real_create_id)(struct rdma_event_channel*, struct rdma_cm_id**, void*, enum rdma_port_space) = NULL;
static int (*real_create_ep)(struct rdma_cm_id**, struct rdma_addrinfo*, struct ibv_pd*, struct ibv_qp_init_attr*) = NULL;
static void (*real_destroy_ep)(struct rdma_cm_id*) = NULL;
static int (*real_destroy_id)(struct rdma_cm_id*) = NULL;
static int (*real_bind_addr)(struct rdma_cm_id*, struct sockaddr*) = NULL;
static int (*real_resolve_addr)(struct rdma_cm_id*, struct sockaddr*, struct sockaddr*, int) = NULL;
static int (*real_resolve_route)(struct rdma_cm_id*, int) = NULL;
static int (*real_create_qp)(struct rdma_cm_id*, struct ibv_pd*, struct ibv_qp_init_attr*) = NULL;
static int (*real_create_qp_ex)(struct rdma_cm_id*, struct ibv_qp_init_attr_ex*) = NULL;
static void (*real_destroy_qp)(struct rdma_cm_id*) = NULL;
static int (*real_connect)(struct rdma_cm_id*, struct rdma_conn_param*) = NULL;
static int (*real_listen)(struct rdma_cm_id*, int) = NULL;
static int (*real_get_request)(struct rdma_cm_id*, struct rdma_cm_id**) = NULL;
static int (*real_accept)(struct rdma_cm_id*, struct rdma_conn_param*) = NULL;
static int (*real_reject)(struct rdma_cm_id*, const void*, uint8_t) = NULL;
static int (*real_notify)(struct rdma_cm_id*, enum ibv_event_type) = NULL;
static int (*real_disconnect)(struct rdma_cm_id*) = NULL;
static int (*real_join_multicast)(struct rdma_cm_id*, struct sockaddr*, void*);
static int (*real_leave_multicast)(struct rdma_cm_id*, struct sockaddr*) = NULL;
static int (*real_join_multicast_ex)(struct rdma_cm_id*, struct rdma_cm_join_mc_attr_ex*, void*) = NULL;
static int (*real_get_cm_event)(struct rdma_event_channel*, struct rdma_cm_event**) = NULL;
static int (*real_ack_cm_event)(struct rdma_cm_event*) = NULL;
static __be16 (*real_get_src_port)(struct rdma_cm_id*) = NULL;
static __be16 (*real_get_dst_port)(struct rdma_cm_id*) = NULL;
static struct ibv_context **(*real_get_devices)(int*) = NULL;
static void (*real_free_devices)(struct ibv_context**) = NULL;
static const char *(*real_event_str)(enum rdma_cm_event_type) = NULL;
static int (*real_set_option)(struct rdma_cm_id*, int, int , void*, size_t) = NULL;
static int (*real_migrate_id)(struct rdma_cm_id*, struct rdma_event_channel*) = NULL;
static int (*real_getaddrinfo)(const char*, const char*, const struct rdma_addrinfo*, struct rdma_addrinfo**) = NULL;
static void (*real_freeaddrinfo)(struct rdma_addrinfo*) = NULL;

/**
 * rdma_create_event_channel - Open a channel used to report communication events.
 * Description:
 *   Asynchronous events are reported to users through event channels.  Each
 *   event channel maps to a file descriptor.
 * Notes:
 *   All created event channels must be destroyed by calling
 *   rdma_destroy_event_channel.  Users should call rdma_get_cm_event to
 *   retrieve events on an event channel.
 * See also:
 *   rdma_get_cm_event, rdma_destroy_event_channel
 */
struct rdma_event_channel *rdma_create_event_channel(void) {
	printf("rdma_create_event_channel\n");
	return NULL;
}

/**
 * rdma_destroy_event_channel - Close an event communication channel.
 * @channel: The communication channel to destroy.
 * Description:
 *   Release all resources associated with an event channel and closes the
 *   associated file descriptor.
 * Notes:
 *   All rdma_cm_id's associated with the event channel must be destroyed,
 *   and all returned events must be acked before calling this function.
 * See also:
 *  rdma_create_event_channel, rdma_get_cm_event, rdma_ack_cm_event
 */
void rdma_destroy_event_channel(struct rdma_event_channel *channel) {
	printf("rdma_destroy_event_channel\n");
}

/**
 * rdma_create_id - Allocate a communication identifier.
 * @channel: The communication channel that events associated with the
 *   allocated rdma_cm_id will be reported on.
 * @id: A reference where the allocated communication identifier will be
 *   returned.
 * @context: User specified context associated with the rdma_cm_id.
 * @ps: RDMA port space.
 * Description:
 *   Creates an identifier that is used to track communication information.
 * Notes:
 *   Rdma_cm_id's are conceptually equivalent to a socket for RDMA
 *   communication.  The difference is that RDMA communication requires
 *   explicitly binding to a specified RDMA device before communication
 *   can occur, and most operations are asynchronous in nature.  Communication
 *   events on an rdma_cm_id are reported through the associated event
 *   channel.  Users must release the rdma_cm_id by calling rdma_destroy_id.
 * See also:
 *   rdma_create_event_channel, rdma_destroy_id, rdma_get_devices,
 *   rdma_bind_addr, rdma_resolve_addr, rdma_connect, rdma_listen,
 */
int rdma_create_id(struct rdma_event_channel *channel,
		   struct rdma_cm_id **id, void *context,
		   enum rdma_port_space ps) {
	printf("rdma_create_id\n");
	return -1;
}

/**
 * rdma_create_ep - Allocate a communication identifier and qp.
 * @id: A reference where the allocated communication identifier will be
 *   returned.
 * @res: Result from rdma_getaddrinfo, which specifies the source and
 *   destination addresses, plus optional routing and connection information.
 * @pd: Optional protection domain.  This parameter is ignored if qp_init_attr
 *   is NULL.
 * @qp_init_attr: Optional attributes for a QP created on the rdma_cm_id.
 * Description:
 *   Create an identifier and option QP used for communication.
 * Notes:
 *   If qp_init_attr is provided, then a queue pair will be allocated and
 *   associated with the rdma_cm_id.  If a pd is provided, the QP will be
 *   created on that PD.  Otherwise, the QP will be allocated on a default
 *   PD.
 *   The rdma_cm_id will be set to use synchronous operations (connect,
 *   listen, and get_request).  To convert to asynchronous operation, the
 *   rdma_cm_id should be migrated to a user allocated event channel.
 * See also:
 *   rdma_create_id, rdma_create_qp, rdma_migrate_id, rdma_connect,
 *   rdma_listen
 */
int rdma_create_ep(struct rdma_cm_id **id, struct rdma_addrinfo *res,
		   struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr) {
	printf("rdma_create_ep\n");
	return -1;
}

/**
 * rdma_destroy_ep - Deallocates a communication identifier and qp.
 * @id: The communication identifier to destroy.
 * Description:
 *   Destroys the specified rdma_cm_id and any associated QP created
 *   on that id.
 * See also:
 *   rdma_create_ep
 */
void rdma_destroy_ep(struct rdma_cm_id *id) {
	printf("rdma_destroy_ep\n");
}

/**
 * rdma_destroy_id - Release a communication identifier.
 * @id: The communication identifier to destroy.
 * Description:
 *   Destroys the specified rdma_cm_id and cancels any outstanding
 *   asynchronous operation.
 * Notes:
 *   Users must free any associated QP with the rdma_cm_id before
 *   calling this routine and ack an related events.
 * See also:
 *   rdma_create_id, rdma_destroy_qp, rdma_ack_cm_event
 */
int rdma_destroy_id(struct rdma_cm_id *id) {
	printf("rdma_destroy_id\n");
	return -1;
}

/**
 * rdma_bind_addr - Bind an RDMA identifier to a source address.
 * @id: RDMA identifier.
 * @addr: Local address information.  Wildcard values are permitted.
 * Description:
 *   Associates a source address with an rdma_cm_id.  The address may be
 *   wildcarded.  If binding to a specific local address, the rdma_cm_id
 *   will also be bound to a local RDMA device.
 * Notes:
 *   Typically, this routine is called before calling rdma_listen to bind
 *   to a specific port number, but it may also be called on the active side
 *   of a connection before calling rdma_resolve_addr to bind to a specific
 *   address.
 * See also:
 *   rdma_create_id, rdma_listen, rdma_resolve_addr, rdma_create_qp
 */
int rdma_bind_addr(struct rdma_cm_id *id, struct sockaddr *addr) {
	printf("rdma_bind_addr\n");
	return -1;
}

/**
 * rdma_resolve_addr - Resolve destination and optional source addresses.
 * @id: RDMA identifier.
 * @src_addr: Source address information.  This parameter may be NULL.
 * @dst_addr: Destination address information.
 * @timeout_ms: Time to wait for resolution to complete.
 * Description:
 *   Resolve destination and optional source addresses from IP addresses
 *   to an RDMA address.  If successful, the specified rdma_cm_id will
 *   be bound to a local device.
 * Notes:
 *   This call is used to map a given destination IP address to a usable RDMA
 *   address.  If a source address is given, the rdma_cm_id is bound to that
 *   address, the same as if rdma_bind_addr were called.  If no source
 *   address is given, and the rdma_cm_id has not yet been bound to a device,
 *   then the rdma_cm_id will be bound to a source address based on the
 *   local routing tables.  After this call, the rdma_cm_id will be bound to
 *   an RDMA device.  This call is typically made from the active side of a
 *   connection before calling rdma_resolve_route and rdma_connect.
 * See also:
 *   rdma_create_id, rdma_resolve_route, rdma_connect, rdma_create_qp,
 *   rdma_get_cm_event, rdma_bind_addr
 */
int rdma_resolve_addr(struct rdma_cm_id *id, struct sockaddr *src_addr,
		      struct sockaddr *dst_addr, int timeout_ms) {
	printf("rdma_resolve_addr\n");
	return -1;
}


/**
 * rdma_resolve_route - Resolve the route information needed to establish a connection.
 * @id: RDMA identifier.
 * @timeout_ms: Time to wait for resolution to complete.
 * Description:
 *   Resolves an RDMA route to the destination address in order to establish
 *   a connection.  The destination address must have already been resolved
 *   by calling rdma_resolve_addr.
 * Notes:
 *   This is called on the client side of a connection after calling
 *   rdma_resolve_addr, but before calling rdma_connect.
 * See also:
 *   rdma_resolve_addr, rdma_connect, rdma_get_cm_event
 */
int rdma_resolve_route(struct rdma_cm_id *id, int timeout_ms) {
	printf("rdma_resolve_route\n");
	return -1;
}

/**
 * rdma_create_qp - Allocate a QP.
 * @id: RDMA identifier.
 * @pd: Optional protection domain for the QP.
 * @qp_init_attr: initial QP attributes.
 * Description:
 *  Allocate a QP associated with the specified rdma_cm_id and transition it
 *  for sending and receiving.
 * Notes:
 *   The rdma_cm_id must be bound to a local RDMA device before calling this
 *   function, and the protection domain must be for that same device.
 *   QPs allocated to an rdma_cm_id are automatically transitioned by the
 *   librdmacm through their states.  After being allocated, the QP will be
 *   ready to handle posting of receives.  If the QP is unconnected, it will
 *   be ready to post sends.
 *   If pd is NULL, then the QP will be allocated using a default protection
 *   domain associated with the underlying RDMA device.
 * See also:
 *   rdma_bind_addr, rdma_resolve_addr, rdma_destroy_qp, ibv_create_qp,
 *   ibv_modify_qp
 */
int rdma_create_qp(struct rdma_cm_id *id, struct ibv_pd *pd,
		   struct ibv_qp_init_attr *qp_init_attr) {
	printf("rdma_create_qp\n");
	return -1;
}

int rdma_create_qp_ex(struct rdma_cm_id *id,
		      struct ibv_qp_init_attr_ex *qp_init_attr) {
	printf("rdma_create_qp_ex\n");
	return -1;
}

/**
 * rdma_destroy_qp - Deallocate a QP.
 * @id: RDMA identifier.
 * Description:
 *   Destroy a QP allocated on the rdma_cm_id.
 * Notes:
 *   Users must destroy any QP associated with an rdma_cm_id before
 *   destroying the ID.
 * See also:
 *   rdma_create_qp, rdma_destroy_id, ibv_destroy_qp
 */
void rdma_destroy_qp(struct rdma_cm_id *id) {
	printf("rdma_destroy_qp\n");
}

/**
 * rdma_connect - Initiate an active connection request.
 * @id: RDMA identifier.
 * @conn_param: optional connection parameters.
 * Description:
 *   For a connected rdma_cm_id, this call initiates a connection request
 *   to a remote destination.  For an unconnected rdma_cm_id, it initiates
 *   a lookup of the remote QP providing the datagram service.
 * Notes:
 *   Users must have resolved a route to the destination address
 *   by having called rdma_resolve_route before calling this routine.
 *   A user may override the default connection parameters and exchange
 *   private data as part of the connection by using the conn_param parameter.
 * See also:
 *   rdma_resolve_route, rdma_disconnect, rdma_listen, rdma_get_cm_event
 */
int rdma_connect(struct rdma_cm_id *id, struct rdma_conn_param *conn_param) {
	printf("rdma_connect\n");
	return -1;
}

/**
 * rdma_establish - Complete an active connection request.
 * @id: RDMA identifier.
 * Description:
 *   Acknowledge an incoming connection response event and complete the
 *   connection establishment.
 * Notes:
 *   If a QP has not been created on the rdma_cm_id, this function should be
 *   called by the active side to complete the connection, after getting connect
 *   response event. This will trigger a connection established event on the
 *   passive side.
 *   This function should not be used on an rdma_cm_id on which a QP has been
 *   created.
 * See also:
 *   rdma_connect, rdma_disconnect, rdma_get_cm_event
 */
int rdma_establish(struct rdma_cm_id *id) {
	printf("rdma_establish\n");
	return -1;
}

/**
 * rdma_listen - Listen for incoming connection requests.
 * @id: RDMA identifier.
 * @backlog: backlog of incoming connection requests.
 * Description:
 *   Initiates a listen for incoming connection requests or datagram service
 *   lookup.  The listen will be restricted to the locally bound source
 *   address.
 * Notes:
 *   Users must have bound the rdma_cm_id to a local address by calling
 *   rdma_bind_addr before calling this routine.  If the rdma_cm_id is
 *   bound to a specific IP address, the listen will be restricted to that
 *   address and the associated RDMA device.  If the rdma_cm_id is bound
 *   to an RDMA port number only, the listen will occur across all RDMA
 *   devices.
 * See also:
 *   rdma_bind_addr, rdma_connect, rdma_accept, rdma_reject, rdma_get_cm_event
 */
int rdma_listen(struct rdma_cm_id *id, int backlog) {
	printf("rdma_listen\n");
	return -1;
}

/**
 * rdma_get_request
 */
int rdma_get_request(struct rdma_cm_id *listen, struct rdma_cm_id **id) {
	printf("rdma_get_request\n");
	return -1;
}

/**
 * rdma_accept - Called to accept a connection request.
 * @id: Connection identifier associated with the request.
 * @conn_param: Optional information needed to establish the connection.
 * Description:
 *   Called from the listening side to accept a connection or datagram
 *   service lookup request.
 * Notes:
 *   Unlike the socket accept routine, rdma_accept is not called on a
 *   listening rdma_cm_id.  Instead, after calling rdma_listen, the user
 *   waits for a connection request event to occur.  Connection request
 *   events give the user a newly created rdma_cm_id, similar to a new
 *   socket, but the rdma_cm_id is bound to a specific RDMA device.
 *   rdma_accept is called on the new rdma_cm_id.
 *   A user may override the default connection parameters and exchange
 *   private data as part of the connection by using the conn_param parameter.
 * See also:
 *   rdma_listen, rdma_reject, rdma_get_cm_event
 */
int rdma_accept(struct rdma_cm_id *id, struct rdma_conn_param *conn_param) {
	printf("rdma_accept\n");
	return -1;
}

/**
 * rdma_reject - Called to reject a connection request.
 * @id: Connection identifier associated with the request.
 * @private_data: Optional private data to send with the reject message.
 * @private_data_len: Size of the private_data to send, in bytes.
 * Description:
 *   Called from the listening side to reject a connection or datagram
 *   service lookup request.
 * Notes:
 *   After receiving a connection request event, a user may call rdma_reject
 *   to reject the request.  If the underlying RDMA transport supports
 *   private data in the reject message, the specified data will be passed to
 *   the remote side.
 * See also:
 *   rdma_listen, rdma_accept, rdma_get_cm_event
 */
int rdma_reject(struct rdma_cm_id *id, const void *private_data,
		uint8_t private_data_len) {
	printf("rdma_reject\n");
	return -1;
}

/**
 * rdma_reject_ece - Called to reject a connection request with ECE
 * rejected reason.
 * The same as rdma_reject()
 */
int rdma_reject_ece(struct rdma_cm_id *id, const void *private_data,
		uint8_t private_data_len) {
	printf("rdma_reject_ece\n");
	return -1;
}

/**
 * rdma_notify - Notifies the librdmacm of an asynchronous event.
 * @id: RDMA identifier.
 * @event: Asynchronous event.
 * Description:
 *   Used to notify the librdmacm of asynchronous events that have occurred
 *   on a QP associated with the rdma_cm_id.
 * Notes:
 *   Asynchronous events that occur on a QP are reported through the user's
 *   device event handler.  This routine is used to notify the librdmacm of
 *   communication events.  In most cases, use of this routine is not
 *   necessary, however if connection establishment is done out of band
 *   (such as done through Infiniband), it's possible to receive data on a
 *   QP that is not yet considered connected.  This routine forces the
 *   connection into an established state in this case in order to handle
 *   the rare situation where the connection never forms on its own.
 *   Events that should be reported to the CM are: IB_EVENT_COMM_EST.
 * See also:
 *   rdma_connect, rdma_accept, rdma_listen
 */
int rdma_notify(struct rdma_cm_id *id, enum ibv_event_type event) {
	printf("rdma_notify\n");
	return -1;
}

/**
 * rdma_disconnect - This function disconnects a connection.
 * @id: RDMA identifier.
 * Description:
 *   Disconnects a connection and transitions any associated QP to the
 *   error state.
 * See also:
 *   rdma_connect, rdma_listen, rdma_accept
 */
int rdma_disconnect(struct rdma_cm_id *id) {
	printf("rdma_disconnect\n");
	return -1;
}

/**
 * rdma_join_multicast - Joins a multicast group.
 * @id: Communication identifier associated with the request.
 * @addr: Multicast address identifying the group to join.
 * @context: User-defined context associated with the join request.
 * Description:
 *   Joins a multicast group and attaches an associated QP to the group.
 * Notes:
 *   Before joining a multicast group, the rdma_cm_id must be bound to
 *   an RDMA device by calling rdma_bind_addr or rdma_resolve_addr.  Use of
 *   rdma_resolve_addr requires the local routing tables to resolve the
 *   multicast address to an RDMA device.  The user must call
 *   rdma_leave_multicast to leave the multicast group and release any
 *   multicast resources.  The context is returned to the user through
 *   the private_data field in the rdma_cm_event.
 * See also:
 *   rdma_leave_multicast, rdma_bind_addr, rdma_resolve_addr, rdma_create_qp
 */
int rdma_join_multicast(struct rdma_cm_id *id, struct sockaddr *addr,
			void *context) {
	printf("rdma_join_multicast\n");
	return -1;
}

/**
 * rdma_leave_multicast - Leaves a multicast group.
 * @id: Communication identifier associated with the request.
 * @addr: Multicast address identifying the group to leave.
 * Description:
 *   Leaves a multicast group and detaches an associated QP from the group.
 * Notes:
 *   Calling this function before a group has been fully joined results in
 *   canceling the join operation.  Users should be aware that messages
 *   received from the multicast group may stilled be queued for
 *   completion processing immediately after leaving a multicast group.
 *   Destroying an rdma_cm_id will automatically leave all multicast groups.
 * See also:
 *   rdma_join_multicast, rdma_destroy_qp
 */
int rdma_leave_multicast(struct rdma_cm_id *id, struct sockaddr *addr) {
	printf("rdma_leave_multicast\n");
	return -1;
}

/**
 * rdma_multicast_ex - Joins a multicast group with options.
 * @id: Communication identifier associated with the request.
 * @mc_join_attr: Extensive struct containing multicast join parameters.
 * @context: User-defined context associated with the join request.
 * Description:
 *  Joins a multicast group with options. Currently supporting MC join flags.
 *  The QP will be attached based on the given join flag.
 *  Join message will be sent according to the join flag.
 * Notes:
 *  Before joining a multicast group, the rdma_cm_id must be bound to
 *  an RDMA device by calling rdma_bind_addr or rdma_resolve_addr.  Use of
 *  rdma_resolve_addr requires the local routing tables to resolve the
 *  multicast address to an RDMA device.  The user must call
 *  rdma_leave_multicast to leave the multicast group and release any
 *  multicast resources.  The context is returned to the user through
 *  the private_data field in the rdma_cm_event.
 * See also:
 *  rdma_leave_multicast, rdma_bind_addr, rdma_resolve_addr, rdma_create_qp
 */
int rdma_join_multicast_ex(struct rdma_cm_id *id,
			   struct rdma_cm_join_mc_attr_ex *mc_join_attr,
			   void *context) {
	printf("rdma_join_multicast_ex\n");
	return -1;
}

/**
 * rdma_get_cm_event - Retrieves the next pending communication event.
 * @channel: Event channel to check for events.
 * @event: Allocated information about the next communication event.
 * Description:
 *   Retrieves a communication event.  If no events are pending, by default,
 *   the call will block until an event is received.
 * Notes:
 *   The default synchronous behavior of this routine can be changed by
 *   modifying the file descriptor associated with the given channel.  All
 *   events that are reported must be acknowledged by calling rdma_ack_cm_event.
 *   Destruction of an rdma_cm_id will block until related events have been
 *   acknowledged.
 * See also:
 *   rdma_ack_cm_event, rdma_create_event_channel, rdma_event_str
 */
int rdma_get_cm_event(struct rdma_event_channel *channel,
		      struct rdma_cm_event **event) {
	printf("rdma_get_cm_event\n");
	return -1;
}

/**
 * rdma_ack_cm_event - Free a communication event.
 * @event: Event to be released.
 * Description:
 *   All events which are allocated by rdma_get_cm_event must be released,
 *   there should be a one-to-one correspondence between successful gets
 *   and acks.
 * See also:
 *   rdma_get_cm_event, rdma_destroy_id
 */
int rdma_ack_cm_event(struct rdma_cm_event *event) {
	printf("rdma_ack_cm_event\n");
	return -1;
}

__be16 rdma_get_src_port(struct rdma_cm_id *id) {
	printf("rdma_get_src_port\n");
	return -1;
}

__be16 rdma_get_dst_port(struct rdma_cm_id *id) {
	printf("rdma_get_dst_port\n");
	return -1;
}

/**
 * rdma_get_devices - Get list of RDMA devices currently available.
 * @num_devices: If non-NULL, set to the number of devices returned.
 * Description:
 *   Return a NULL-terminated array of opened RDMA devices.  Callers can use
 *   this routine to allocate resources on specific RDMA devices that will be
 *   shared across multiple rdma_cm_id's.
 * Notes:
 *   The returned array must be released by calling rdma_free_devices.  Devices
 *   remain opened while the librdmacm is loaded.
 * See also:
 *   rdma_free_devices
 */
struct ibv_context **rdma_get_devices(int *num_devices) {
	printf("rdma_get_devices\n");
	return NULL;
}

/**
 * rdma_free_devices - Frees the list of devices returned by rdma_get_devices.
 * @list: List of devices returned from rdma_get_devices.
 * Description:
 *   Frees the device array returned by rdma_get_devices.
 * See also:
 *   rdma_get_devices
 */
void rdma_free_devices(struct ibv_context **list) {
	printf("rdma_free_devices\n");
}

/**
 * rdma_event_str - Returns a string representation of an rdma cm event.
 * @event: Asynchronous event.
 * Description:
 *   Returns a string representation of an asynchronous event.
 * See also:
 *   rdma_get_cm_event
 */
const char *rdma_event_str(enum rdma_cm_event_type event) {
    // Just call the existing function for this.
	return real_event_str(event);
}

/**
 * rdma_set_option - Set options for an rdma_cm_id.
 * @id: Communication identifier to set option for.
 * @level: Protocol level of the option to set.
 * @optname: Name of the option to set.
 * @optval: Reference to the option data.
 * @optlen: The size of the %optval buffer.
 */
int rdma_set_option(struct rdma_cm_id *id, int level, int optname,
		    void *optval, size_t optlen) {
	printf("rdma_set_option\n");
	return -1;
}

/**
 * rdma_migrate_id - Move an rdma_cm_id to a new event channel.
 * @id: Communication identifier to migrate.
 * @channel: New event channel for rdma_cm_id events.
 */
int rdma_migrate_id(struct rdma_cm_id *id, struct rdma_event_channel *channel) {
	printf("rdma_migrate_id\n");
	return -1;
}

/**
 * rdma_getaddrinfo - RDMA address and route resolution service.
 */
int rdma_getaddrinfo(const char *node, const char *service,
		     const struct rdma_addrinfo *hints,
		     struct rdma_addrinfo **res) {
	printf("rdma_getaddrinfo\n");
	return -1;
}

void rdma_freeaddrinfo(struct rdma_addrinfo *res) {
	printf("rdma_freeaddrinfo\n");
}

/* Binds a symbol to the real library's symbol. */
static void *bind_symbol(const char *sym) {
    void *handle = dlopen("/users/sg99/softiwarp-user-for-linux-rdma/build/lib/librdmacm.so", RTLD_LAZY);
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
    printf("Loading librdmacm interposition ...\n");
    // Bind the real librdmacm functions.
    real_create_event_channel = (rdma_event_channel* (*)()) bind_symbol("rdma_create_event_channel");
    real_destroy_event_channel = (void (*)(rdma_event_channel*)) bind_symbol("rdma_destroy_event_channel");
    real_create_id = (int (*)(rdma_event_channel*, rdma_cm_id**, void*, rdma_port_space)) bind_symbol("rdma_create_id");
    real_create_ep = (int (*)(rdma_cm_id**, rdma_addrinfo*, ibv_pd*, ibv_qp_init_attr*)) bind_symbol("rdma_create_ep");
    real_destroy_ep = (void (*)(rdma_cm_id*)) bind_symbol("rdma_destroy_ep");
    real_destroy_id = (int (*)(rdma_cm_id*)) bind_symbol("rdma_destroy_id");
    real_bind_addr = (int (*)(rdma_cm_id*, sockaddr*)) bind_symbol("rdma_bind_addr");
    real_resolve_addr = (int (*)(rdma_cm_id*, sockaddr*, sockaddr*, int)) bind_symbol("rdma_resolve_addr");
    real_resolve_route = (int (*)(rdma_cm_id*, int)) bind_symbol("rdma_resolve_route");
    real_create_qp = (int (*)(rdma_cm_id*, ibv_pd*, ibv_qp_init_attr*)) bind_symbol("rdma_create_qp");
    real_create_qp_ex = (int (*)(rdma_cm_id*, ibv_qp_init_attr_ex*)) bind_symbol("rdma_create_qp_ex");
    real_destroy_qp = (void (*)(rdma_cm_id*)) bind_symbol("rdma_destroy_qp");
    real_connect = (int (*)(rdma_cm_id*, rdma_conn_param*)) bind_symbol("rdma_connect");
    real_listen = (int (*)(rdma_cm_id*, int)) bind_symbol("rdma_listen");
    real_get_request = (int (*)(rdma_cm_id*, rdma_cm_id**)) bind_symbol("rdma_get_request");
    real_accept = (int (*)(rdma_cm_id*, rdma_conn_param*)) bind_symbol("rdma_accept");
    real_reject = (int (*)(rdma_cm_id*, const void*, uint8_t)) bind_symbol("rdma_reject");
    real_notify = (int (*)(rdma_cm_id*, ibv_event_type)) bind_symbol("rdma_notify");
    real_disconnect = (int (*)(rdma_cm_id*)) bind_symbol("rdma_disconnect");
    real_join_multicast = (int (*)(rdma_cm_id*, sockaddr*, void*)) bind_symbol("rdma_join_multicast");
    real_leave_multicast = (int (*)(rdma_cm_id*, sockaddr*)) bind_symbol("rdma_leave_multicast");
    real_join_multicast_ex = (int (*)(rdma_cm_id*, rdma_cm_join_mc_attr_ex*, void*)) bind_symbol("rdma_join_multicast_ex");
    real_get_cm_event = (int (*)(rdma_event_channel*, rdma_cm_event**)) bind_symbol("rdma_get_cm_event");
    real_ack_cm_event = (int (*)(rdma_cm_event*)) bind_symbol("rdma_ack_cm_event");
    real_get_src_port = (__be16 (*)(rdma_cm_id*)) bind_symbol("rdma_get_src_port");
    real_get_dst_port = (__be16 (*)(rdma_cm_id*)) bind_symbol("rdma_get_dst_port");
    real_get_devices = (ibv_context** (*)(int*)) bind_symbol("rdma_get_devices");
    real_free_devices = (void (*)(ibv_context**)) bind_symbol("rdma_free_devices");
    real_event_str = (const char* (*)(rdma_cm_event_type)) bind_symbol("rdma_event_str");
    real_set_option = (int (*)(rdma_cm_id*, int, int, void*, size_t)) bind_symbol("rdma_set_option");
    real_migrate_id = (int (*)(rdma_cm_id*, rdma_event_channel*)) bind_symbol("rdma_migrate_id");
    real_getaddrinfo = (int (*)(const char*, const char*, const rdma_addrinfo*, rdma_addrinfo**)) bind_symbol("rdma_getaddrinfo");
    real_freeaddrinfo = (void (*)(rdma_addrinfo*)) bind_symbol("rdma_freeaddrinfo");
}

