/* RTOS 模式：用桩锁宏验证所有容器在 EKCFG_RTOS=1 下能编译；轻量断言 */
#ifndef EK_CONF_H
#define EK_CONF_H
#define EKCFG_RTOS (1)

typedef struct
{
    volatile int taken;
} ek_test_mutex_t;

#define EK_LOCK_TYPE         ek_test_mutex_t
#define EK_LOCK_INIT(lock)   ((lock).taken = 0)
#define EK_LOCK_DEINIT(lock) ((void)(lock))
#define EK_LOCK_ACQUIRE(obj) (((obj)->lock.taken = 1) == 1)
#define EK_LOCK_RELEASE(obj) ((obj)->lock.taken = 0)

#define EKCFG_PICOLIBC       (0)
#define EKCFG_IO_LWPRTF      (1)
#define EKCFG_ASSERT         (1)
#define EKCFG_ASSERT_TINY    (1)
#define EKCFG_LOG            (1)
#define EKCFG_STR            (1)
#define EKCFG_STR_VIEW       (1)
#define EKCFG_LIST           (1)
#define EKCFG_VEC            (1)
#define EKCFG_RINGBUF        (1)
#define EKCFG_RINGBUF_SPSC   (1)
#define EKCFG_STACK          (1)
#define EKCFG_SNAPSHOT       (1)
#define EKCFG_KFIFO          (1)
#define EKCFG_PICOTHREAD     (1)
#define EKCFG_PICOTHREAD_SEM (1)
#define EKCFG_PICOTHREAD_MSG (1)
#define EKCFG_HEAP_SIZE      (8 * 1024)
#endif
