/**
 * @file ek_conf.h
 * @brief 主机单元测试配置：打开全部模块，IO 走系统 libc
 */

#ifndef EK_CONF_H
#define EK_CONF_H

#define EKCFG_RTOS      (0)
#define EKCFG_PICOLIBC  (0)
#define EKCFG_IO_LWPRTF (0)
#define EKCFG_NEWLIB    (0)

/* export/static_alloc 依赖链接脚本段，由 test_export.c 单独覆盖测试 */
#ifndef EKCFG_EXPORT
#    define EKCFG_EXPORT (0)
#endif
#ifndef EKCFG_EXPORT_AUTO_INIT
#    define EKCFG_EXPORT_AUTO_INIT (0)
#endif
#ifndef EKCFG_STATIC_ALLOC
#    define EKCFG_STATIC_ALLOC (0)
#endif

#define EKCFG_ASSERT      (1)
#define EKCFG_ASSERT_TINY (0) /* full 模式：断言失败调用 ek_assert_hook，测试里直接退出 */
#define EKCFG_ASSERT_LOG  (0)
#define EKCFG_LOG         (1)
#define EKCFG_LOG_COLOR   (0)

#define EKCFG_STR            (1)
#define EKCFG_STR_VIEW       (1)
#define EKCFG_LIST           (1)
#define EKCFG_VEC            (1)
#define EKCFG_RINGBUF        (1)
#define EKCFG_RINGBUF_SPSC   (1)
#define EKCFG_STACK          (1)
#define EKCFG_SNAPSHOT       (1)
#define EKCFG_KFIFO          (1)
#define EKCFG_EVOKE          (1)
#define EKCFG_PICOTHREAD     (1)
#define EKCFG_PICOTHREAD_SEM (1)
#define EKCFG_PICOTHREAD_MSG (1)

#define EKCFG_HEAP_TLSF                (1)
#define EKCFG_HEAP_SIZE                (64 * 1024)
#define EKCFG_EVOKE_MIN_DEEPSLEEP_TICK (10)
#define EKCFG_PT_PRIO_LOWEST           (7)

#endif /* EK_CONF_H */
