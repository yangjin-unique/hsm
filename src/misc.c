/*
 * @file  misc.c
 *
 * @brief some useful 
 *
 * @author jinyang.hust@gmail.com
 *
 * @bugs
 */

#include <stdlib.h>
#include "misc.h"



int 
mesg_queue_init(mesg_queue_t *queue, uint32_t mesg_len, uint32_t max_queued,
                mesg_handler_t handler, void *ctx, mesgq_delivery_type_t type)
{
    mesg_t  *mesg = NULL;
    int len;

    len = mesg_len + sizeof(mesg_t);
    queue->qbuf = malloc(len * max_queued);
    if (queue->qbuf == NULL) {
        return -1;
    }
    queue->num_mesg = 0;
    queue->mesg_len = mesg_len;
    pthread_mutex_init(queue->qlock);
    queue->qhandler = handler;
    queue->ctx = ctx;
    STAILQ_INIT(&queue->mesg_head);
    STAILQ_INIT(&queue->free_mesg);
    /* mark qbuf into free list */
    mesg = (mesg_t *) queue->qbuf;
    for (int i=0; i < max_queued; i++) {
        STAILQ_INSERT_TAIL(&queue->free_mesg, mesg, list);
        mesg = (mesg_t *) ((uint8_t *)mesg + sizeof(mesg_t));
    }
    if (type == MESG_DELIVERY_SYNC) {
        queue->is_synchronous = 1;
    }
    else {
        queue->is_synchronous = 0;
    }
    return 0;
}


int 
mesg_queue_send(mesg_queue_t *queue, uint16_t type, uint16_t len, uint8_t *mesg_data) 
{
    mesg_t *mesg = NULL;

    pthread_mutex_lock(queue->qlock);

    if (queue->is_synchronous) {
        queue->qhandler(queue->ctx, type, len, mesg_data);
    }
    else {
        mesg = STAILQ_FIRST(&queue->free_mesg);
        if (mesg == NULL) {
            pthread_mutex_unlock(queue->qlock);
            return -1;
        }
        mesg->type = type;
        mesg->len = len;
        if (len) {
            memcpy(mesg->data, mesg_data, len);
        }
        STAILQ_INSERT_TAIL(&queue->mesg_head, mesg, list);
        queue->num_mesg++;
        if (queue->num_mesg == 1) {
            timer_set(&queue->timer, 0);
        }
    }

    pthread_mutex_unlock(queue->qlock);
    return 0;
}


void
mesg_queue_drain(mesg_queue_t *queue, mesg_handler_t msg_handler)
{
    mesg_t *mesg = NULL; 

    assert(msg_handler);
    pthread_mutex_lock(queue->qlock);
    mesg = STAILQ_FIRST(&queue->mesg_head);
    while (mesg) {
        STAILQ_REMOVE_HEAD(&queue->mesg_head, list);
        queue->num_mesg--;
        msg_handler(queue->ctx, mesg->type, mesg->len, mesg->data);
        STAILQ_INSERT_TAIL(&queue->free_mesg, mesg, list);
        mesg = STAILQ_FIRST(&queue->mesg_head);
    }
    pthread_mutex_unlock(queue->qlock);
}


void 
mesg_queue_destroy(mesg_queue_t *queue)
{
    pthread_mutex_lock(queue->qlock);
    queue->num_mesg = 0;
    STAILQ_INIT(&queue->mesg_head);
    STAILQ_INIT(&queue->free_mesg);
    pthread_mutex_unlock(queue->qlock);
    pthread_mutex_destroy(queue->qlock);
}
