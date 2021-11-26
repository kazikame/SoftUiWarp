#include "cq.h"

struct cq* create_cq(struct rdmap_stream_context* ctx, int num_cqe) {
    struct cq* cq = (struct cq*)malloc(sizeof(struct cq));

    cq->ctx = ctx;
    cq->q = new moodycamel::ConcurrentQueue<work_completion>(num_cqe);

    return cq;
}

int destroy_cq(struct cq* cq) {
    delete cq->q;
    free(cq);
    return 0;
}

int poll_cq(struct cq *cq, int num_entries, struct work_completion *wc) {
    return cq->q->try_dequeue_bulk(wc, num_entries);
}


struct wq* create_wq(struct rdmap_stream_context *context,
					   struct wq_init_attr *wq_init_attr) {
    
    struct wq* q = (struct wq*)malloc(sizeof(struct wq));

    q->context = context;
    q->pd = wq_init_attr->pd;
    q->cq = wq_init_attr->cq;
    q->wq_type = wq_init_attr->wq_type;
    switch(wq_init_attr->wq_type)
    {
        case WQT_SQ: {
            q->send_q = new moodycamel::ConcurrentQueue<send_wr>(wq_init_attr->max_wr);
            break;
        }
        case WQT_RQ: {
            q->recv_q = new moodycamel::ConcurrentQueue<recv_wr>(wq_init_attr->max_wr);
            break;
        }
    }
    q->comp_mask = wq_init_attr->comp_mask;

    return q;
}

int destroy_wq(struct wq* q)
{
    switch(q->wq_type)
    {
        case WQT_SQ: {
            delete q->send_q;
            break;
        }
        case WQT_RQ: {
            delete q->recv_q;
            break;
        }
    }

    free(q);
    return 0;
}

int send_wr_to_wce(struct send_wr* wr, struct work_completion* wce)
{
	wce->wr_id = wr->wr_id;
	wce->opcode = (enum wc_opcode)wr->opcode;
}