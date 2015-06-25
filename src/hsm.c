/*
 * @file  hsm.c
 *
 * @brief  a general framework of hierarchical state machine
 *
 * @author jinyang.hust@gmail.com
 *
 * @bugs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "misc.h"
#include "hsm_priv.h"
#include "hsm.h"

#if HAVE_HSM_TRACER
#define HSM_TRACER_SIZE        10

typedef enum _hsm_trace_type {
    HSM_EVENT_STATE_TRANSTION = 1,
    HSM_EVENT_MSG_PROCESSING,
    HSM_EVENT_MSG_QUEUEING
} hsm_trace_type_t;


typedef struct _hsm_trace_info {
    hsm_trace_type_t    trace_type;
    int                 event_type;
    uint8_t             initial_state;
    uint8_t             final_state;
} hsm_trace_info_t;


/* hsm tracer records state machine past info */
typedef struct _hsm_tracer {
    hsm_lock_t      ht_lock;
    int             index;
    hsm_trace_info_t  ht_info[HSM_TRACER_SIZE];
} hsm_tracer_t;

#endif /*HAVE_HSM_TRACER*/


/* hsm engine */
struct _hsm_engine {
    uint8_t     name[HSM_MAX_NAME]; /* state name */
    uint8_t     cur_state; /* current state */
    uint8_t     next_state;
    uint8_t     event_state; /* state that receive and handle this event */
    uint8_t     num_states;
    uint8_t     last_event;
    const hsm_state_info_t  *state_info; /* users defined all states */
    void        *ctx;               /* caller's context */
    uint32_t    in_state_transition : 1; /* in state transition */
    mesg_queue_t    mesg_q;
    const char  **event_names; /* only for debug use */
    uint32_t    num_event_names;
#if HAVE_HSM_TRACER
    hsm_tracer_t   tracer; /* trace past states, events */
#endif
    void    (*hsm_debug_print)(void *ctx, const char *fmt, ...);
};


#define HSM_DPRINT(hsm, fmt, ...)   do {                        \
        if (hsm->hsm_debug_print) {                             \
            (*hsm->hsm_debug_print)(hsm->ctx, fmt, __VA_ARGS__); \
        }                                                       \
    } while (0)                                                  


static void
hsm_dispatch_sync_internal(void *ctx, uint16_t event, uint16_t len, void *event_data);


#if HAVE_HSM_HISTORY

/* @brief record states and events 
 * @param hsm_engine a hsm engine 
 */
static void
hsm_tracer_record(hsm_engine_t hsm_engine, hsm_trace_type_t trace_type,
                    uint8_t initial_state, uint8_t final_state, int event_type)
{
    hsm_tracer_t *ptracer;
    hsm_trace_info_t *ptinfo;

    assert(hsm_engine);
    ptracer = &hsm_engine->tracer;

    hsm_lock(ptracer->ht_lock);
    ptinfo = ptracer->ht_info[ptracer->index];
    ptracer->index++;
    if (ptracer->index >= HSM_TRACER_SIZE) {
        ptracer->index = 0;
    }
    hsm_unlock(ptracer->ht_lock);

    ptinfo->trace_type = trace_type;
    ptinfo->initial_state = initial_state;
    ptinfo->final_state = final_state;
    ptinfo->event_type = event_type;
}


static void
hsm_tracer_init(hsm_engine_t hsm_engine)
{
    hsm_tracer_t *ptracer = &hsm_engine->tracer;

    hsm_lock_init(ptracer->lock);
    ptracer->index = 0;
}


static void 
hsm_tracer_delete(hsm_engine_t hsm)
{
    hsm_lock_destroy(&hsm_engin->tracer.ht_lock);
}
#endif


/* @brief create a hsm engine.
 * @param name user defined hsm name.
 * @param ctx user context.
 * @param initial_state
 * @param state_info 
 * @param num_states # of states.
 * @param flags message queue type, sync or async.
 * @param hsm_debug_print
 * @param event_names users defined event names
 * @return hsm engine
 */
