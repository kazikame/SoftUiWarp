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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "mpa.h"

#include "lwlog.h"

char log_buf[1024];
int mpa_protocol_version = 1;

int send_mpa_rr(int sockfd, const void* pdata, __u8 pd_len, int req)
{
    //! Make MPA request header
    struct mpa_rr hdr;
    memset(&hdr, 0, sizeof hdr);

    strncpy(hdr.key, req ? MPA_KEY_REQ : MPA_KEY_REP, MPA_RR_KEY_LEN);
    int version = MPA_REVISION_1;
    __mpa_rr_set_revision(&hdr.params.bits, version);

    hdr.params.pd_len = __cpu_to_be16(pd_len);
    
    int mpa_len = 0;
    
    //! Make sendmsg() msg struct
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    
    struct iovec iov[3];
    int iovec_num = 0;

    iov[iovec_num].iov_base = &hdr;
    iov[iovec_num].iov_len = sizeof(hdr);
    mpa_len += sizeof(hdr);

    if (pd_len)
    {
        iovec_num++;
        iov[iovec_num].iov_base = (char *)pdata;
		iov[iovec_num].iov_len = pd_len;
		mpa_len += pd_len;
    }

    msg.msg_iov = iov;
    msg.msg_iovlen = iovec_num+1;

    print_mpa_rr(&hdr, log_buf);
    lwlog_info("Sending Message:\n%s", log_buf);

    int ret = sendmsg(sockfd, &msg, 0);
    return ret;
}

int recv_mpa_rr(int sockfd, struct siw_mpa_info* info)
{
    int bytes_rcvd = 0;

    struct mpa_rr* hdr = &info->hdr;

    //! Get header
    int rcvd = recv(sockfd, hdr, sizeof(struct mpa_rr), 0);

    if (rcvd < sizeof(struct mpa_rr))
    {
        lwlog_err("recv_mpa_rr: didn't receive enough bytes");
        return -1;
    }

    print_mpa_rr(hdr, log_buf);
    lwlog_info("Received Message:\n%s", log_buf);

    __u16 pd_len = __be16_to_cpu(hdr->params.pd_len);

    int is_req = strncmp(MPA_KEY_REQ, hdr->key, MPA_RR_KEY_LEN) == 0;
    //! private data length is 0, and is request
    if (!pd_len)
    {
        //! If received request, ensure no garbage data is sent
        if (is_req)
        {
             int garbage;
            rcvd = recv(sockfd, (char *)&garbage, sizeof(garbage), MSG_DONTWAIT);

            //! No data on socket, the peer is protocol compliant :)
            if (rcvd == -EAGAIN) return 0;

            if (rcvd > 0)
            {
                lwlog_err("recv_mpa_rr: peer sent extra data after req/resp");
                return rcvd;
            }

            if (rcvd < 0)
            {
                lwlog_err("recv_mpa_rr: connection error after header received");
                return rcvd;
            }

            if (rcvd == 0)
            {
                lwlog_err("recv_mpa_rr: peer EOF");
                return -EPIPE;
            }
        }

        return sizeof(struct mpa_rr);
    }

    //! private data length is nonzero. Must allocate private data.
    //! TODO: this is basically a memory leak. remove
    if (!info->pdata)
    {
        info->pdata = malloc(pd_len + 4);
        if (!info->pdata)
        {
            lwlog_err("recv_mpa_rr: out of memory");
            return -ENOMEM;
        }
    }


    rcvd = recv(sockfd, info->pdata, is_req ? pd_len + 4 : pd_len, MSG_DONTWAIT);

    if (rcvd < 0)
    {
        lwlog_err("recv_mpa_rr: connection error after trying to get private data");
        return rcvd;
    }
    if (rcvd > pd_len)
    {
        lwlog_err("recv_mpa_rr: received more than private data length");
        return -EPROTO;
    }

    lwlog_info("MPA req/rep received pd_len: %d, pdata: %d", pd_len, *((int*)info->pdata));
    return rcvd;
}

int mpa_client_connect(int sockfd, void* pdata_send, __u8 pd_len, void* pdata_recv)
{
    int ret = send_mpa_rr(sockfd, pdata_send, pd_len, 1);
    if (ret < 0) return ret;

    struct siw_mpa_info* info = malloc(sizeof(struct siw_mpa_info));
    if (!info)
    {
        lwlog_err("out of memory");
        return -1;
    }
    info->pdata = pdata_recv;
    
    ret = recv_mpa_rr(sockfd, info);

    mpa_protocol_version = __mpa_rr_revision(info->hdr.params.bits);
    free(info);
    return ret;
}

//! TODO: Complete
int mpa_send(int sockfd, void* ulpdu, int len, int flags)
{
    return 0;
}