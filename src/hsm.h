/*
 * @file  hsm.h
 *
 * @brief  a general framework of hierarchical state machine
 *
 * @author jinyang.hust@gmail.com
 *
 * @bugs
 */

#ifndef _HSM_H_
#define _HSM_H_

#define HSM_STATE_NONE      255 /* invalid state */
#define HSM_EVENT_NONE      255 /* invalid event */


#define HSM_MAX_NAME        64
#define HSM_MAX_STATES      200
#define HSM_MAX_EVENTs      200


#define HSM_ASYNCHRONOUS    0x0
#define HSM_SYNCHRONOUS     0x1


/* @brief each instance of this structure indicates a state,
 * generally, users define an array of this object to 
 * initialize a serial states.
 */
typedef struct _hsm_state_info {
    uint8_t     state;
    uint8_t     parent_state;
    uint8_t     initial_substate;
    uint8_t     has_substates;
    const char      *name;
    void    (*hsm_state_entry) (void *ctx); /* called when enter a state first */
    void    (*hsm_state_exit) (void *ctx); /* called when exit a state */
    bool_t  (*hsm_state_event) (void *ctx, uint16_t event, uint16_t len, void *event_data); /* called when an event comes in a state */
} hsm_state_info_t;


typedef struct _hsm_engine*     hsm_engine_t;


/* common apis */

/* @brief create a hsm engine.
 * @param name user defined hsm name.
 * @param ctx user context.
 * @param initial_state
 * @param state_info 
 * @param num_states # of states.
 * @param prio message queue priority.
 * @param flags message queue type, sync or async.
 * @param hsm_debug_print
 * @param event_names users defined event names
 * @return hsm engine
 */
hsm_engine_t 
hsm_engine_create(const char *name, void *ctx, uint8_t init_state, const hsm_state_info_t *state_info, 
        uint8_t num_states, uint8_t max_queued_event, uint16_t event_data_len, 
        uint32_t flags, void (*hsm_debug_print)(void *ctx, const char *fmt, ...),
        const char **event_names, uint32_t num_event_names);

void hsm_engine_delete(hsm_engine_t hsm_engine);

uint8_t hsm_get_lastevent(hsm_engine_t hsm_engine);

uint8_t hsm_get_curstate(hsm_engine_t hsm_engine);

uint8_t hsm_get_nextstate(hsm_engine_t hsm_engine);

const char *hsm_get_state_name(hsm_engine_t hsm_engine, uint8_t state);

const char *hsm_get_current_state_name(hsm_engine_t hsm_engine);

/* @brief all events dispatched via this interface 
 * @param hsm_enginei
 * @param event
 * @param len
 * @param event_data
 * @return no*/
void hsm_dispatch(hsm_engine_t hsm_engine, uint16_t event, uint16_t len, void *event_data);

/* @brief hsm transit to new state
 * This makes state machine transit to a new state, before that, exit
 * routines (from substate to superstate) should be triggered, then 
 * entry routines (from superstate to substate) should be called.
 * @param hsm
 * @param state new state 
 * @return no
 */
void hsm_state_transtion(hsm_engine_t hsm, uint16_t state);

/* @brief reset hsm engine */
void hsm_engine_reset(hsm_engine_t hsm_engine, uint8_t initial_state, mesg_handler_t handler);


#endif
