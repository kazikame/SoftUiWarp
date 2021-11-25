#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <cstdint>
#include <cassert>
#include<cstring>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ddp.h"
#include "mpa/mpa.h"
#include "lwlog.h"
char log_buf1[1024];
//using namespace std;

tagged_buffer** stag_array = new tagged_buffer*[TAGGED_BUFFERS_NUM];

untagged_buffer** qn_array = new untagged_buffer*[UNTAGGED_BUFFERS_NUM];

uint32_t* tag_to_pd = new uint32_t[TAGGED_BUFFERS_NUM];

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

void register_tagged_buffer(stag_t* stag, void* pointer_to_memory, int len){
    if(stag->id >= TAGGED_BUFFERS_NUM){
        lwlog_err("stag greater than allocated tagged buffers");
        return;
    }
    if(len > TAGGED_BUFFER_SIZE){
        lwlog_err("length alllocated to tagged buffer is greater than max tagged buffer size");
        return;
    }
    struct tagged_buffer* buf = (struct tagged_buffer*) malloc(sizeof(struct tagged_buffer));
    buf->stag = stag->id;
    if(pointer_to_memory == NULL){
        pointer_to_memory = (char*)malloc(len*sizeof(char));
    }
    buf->data = pointer_to_memory;
    buf->len = len;
    stag_array[stag->id] = buf;
    return;
}

void  deregister_tagged_buffer(tagged_buffer* buf){
    delete(buf);
}

void register_untagged_buffer(int qn, int queue_len){
    if(qn >= UNTAGGED_BUFFERS_NUM){
        lwlog_err("qn greater than allocated untagged buffers");
        return;
    }
    if(queue_len > UNTAGGED_BUFFER_SIZE){
        lwlog_err("length alllocated to untagged buffer is greater than max untagged buffer size");
        return;
    }
    struct untagged_buffer* buf = (struct untagged_buffer*) malloc(sizeof(struct untagged_buffer));
    buf->data = (char*)malloc(len*sizeof(char));
    buf->len = len;
    qn_array[qn] = buf;
    return;
}

void  deregister_tagged_buffer(untagged_buffer* buf){
    delete(buf);
}

int ddp_tagged_send(struct ddp_stream_context* ctx, struct stag_t* tag, uint64_t offset, void* data, uint32_t len, uint8_t rsrvdULP){
    char *datac = static_cast<char*>(data);
    uint32_t data_size = MULPDU-DDP_TAGGED_HDR_SIZE;
    uint32_t num_packets = (len/data_size);
    if(len%data_size != 0)
        num_packets++;
    std::cout<<"num packets are "<<num_packets<<"\n";
    uint8_t resv = 129;
    for(uint32_t i = 0;i<len;i = i + data_size){
        if(i+data_size>=len){
            data_size = len-i;
            std::cout<<"last packet "<<data_size<<"\n";
            resv = resv | 64; //bit set for last packet;
        }
        char *buf = new char[DDP_TAGGED_HDR_SIZE+data_size];
        buf[0] = resv;
        buf[1] = rsrvdULP;
	uint32_t tag_in = tag->pd_id->pd_id;
        buf[2] = tag_in >> 24; buf[3] = tag_in >> 16; buf[4] = tag_in >> 8; buf[5] = tag_in; 
        buf[6] = offset >> 56; buf[7] = offset >> 48; buf[8] = offset >> 40; buf[9] = offset >> 32;
        buf[10] = offset >> 24; buf[11] = offset >> 16; buf[12] = offset >> 8; buf[13] = offset;
        for(int i = 0;i<data_size;i++){
            buf[DDP_TAGGED_HDR_SIZE+i] = datac[i];
        }
        offset = offset + data_size;
        mpa_send(ctx->sockfd, buf, DDP_TAGGED_HDR_SIZE+data_size, 1);
    }
}

int ddp_untagged_send(struct ddp_stream_context* ctx, struct stag_t* tag, void* data, uint32_t len,
                    uint64_t reserved, uint32_t qn, uint32_t msn){
    char *datac = static_cast<char*>(data);
    uint32_t data_size = MULPDU-DDP_UNTAGGED_HDR_SIZE;
    uint32_t num_packets = (len/data_size);
    if(len%data_size != 0)
        num_packets++;
    std::cout<<"num packets are "<<num_packets<<"\n";
    uint32_t offset = 0;
    uint8_t resv = 1;
    for(uint32_t i = 0;i<len;i = i + data_size){
        if(i+data_size>=len){
            data_size = len-i;
            std::cout<<"last packet "<<data_size<<"\n";
            resv = resv | 64; //bit set for last packet;
        }
        char *buf = new char[DDP_UNTAGGED_HDR_SIZE+data_size];
        buf[0] = resv;
        buf[1] = reserved >> 32;
        uint32_t reserved2 = reserved << 32;
        buf[2] = reserved2 >> 24;buf[3] = reserved2 >> 16 , buf[4]= reserved2 >> 8, buf[5] = reserved2;
        buf[6] = qn >> 24; buf[7] = qn >> 16; buf[8] = qn >> 8; buf[9] = qn;
        msn = msn%MOD32;
        buf[10] = msn >>24; buf[11] = msn >> 16; buf[12] = msn >> 8; buf[13] = msn;
        buf[14] = offset >> 24; buf[15] = offset >> 16; buf[16] = offset >> 8; buf[17] = offset;
        for(int i = 0;i<data_size;i++){
            buf[DDP_UNTAGGED_HDR_SIZE+i] = datac[i];
        }
        offset = offset + data_size;
        mpa_send(ctx->sockfd, buf, DDP_UNTAGGED_HDR_SIZE+data_size, 1);
    }
}

