/*
 * Software Userspace iWARP device driver for Linux 
 *
 * MIT License
 * 
 * Copyright (c) 2021 Saksham Goel
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _IWARP_H
#define _IWARP_H

#include <linux/types.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <asm/byteorder.h>

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

// Uncomment to compile TAS workarounds.
//#define USE_TAS
// Uncomment to build working rping.
// (this is necessary because the real softiwarp stack expects a certain byte-order
// convention, that would take some time to do correctly without breaking things)
// #define RPING

#define RDMAP_VERSION		1
#define DDP_VERSION		1
#define MPA_REVISION_1		1
#define MPA_REVISION_2		2
#define MPA_MAX_PRIVDATA	256
#define MPA_KEY_REQ		"MPA ID Req Frame"
#define MPA_KEY_REP		"MPA ID Rep Frame"
#define	MPA_IRD_ORD_MASK	0x3fff
#define MPA_RR_KEY_LEN	  	16

struct mpa_rr_params {
	__be16	bits;
	__be16	pd_len;
};

/*
 * MPA request/response Hdr bits & fields
 */
enum {
	MPA_RR_FLAG_MARKERS	= __cpu_to_be16(0x8000),
	MPA_RR_FLAG_CRC		= __cpu_to_be16(0x4000),
	MPA_RR_FLAG_REJECT	= __cpu_to_be16(0x2000),
	MPA_RR_FLAG_ENHANCED	= __cpu_to_be16(0x1000),
	MPA_RR_RESERVED		= __cpu_to_be16(0x0f00),
	MPA_RR_MASK_REVISION	= __cpu_to_be16(0x00ff)
};

/*
 * MPA request/reply header
 */
struct mpa_rr {
	__u8	key[MPA_RR_KEY_LEN];
	struct mpa_rr_params params;
};
void print_mpa_rr(const struct mpa_rr*, char*);
void print_ddp(struct ddp_packet*, char*);
static inline void __mpa_rr_set_revision(__u16 *bits, __u8 rev)
{
	*bits = (*bits & ~MPA_RR_MASK_REVISION)
		| (__cpu_to_be16(rev) & MPA_RR_MASK_REVISION);
}

static inline __u8 __mpa_rr_revision(__u16 mpa_rr_bits)
{
	__u16 rev = mpa_rr_bits & MPA_RR_MASK_REVISION;

	return (__u8)__be16_to_cpu(rev);
}

enum mpa_v2_ctrl {
	MPA_V2_PEER_TO_PEER	= __cpu_to_be16(0x8000),
	MPA_V2_ZERO_LENGTH_RTR	= __cpu_to_be16(0x4000),
	MPA_V2_RDMA_WRITE_RTR	= __cpu_to_be16(0x8000),
	MPA_V2_RDMA_READ_RTR	= __cpu_to_be16(0x4000),
	MPA_V2_RDMA_NO_RTR	= __cpu_to_be16(0x0000),
	MPA_V2_MASK_IRD_ORD	= __cpu_to_be16(0x3fff)
};

struct mpa_v2_data {
	__be16		ird;
	__be16		ord;
};

struct mpa_marker {
	__be16	rsvd;
	__be16	fpdu_hmd; /* FPDU header-marker distance (= MPA's FPDUPTR) */
};

/*
 * maximum MPA trailer
 */
struct mpa_trailer {
	char	pad[4];
	__be32	crc;
};


/*
 * Common portion of iWARP headers (MPA, DDP, RDMAP)
 * for any FPDU
 */
struct iwarp_ctrl {
	__be16	mpa_len;
	__be16	ddp_rdmap_ctrl;
};

/*
 * DDP/RDMAP Hdr bits & fields
 */
enum {
	DDP_FLAG_TAGGED		= __cpu_to_be16(0x8000),
	DDP_FLAG_LAST		= __cpu_to_be16(0x4000),
	DDP_MASK_RESERVED	= __cpu_to_be16(0x3C00),
	DDP_MASK_VERSION	= __cpu_to_be16(0x0300),
	RDMAP_MASK_VERSION	= __cpu_to_be16(0x00C0),
	RDMAP_MASK_RESERVED	= __cpu_to_be16(0x0030),
	RDMAP_MASK_OPCODE	= __cpu_to_be16(0x000f)
};

static inline __u8 __ddp_version(struct iwarp_ctrl *ctrl)
{
	return (__u8)(__be16_to_cpu(ctrl->ddp_rdmap_ctrl & DDP_MASK_VERSION) >> 8);
};

static inline void __ddp_set_version(struct iwarp_ctrl *ctrl, __u8 version)
{
	ctrl->ddp_rdmap_ctrl = (ctrl->ddp_rdmap_ctrl & ~DDP_MASK_VERSION)
		| (__cpu_to_be16((__u16)version << 8) & DDP_MASK_VERSION);
};

static inline __u8 __rdmap_version(struct iwarp_ctrl *ctrl)
{
	__u16 ver = ctrl->ddp_rdmap_ctrl & RDMAP_MASK_VERSION;

	return (__u8)(__be16_to_cpu(ver) >> 6);
};

