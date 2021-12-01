/*
 * Performance test for SoftUIWarp library.
 *
 * Authors: Matthew Pabst <pabstmatthew@gmail.com>
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
#ifndef PERFTEST_H
#define PERFTEST_H

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "lwlog.h"

//#define PERFTEST_TEST

#define DEFAULT_BUF_SIZE 1024
#define DEFAULT_ITERS 1024
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT "9999"

struct perftest_context {
    char *port; 
    char *ip;
    int buf_size;
    char *buf;
    int iters;
    bool is_client;
    int serverfd;
    int huge_shmid;
    struct rdmap_stream_context* ctx;
};

struct send_data {
    uint64_t offset;
    uint64_t stag;
    uint64_t size;
};

void perftest_run(int argc, char **argv, 
                  int (*test_init)(perftest_context*, uint32_t, struct send_data*), 
                  int (*test_iter)(perftest_context*));

#endif // PERFTEST_H
