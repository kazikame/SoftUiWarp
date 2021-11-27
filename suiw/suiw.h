/*
 * Software iWARP device driver for Linux
 *
 * Authors: Bernard Metzler <bmt@zurich.ibm.com>
 *
 * Copyright (c) 2008-2017, IBM Corporation
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

#ifndef _SUIW_H
#define _SUIW_H

#include <linux/types.h>
#include <asm/byteorder.h>
#include <stdatomic.h>

#include <unistd.h>

#include <infiniband/verbs.h>

#include "common/suiw_user.h"
#include "iwarp.h"
#include "lwlog.h"
#include "pthread.h"

#define _load_shared(a)		(*(volatile typeof(a) *)&(a))
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define DEVICE_ID_SOFTIWARP	0x0815
#define SIW_VENDOR_ID		0x626d74	/* ascii 'bmt' for now */
#define SIW_VENDORT_PART_ID	0
#define SIW_MAX_QP		(1024 * 100)
#define SIW_MAX_QP_WR		(1024 * 32)
#define SIW_MAX_ORD_QP		128
#define SIW_MAX_IRD_QP		128
#define SIW_MAX_SGE_PBL		256	/* max num sge's for PBL */
#define SIW_MAX_SGE_RD		1	/* iwarp limitation. we could relax */
#define SIW_MAX_CQ		(1024 * 100)
#define SIW_MAX_CQE		(SIW_MAX_QP_WR * 100)
#define SIW_MAX_MR		(SIW_MAX_QP * 10)
#define SIW_MAX_PD		SIW_MAX_QP
#define SIW_MAX_MW		0	/* to be set if MW's are supported */
#define SIW_MAX_FMR		SIW_MAX_MR
#define SIW_MAX_SRQ		SIW_MAX_QP
#define SIW_MAX_SRQ_WR		(SIW_MAX_QP_WR * 10)
#define SIW_MAX_CONTEXT		SIW_MAX_PD

#define SENDPAGE_THRESH		sysconf(_SC_PAGESIZE) /* min bytes for using sendpage() */
#define SQ_USER_MAXBURST	100

struct siw_devinfo {
	unsigned int	device;
	unsigned int	version;

	__u32	vendor_id;
	__u32	vendor_part_id;
	__u32	sw_version;
	int	max_qp;
	int	max_qp_wr;
	int	max_ord; /* max. outbound read queue depth */
	int	max_ird; /* max. inbound read queue depth */

	enum ibv_device_cap_flags cap_flags;
	int	max_sge;
	int	max_sge_rd;
	int	max_cq;
	int	max_cqe;
	__u64	max_mr_size;
	int	max_mr;
	int	max_pd;
	int	max_mw;
	int	max_fmr;
	int	max_srq;
	int	max_srq_wr;
	int	max_srq_sge;
};


struct siw_dev {
	struct ibv_device	ofa_dev;
	struct list_head	list;
	struct net_device	*netdev;
	struct siw_devinfo	attrs;
	int			is_registered; /* Registered with OFA core */

	/* physical port state (only one port per device) */
	enum ibv_port_state	state;

	/* object management */
	struct list_head	cep_list;
	struct list_head	qp_list;
	pthread_spinlock_t		idr_lock;
	struct idr		qp_idr;
	struct idr		cq_idr;
	struct idr		pd_idr;
	struct idr		mem_idr;

	/* active objects statistics */
	atomic_int		num_qp;
	atomic_int		num_cq;
	atomic_int		num_pd;
	atomic_int		num_mem;
	atomic_int		num_srq;
	atomic_int		num_cep;
	atomic_int		num_ctx;

	struct dentry		*debugfs;
};

struct siw_objhdr {
	__u32			id;	/* for idr based object lookup */
	struct kref		ref;
	struct siw_dev		*sdev;
};

struct siw_uobj {
	struct list_head	list;
	void	*addr;
	__u32	size;
	__u32	key;
};

