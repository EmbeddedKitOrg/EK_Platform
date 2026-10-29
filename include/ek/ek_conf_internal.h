/**
 * @file ek_conf_internal.h
 * @brief EmbeddedKit 内部配置入口——库内所有模块均通过此文件获取配置
 * @author N1netyNine99
 *
 * 本文件负责：
 *   1. 引入用户提供的 ek_conf.h
 *   2. 为用户未定义的 EKCFG_* 宏填充默认值
 *   3. 执行配置依赖校验
 *
 * 用户无需直接 include 或修改此文件。
 * 只需在自己的项目中创建 ek_conf.h（复制仓库根目录的 ek_conf_template.h 并改名），
 * 覆盖所需的宏，其余自动取默认值。
 *
 * 指定用户 ek_conf.h 的两种方式：
 *   A. CMake 参数：cmake -DEK_CONF_PATH="path/to/ek_conf.h"
 *   B. include 优先级：确保你的 ek_conf.h 所在路径先于 ek_utils/inc/ 被搜索到
 */

#ifndef EK_CONF_INTERNAL_H
#define EK_CONF_INTERNAL_H

/*
 * ========================================================================
 * 引入用户配置
 * ========================================================================
 * 方式 A：CMake 传 -DEK_CONF_PATH="xxx" → 编译期宏展开为 include 路径
 * 方式 B：未设置 EK_CONF_PATH → 使用标准 include 搜索，需保证用户路径优先
 */
#ifdef EK_CONF_PATH
#    include EK_CONF_PATH
#else
#    include "ek_conf.h"
#endif

/* ========================================================================
 * 平台/运行环境
 * ======================================================================== */

#ifndef EKCFG_RTOS
#    define EKCFG_RTOS (0) /**< 是否使用 RTOS (0=裸机) */
#endif
#ifndef EKCFG_PICOLIBC
#    define EKCFG_PICOLIBC (1) /**< 是否使用 picolibc 替代标准 libc */
#endif
#ifndef EKCFG_IO_LWPRTF
#    define EKCFG_IO_LWPRTF (0) /**< IO 是否使用 lwprintf (1=使用, 0=不使用) */
#endif
#ifndef EKCFG_NEWLIB
#    define EKCFG_NEWLIB (0) /**< 是否启用 newlib/nanolibc syscall port */
#endif
#ifndef EKCFG_STATIC_ALLOC
#    define EKCFG_STATIC_ALLOC (0) /**< 是否启用 EK_DEFINE_* 静态对象注册 (1=使用, 0=不使用);静态初始化函数始终可用 */
#endif
/* ========================================================================
 * 锁抽象层 — RTOS 线程安全
 * ========================================================================
 * 裸机模式下所有宏为空操作；RTOS 模式下用户在 ek_conf.h 中定义：
 *   #include "FreeRTOS.h"
 *   #include "semphr.h"
 *   #define EK_LOCK_TYPE         SemaphoreHandle_t
 *   #define EK_LOCK_INIT(lock)   ((lock) = xSemaphoreCreateMutex())
 *   #define EK_LOCK_DEINIT(lock) vSemaphoreDelete(lock)
 *   #define EK_LOCK_TRY(obj)     (xSemaphoreTake((obj)->lock, 0) == pdTRUE)
 *   #define EK_LOCK_ACQUIRE(obj) (xSemaphoreTake((obj)->lock, portMAX_DELAY) == pdTRUE)
 *   #define EK_LOCK_RELEASE(obj) xSemaphoreGive((obj)->lock)
 * 因 ek_conf.h 在本文件 #include 阶段已被引入，用户定义优先于以下默认值。
 */
#if EKCFG_RTOS == 1
#    ifndef EK_LOCK_TYPE
#        error "EKCFG_RTOS == 1: EK_LOCK_TYPE must be defined in ek_conf.h"
#    endif
#    ifndef EK_LOCK_INIT
#        error "EKCFG_RTOS == 1: EK_LOCK_INIT must be defined in ek_conf.h"
#    endif
#    ifndef EK_LOCK_DEINIT
#        error "EKCFG_RTOS == 1: EK_LOCK_DEINIT must be defined in ek_conf.h"
#    endif
#    ifndef EK_LOCK_TRY
#        error "EKCFG_RTOS == 1: EK_LOCK_TRY must be defined in ek_conf.h"
#    endif
#    ifndef EK_LOCK_ACQUIRE
#        error "EKCFG_RTOS == 1: EK_LOCK_ACQUIRE must be defined in ek_conf.h"
#    endif
#    ifndef EK_LOCK_RELEASE
#        error "EKCFG_RTOS == 1: EK_LOCK_RELEASE must be defined in ek_conf.h"
#    endif
#endif

