#include <iostream>
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
#include<map>
#include<queue>
#include "ddp.h"
#include "mpa/mpa.h"
#include "common/iwarp.h"

#include "lwlog.h"
char log_buf1[1024];
char* tagged_buffer[TAGGED_BUFFERS_NUM];
char* untagged_buffer[UNTAGGED_BUFFERS_NUM];
//using namespace std;
std::map<uint32_t,uint32_t> tag_to_pd;
std::map<uint8_t, std::pair<u_int32_t, std::pair<u_int32_t, u_int32_t> > > ULP_to_tagged_buffer; //stag and start and end (included) of the buffer for that ULP/message
std::map<uint64_t, std::pair<u_int32_t, std::pair<u_int32_t, u_int32_t> > > ULP_to_untagged_buffer;
ddp_stream_context* ddp_init_stream(int sockfd, struct pd* pd_id){
    ddp_stream_context* ddp_strem_ctx = new ddp_stream_context;
    ddp_strem_ctx->sockfd = sockfd;
    ddp_strem_ctx->pd_id = new pd;
    ddp_strem_ctx->pd_id->pd_id = pd_id->pd_id;
    return ddp_strem_ctx;
}

void ddp_kill_stream(struct ddp_stream_context* ddp_strem_ctx){
    delete ddp_strem_ctx->pd_id;
    delete ddp_strem_ctx;
    return;
}

int register_stag(struct stag_t* tag){
    tag_to_pd[tag->id] = (tag->pd_id)->pd_id;
    return tag->id;
}

void register_tagged_buffer(){
    for(int i = 0;i<TAGGED_BUFFERS_NUM;i++){
	    //char* buff = new char[TAGGED_BUFFER_SIZE];
	tagged_buffer[i] = new char[TAGGED_BUFFER_SIZE]; 
    }
    return;
}
void register_untagged_buffer(){
    for(int i = 0;i<UNTAGGED_BUFFERS_NUM;i++){
	//char* buff = new char[UNTAGGED_BUFFER_SIZE];
        untagged_buffer[i] = new char[UNTAGGED_BUFFER_SIZE]; 
    }
    return;
}

int ddp_tagged_send(struct ddp_stream_context* ctx, struct stag_t* tag, uint32_t offset, void* data, uint32_t len, uint8_t rsrvdULP){
    //create ddp header for each fragment
    ddp_tagged_hdr* hdr = new ddp_tagged_hdr;
    hdr->reserved = 129; //first and last bit set
    hdr->reservedULP = rsrvdULP;
    hdr->stag = tag->id;
    //fragmentation
    char *datac = static_cast<char*>(data);
    uint32_t data_size = MULPDU-DDP_TAGGED_HDR_SIZE;
    uint32_t num_packets = (len/data_size);
    if(len%data_size != 0)
        num_packets++;
    ddp_packet *pkts = new ddp_packet[num_packets];
    for(uint32_t i = 0;i<len;i = i + data_size){
        ddp_packet* pkt = new ddp_packet;
        pkt->hdr = new ddp_hdr;
        pkt->hdr->tagged = hdr;
        pkt->hdr->tagged->to = offset;
        if(i+data_size>=len){
            pkt->data = new char[len-i+1];
            pkt->hdr->tagged->reserved = pkt->hdr->tagged->reserved | 64; //bit set for last packet;
        }
        else{
            pkt->data = new char[data_size];
        }
        for(uint32_t j = i, k = 0;k<data_size && j<len;j++,k++){
            pkt->data[k] = datac[j];
        }
        offset = offset + data_size;
	print_ddp(pkts, log_buf1);
        pkts[i] = *pkt;
    }
//print_ddp(&pkts[0], log_buf);
    lwlog_info("Sending DDP Message:\n%s", log_buf1);
    mpa_send(ctx->sockfd, pkts, num_packets, NULL);
    //mpa_send pass to this here or fake client ??
    //what to return in this fn ? 
}

int ddp_untagged_send(struct ddp_stream_context* ctx, struct stag_t* tag, void* data, uint32_t len, 
                    uint64_t reserved, uint32_t qn, uint32_t msn){
    ddp_untagged_hdr* hdr = new ddp_untagged_hdr;
    hdr->reserved = 129; //first and last bit set
    hdr->reserved2 = static_cast<uint32_t>(reserved & 0xFFFFFFFF);
    hdr->reservedULP = reserved >> 32;
    hdr->qn = qn;
    hdr->msn = msn%MOD32;
    //fragmentation
    char *datac = static_cast<char*>(data);
    uint32_t data_size = MULPDU-DDP_UNTAGGED_HDR_SIZE;
    uint32_t num_packets = (len/data_size);
    if(len%data_size != 0)
        num_packets++;
    uint32_t offset = 0;
    ddp_packet *pkts = new ddp_packet[num_packets];
    for(uint32_t i = 0;i<len;i = i + data_size){
        ddp_packet* pkt = new ddp_packet;
        pkt->hdr = new ddp_hdr;
        pkt->hdr->untagged = hdr;
        pkt->hdr->untagged->mo = offset;
        if(i+data_size>=len){
            pkt->data = new char[len-i+1];
            pkt->hdr->untagged->reserved = pkt->hdr->untagged->reserved | 64; //bit set for last packet;
        }
        else{
            pkt->data = new char[data_size];
        }
        for(uint32_t j = i, k = 0;k<data_size && j<len;j++,k++){
            pkt->data[k] = datac[j];
        }
        offset = offset + data_size;
        pkts[i] = *pkt;
    }
}

int ddp_tagged_recv(struct ddp_stream_context* ctx, struct ddp_packet* pkt){
    uint32_t data_size = MULPDU-DDP_TAGGED_HDR_SIZE;
    uint32_t stag = pkt->hdr->tagged->stag;
    uint32_t last = pkt->hdr->tagged->reserved & 64 ;
    if(tag_to_pd.find(stag)==tag_to_pd.end()){
        lwlog_err("invalid stag");
        return 0;
    }
    if(ctx->pd_id->pd_id!=tag_to_pd[stag]){
        lwlog_err("invalid pd_id for the stag");
        return 0;
    }
    uint32_t offset = pkt->hdr->tagged->to;
    for(uint32_t i = 0;i<data_size;i++){
        if(offset+i>=TAGGED_BUFFERS_NUM)
        {
            lwlog_err("buffer comsumed already");
            return 0;
        }
        if(pkt->data[i]==NULL){
            data_size = i+1;
            break;
        }
        tagged_buffer[stag][offset+i] = pkt->data[i];
    }
    uint8_t ulp_key= pkt->hdr->tagged->reservedULP;
    if(ULP_to_tagged_buffer.find(ulp_key)==ULP_to_tagged_buffer.end()){
        ULP_to_tagged_buffer[ulp_key] = std::make_pair(stag, std::make_pair(offset,0));
    }
    if(last==64){
        ULP_to_tagged_buffer[ulp_key].second.second = offset+data_size-1;
    }
    return ulp_key; //reservedULP need to be returned to the fn, so returned that for now. 
}
//TODO: finish untagged recv ddp 
int ddp_untagged_recv(struct ddp_stream_context* ctx, struct ddp_packet* packet){
    uint32_t data_size = MULPDU-DDP_UNTAGGED_HDR_SIZE;
    //uint64_t ulp_key = pkt->hdr->untagged->reservedULP << 32 | pkt->hdr->untagged->reserved2;
    //uint32_t last = pkt->hdr->untagged->reserved & 64 ;
    return 0;
}
    
