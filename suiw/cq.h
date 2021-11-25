#ifndef _CQ_H
#define _CQ_H
#include <pthread.h>
#include <stdlib.h>

#include <linux/types.h>
#include "common/iwarp.h"
#include "blockingconcurrentqueue.h"

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

//! Strict superset of rdmap_opcode
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
};

struct cq* create_cq(struct rdmap_stream_context* ctx, int num_cqe) {
    struct cq* cq = (struct cq*)malloc(sizeof(struct cq));

    cq->ctx = ctx;
    cq->q = new moodycamel::ConcurrentQueue<work_completion>(num_cqe);

    return cq;
}

int destroy_cq(struct cq* cq) {
    delete cq->q;
    free(cq);
    return 0;
}

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
int poll_cq(struct cq *cq, int num_entries, struct work_completion *wc) {
    return cq->q->try_dequeue_bulk(wc, num_entries);
}


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
	struct recv_wr     *next;
	struct sge	       *sg_list;
	int			num_sge;
};

struct send_wr {
	uint64_t		wr_id;

	//! Using moodycamel queues here
	// struct send_wr     *next;

	struct sge	       *sg_list;
	int			num_sge;
	enum rdmap_opcode	opcode;
	unsigned int		send_flags;
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
	struct	pd	       *pd;
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
	void		       *wq_context;
	enum wq_type	wq_type;
	uint32_t		max_wr;
	uint32_t		max_sge;
	struct	pd	       *pd;
	struct	cq	       *cq;
	uint32_t		comp_mask; /* Use wq_init_attr_mask */
};

struct wq* create_wq(struct rdmap_stream_context *context,
					   struct wq_init_attr *wq_init_attr) {
    
    struct wq* q = (struct wq*)malloc(sizeof(struct wq));

    q->context = context;
    q->pd = wq_init_attr->pd;
    q->cq = wq_init_attr->cq;
    q->wq_type = wq_init_attr->wq_type;
    switch(wq_init_attr->wq_type)
    {
        case WQT_SQ: {
            q->send_q = new moodycamel::ConcurrentQueue<send_wr>(wq_init_attr->max_wr);
            break;
        }
        case WQT_RQ: {
            q->recv_q = new moodycamel::ConcurrentQueue<recv_wr>(wq_init_attr->max_wr);
            break;
        }
    }
    q->comp_mask = wq_init_attr->comp_mask;

    return q;
}

int destroy_wq(struct wq* q)
{
    switch(q->wq_type)
    {
        case WQT_SQ: {
            delete q->send_q;
            break;
        }
        case WQT_RQ: {
            delete q->recv_q;
            break;
        }
    }

    free(q);
    return 0;
}

int send_wr_to_wce(struct send_wr* wr, struct work_completion* wce)
{
	wce->wr_id = wr->wr_id;
	wce->opcode = (enum wc_opcode)wr->opcode;
}

#endif