#ifndef EK_LOCK_TYPE
#    define EK_LOCK_TYPE int
#endif
#ifndef EK_LOCK_INIT
#    define EK_LOCK_INIT(lock) (void)(lock)
#endif
#ifndef EK_LOCK_DEINIT
#    define EK_LOCK_DEINIT(lock) (void)(lock)
#endif
#ifndef EK_LOCK_TRY
#    define EK_LOCK_TRY(obj) (1)
#endif
#ifndef EK_LOCK_ACQUIRE
#    define EK_LOCK_ACQUIRE(obj) (void)(obj)
#endif
#ifndef EK_LOCK_RELEASE
#    define EK_LOCK_RELEASE(obj) (void)(obj)
#endif

/* ========================================================================
 * 核心服务开关
 * ======================================================================== */

#ifndef EKCFG_EXPORT
#    define EKCFG_EXPORT (0) /**< 自动初始化机制 */
#endif
#ifndef EKCFG_ASSERT
#    define EKCFG_ASSERT (1) /**< 断言模块 */
#endif
#ifndef EKCFG_LOG
#    define EKCFG_LOG (1) /**< 日志模块 */
#endif

/* ========================================================================
 * 数据结构开关
 * ======================================================================== */

#ifndef EKCFG_STR
#    define EKCFG_STR (0) /**< 动态字符串 */
#endif
#ifndef EKCFG_STR_VIEW
#    define EKCFG_STR_VIEW (0) /**< 字符串视图 */
#endif
#ifndef EKCFG_LIST
#    define EKCFG_LIST (0) /**< 双向循环链表 */
#endif
#ifndef EKCFG_VEC
#    define EKCFG_VEC (0) /**< 动态数组 */
#endif
#ifndef EKCFG_RINGBUF
#    define EKCFG_RINGBUF (0) /**< 通用环形缓冲区 */
#endif
#ifndef EKCFG_RINGBUF_SPSC
#    define EKCFG_RINGBUF_SPSC (0) /**< SPSC 无锁环形缓冲区 */
#endif
#ifndef EKCFG_STACK
#    define EKCFG_STACK (0) /**< 通用栈 */
#endif
#ifndef EKCFG_SNAPSHOT
#    define EKCFG_SNAPSHOT (0) /**< 快照 */
#endif
#ifndef EKCFG_EVOKE
#    define EKCFG_EVOKE (0) /**< 事件驱动调度器 */
#endif
#ifndef EKCFG_PICOTHREAD
#    define EKCFG_PICOTHREAD (0) /**< 微线程 */
#endif
#ifndef EKCFG_PICOTHREAD_SEM
#    define EKCFG_PICOTHREAD_SEM (0) /**< 微线程信号量 */
#endif
#ifndef EKCFG_PICOTHREAD_MSG
#    define EKCFG_PICOTHREAD_MSG (0) /**< 微线程消息队列 */
#endif

/* ========================================================================
 * 模块子配置
 * ======================================================================== */