struct siw_ucontext {
	struct ibv_ucontext	ibv_ucontext;
	struct siw_dev		*sdev;

	/* List of user mappable queue objects */
	struct list_head	uobj_list;
	pthread_spinlock_t		uobj_lock;
	__u32			uobj_key;
};

struct siw_pd {
	struct siw_objhdr	hdr;
	struct ibv_pd		ofa_pd;
};

enum siw_access_flags {
	SIW_MEM_LREAD	= (1<<0),
	SIW_MEM_LWRITE	= (1<<1),
	SIW_MEM_RREAD	= (1<<2),
	SIW_MEM_RWRITE	= (1<<3),

	SIW_MEM_FLAGS_LOCAL =
		(SIW_MEM_LREAD | SIW_MEM_LWRITE),
	SIW_MEM_FLAGS_REMOTE =
		(SIW_MEM_RWRITE | SIW_MEM_RREAD)
};

#define SIW_STAG_MAX	0xffffffff

struct siw_mr;

/*
 * siw presentation of user memory registered as source
 * or target of RDMA operations.
 */

struct siw_page_chunk {
	struct page **p;
};

struct siw_umem {
	struct siw_page_chunk	*page_chunk;
	int			num_pages;
	__u64			fp_addr;	/* First page base address */
	struct pid		*pid;
	struct mm_struct	*mm_s;
	struct work_struct	work;
};

struct siw_pble {
	__u64	addr;		/* Address of assigned user buffer */
	__u64	size;		/* Size of this entry */
	__u64	pbl_off;	/* Total offset form start of PBL */
};

struct siw_pbl {
	unsigned int	num_buf;
	unsigned int	max_buf;
	struct siw_pble	pbe[1];
};

/*
 * generic memory representation for registered siw memory.
 * memory lookup always via higher 24 bit of stag (stag index).
 * the stag is stored as part of the siw object header (id).
 * object relates to memory window if embedded mr pointer is valid
 */
struct siw_mem {
	struct siw_objhdr	hdr;

	struct siw_mr	*mr;	/* assoc. MR if MW, NULL if MR */
	__u64	va;		/* VA of memory */
	__u64	len;		/* amount of memory bytes */

	__u32	stag_valid:1,		/* VALID or INVALID */
		is_pbl:1,		/* PBL or user space mem */
		is_zbva:1,		/* zero based virt. addr. */
		mw_bind_enabled:1,	/* check only if MR */
		remote_inval_enabled:1,	/* VALID or INVALID */
		consumer_owns_key:1,	/* key/index split ? */
		rsvd:26;

	enum siw_access_flags	perms;	/* local/remote READ & WRITE */
};

#define SIW_MEM_IS_MW(m)	((m)->mr != NULL)

/*
 * MR and MW definition.
 * Used OFA structs ibv_mr/ibv_mw holding:
 * lkey, rkey, MW reference count on MR
 */
struct siw_mr {
	struct ibv_mr	ofa_mr;
	struct siw_mem	mem;
	struct rcu_head rcu;
	union {
		struct siw_umem	*umem;
		struct siw_pbl	*pbl;
		void *mem_obj;
	};
	struct siw_pd	*pd;
};

struct siw_mw {
	struct ibv_mw	ofa_mw;
	struct siw_mem	mem;
	struct rcu_head rcu;
};

enum siw_wr_state {
	SIW_WR_IDLE		= 0,
	SIW_WR_QUEUED		= 1,	/* processing has not started yet */
	SIW_WR_INPROGRESS	= 2	/* initiated processing of the WR */
};

union siw_mem_resolved {
	struct siw_mem	*obj;	/* reference to registered memory */
	char		*buf;	/* linear kernel buffer */
};

