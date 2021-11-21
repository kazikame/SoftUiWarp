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

#ifndef _MPA_H
#define _MPA_H

#include "common/iwarp.h"

//! Assuming ethernet
#define EMSS 1460
#define MULPDU EMSS - (6 + 4 * Ceiling(EMSS / 512) + EMSS % 4)
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
int send_mpa_rr(int sockfd, const void* pdata, __u8 pd_len, int req);

/**
 * @brief receives an MPA request/reply
 * 
 * @param sockfd TCP connection socket
 * @param info request/reply struct in which data is received. Must be allocated
 * @return int number of bytes received, < 0 if error
 */
int recv_mpa_rr(int sockfd, struct siw_mpa_info* info);

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

int mpa_send(int sockfd, void* ulpdu, __u16 len, int flags);

int mpa_recv(int sockfd, void* ulpdu, int* len);

#endif