hsm_engine_t
hsm_engine_create(const char *name, void *ctx, uint8_t init_state, 
                    const hsm_state_info_t *state_info, uint8_t num_states, 
                    uint8_t max_queued_event, uint16_t event_data_len, 
                    uint32_t flags,
                    void (*hsm_debug_print)(void *ctx, const char *fmt, ...),
                    const char **event_names, uint32_t num_event_names)
{
    hsm_engine_t hsm_engine;
    uint32_t i;
    mesgq_delivery_type_t mq_type;

    assert(state_info);
    /* validate state_info table, entries in the table need to
     * be valid and in order
     */
    for (i = 0; i < num_states; i++) {
        uint8_t state_visited[HSM_MAX_STATES] = {0};
        uint8_t state, next_state;

        if ((state_info[i].state >= HSM_MAX_STATES) || (state_info[i].state != i) )
        {
            fprintf(stderr, "state %d has invalid state %d\n", i, state_info[i].state);
            return NULL;
        }

        if (state_info[i].has_substates && (state_info[i].initial_substate == HSM_STATE_NONE))
        {
            fprintf(stderr,  "state %d is marked as super state but has no initial sub state \n", i);
            return NULL;
        }

        if (!state_info[i].has_substates && (state_info[i].initial_substate != HSM_STATE_NONE))
        {
            fprintf(stderr,  "state %d is not a super state but has initial sub state \n", i);
            return NULL;
        }

        if (state_info[i].has_substates && state_info[state_info[i].initial_substate].parent_state != i)
        {
            fprintf(stderr,  "state %d initial sub state is not a sub state \n", i);
            return NULL;
        }

        /* detect any loop exists */
        state = state_info[i].state;
        while (state != HSM_STATE_NONE) {
            if (state_visited[state]) {
                fprintf(stderr, "HSM: %s : loop existed on state macheine \n", name);
                return NULL;
            }
            state_visited[state] = 1;
            next_state = state_info[state].parent_state;
            
            if (next_state != HSM_STATE_NONE) {
                if (!state_info[next_state].initial_substate) {
                    fprintf(stderr, "state %d is marked as a super state but has no initial sub state \n", next_state);
                    return NULL;
                }
            }

        }
    }

    /* create a hsm engine */
    hsm_engine = (hsm_engine_t) hsm_malloc(sizeof(struct _hsm_engine));
    if (hsm_engine == NULL) {
        fprintf(stderr, "HSM: %s: malloc failed\n", name);
        return NULL;
    }

    hsm_memset(hsm_engine, 0, sizeof(struct _hsm_engine));

    /* initialize message queue */
    if (flags & HSM_SYNCHRONOUS) {
        mq_type = MESG_DELIVERY_SYNC;
    }
    else {
        mq_type = MESG_DELIVERY_ASYNC;
    }

    mesg_queue_init(&hsm_engine->mesg_q, event_data_len, max_queued_event, 
                hsm_dispatch_sync_internal, hsm_engine, mq_type);

    /* initialize members of hsm engine */
    hsm_strncpy((char *)hsm_engine->name, name, strlen(name)); 
    hsm_engine->cur_state = init_state;
    hsm_engine->num_states = num_states;
    hsm_engine->state_info = state_info;
    hsm_engine->last_event = HSM_EVENT_NONE;
    hsm_engine->in_state_transition = FALSE;
    hsm_engine->event_names = event_names;
    hsm_engine->num_event_names = num_event_names;
    hsm_engine->hsm_debug_print = hsm_debug_print;

#if HAVE_HSM_TRACER
    hsm_tracer_init(hsm_engine);
#endif

    return hsm_engine;
}


void
hsm_engine_delete(hsm_engine_t hsm_engine)
{
    mesg_queue_destroy(&hsm_engine->mesg_q);
    hsm_free(hsm_engine);
}


uint8_t 
hsm_get_lastevent(hsm_engine_t hsm_engine)
{
    return hsm_engine->last_event;
}


uint8_t
hsm_get_curstate(hsm_engine_t hsm_engine)
{
    return hsm_engine->cur_state;
}