/* The WQE currently being processed (RT or TX) */
struct siw_wqe {
	/* Copy of applications SQE or RQE */
	union {
		struct siw_sqe	sqe;
		struct siw_rqe	rqe;
	};
	union siw_mem_resolved	mem[SIW_MAX_SGE]; /* per sge's resolved mem */
	enum siw_wr_state	wr_status;
	enum siw_wc_status	wc_status;
	__u32			bytes;		/* total bytes to process */
	__u32			processed;	/* bytes processed */
	int			error;
};

struct siw_cq {
	struct ibv_cq		ofa_cq;
	struct siw_objhdr	hdr;
	enum siw_notify_flags	*notify;
	pthread_spinlock_t		lock;
	struct siw_cqe		*queue;
	__u32			cq_put;
	__u32			cq_get;
	__u32			num_cqe;
	int			kernel_verbs;
};

enum siw_qp_state {
	SIW_QP_STATE_IDLE	= 0,
	SIW_QP_STATE_RTR	= 1,
	SIW_QP_STATE_RTS	= 2,
	SIW_QP_STATE_CLOSING	= 3,
	SIW_QP_STATE_TERMINATE	= 4,
	SIW_QP_STATE_ERROR	= 5,
	SIW_QP_STATE_COUNT	= 6
};

enum siw_qp_flags {
	SIW_RDMA_BIND_ENABLED	= (1 << 0),
	SIW_RDMA_WRITE_ENABLED	= (1 << 1),
	SIW_RDMA_READ_ENABLED	= (1 << 2),
	SIW_SIGNAL_ALL_WR	= (1 << 3),
	SIW_MPA_CRC		= (1 << 4),
	SIW_QP_IN_DESTROY	= (1 << 5)
};

enum siw_qp_attr_mask {
	SIW_QP_ATTR_STATE		= (1 << 0),
	SIW_QP_ATTR_ACCESS_FLAGS	= (1 << 1),
	SIW_QP_ATTR_LLP_HANDLE		= (1 << 2),
	SIW_QP_ATTR_ORD			= (1 << 3),
	SIW_QP_ATTR_IRD			= (1 << 4),
	SIW_QP_ATTR_SQ_SIZE		= (1 << 5),
	SIW_QP_ATTR_RQ_SIZE		= (1 << 6),
	SIW_QP_ATTR_MPA			= (1 << 7)
};

struct siw_sk_upcalls {
	void	(*sk_state_change)(struct sock *sk);
	void	(*sk_data_ready)(struct sock *sk, int bytes);
	void	(*sk_write_space)(struct sock *sk);
	void	(*sk_error_report)(struct sock *sk);
};

struct siw_srq {
	struct ibv_srq		ofa_srq;
	struct siw_pd		*pd;
	atomic_int		rq_index;
	pthread_spinlock_t		lock;
	__u32			max_sge;
	atomic_int		space;	/* current space for posting wqe's */
	__u32			limit;	/* low watermark for async event */
	struct siw_rqe		*recvq;
	__u32			rq_put;
	__u32			rq_get;
	__u32			num_rqe;/* max # of wqe's allowed */
	char			armed;	/* inform user if limit hit */
	char			kernel_verbs; /* '1' if kernel client */
};

struct siw_qp_attrs {
	enum siw_qp_state	state;
	char			terminate_buffer[52];
	__u32			terminate_msg_length;
	__u32			ddp_rdmap_version; /* 0 or 1 */
	char			*stream_msg_buf;
	__u32			stream_msg_buf_length;
	__u32			rq_hiwat;
	__u32			sq_size;
	__u32			rq_size;
	__u32			orq_size;
	__u32			irq_size;
	__u32			sq_max_sges;
	__u32			sq_max_sges_rdmaw;
	__u32			rq_max_sges;
	enum siw_qp_flags	flags;

	struct socket		*llp_stream_handle;
};

enum siw_tx_ctx {
	SIW_SEND_HDR = 0,	/* start or continue sending HDR */
	SIW_SEND_DATA = 1,	/* start or continue sending DDP payload */
	SIW_SEND_TRAILER = 2,	/* start or continue sending TRAILER */
	SIW_SEND_SHORT_FPDU = 3 /* send whole FPDU hdr|data|trailer at once */
};

