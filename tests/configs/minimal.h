/* 全部关闭：断言、日志都关，只剩库的空壳与 TLSF 堆 */
#ifndef EK_CONF_H
#define EK_CONF_H
#define EKCFG_RTOS      (0)
#define EKCFG_PICOLIBC  (0)
#define EKCFG_IO_LWPRTF (0)
#define EKCFG_NEWLIB    (0)
#define EKCFG_ASSERT    (0)
#define EKCFG_LOG       (0)
#define EKCFG_HEAP_SIZE (4 * 1024)
#endif
