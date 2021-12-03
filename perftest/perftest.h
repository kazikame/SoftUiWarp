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
#define DEFAULT_MAX_REQS 1024

struct perftest_context {
    char *port; 
    char *ip;
    int buf_size;
    char *buf;
    int iters;
    int max_reqs;
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
                  int (*test_iter)(perftest_context*),
                  void (*test_fini)(perftest_context*, float));

#endif // PERFTEST_H
