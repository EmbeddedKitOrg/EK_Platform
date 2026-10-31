/**
 * @file ek_conf.h 模板
 *
 * 使用方法:
 *   1. 将此文件复制到你的项目 include 路径中，命名为 ek_conf.h
 *   2. 按需修改各宏的值（不修改的宏自动取默认值）
 *   3. 确保 ek_conf.h 能被编译器找到（两种方式，详见 README）：
 *      - CMake 传参：cmake -DEK_CONF_PATH="Core/Inc/ek_conf.h"
 *      - include 路径：把存放本文件的目录加给 ek_utils 对象库
 *        （target_include_directories(ek_utils PUBLIC Core/Inc)）；
 *        注意仅给主 target 设 PRIVATE 不会传播给子项目 ek_utils。
 */

#ifndef EK_CONF_H
#define EK_CONF_H

/* ========================================================================
 * 平台/运行环境
 * ======================================================================== */

#define EKCFG_RTOS (0) /**< 是否使用 RTOS (0=裸机) */
/* RTOS 锁配置（EKCFG_RTOS == 1 时需在 ek_conf.h 中定义）：
 * 包含你的 RTOS 头文件，然后定义锁类型和操作宏：
 *   #include "FreeRTOS.h"
 *   #include "semphr.h"
 *   #define EK_LOCK_TYPE         SemaphoreHandle_t
 *   #define EK_LOCK_INIT(lock)   ((lock) = xSemaphoreCreateMutex())
 *   #define EK_LOCK_DEINIT(lock) vSemaphoreDelete(lock)
 *   #define EK_LOCK_TRY(obj)     (xSemaphoreTake((obj)->lock, 0) == pdTRUE)
 *   #define EK_LOCK_ACQUIRE(obj) (xSemaphoreTake((obj)->lock, portMAX_DELAY) == pdTRUE)
 *   #define EK_LOCK_RELEASE(obj) xSemaphoreGive((obj)->lock)
 */
/* EKCFG_PICOLIBC / EKCFG_NEWLIB 由 CMake 的 PICOLIBC 选项派生。
 * 裸机 newlib 需要 syscall port 时，在项目 ek_conf.h 中定义 EKCFG_NEWLIB 1。 */
#define EKCFG_IO_LWPRTF    (1) /**< IO 是否使用 lwprintf (1=使用, 0=不使用) */
#define EKCFG_STATIC_ALLOC (0) /**< 是否启用 EK_DEFINE_* 静态对象注册 (1=使用, 0=不使用);静态初始化函数始终可用 */

/* ========================================================================
 * 核心服务
 * ======================================================================== */

#define EKCFG_EXPORT (0) /**< 自动初始化机制 */
#define EKCFG_ASSERT (1) /**< 断言模块 */
#define EKCFG_LOG    (1) /**< 日志模块 */

/* ========================================================================
 * 数据结构
 * ======================================================================== */

#define EKCFG_STR            (1) /**< 动态字符串 */
#define EKCFG_STR_VIEW       (1) /**< 字符视图 */
#define EKCFG_LIST           (1) /**< 双向循环链表 */
#define EKCFG_VEC            (1) /**< 动态数组 */
#define EKCFG_RINGBUF        (1) /**< 通用环形缓冲区 */
#define EKCFG_RINGBUF_SPSC   (1) /**< SPSC 无锁环形缓冲区 */
#define EKCFG_STACK          (1) /**< 通用栈 */
#define EKCFG_SNAPSHOT       (1) /**< 快照 */
#define EKCFG_BITSTREAM      (1) /**< 比特流读写 */
#define EKCFG_EVOKE          (1) /**< 事件驱动调度器 */
#define EKCFG_PICOTHREAD     (1) /**< 微线程 */
#define EKCFG_PICOTHREAD_SEM (1) /**< 微线程信号量 (依赖 PICOTHREAD) */
#define EKCFG_PICOTHREAD_MSG (1) /**< 微线程消息队列 (依赖 PICOTHREAD) */
#define EKCFG_VFS            (1) /**< VFS 虚拟文件系统 */

/* ========================================================================
 * 模块子配置 — 根据实际硬件调整
 * ======================================================================== */

#define EKCFG_HEAP_SECTION             ".heap" /**< 堆内存所在链接器段 (如 ".tcmram") */
#define EKCFG_HEAP_TLSF                (1) /**< 内存堆使用 TLSF 分配器 (1=TLSF, 0=自定义) */
#define EKCFG_HEAP_SIZE                (16 * 1024) /**< 内存堆大小（字节） */
#define EKCFG_TLSF_FL_INDEX_MAX        (24) /**< TLSF 一级索引最大值，最大连续块为 2 的该值次方字节 */
#define EKCFG_TLSF_SL_INDEX_COUNT_LOG2 (3) /**< TLSF 二级索引数量的 log2，二级链表数为 2 的该值次方 */
#define EKCFG_LOG_DEBUG                (1) /**< 启用 DEBUG 级别日志 */
#define EKCFG_LOG_COLOR                (0) /**< 启用 ANSI 彩色日志 */
#define EKCFG_LOG_BUF_SIZE             (128) /**< 日志缓冲区大小（字节） */
#define EKCFG_ASSERT_TINY              (1) /**< 使用轻量级断言模式 */
#define EKCFG_ASSERT_LOG               (1) /**< 断言失败时输出日志 */
#define EKCFG_EVOKE_MIN_DEEPSLEEP_TICK (10) /**< 进入深度睡眠最小 tick */
#define EKCFG_PT_PRIO_LOWEST           (31) /**< 微线程最低优先级，就绪队列档位数为该值 + 1 */
#define EKCFG_VFS_MAX_FDS              (16) /**< VFS 最多可以打开的文件数目 */
#define EKCFG_VFS_MAX_NODES            (30) /**< VFS 最大节点数量 */
#define EKCFG_VFS_NODE_NAME_LEN        (12) /**< VFS 节点的最大名称长度 */
#define EKCFG_VFS_TREE_DUMP_UTF8       (0) /**< VFS 树输出时候是启用 UTF-8 线条支持 */

#endif /* EK_CONF_H */
