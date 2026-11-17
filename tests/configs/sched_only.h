/* 只开调度器，依赖的链表/环形缓冲区由配置层自动打开；断言、日志关闭 */
#ifndef EK_CONF_H
#define EK_CONF_H
#define EKCFG_RTOS           (0)
#define EKCFG_PICOLIBC       (0)
#define EKCFG_IO_LWPRTF      (1)
#define EKCFG_ASSERT         (0)
#define EKCFG_LOG            (0)
#define EKCFG_EVOKE          (1)
#define EKCFG_PICOTHREAD     (1)
#define EKCFG_PICOTHREAD_SEM (1)
#define EKCFG_PICOTHREAD_MSG (1)
#define EKCFG_KFIFO          (1)
#define EKCFG_HEAP_SIZE      (4 * 1024)
#endif