int ddp_recv(struct ddp_stream_context* ctx, struct ddp_message* message){
    struct siw_mpa_packet* info = (struct siw_mpa_packet*) malloc(sizeof(struct siw_mpa_packet));
    mpa_recv(ctx->sockfd, info, 1); //can do this in only one mpa_recv, change in ddp_recv_tag/untag a bit in input params
    uint8_t resv = info->ulpdu[0];
    int data_size = 0;
    if(resv&128==1){
        message->type = 1;
        data_size = ddp_tagged_recv(ctx,message);
    }
    else{
        message->type = 0;
        data_size = ddp_untagged_recv(ctx,message);
    }
    return data_size;
}

int ddp_tagged_recv(struct ddp_stream_context* ctx, struct ddp_message* message){
    int num_packets = 0;
    int data_size = 0;
    while(1){
        struct siw_mpa_packet* info = (struct siw_mpa_packet*) malloc(sizeof(struct siw_mpa_packet));
        mpa_recv(ctx->sockfd, info, MUL_PDU);
        num_packets++;
        int ptr = 0;
        message->hdr->tagged->reserved = info->ulpdu[ptr++]; //maybe need to put this in ddp_recv
        message->hdr->tagged->reservedULP = info->ulpdu[ptr++];
        for(int i = 0;i<4;i++){
            int move = 24;
            message->hdr->tagged->stag = info->ulpdu[ptr++] << move;
            move = move-8;
        }
        uint32_t stag = message->hdr->tagged->stag;
        if(ctx->pd_id->pd_id!=tag_to_pd[stag]){
            lwlog_err("invalid pd_id for the stag");
            return 0;
        }
        for(int i = 0;i<8;i++){
            int move = 56;
            message->hdr->tagged->to = info->ulpdu[ptr++] << move;
            move = move-8;
        }
        uint64_t offset = message->hdr->tagged->to;
        data_size += (info->ulpdu_len - DDP_TAGGED_HDR_SIZE);
        tagged_buffer* buf = stag_array[stag];
        if(num_packets == 1){
            message->data = *buf->data[offset]; //pointer to offset at tag buffer
            message->tag_buf = buf;
        }
        while(ptr < info->ulpdu_len){
            buf->data[offset++] =  info->ulpdu[ptr++];
        }
        if(message->hdr->tagged->reserved&64==1){ //last packet for this message
            message->len = data_size;
            std::cout<<"num packets received tagged "<<num_packets<<"\n";
            break;
        }
    }
    return data_size; //length of the message
}

int ddp_untagged_recv(struct ddp_stream_context* ctx, struct ddp_message* message){
    int num_packets = 0;
    int data_size = 0;
    while(1){
        struct siw_mpa_packet* info = (struct siw_mpa_packet*) malloc(sizeof(struct siw_mpa_packet));
        mpa_recv(ctx->sockfd, info, MUL_PDU);
        num_packets++;
        int ptr = 0;
        message->hdr->untagged->reserved = info->ulpdu[ptr++]; //maybe need to put this in ddp_recv
        message->hdr->untagged->reservedULP = info->ulpdu[ptr++];
        for(int i = 0;i<4;i++){
            int move = 24;
            message->hdr->untagged->reserved2 = info->ulpdu[ptr++] << move;
            move = move-8;
        }
        for(int i = 0;i<4;i++){
            int move = 24;
            message->hdr->untagged->qn = info->ulpdu[ptr++] << move;
            move = move-8;
        }
        for(int i = 0;i<4;i++){
            int move = 24;
            message->hdr->untagged->msn = info->ulpdu[ptr++] << move;
            move = move-8;
        }
        for(int i = 0;i<4;i++){
            int move = 24;
            message->hdr->untagged->mo = info->ulpdu[ptr++] << move;
            move = move-8;
        }
        uint64_t offset = message->hdr->untagged->mo;
        data_size += (info->ulpdu_len - DDP_TAGGED_HDR_SIZE);
        untagged_buffer* buf = qn_array[message->hdr->untagged->qn];
        if(num_packets == 1){
            message->data = buf->data; //pointer to offset at tag buffer
            message->untag_buf = buf;
        }
        while(ptr < info->ulpdu_len){
            buf->data[offset++] =  info->ulpdu[ptr++];
        }
        if(message->hdr->tagged->reserved&64==1){ //last packet for this message
            message->len = data_size;
            std::cout<<"num packets received untagged "<<num_packets<<"\n";
            break;
        }
    }
    return data_size;
}
    
