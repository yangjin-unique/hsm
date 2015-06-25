/*
 * @file hsm_priv.h 
 *
 * @brief types define, os wrapper functions 
 *
 * @author jinyang.hust@gmail.com
 *
 * @bugs
 */

#ifndef _HSM_PRIV_H_
#define _HSM_PRIV_H_

typedef unsigned char       uint8_t;
typedef short               int16_t;
typedef unsigned short      uint16_t;
typedef int                 int32_t;
typedef unsigned int        uint32_t;
typedef unsigned long long uint64_t;

typedef enum {
    FALSE = 0,
    TRUE = 1
} bool_t;

#define hsm_malloc      malloc
#define hsm_free        free
#define hsm_memset      memset
#define hsm_strncpy     strncpy
#define hsm_lock        pthread_mutex_lock
#define hsm_unlock      pthread_mutex_unlock
#define hsm_lock_init   pthread_mutex_init
#define hsm_lock_destroy    pthread_mutex_destroy

#endif