uint8_t
hsm_get_nextstate(hsm_engine_t hsm_engine)
{
    return hsm_engine->next_state;
}


const char *
hsm_get_state_name(hsm_engine_t hsm_engine, uint8_t state)
{
    return hsm_engine->state_info[state].name;
}


const char *
hsm_get_current_state_name(hsm_engine_t hsm_engine)
{
    return hsm_engine->state_info[hsm_engine->cur_state].name;
}


/* @brief all events dispatched via this interface 
 * @param hsm_enginei
 * @param event
 * @param len
 * @param event_data
 * @return no*/
void
hsm_dispatch(hsm_engine_t hsm_engine, uint16_t event, uint16_t len, void *event_data)
{
#if HAVE_HSM_HISTORY
    hsm_tracer_record(hsm_engine, HSM_EVENT_MSG_QUEUEING, hsm_engine->cur_state, hsm_engine->cur_state, event);
#endif
    if (mesg_queue_send(&hsm_engine->mesg_q, event, len, event_data)) {
        HSM_DPRINT(hsm_engine, "%s: failed to deliver event %d\n", hsm_engine->name[hsm_engine->cur_state], event);
    }
}


/* @brief the event handlers
 * all events are handled by call registered event handlers
 * @param ctx hsm engine
 * @param event 
 * @param len length of event date
 * @param event_data 
 * @return no
 */
static void
hsm_dispatch_sync_internal(void *ctx, uint16_t event, uint16_t len, void *event_data)
{
    hsm_engine_t hsm_engine = (hsm_engine_t) ctx;
    bool_t event_handled = FALSE;
    uint8_t state;

    hsm_engine->last_event = event;
    state = hsm_engine->cur_state;
    
    /* go through hsm */
    while (!event_handled && (state != HSM_STATE_NONE)) {
       hsm_engine->event_state = state;
       event_handled = (*hsm_engine->state_info[state].hsm_state_event) (hsm_engine->ctx, event, len, event_data);
       state = hsm_engine->state_info[state].parent_state;
    }

    if (!event_handled) {
        HSM_DPRINT(hsm_engine, "%s : event %d in state %d not handled\n", hsm_engine->name, event, hsm_engine->cur_state);
    }
}


/* @brief hsm transit to new state
 * This makes state machine transit to a new state, before that, exit
 * routines (from substate to superstate) should be triggered, then 
 * entry routines (from superstate to substate) should be called.
 * @param hsm
 * @param state new state 
 * @return no
 */
void
hsm_state_transtion(hsm_engine_t hsm_engine, uint16_t state)
{
    const hsm_state_info_t *state_info = hsm_engine->state_info;
    uint8_t cur_state = hsm_engine->cur_state;

    if ((state == HSM_STATE_NONE) || (state > HSM_MAX_STATES)) {
        HSM_DPRINT(hsm_engine, "invalid state %d\n", state);
        return;
    }
    assert(hsm_engine->in_state_transition == 0);
    hsm_engine->in_state_transition = 1;

    hsm_engine->next_state = state;
    /* call exit routines from substate */
    while (cur_state != HSM_STATE_NONE) {
        if (state_info[cur_state].hsm_state_exit) {
            (*state_info[cur_state].hsm_state_exit) (hsm_engine->ctx);
        }
        cur_state = state_info[cur_state].parent_state;
    }

    cur_state = state;
    /* call all entry routines from superstate*/
    while (cur_state != HSM_STATE_NONE) {
        if (state_info[cur_state].hsm_state_entry) {
            (*state_info[cur_state].hsm_state_entry) (hsm_engine->ctx);
        }
        hsm_engine->cur_state = cur_state;
        cur_state = state_info[cur_state].initial_substate;
    }
    
    hsm_engine->cur_state = cur_state;
    hsm_engine->in_state_transition = 0;
}


/* @brief reset hsm engine */
void 
hsm_engine_reset(hsm_engine_t hsm_engine, uint8_t initial_state, mesg_handler_t handler)
{
    mesg_queue_drain(&hsm_engine->mesg_q, handler);
    hsm_engine->cur_state = initial_state;
}