static inline void __rdmap_set_version(struct iwarp_ctrl *ctrl, __u8 version)
{
	ctrl->ddp_rdmap_ctrl = (ctrl->ddp_rdmap_ctrl & ~RDMAP_MASK_VERSION)
			| (__cpu_to_be16(version << 6) & RDMAP_MASK_VERSION);
}

static inline __u8 __rdma_opcode(struct iwarp_ctrl *ctrl)
{
	return (__u8)__be16_to_cpu(ctrl->ddp_rdmap_ctrl & RDMAP_MASK_OPCODE);
}

static inline void __rdmap_set_opcode(struct iwarp_ctrl *ctrl, __u8 opcode)
{
	ctrl->ddp_rdmap_ctrl = (ctrl->ddp_rdmap_ctrl & ~RDMAP_MASK_OPCODE)
				| (__cpu_to_be16(opcode) & RDMAP_MASK_OPCODE);
}


struct iwarp_rdma_write {
	struct iwarp_ctrl	ctrl;
	__be32			sink_stag;
	__be64			sink_to;
};

struct iwarp_rdma_rreq {
	struct iwarp_ctrl	ctrl;
	__be32			rsvd;
	__be32			ddp_qn;
	__be32			ddp_msn;
	__be32			ddp_mo;
	__be32			sink_stag;
	__be64			sink_to;
	__be32			read_size;
	__be32			source_stag;
	__be64			source_to;
};

struct iwarp_rdma_rresp {
	struct iwarp_ctrl	ctrl;
	__be32			sink_stag;
	__be64			sink_to;
};

struct iwarp_send {
	struct iwarp_ctrl	ctrl;
	__be32			rsvd;
	__be32			ddp_qn;
	__be32			ddp_msn;
	__be32			ddp_mo;
};

struct iwarp_send_inv {
	struct iwarp_ctrl	ctrl;
	__be32			inval_stag;
	__be32			ddp_qn;
	__be32			ddp_msn;
	__be32			ddp_mo;
};

struct iwarp_terminate {
	struct iwarp_ctrl	ctrl;
	__be32			rsvd;
	__be32			ddp_qn;
	__be32			ddp_msn;
	__be32			ddp_mo;
	__be32			term_ctrl;
};

/*
 * Terminate Hdr bits & fields
 */
enum {
	RDMAP_TERM_MASK_LAYER	= __cpu_to_be32(0xf0000000),
	RDMAP_TERM_MASK_ETYPE	= __cpu_to_be32(0x0f000000),
	RDMAP_TERM_MASK_ECODE	= __cpu_to_be32(0x00ff0000),
	RDMAP_TERM_FLAG_M	= __cpu_to_be32(0x00008000),
	RDMAP_TERM_FLAG_D	= __cpu_to_be32(0x00004000),
	RDMAP_TERM_FLAG_R	= __cpu_to_be32(0x00002000),
	RDMAP_TERM_MASK_RESVD	= __cpu_to_be32(0x00001fff)
};

static inline __u8 __rdmap_term_layer(struct iwarp_terminate *ctrl)
{
	return (__u8)(__be32_to_cpu(ctrl->term_ctrl & RDMAP_TERM_MASK_LAYER)
		    >> 28);
};

static inline __u8 __rdmap_term_etype(struct iwarp_terminate *ctrl)
{
	return (__u8)(__be32_to_cpu(ctrl->term_ctrl & RDMAP_TERM_MASK_ETYPE)
		    >> 24);
};

static inline __u8 __rdmap_term_ecode(struct iwarp_terminate *ctrl)
{
	return (__u8)(__be32_to_cpu(ctrl->term_ctrl & RDMAP_TERM_MASK_ECODE)
		    >> 20);
};


/*
 * Common portion of iWARP headers (MPA, DDP, RDMAP)
 * for an FPDU carrying an untagged DDP segment
 */
struct iwarp_ctrl_untagged {
	struct iwarp_ctrl	ctrl;
	__be32			rsvd;
	__be32			ddp_qn;
	__be32			ddp_msn;
	__be32			ddp_mo;
};

/*
 * Common portion of iWARP headers (MPA, DDP, RDMAP)
 * for an FPDU carrying a tagged DDP segment
 */
struct iwarp_ctrl_tagged {
	struct iwarp_ctrl	ctrl;
	__be32			ddp_stag;
	__be64			ddp_to;
};

union iwarp_hdrs {
	struct iwarp_ctrl		ctrl;
	struct iwarp_ctrl_untagged	c_untagged;
	struct iwarp_ctrl_tagged	c_tagged;
	struct iwarp_rdma_write		rwrite;
	struct iwarp_rdma_rreq		rreq;
	struct iwarp_rdma_rresp		rresp;
	struct iwarp_terminate		terminate;
	struct iwarp_send		send;
	struct iwarp_send_inv		send_inv;
};

