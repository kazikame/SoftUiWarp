#ifndef _CQ_H
#define _CQ_H
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

#include <pthread.h>
#include <stdlib.h>

#include <linux/types.h>
#include "common.h"
#include "concurrentqueue.h"

enum wc_status {
	WC_SUCCESS,
	WC_LOC_LEN_ERR,
	WC_LOC_QP_OP_ERR,
	WC_LOC_EEC_OP_ERR,
	WC_LOC_PROT_ERR,
	WC_WR_FLUSH_ERR,
	WC_MW_BIND_ERR,
	WC_BAD_RESP_ERR,
	WC_LOC_ACCESS_ERR,
	WC_REM_INV_REQ_ERR,
	WC_REM_ACCESS_ERR,
	WC_REM_OP_ERR,
	WC_RETRY_EXC_ERR,
	WC_RNR_RETRY_EXC_ERR,
	WC_LOC_RDD_VIOL_ERR,
	WC_REM_INV_RD_REQ_ERR,
	WC_REM_ABORT_ERR,
	WC_INV_EECN_ERR,
	WC_INV_EEC_STATE_ERR,
	WC_FATAL_ERR,
	WC_RESP_TIMEOUT_ERR,
	WC_GENERAL_ERR,
	WC_TM_ERR,
	WC_TM_RNDV_INCOMPLETE,
};

//! Strict superset of rdma_opcode
enum wc_opcode {
	WC_WRITE = 0,
    WC_READ_REQUEST = 1,
    WC_READ_RESPONSE = 2,
    WC_SEND = 3,
    WC_SEND_INVALIDATE = 4,
    WC_SEND_SOLICIT = 5,
    WC_SEND_SOLICIT_INVALIDATE = 6,
    WC_TERMINATE = 7,
	// WC_COMP_SWAP,
	// WC_FETCH_ADD,
	// WC_BIND_MW,
	// WC_LOCAL_INV,
	// WC_TSO,
/*
 * Set value of WC_RECV so consumers can test if a completion is a
 * receive by testing (opcode & WC_RECV).
 */
	WC_RECV			= 1 << 7,
	// WC_RECV_RDMA_WITH_IMM,

	// WC_TM_ADD,
	// WC_TM_DEL,
	// WC_TM_SYNC,
	// WC_TM_RECV,
	// WC_TM_NO_TAG,
};

enum {
	WC_IP_CSUM_OK_SHIFT	= 2
};

enum wc_flags {
	WC_GRH		= 1 << 0,
	WC_WITH_IMM		= 1 << 1,
	WC_IP_CSUM_OK	= 1 << WC_IP_CSUM_OK_SHIFT,
	WC_WITH_INV		= 1 << 3,
	WC_TM_SYNC_REQ	= 1 << 4,
	WC_TM_MATCH		= 1 << 5,
	WC_TM_DATA_VALID	= 1 << 6,
};

struct work_completion {
	uint64_t		wr_id;
	enum wc_status	status;
	enum wc_opcode	opcode;
	uint32_t		vendor_err = 0;
	uint32_t		byte_len = 0;
	/* When (wc_flags & WC_WITH_IMM): Immediate data in network byte order.
	 * When (wc_flags & WC_WITH_INV): Stores the invalidated rkey.
	 */
	union {
		__be32		imm_data;
		uint32_t	invalidated_rkey;
	};

	//! Ignored for now
	uint32_t		qp_num;
	uint32_t		src_qp;

	//! GSI/Immediate
	unsigned int		wc_flags = 0;

	//! Only for GSI QP
	uint16_t		pkey_index;
	
	//! Only UD QP
	uint16_t		slid;
	uint8_t			sl;
	uint8_t			dlid_path_bits;
};

struct cq {
    struct rdmap_stream_context* ctx;
    moodycamel::ConcurrentQueue<work_completion>* q;

	//! Maintains future work completions for pending read requests
    moodycamel::ConcurrentQueue<work_completion>* pending_q;
};

struct cq* create_cq(struct rdmap_stream_context* ctx, int num_cqe);

int destroy_cq(struct cq* cq);

/**
 * @brief returns an array of WC events
 * 
 * Assumes `wc` is allocated
 * 
 * @param cq 
 * @param num_entries 
 * @param wc 
 * @return int number of entries dequed
 */
int poll_cq(struct cq *cq, int num_entries, struct work_completion *wc);


/** Work Queues **/

enum wq_state {
	WQS_RESET,
	WQS_RDY,
	WQS_ERR,
	WQS_UNKNOWN
};

enum wq_type {
	WQT_RQ,
    WQT_SQ
};

//! Receive Work request is only for send response packets
struct recv_wr {
	uint64_t		wr_id;

	//! Using moodycamel queues here
	// struct recv_wr     *next;
	
	struct sge	       *sg_list;
	int			num_sge;
};

struct send_wr {
	uint64_t		wr_id;

	//! Using moodycamel queues here
	// struct send_wr     *next;

	struct sge	       *sg_list;
	int			num_sge;
	enum rdma_opcode	opcode;
	unsigned int		send_flags = 0;
	/* When opcode is *_WITH_IMM: Immediate data in network byte order.
	 * When opcode is *_INV: Stores the rkey to invalidate
	 */
	union {
		//! Since everything is in userspace, no need of immediate
		// __be32			imm_data;
		uint32_t		invalidate_rkey;
	};
    
    //! No support for UD or UC RDMA
	union {
		struct {
            /*Start address of remote memory block to access 
            (read or write, depends on the opcode). 
            Relevant only for RDMA WRITE (with immediate) and RDMA READ opcodes*/   
			uint64_t	remote_addr;
			uint32_t	rkey;
		} rdma;
	} wr;
};

struct wq {
	struct rdmap_stream_context     *context;
	struct	pd_t	   *pd;
	struct	cq	       *cq;
	uint32_t		wq_num;
	uint32_t		handle;
	enum wq_state       state;
	enum wq_type	wq_type;
    union {
        moodycamel::ConcurrentQueue<send_wr>* send_q;
        moodycamel::ConcurrentQueue<recv_wr>* recv_q;
    };
	uint32_t		events_completed;
	uint32_t		comp_mask;
};

struct wq_init_attr {
	void		       *wq_context = NULL;
	enum wq_type	wq_type;
	uint32_t		max_wr;
	uint32_t		max_sge;
	struct	pd_t	 	*pd;
	struct	cq	        *cq;
	uint32_t		comp_mask = 0; /* Use wq_init_attr_mask */
};

struct wq* create_wq(struct rdmap_stream_context *context,
					   struct wq_init_attr *wq_init_attr);

int destroy_wq(struct wq* q);

int send_wr_to_wce(struct send_wr* wr, struct work_completion* wce);

#endif