#ifndef EKCFG_HEAP_TLSF
#    define EKCFG_HEAP_TLSF (1) /**< 内存堆使用 TLSF 分配器 (1=TLSF, 0=自定义) */
#endif
#ifndef EKCFG_HEAP_SIZE
#    define EKCFG_HEAP_SIZE (30 * 1024) /**< 内存堆大小（字节） */
#endif
#ifndef EKCFG_TLSF_FL_INDEX_MAX
#    define EKCFG_TLSF_FL_INDEX_MAX (24) /**< TLSF 一级索引最大值，最大连续块为 2 的该值次方字节 */
#endif
#ifndef EKCFG_TLSF_SL_INDEX_COUNT_LOG2
#    define EKCFG_TLSF_SL_INDEX_COUNT_LOG2 (3) /**< TLSF 二级索引数量的 log2，二级链表数为 2 的该值次方 */
#endif
#ifndef EKCFG_LOG_DEBUG
#    define EKCFG_LOG_DEBUG (1) /**< 启用 DEBUG 级别日志 */
#endif
#ifndef EKCFG_LOG_COLOR
#    define EKCFG_LOG_COLOR (1) /**< 启用 ANSI 彩色日志 */
#endif
#ifndef EKCFG_LOG_BUF_SIZE
#    define EKCFG_LOG_BUF_SIZE (256) /**< 日志缓冲区大小（字节） */
#endif
#ifndef EKCFG_ASSERT_TINY
#    define EKCFG_ASSERT_TINY (1) /**< 使用轻量级断言模式 */
#endif
#ifndef EKCFG_ASSERT_LOG
#    define EKCFG_ASSERT_LOG (1) /**< 断言失败时输出日志 */
#endif
#ifndef EKCFG_EVOKE_MIN_DEEPSLEEP_TICK
#    define EKCFG_EVOKE_MIN_DEEPSLEEP_TICK (10) /**< 进入深度睡眠最小tick */
#endif
#ifndef EKCFG_PT_PRIO_LOWEST
#    define EKCFG_PT_PRIO_LOWEST (31) /**< 微线程最低优先级，就绪队列档位数为该值 + 1 */
#endif
#ifndef EKCFG_VFS_MAX_NODES
#    define EKCFG_VFS_MAX_NODES (30) /**< VFS 最大节点数量 */
#endif
#ifndef EKCFG_VFS_NODE_NAME_LEN
#    define EKCFG_VFS_NODE_NAME_LEN (12) /**< VFS 节点的最大名称长度 */
#endif
#ifndef EKCFG_VFS_TREE_DUMP_UTF8
#    define EKCFG_VFS_TREE_DUMP_UTF8 (0) /**< VFS 树输出时候是启用 UTF-8 线条支持 */
#endif

/* ========================================================================
 * 配置依赖校验
 * ======================================================================== */

// DEPEND_ON 为真表示依赖满足：module 未开启，或 dependency 已开启。
#define DEPEND_ON(module, dependency) (!(module) || (dependency))
// MUTUAL_EXCLUSION 为真表示互斥，为假为不互斥。
#define MUTUAL_EXCLUSION(module1, module2) ((module1) && (module2))

#if EKCFG_PICOLIBC == 1
#    undef EKCFG_IO_LWPRTF
#    define EKCFG_IO_LWPRTF (0) /* picolibc 接管 printf，关闭 lwprintf */
#endif

#if MUTUAL_EXCLUSION(EKCFG_PICOLIBC, EKCFG_NEWLIB)
#    error "EKCFG_PICOLIBC and EKCFG_NEWLIB are mutually exclusive"
#endif
#if MUTUAL_EXCLUSION(EKCFG_EVOKE, EKCFG_RTOS)
#    error "EKCFG_EVOKE and EKCFG_RTOS are mutually exclusive"
#endif

#if !DEPEND_ON(EKCFG_PICOTHREAD_SEM, EKCFG_PICOTHREAD)
#    error "EKCFG_PICOTHREAD_SEM requires EKCFG_PICOTHREAD=1"
#endif
#if !DEPEND_ON(EKCFG_PICOTHREAD_MSG, EKCFG_PICOTHREAD)
#    error "EKCFG_PICOTHREAD_MSG requires EKCFG_PICOTHREAD=1"
#endif
#if !DEPEND_ON(EKCFG_PICOTHREAD, EKCFG_LIST)
#    error "EKCFG_PICOTHREAD requires EKCFG_LIST=1"
#endif
#if !DEPEND_ON(EKCFG_PICOTHREAD_MSG, EKCFG_RINGBUF)
#    error "EKCFG_PICOTHREAD_MSG requires EKCFG_RINGBUF=1"
#endif
#if !DEPEND_ON(EKCFG_EVOKE, EKCFG_LIST)
#    error "EKCFG_EVOKE requires EKCFG_LIST=1"
#endif
#if !DEPEND_ON(EKCFG_EVOKE, EKCFG_RINGBUF_SPSC)
#    error "EKCFG_EVOKE requires EKCFG_RINGBUF_SPSC=1"
#endif

#if EKCFG_PT_PRIO_LOWEST > 31
#    error "EKCFG_PT_PRIO_LOWEST must be <= 31 (ready mask is 32-bit)"
#endif

#if EKCFG_VFS_NODE_NAME_LEN % 4
#    error "EKCFG_VFS_NODE_NAME_LEN must be 4-byte aligned (multiple of 4)"
#endif

#undef DEPEND_ON
#undef MUTUAL_EXCLUSION

#endif /* EK_CONF_INTERNAL_H */