enum ddp_etype {
	DDP_ETYPE_CATASTROPHIC	= 0x0,
	DDP_ETYPE_TAGGED_BUF	= 0x1,
	DDP_ETYPE_UNTAGGED_BUF	= 0x2,
	DDP_ETYPE_RSVD		= 0x3
};

enum ddp_ecode {
	DDP_ECODE_CATASTROPHIC		= 0x00,
	/* Tagged Buffer Errors */
	DDP_ECODE_T_INVALID_STAG	= 0x00,
	DDP_ECODE_T_BASE_BOUNDS		= 0x01,
	DDP_ECODE_T_STAG_NOT_ASSOC	= 0x02,
	DDP_ECODE_T_TO_WRAP		= 0x03,
	DDP_ECODE_T_DDP_VERSION		= 0x04,
	/* Untagged Buffer Errors */
	DDP_ECODE_UT_INVALID_QN		= 0x01,
	DDP_ECODE_UT_INVALID_MSN_NOBUF	= 0x02,
	DDP_ECODE_UT_INVALID_MSN_RANGE	= 0x03,
	DDP_ECODE_UT_INVALID_MO		= 0x04,
	DDP_ECODE_UT_MSG_TOOLONG	= 0x05,
	DDP_ECODE_UT_DDP_VERSION	= 0x06
};


enum rdmap_untagged_qn {
	RDMAP_UNTAGGED_QN_SEND		= 0,
	RDMAP_UNTAGGED_QN_RDMA_READ	= 1,
	RDMAP_UNTAGGED_QN_TERMINATE	= 2,
	RDMAP_UNTAGGED_QN_COUNT		= 3
};

enum rdmap_etype {
	RDMAP_ETYPE_CATASTROPHIC	= 0x0,
	RDMAP_ETYPE_REMOTE_PROTECTION	= 0x1,
	RDMAP_ETYPE_REMOTE_OPERATION	= 0x2
};

enum rdmap_ecode {
	RDMAP_ECODE_INVALID_STAG	= 0x00,
	RDMAP_ECODE_BASE_BOUNDS		= 0x01,
	RDMAP_ECODE_ACCESS_RIGHTS	= 0x02,
	RDMAP_ECODE_STAG_NOT_ASSOC	= 0x03,
	RDMAP_ECODE_TO_WRAP		= 0x04,
	RDMAP_ECODE_RDMAP_VERSION	= 0x05,
	RDMAP_ECODE_UNEXPECTED_OPCODE	= 0x06,
	RDMAP_ECODE_CATASTROPHIC_STREAM	= 0x07,
	RDMAP_ECODE_CATASTROPHIC_GLOBAL	= 0x08,
	RDMAP_ECODE_STAG_NOT_INVALIDATE	= 0x09,
	RDMAP_ECODE_UNSPECIFIED		= 0xff
};

enum rdmap_elayer {
	RDMAP_ERROR_LAYER_RDMA	= 0x00,
	RDMAP_ERROR_LAYER_DDP	= 0x01,
	RDMAP_ERROR_LAYER_LLP	= 0x02	/* eg., MPA */
};

enum llp_ecode {
	LLP_ECODE_LOCAL_CATASTROPHIC	= 0x05,
	LLP_ECODE_INSUFFICIENT_IRD	= 0x06,
	LLP_ECODE_NO_MATCHING_RTR	= 0x07
};

enum llp_etype {
	LLP_ETYPE_MPA	= 0x00
};

/* RFC 5040 */
enum rdma_opcode {
	RDMAP_RDMA_WRITE	= 0x0,
	RDMAP_RDMA_READ_REQ	= 0x1,
	RDMAP_RDMA_READ_RESP	= 0x2,
	RDMAP_SEND		= 0x3,
	RDMAP_SEND_INVAL	= 0x4,
	RDMAP_SEND_SE		= 0x5,
	RDMAP_SEND_SE_INVAL	= 0x6,
	RDMAP_TERMINATE		= 0x7,
	RDMAP_NOT_SUPPORTED	= RDMAP_TERMINATE + 1
};


/*
 * MPA request/reply
 * 
 * Note that the data is stored indirectly via a pointer
 * This is only used for received data
 * and cannot be used to send data
 */
struct siw_mpa_info {
	struct mpa_rr		hdr;	/* peer mpa hdr in host byte order */
	struct mpa_v2_data	v2_ctrl;
	struct mpa_v2_data	v2_ctrl_req;
	char			*pdata;
	int			bytes_rcvd;
};

/*
 * MPA packet in host byte order
 */
struct siw_mpa_packet {
	__u16 ulpdu_len = 0;
	char *ulpdu;
	__u32 crc = 0;
	int bytes_rcvd = 0;
};

#if __BIG_ENDIAN__
# define htonll(x) (x)
# define ntohll(x) (x)
#else
# define htonll(x) ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32)
# define ntohll(x) ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32)
#endif

struct sge {
	uint64_t		addr;
	uint32_t		length;
	uint32_t		lkey;
};

#endif
