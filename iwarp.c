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