enum siw_rx_state {
	SIW_GET_HDR = 0,	/* await new hdr or within hdr */
	SIW_GET_DATA_START = 1,	/* start of inbound DDP payload */
	SIW_GET_DATA_MORE = 2,	/* continuation of (misaligned) DDP payload */
	SIW_GET_TRAILER	= 3	/* await new trailer or within trailer */
};


struct siw_iwarp_rx {
	struct sk_buff		*skb;
	union iwarp_hdrs	hdr;
	struct mpa_trailer	trailer;
	/*
	 * local destination memory of inbound iwarp operation.
	 * valid, according to wqe->wr_status
	 */
	struct siw_wqe		wqe_active;

	struct shash_desc	*mpa_crc_hd;
	/*
	 * Next expected DDP MSN for each QN +
	 * expected steering tag +
	 * expected DDP tagget offset (all HBO)
	 */
	__u32			ddp_msn[RDMAP_UNTAGGED_QN_COUNT];
	__u32			ddp_stag;
	__u64			ddp_to;

	/*
	 * For each FPDU, main RX loop runs through 3 stages:
	 * Receiving protocol headers, placing DDP payload and receiving
	 * trailer information (CRC + eventual padding).
	 * Next two variables keep state on receive status of the
	 * current FPDU part (hdr, data, trailer).
	 */
	int			fpdu_part_rcvd;/* bytes in pkt part copied */
	int			fpdu_part_rem; /* bytes in pkt part not seen */

	int			skb_new;      /* pending unread bytes in skb */
	int			skb_offset;   /* offset in skb */
	int			skb_copied;   /* processed bytes in skb */

	int			pbl_idx;	/* Index into current PBL */

	int			sge_idx;	/* current sge in rx */
	unsigned int		sge_off;	/* already rcvd in curr. sge */

	enum siw_rx_state	state;

	__u32			inval_stag;

	__u8			first_ddp_seg:1,   /* this is first DDP seg */
				more_ddp_segs:1,   /* more DDP segs expected */
				rx_suspend:1,	   /* stop rcv DDP segs. */
				unused:1,
				prev_rdma_opcode:4; /* opcode of prev msg */
	char			pad;		/* # of pad bytes expected */
};

#define siw_rx_data(qp, rctx)	\
	(iwarp_pktinfo[__rdma_opcode(&rctx->hdr.ctrl)].proc_data(qp, rctx))

/*
 * Shorthands for short packets w/o payload
 * to be transmitted more efficient.
 */
struct siw_send_pkt {
	struct iwarp_send	send;
	__be32			crc;
};

struct siw_write_pkt {
	struct iwarp_rdma_write	write;
	__be32			crc;
};

struct siw_rreq_pkt {
	struct iwarp_rdma_rreq	rreq;
	__be32			crc;
};

struct siw_rresp_pkt {
	struct iwarp_rdma_rresp	rresp;
	__be32			crc;
};

struct siw_iwarp_tx {
	union {
		union iwarp_hdrs		hdr;

		/* Generic part of FPDU header */
		struct iwarp_ctrl		ctrl;
		struct iwarp_ctrl_untagged	c_untagged;
		struct iwarp_ctrl_tagged	c_tagged;

		/* FPDU headers */
		struct iwarp_rdma_write		rwrite;
		struct iwarp_rdma_rreq		rreq;
		struct iwarp_rdma_rresp		rresp;
		struct iwarp_terminate		terminate;
		struct iwarp_send		send;
		struct iwarp_send_inv		send_inv;

		/* complete short FPDUs */
		struct siw_send_pkt		send_pkt;
		struct siw_write_pkt		write_pkt;
		struct siw_rreq_pkt		rreq_pkt;
		struct siw_rresp_pkt		rresp_pkt;
	} pkt;

