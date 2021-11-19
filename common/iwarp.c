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

#include "iwarp.h"
#include <stdio.h>
#include <string.h>

#define MAX_BUF_LEN 1024

void print_mpa_rr(const struct mpa_rr* hdr, char* buf)
{
    const struct mpa_rr_params* params = &hdr->params;
    if (strncmp(MPA_KEY_REQ, hdr->key, MPA_RR_KEY_LEN) || strncmp(MPA_KEY_REP, hdr->key, MPA_RR_KEY_LEN))
    {
        snprintf(buf, 1024, "iWARP MPA RR Header\n"
                            "\tKey: %.*s\n"
                            "\tMarker Flag: %d\n"
                            "\tCRC Flag: %d\n"
                            "\tConnection Rejected Flag: %d\n"
                            "\tRevision: %d\n"
                            "\tPD Length: %d\n",
                            MPA_RR_KEY_LEN,
                            hdr->key,
                            (params->bits & MPA_RR_FLAG_MARKERS) > 0,
                            (params->bits & MPA_RR_FLAG_CRC) > 0,
                            (params->bits & MPA_RR_FLAG_REJECT) > 0,
                            __mpa_rr_revision(params->bits),
                            __be16_to_cpu(params->pd_len));
        return;
    }
}