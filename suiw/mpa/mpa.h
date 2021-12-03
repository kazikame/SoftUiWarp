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

#ifndef _MPA_H
#define _MPA_H

#include "common.h"

#define EMSS 65535
#define MULPDU EMSS - (6 + 4 * (EMSS / 512 + 1) + EMSS % 4)

#define MPA_HDR_SIZE	2
#define MPA_CRC_SIZE	4

//! Assuming ethernet
#define ULPDU_MAX_SIZE 1 << 16
#define FPDU_MAX_SIZE ULPDU_MAX_SIZE + 2 + MPA_CRC_SIZE

//!
extern int mpa_protocol_version;

/**
 * @brief sends an MPA request/reply
 * 
 * @param sockfd TCP connected socket
 * @param pdata private data to be sent
 * @param pd_len length of the private data
 * @param req 0 if response, else request
 * @return int number of total bytes sent, negative if failed
 */
int mpa_send_rr(int sockfd, const void* pdata, __u8 pd_len, int req);

/**
 * @brief receives an MPA request/reply
 * 
 * @param sockfd TCP connection socket
 * @param info request/reply struct in which data is received. Must be allocated
 * @return int number of bytes received, < 0 if error
 */
int mpa_recv_rr(int sockfd, struct siw_mpa_info* info);

/**
 * @brief 
 * 
 * @param sockfd TCP connection socket
 * @param pdata_send private data to be sent
 * @param pd_len length of private data to be sent
 * @param pdata_recv private data to be received. Must be large enough to hold
 * @return int number of bytes received, < 0 if error
 */
int mpa_client_connect(int sockfd, void* pdata_send, __u8 pd_len, void* pdata_recv);

int mpa_server_accept(int sockfd, void *pdata_send, __u8 pd_len, void* pdata_recv);

int mpa_send(int sockfd, sge* sg_list, int num_sge, int flags);

/**
 * @brief receives an MPA packet
 * 
 * info struct must be pointing to valid memory
 * 
 * @note instead of receiving the entire packet in one go
 *       this function can be called multiple times.
 *       Each invocation will change info->bytes_received.
 *       The *first* invocation per packet will fill in the ulpdu_len
 *       and other header fields.
 *       Each invocation will directly copy the _remaining_
 *       ulpdu bytes to info->ulpdu (no offset will be added).
 * 
 * @param sockfd TCP socket connection
 * @param info pointer to packet struct
 * @param num_bytes number of bytes of ulpdu to read
 * @return int numbers of bytes received, negative if error
 */
int mpa_recv(int sockfd, struct siw_mpa_packet* info, int num_bytes);

#endif