	struct mpa_trailer			trailer;
	/* DDP MSN for untagged messages */
	__u32			ddp_msn[RDMAP_UNTAGGED_QN_COUNT];

	enum siw_tx_ctx		state;
	wait_queue_head_t	waitq;
	__u16			ctrl_len;	/* ddp+rdmap hdr */
	__u16			ctrl_sent;
	int			burst;

	int			bytes_unsent;	/* ddp payload bytes */

	struct shash_desc	*mpa_crc_hd;

	atomic_int		in_use;		/* tx currently under way */

	__u8			do_crc:1,	/* do crc for segment */
				use_sendpage:1,	/* send w/o copy */
				tx_suspend:1,	/* stop sending DDP segs. */
				pad:2,		/* # pad in current fpdu */
				orq_fence:1,	/* ORQ full or Send fenced */
				unused:2;

	__u16			fpdu_len;	/* len of FPDU to tx */

	int			tcp_seglen;	/* remaining tcp seg space */

	struct siw_wqe		wqe_active;

	int			pbl_idx;	/* Index into current PBL */
	int			sge_idx;	/* current sge in tx */
	__u32			sge_off;	/* already sent in curr. sge */
	int			in_syscall;	/* TX out of user context */
};

struct siw_qp {
	struct ibv_qp		ofa_qp;
	struct siw_objhdr	hdr;
	struct list_head	devq;
	int			cpu;
	int			kernel_verbs;
	struct siw_iwarp_rx	rx_ctx;
	struct siw_iwarp_tx	tx_ctx;

	struct siw_cep		*cep;
	struct rw_semaphore	state_lock;

	struct siw_pd		*pd;
	struct siw_cq		*scq;
	struct siw_cq		*rcq;
	struct siw_srq		*srq;

	struct siw_qp_attrs	attrs;

	struct siw_sqe		*sendq;	/* send queue element array */
	uint32_t		sq_get;	/* consumer index into sq array */
	uint32_t		sq_put;	/* kernel prod. index into sq array */
	struct llist_node	tx_list;

	struct siw_sqe		*irq;	/* inbound read queue element array */
	uint32_t		irq_get;/* consumer index into irq array */
	uint32_t		irq_put;/* producer index into irq array */

	struct siw_rqe		*recvq;	/* recv queue element array */
	uint32_t		rq_get;	/* consumer index into rq array */
	uint32_t		rq_put;	/* kernel prod. index into rq array */

	struct siw_sqe		*orq; /* outbound read queue element array */
	uint32_t		orq_get;/* consumer index into orq array */
	uint32_t		orq_put;/* shared producer index for ORQ */

	pthread_spinlock_t		sq_lock;
	pthread_spinlock_t		rq_lock;
	pthread_spinlock_t		orq_lock;
};

#define RX_QP(rx)		container_of(rx, struct siw_qp, rx_ctx)
#define TX_QP(tx)		container_of(tx, struct siw_qp, tx_ctx)
#define QP_ID(qp)		((qp)->hdr.id)
#define OBJ_ID(obj)		((obj)->hdr.id)
#define RX_QPID(rx)		QP_ID(RX_QP(rx))
#define TX_QPID(tx)		QP_ID(TX_QP(tx))

/* helper macros */
#define tx_wqe(qp)		(&(qp)->tx_ctx.wqe_active)
#define rx_wqe(qp)		(&(qp)->rx_ctx.wqe_active)
#define rx_mem(qp)		((qp)->rx_ctx.wqe_active.mem[0].obj)
#define tx_type(wqe)		((wqe)->sqe.opcode)
#define rx_type(wqe)		((wqe)->rqe.opcode)
#define tx_flags(wqe)		((wqe)->sqe.flags)

#define tx_more_wqe(qp)		(!siw_sq_empty(qp) || !siw_irq_empty(qp))

struct iwarp_msg_info {
	int			hdr_len;
	struct iwarp_ctrl	ctrl;
	int	(*proc_data)(struct siw_qp *qp, struct siw_iwarp_rx *rctx);
};

