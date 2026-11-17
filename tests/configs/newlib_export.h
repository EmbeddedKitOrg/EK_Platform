/* newlib syscall port + 系统 printf，自动初始化手动调用 */
#ifndef EK_CONF_H
#define EK_CONF_H
#define EKCFG_RTOS             (0)
#define EKCFG_PICOLIBC         (0)
#define EKCFG_IO_LWPRTF        (0)
#define EKCFG_NEWLIB           (1)
#define EKCFG_EXPORT           (1)
#define EKCFG_EXPORT_AUTO_INIT (0)
#define EKCFG_STATIC_ALLOC     (1)
#define EKCFG_ASSERT           (1)
#define EKCFG_ASSERT_TINY      (1)
#define EKCFG_LOG              (1)
#define EKCFG_RINGBUF          (1)
#define EKCFG_STACK            (1)
#define EKCFG_SNAPSHOT         (1)
#define EKCFG_KFIFO            (1)
#define EKCFG_EVOKE            (1)
#define EKCFG_HEAP_SIZE        (8 * 1024)
#endif
