/**
 * @file osal.h
 * @brief OS 抽象层（OSAL）公共类型定义
 * @author N1netyNine99
 */

#ifndef EK_OSAL_H
#define EK_OSAL_H

#include "ek/conf_internal.h"

#include "ek/core/def.h"

#if EKCFG_RTOS

#    if EKCFG_RTOS == 1

#        include "ek/os/thread.h"
#        include "ek/os/mutex.h"
#        include "ek/os/msgq.h"
#        include "ek/os/sem.h"
#        include "ek/os/condvar.h"
#        include "ek/os/barrier.h"
#        include "ek/os/kfifo.h"

typedef os_thread_t ek_osal_thread_t;
typedef ek_os_sem_t ek_osal_sem_t;
typedef ek_os_mutex_t ek_osal_mutex_t;
typedef ek_os_msgq_t ek_osal_msgq_t;
typedef ek_os_condvar_t ek_osal_condvar_t;
typedef ek_os_barrier_t ek_osal_barrier_t;
typedef ek_os_kfifo_t ek_osal_kfifo_t;

#    endif

#endif

#endif // EK_OSAL_H