extern struct iwarp_msg_info iwarp_pktinfo[RDMAP_TERMINATE + 1];
extern struct siw_dev *siw;


/* QP general functions */
int siw_qp_modify(struct siw_qp *qp, struct siw_qp_attrs *attr,
		  enum siw_qp_attr_mask mask);
int siw_qp_mpa_rts(struct siw_qp *qp, enum mpa_v2_ctrl ctrl);
void siw_qp_llp_close(struct siw_qp *qp);
void siw_qp_cm_drop(struct siw_qp *qp, int when);
void siw_send_terminate(struct siw_qp *qp, __u8 layer, __u8 etype, __u8 ecode);


struct ibv_qp *siw_get_ofaqp(struct ibv_device *dev, int id);
void siw_qp_get_ref(struct ibv_qp *qp);
void siw_qp_put_ref(struct ibv_qp *qp);

enum siw_qp_state siw_map_ibstate(enum ibv_qp_state state);

int siw_check_mem(struct siw_pd *pd, struct siw_mem *mem, __u64 addr,
		  enum siw_access_flags perm, int len);
int siw_check_sge(struct siw_pd *pd, struct siw_sge *sge,
		  union siw_mem_resolved *mem, enum siw_access_flags perm,
		  __u32 off, int len);
int siw_check_sgl(struct siw_pd *pd, struct siw_wqe *wqe,
		  enum siw_access_flags perm);

void siw_read_to_orq(struct siw_sqe *rreq, struct siw_sqe *sqe);
int siw_sqe_complete(struct siw_qp *qp, struct siw_sqe *sqe, __u32 bytes,
		     enum siw_wc_status status);
int siw_rqe_complete(struct siw_qp *qp, struct siw_rqe *rqe, __u32 bytes,
		     enum siw_wc_status status);
void siw_qp_llp_data_ready(struct sock *sock);
void siw_qp_llp_write_space(struct sock *sock);

/* SIW user memory management */

#define CHUNK_SHIFT	9	/* sets number of pages per chunk */
#define PAGES_PER_CHUNK	(_AC(1, UL) << CHUNK_SHIFT)
#define CHUNK_MASK	(~(PAGES_PER_CHUNK - 1))
#define PAGE_CHUNK_SIZE	(PAGES_PER_CHUNK * sizeof(struct page *))

/*
 * siw_get_upage()
 *
 * Get page pointer for address on given umem.
 *
 * @umem: two dimensional list of page pointers
 * @addr: user virtual address
 */
static inline struct page *siw_get_upage(struct siw_umem *umem, __u64 addr)
{
	unsigned int	page_idx	= (addr - umem->fp_addr) >> PAGE_SHIFT,
			chunk_idx	= page_idx >> CHUNK_SHIFT,
			page_in_chunk	= page_idx & ~CHUNK_MASK;

	if (likely(page_idx < umem->num_pages))
		return umem->page_chunk[chunk_idx].p[page_in_chunk];

	return NULL;
}

extern struct siw_umem *siw_umem_get(__u64 start, __u64 len);
extern void siw_umem_release(struct siw_umem *umem);
extern struct siw_pbl *siw_pbl_alloc(__u32 num_buf);
extern __u64 siw_pbl_get_buffer(struct siw_pbl *pbl, __u64 off, int *len, int *idx);
extern void siw_pbl_free(struct siw_pbl *pbl);


/* QP TX path functions */
extern int siw_run_sq(void *arg);
extern int siw_qp_sq_process(struct siw_qp *qp);
extern int siw_sq_start(struct siw_qp *qp);
extern int siw_activate_tx(struct siw_qp *qp);
extern void siw_stop_tx_thread(int nr_cpu);

