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

#include "common.h"
#include <stdio.h>
#include <string.h>
#include <iostream>

#define MAX_BUF_LEN 1024

void print_mpa_rr(const struct mpa_rr* hdr, char* buf)
{
    const struct mpa_rr_params* params = &hdr->params;
    if (strncmp(MPA_KEY_REQ, (char*)hdr->key, MPA_RR_KEY_LEN) || strncmp(MPA_KEY_REP, (char*)hdr->key, MPA_RR_KEY_LEN))
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

void print_ddp(ddp_packet *pkt, char* buf)
{
    //const struct mpa_rr_params* params = &hdr->params;
        snprintf(buf, 1024, "iWARP DDP Header\n");
			    //"\tdata: %s\n"
                            //"\reservedULP Flag: %d\n"
                            //"\tstag: %d\n"
                            //"\tto: %d\n",
                            //std::string str(pkt->data),
                            //pkt->hdr->tagged->reservedULP,
                            //pkt->hdr->tagged->stag,
                            //pkt->hdr->tagged->to);
        return;
}
