/*
 * Software Userspace iWARP device driver for Linux 
 *
 * MIT License
 * 
 * Copyright (c) 2021 Saksham Goel, Matthew Pabst
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
#include "mpa/mpa.h"

#include "lwlog.h"

char log_buf[1024];
int mpa_protocol_version = 1;

int mpa_send_rr(int sockfd, const void* pdata, __u8 pd_len, int req)
{
    //! Make MPA request header
    struct mpa_rr hdr;
    memset(&hdr, 0, sizeof hdr);

    strncpy((char*)hdr.key, req ? MPA_KEY_REQ : MPA_KEY_REP, MPA_RR_KEY_LEN);
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
#ifdef USE_TAS
    int g;
    lwlog_debug("doing dummy recv ...");
    recv(sockfd, &g, 0, 0);
    lwlog_debug("Yay dummy recv done");
#endif // USE_TAS
    return ret;
}

int mpa_recv_rr(int sockfd, struct siw_mpa_info* info)
{
    int bytes_rcvd = 0;

    struct mpa_rr* hdr = &info->hdr;

    //! Get header
    int rcvd = recv(sockfd, hdr, sizeof(struct mpa_rr), 0);

    if (rcvd < sizeof(struct mpa_rr))
    {
        lwlog_err("mpa_recv_rr: didn't receive enough bytes %d", rcvd);
        return -1;
    }

    print_mpa_rr(hdr, log_buf);
    lwlog_info("Received Message:\n%s", log_buf);

    __u16 pd_len = __be16_to_cpu(hdr->params.pd_len);

    int is_req = strncmp(MPA_KEY_REQ, (char*)hdr->key, MPA_RR_KEY_LEN) == 0;
    //! private data length is 0, and is request
    if (!pd_len)
    {
        //! If received request, ensure no garbage data is sent
#ifndef USE_TAS // TAS does not support the MSG_DONTWAIT flag, so just assume the client is protocol compliant
        if (is_req)
        {
            int garbage;
            rcvd = recv(sockfd, (char *)&garbage, sizeof(garbage), MSG_DONTWAIT);

            //! No data on socket, the peer is protocol compliant :)
            if (rcvd == -1 && errno == EAGAIN) return 0;

            if (rcvd > 0)
            {
                lwlog_err("mpa_recv_rr: peer sent extra data after req/resp");
                return rcvd;
            }

            if (rcvd < 0)
            {
                lwlog_err("mpa_recv_rr: connection error after header received");
                return rcvd;
            }

            if (rcvd == 0)
            {
                lwlog_err("mpa_recv_rr: peer EOF");
                return -EPIPE;
            }
        }
#endif // !USE_TAS

        return sizeof(struct mpa_rr);
    }

    //! private data length is nonzero. Must allocate private data.
    //! TODO: this is basically a memory leak. remove
    if (!info->pdata)
    {
        info->pdata = (char*)malloc(pd_len + 4);
        if (!info->pdata)
        {
            lwlog_err("mpa_recv_rr: out of memory");
            return -ENOMEM;
        }
    }

    rcvd = recv(sockfd, info->pdata, is_req ? pd_len + 4 : pd_len, MSG_DONTWAIT);

    if (rcvd < 0)
    {
        lwlog_err("mpa_recv_rr: connection error after trying to get private data");
        return rcvd;
    }
    if (rcvd > pd_len)
    {
        lwlog_err("mpa_recv_rr: received more than private data length");
        return -EPROTO;
    }

    lwlog_info("MPA req/rep received pd_len: %d, pdata: %d", pd_len, *((int*)info->pdata));
    return rcvd;
}

//! TODO: Add config options to choose CRC, Markers, etc.
int mpa_client_connect(int sockfd, void* pdata_send, __u8 pd_len, void* pdata_recv)
{
    int ret = mpa_send_rr(sockfd, pdata_send, pd_len, 1);
    if (ret < 0) return ret;
    struct siw_mpa_info info;
    memset(&info, 0, sizeof(siw_mpa_info));
    info.pdata = (char*)pdata_recv;
    
    ret = mpa_recv_rr(sockfd, &info);
    if (ret < 0)
    {
        return ret;
    }

    mpa_protocol_version = __mpa_rr_revision(info.hdr.params.bits);
    return 0;
}

int mpa_server_accept(int sockfd, void *pdata_send, __u8 pd_len, void* pdata_recv)
{
    int ret;
    struct siw_mpa_info info;
    memset(&info, 0, sizeof(siw_mpa_info));
    info.pdata = (char*)pdata_recv;
    ret = mpa_recv_rr(sockfd, &info);
    if (ret < 0) return ret;
    mpa_protocol_version = __mpa_rr_revision(info.hdr.params.bits);

    ret = mpa_send_rr(sockfd, pdata_send, pd_len, 0);
    if (ret < 0) return ret;
    return 0;
}

int mpa_send(int sockfd, sge* sg_list, int num_sge, int flags)
{
    //! Make sendmsg() msg struct
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));

    struct iovec iov[5 + num_sge];
    int iovec_num = 0;

    //! Find total Length of ULPDU
    __u16 len = 0;
    for (int i = 0; i < num_sge; i++)
    {
        len += sg_list[i].length;
    }

    if (unlikely(len > EMSS)) 
    {
        lwlog_err("Sending MPA packet larger than EMSS: %d", len);
        return -1;
    }

    //! MPA Header
    __be16 mpa_len = htons(len);
    iov[iovec_num].iov_base = &mpa_len;
    iov[iovec_num].iov_len = sizeof(mpa_len);
    iovec_num++;

    //! ULPDU
    for (int i = 0; i < num_sge; i++)
    {
        char* pkt = (char*)(sg_list[i].addr);
        iov[iovec_num].iov_base = pkt;
        iov[iovec_num].iov_len = sg_list[i].length;
        iovec_num++;
    }

    //! Padding
    int padding_bytes = (len + sizeof(mpa_len)) % 4;
    __u8 pad = 0;
    for (int i = 0; i < padding_bytes; i++)
    {
            iov[iovec_num].iov_base = &pad;
            iov[iovec_num].iov_len = sizeof(pad);
            iovec_num++;
    }

    //! TODO: Add support for CRC
    __u32 crc = 0;
    iov[iovec_num].iov_base = &crc;
    iov[iovec_num].iov_len = sizeof(crc);
    iovec_num++;

    //! TODO: Add support for markers

    msg.msg_iov = iov;
    msg.msg_iovlen = iovec_num;
    lwlog_debug("MPA Packet Header: %d", ntohs(mpa_len));
    lwlog_debug("MPA Packet CRC: %d", ntohs(crc));
    
    int ret = sendmsg(sockfd, &msg, 0);
#ifdef USE_TAS
    int g;
    lwlog_debug("doing dummy recv ...");
    recv(sockfd, &g, 0, 0);
    lwlog_debug("Yay dummy recv done");
#endif // USE_TAS
    return ret;
}

int mpa_recv(int sockfd, struct siw_mpa_packet* info, int num_bytes)
{
    //! If receiving the packet for the first time, get MPA header
    lwlog_debug("Reading num_bytes %d", num_bytes);
    int rcvd = 0;
    if (info->bytes_rcvd < MPA_HDR_SIZE)
    {
        int bytes_rcvd = info->bytes_rcvd;

        //! Get header
        lwlog_debug("receiving %u bytes", MPA_HDR_SIZE - bytes_rcvd);
        rcvd = recv(sockfd, (char*)&info->ulpdu_len + (uint)bytes_rcvd, MPA_HDR_SIZE - bytes_rcvd, 0);

        if (rcvd < MPA_HDR_SIZE - bytes_rcvd)
        {
            lwlog_err("mpa_recv: didn't receive enough bytes");
            return -1;
        }
        info->ulpdu_len = ntohs(info->ulpdu_len);
        lwlog_info("Received MPA Message Header: %d, ULPDU Len: %u", rcvd, info->ulpdu_len);
        info->bytes_rcvd = MPA_HDR_SIZE;
    }

    int packet_len = info->ulpdu_len;
    

    if (unlikely(packet_len <= 0))
    {
        lwlog_err("mpa_recv: packet header is %d", packet_len);
        return -2;
    }

    int ulpdu_rem_size = packet_len - (info->bytes_rcvd - MPA_HDR_SIZE);

    int num_bytes_to_read = num_bytes;
    int read_trailers = 0;
    //! If `num_bytes` to read are more than the remaining payload left,
    //! then read the trailers as well
    if (ulpdu_rem_size <= num_bytes)
    {
        num_bytes_to_read = ulpdu_rem_size;
        lwlog_debug("Reading %d and MPA packet trailers as well", num_bytes_to_read);
        read_trailers = 1;
    }
    
    rcvd = 0;
    int tries = 0;
    do {
        rcvd += recv(sockfd, ((char*)info->ulpdu)+rcvd, num_bytes_to_read-rcvd, 0);
        if (unlikely(rcvd <= 0)) break;
    } while (rcvd < num_bytes_to_read);

    if (unlikely(rcvd != num_bytes_to_read))
    {
        lwlog_err("mpa_recv: didn't receive enough bytes after header (%d)", rcvd);
        return -1;
    }

    info->bytes_rcvd += rcvd;

    if (read_trailers)
    {
        //! Get padding
        int padding_len = (packet_len + MPA_HDR_SIZE) % 4;
        rcvd = recv(sockfd, &info->crc, padding_len, 0);
        if (unlikely(rcvd < padding_len))
        {
            lwlog_err("mpa_recv: didn't receive enough bytes for padding (%d)", rcvd);
            return -1;
        }
        info->bytes_rcvd += rcvd;

        //! Get crc
        rcvd = recv(sockfd, &info->crc, MPA_CRC_SIZE, 0);
        if (unlikely(rcvd != MPA_CRC_SIZE))
        {
            lwlog_err("mpa_recv: didn't receive enough bytes for crc (%d)", rcvd);
            return -1;
        }
        info->bytes_rcvd += rcvd;

        //! TODO: Check CRC
    }

    return info->bytes_rcvd;
}