/* QP RX path functions */
extern int siw_proc_send(struct siw_qp *qp, struct siw_iwarp_rx *rx);
extern int siw_proc_rreq(struct siw_qp *qp, struct siw_iwarp_rx *rx);
extern int siw_proc_rresp(struct siw_qp *qp, struct siw_iwarp_rx *rx);
extern int siw_proc_write(struct siw_qp *qp, struct siw_iwarp_rx *rx);
extern int siw_proc_terminate(struct siw_qp *qp, struct siw_iwarp_rx *rx);
extern int siw_proc_unsupp(struct siw_qp *qp, struct siw_iwarp_rx *rx);

extern int siw_tcp_rx_data(read_descriptor_t *rd_desc, struct sk_buff *skb,
			   unsigned int off, size_t len);

/* MPA utilities */
static inline int siw_crc_array(struct shash_desc *desc, __u8 *start,
				size_t len)
{
	// return crypto_shash_update(desc, start, len);
}

static inline int siw_crc_page(struct shash_desc *desc, struct page *p,
			       int off, int len)
{
	// return crypto_shash_update(desc, page_address(p) + off, len);
}

extern struct task_struct *qp_tx_thread[];
extern int default_tx_cpu;


/* Varia */
extern void siw_cq_flush(struct siw_cq *cq);
extern void siw_sq_flush(struct siw_qp *qp);
extern void siw_rq_flush(struct siw_qp *qp);
extern int siw_reap_cqe(struct siw_cq *cq, struct ibv_wc *wc);

/* RDMA core event dipatching */
extern void siw_qp_event(struct siw_qp *qp, enum ibv_event_type type);
extern void siw_cq_event(struct siw_cq *cq, enum ibv_event_type type);
extern void siw_srq_event(struct siw_srq *srq, enum ibv_event_type type);
extern void siw_port_event(struct siw_dev *dev, __u8 port,
			   enum ibv_event_type type);


static inline struct siw_qp *siw_qp_ofa2siw(struct ibv_qp *ofa_qp)
{
	return container_of(ofa_qp, struct siw_qp, ofa_qp);
}

static inline int siw_sq_empty(struct siw_qp *qp)
{
	return qp->sendq[qp->sq_get % qp->attrs.sq_size].flags == 0;
}

static inline struct siw_sqe *sq_get_next(struct siw_qp *qp)
{
	struct siw_sqe *sqe = &qp->sendq[qp->sq_get % qp->attrs.sq_size];

	if (sqe->flags & SIW_WQE_VALID)
		return sqe;

	return NULL;
}

static inline struct siw_sqe *orq_get_current(struct siw_qp *qp)
{
	return &qp->orq[qp->orq_get % qp->attrs.orq_size];
}

static inline struct siw_sqe *orq_get_tail(struct siw_qp *qp)
{
	if (likely(qp->attrs.orq_size))
		return &qp->orq[qp->orq_put % qp->attrs.orq_size];

	lwlog_err("QP[%d]: ORQ has zero length", QP_ID(qp));
	return NULL;
}

static inline struct siw_sqe *orq_get_free(struct siw_qp *qp)
{
	struct siw_sqe *orq_e = orq_get_tail(qp);

	if (orq_e && orq_e->flags == 0)
		return orq_e;

	return NULL;
}

static inline int siw_orq_empty(struct siw_qp *qp)
{
	return qp->orq[qp->orq_get % qp->attrs.orq_size].flags == 0 ? 1 : 0;
}

static inline struct siw_sqe *irq_alloc_free(struct siw_qp *qp)
{
	struct siw_sqe *irq_e = &qp->irq[qp->irq_put % qp->attrs.irq_size];

	if (irq_e->flags == 0) {
		qp->irq_put++;
		return irq_e;
	}
	return NULL;
}

static inline int siw_irq_empty(struct siw_qp *qp)
{
	return qp->irq[qp->irq_get % qp->attrs.irq_size].flags == 0;
}

static inline struct siw_mr *siw_mem2mr(struct siw_mem *m)
{
	if (!SIW_MEM_IS_MW(m))
		return container_of(m, struct siw_mr, mem);
	return m->mr;
}

#endif
