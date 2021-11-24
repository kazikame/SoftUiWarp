#ifndef _CQ_H
#define _CQ_H
#include <pthread.h>
#include <stdlib.h>

#include <linux/types.h>

#include "blockingconcurrentqueue.h"
#include "rdmap_structs.h"

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

enum wc_opcode {
	WC_SEND,
	WC_RDMA_WRITE,
	WC_RDMA_READ,
	WC_COMP_SWAP,
	WC_FETCH_ADD,
	WC_BIND_MW,
	WC_LOCAL_INV,
	WC_TSO,
/*
 * Set value of WC_RECV so consumers can test if a completion is a
 * receive by testing (opcode & WC_RECV).
 */
	WC_RECV			= 1 << 7,
	WC_RECV_RDMA_WITH_IMM,

	WC_TM_ADD,
	WC_TM_DEL,
	WC_TM_SYNC,
	WC_TM_RECV,
	WC_TM_NO_TAG,
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
	uint32_t		vendor_err;
	uint32_t		byte_len;
	/* When (wc_flags & WC_WITH_IMM): Immediate data in network byte order.
	 * When (wc_flags & WC_WITH_INV): Stores the invalidated rkey.
	 */
	union {
		__be32		imm_data;
		uint32_t	invalidated_rkey;
	};
	uint32_t		qp_num;
	uint32_t		src_qp;
	unsigned int		wc_flags;
	uint16_t		pkey_index;
	uint16_t		slid;
	uint8_t			sl;
	uint8_t			dlid_path_bits;
};

struct cq {
    struct rdmap_stream_context* ctx;
    moodycamel::BlockingConcurrentQueue<work_completion>* q;
};

struct cq* create_cq(struct rdmap_stream_context* ctx, int num_cqe) {
    struct cq* cq = (struct cq*)malloc(sizeof(struct cq));

    cq->ctx = ctx;
    cq->q = new moodycamel::BlockingConcurrentQueue<work_completion>(num_cqe);

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

#endif