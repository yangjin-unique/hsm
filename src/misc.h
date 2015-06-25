/*
 * @file hsm_priv.h 
 *
 * @brief types define, os wrapper functions 
 *
 * @author jinyang.hust@gmail.com
 *
 * @bugs
 */

#ifndef _MISC_H_
#define _MISC_H_
#include "list.h"
#include "hsm_priv.h"
#include "heap-inl.h"
#include <pthread.h>


typedef enum _mesgq_delivery_type {
    MESG_DELIVERY_SYNC,
    MESG_DELIVERY_ASYNC
} mesgq_delivery_type_t;


typedef void (*timer_handler_t) (void *ctx);
typedef void (*mesg_handler_t) (void *ctx, uint16_t mesg_type, uint16_t mesg_len, void *mesg);

typedef struct _hsm_timer {
    struct heap_node    heap_node;
    timer_handler_t     thandler;
    void                *ctx;
    uint32_t            expire;
} hsm_timer_t;


typedef struct _mesg {
    SLIST_ENTRY(_mesg) list;
    uint16_t    type;
    uint16_t    len;
    uint8_t     data[0]; /* msg content, len bytes */
} mesg_t;

typedef struct _mesg_queue {
    STAILQ_HEAD(, _mesg) mesg_head; /* mesg queued */
    STAILQ_HEAD(, _mesg) free_mesg; /* freed mesg list, used for reuse */
    uint32_t    num_mesg; /* # of mesqs queued */
    uint32_t    mesg_len;
    pthread_mutex_t  qlock;
    uint8_t     *qbuf; /* queue buf, holds all mesgs */
    hsm_timer_t     timer;
    mesg_handler_t  qhandler;
    void        *ctx;
    uint8_t     is_synchronous : 1;
} mesg_queue_t;



int mesg_queue_init(mesg_queue_t *queue, uint32_t mesg_len, uint32_t max_queued,
                mesg_handler_t handler, void *ctx, mesgq_delivery_type_t type);


int mesg_queue_send(mesg_queue_t *queue, uint16_t type, uint16_t len, uint8_t *mesg_data);

void mesg_queue_drain(mesg_queue_t *queue, mesg_handler_t msg_handler);


void mesg_queue_destroy(mesg_queue_t *queue);


#endif






