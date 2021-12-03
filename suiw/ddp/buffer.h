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

#ifndef _BUFFERS_H
#define _BUFFERS_H

#include <stdlib.h>
#include "concurrentqueue.h"
#include "lwlog.h"
#include <linux/types.h>
#include <asm/byteorder.h>

struct untagged_buffer {
    char* data;
    uint32_t len;

    //! When passing a chain of untagged buffers
    struct untagged_buffer* next;
};

struct untagged_buffer_queue {
    moodycamel::ConcurrentQueue<struct untagged_buffer>* q;
    int msn = 1;
};

struct pd_t {
    __u32 pd_id;
};

struct stag_t {
    __u32 tag;
    struct pd_t pd;
};

struct tagged_buffer {
    stag_t stag;
    void* data;
    uint64_t len;
    int access_ctrl;
};

#endif