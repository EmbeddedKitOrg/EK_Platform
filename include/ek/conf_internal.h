/**
 * @file conf_internal.h
 * @brief EmbeddedKit 内部配置入口——库内所有模块均通过此文件获取配置
 * @author N1netyNine99
 *
 * 本文件负责：
 *   1. 引入用户提供的 ek_conf.h
 *   2. 按模块为用户未定义的 EKCFG_* 宏填充默认值
 *   3. 执行单模块参数校验与跨模块依赖校验
 *
 * 用户无需直接 include 或修改此文件。
 * 只需在自己的项目中创建 ek_conf.h（复制仓库根目录的 ek_conf_template.h 并改名），
 * 覆盖所需的宏，其余自动取默认值。
 *
 * 指定用户 ek_conf.h：
 *   CMake 必须定义 EK_CONF_PATH；该宏只用于校验，
 *   实际包含通过 include 路径中的 "ek_conf.h" 完成。
 */

#ifndef EK_CONF_INTERNAL_H
#define EK_CONF_INTERNAL_H

/*
 * ========================================================================
 * 引入用户配置
 * ========================================================================
 * EK_CONF_PATH 只做存在性校验；ek_conf.h 由 include 路径提供。
 */
#ifndef EK_CONF_PATH
#    error "EK_CONF_PATH is not set: add ek_conf.h to the project and pass its path via CMake CONF"
#endif

#ifndef __ASSEMBLER__
#    include <stdint.h>
#endif
#include "ek_conf.h"

/* CMake 的 PICOLIBC 选项是构建事实，优先于 ek_conf.h 中的旧定义。 */
#ifdef EK_PLATFORM_PICOLIBC
#    undef EKCFG_PICOLIBC
#    define EKCFG_PICOLIBC EK_PLATFORM_PICOLIBC
#    if EKCFG_PICOLIBC == 1
#        undef EKCFG_NEWLIB
#        define EKCFG_NEWLIB 0
#    endif
#endif

/* CMake 的构建模式是构建事实，优先于 ek_conf.h 或默认兜底。 */
#ifdef EK_PLATFORM_DEBUG
#    undef EKCFG_DEBUG
#    define EKCFG_DEBUG EK_PLATFORM_DEBUG
#elif !defined(EKCFG_DEBUG)
#    define EKCFG_DEBUG (1) /**< 构建模式 (1=Debug, 0=Release) */
#endif
#if (EKCFG_DEBUG != 0) && (EKCFG_DEBUG != 1)
#    error "EKCFG_DEBUG must be 0 or 1"
#endif
#define EKCFG_RELEASE (!(EKCFG_DEBUG))

/* ========================================================================
 * 平台 / 运行环境
 * ======================================================================== */

#ifndef EKCFG_PICOLIBC
#    define EKCFG_PICOLIBC (1) /**< 是否使用 picolibc 替代标准 libc */
#endif
#ifndef EKCFG_NEWLIB
#    define EKCFG_NEWLIB (0) /**< 是否启用 newlib/nanolibc syscall port */
#endif
#ifndef EKCFG_IO_LWPRTF
#    define EKCFG_IO_LWPRTF (0) /**< IO 是否使用 lwprintf (1=使用, 0=不使用) */
#endif
#if EKCFG_PICOLIBC == 1
#    undef EKCFG_IO_LWPRTF
#    define EKCFG_IO_LWPRTF (0) /* picolibc 接管 printf，关闭 lwprintf */
#endif
#ifndef EKCFG_STATIC_ALLOC
#    define EKCFG_STATIC_ALLOC \
        (1) /**< 是否启用 EK_DEFINE_* 静态对象注册 (1=启用, 0=禁用；各模块的手动静态初始化函数始终可用) */
#endif

/* ========================================================================
 * Arch 架构模块
 * ======================================================================== */

#ifndef EKCFG_ARCH
#    define EKCFG_ARCH (1) /**< 硬件架构抽象层 */
#endif
#if EKCFG_ARCH == 1
#    if defined(__arm__) || defined(__thumb__)
#        ifndef EKCFG_ARCH_IRQ_COUNT
#            error "EKCFG_ARCH_IRQ_COUNT must be defined in ek_conf.h for this MCU"
#        endif
#        if EKCFG_ARCH_IRQ_COUNT < 1
#            error "EKCFG_ARCH_IRQ_COUNT must be >= 1"
#        endif
#        if EKCFG_ARCH_IRQ_COUNT > 240
#            error "EKCFG_ARCH_IRQ_COUNT must be <= 240 for ARMv7-M NVIC"
#        endif
#    endif
#endif

/* ========================================================================
 * Export 自动初始化模块
 * ======================================================================== */

#ifndef EKCFG_EXPORT
#    define EKCFG_EXPORT (1) /**< 自动初始化机制 */
#endif

/* ========================================================================
 * Assert 断言模块
 * ======================================================================== */

#ifndef EKCFG_ASSERT
#    define EKCFG_ASSERT EKCFG_DEBUG /**< 断言模块（未显式配置时默认跟随 Debug 模式） */
#endif
#if EKCFG_ASSERT == 1
#    ifndef EKCFG_ASSERT_TINY
#        define EKCFG_ASSERT_TINY EKCFG_RELEASE /**< 轻量级断言（Debug 用完整断言，Release 用轻量版） */
#    endif
#    ifndef EKCFG_ASSERT_LOG
#        define EKCFG_ASSERT_LOG EKCFG_DEBUG /**< 断言失败时输出日志 */
#    endif
#endif

/* ========================================================================
 * Log 日志模块
 * ======================================================================== */

#ifndef EKCFG_LOG
#    define EKCFG_LOG (1) /**< 日志模块 */
#endif
#if EKCFG_LOG == 1
#    ifndef EKCFG_LOG_DEBUG
#        define EKCFG_LOG_DEBUG EKCFG_DEBUG /**< 启用 DEBUG 级别日志（默认跟随 Debug 模式） */
#    endif
#    ifndef EKCFG_LOG_COLOR
#        define EKCFG_LOG_COLOR (1) /**< 启用 ANSI 彩色日志 */
#    endif
#    ifndef EKCFG_LOG_BUF_SIZE
#        define EKCFG_LOG_BUF_SIZE (256) /**< 日志缓冲区大小（字节） */
#    endif
#endif

/* ========================================================================
 * Heap 内存堆模块
 * ======================================================================== */

#ifndef EKCFG_HEAP_TLSF
#    define EKCFG_HEAP_TLSF (1) /**< 内存堆使用 TLSF 分配器 (1=TLSF, 0=自定义) */
#endif
#if EKCFG_HEAP_TLSF == 1
/* EKCFG_HEAP_SECTION 为可选宏：定义后将默认堆放入指定链接器段 */
#    ifndef EKCFG_HEAP_SIZE
#        define EKCFG_HEAP_SIZE (30 * 1024) /**< 内存堆大小（字节） */
#    endif
#    ifndef EKCFG_HEAP_MAX_POOL_SIZE
#        define EKCFG_HEAP_MAX_POOL_SIZE EKCFG_HEAP_SIZE /**< 单个内存池的最大容量（字节），多池时可显式覆盖 */
#    endif

#    define _EK_TLSF_FL_MAX_AUTO(sz) \
        ((sz) <= (1UL << 10)   ? 10  \
         : (sz) <= (1UL << 11) ? 11  \
         : (sz) <= (1UL << 12) ? 12  \
         : (sz) <= (1UL << 13) ? 13  \
         : (sz) <= (1UL << 14) ? 14  \
         : (sz) <= (1UL << 15) ? 15  \
         : (sz) <= (1UL << 16) ? 16  \
         : (sz) <= (1UL << 17) ? 17  \
         : (sz) <= (1UL << 18) ? 18  \
         : (sz) <= (1UL << 19) ? 19  \
         : (sz) <= (1UL << 20) ? 20  \
         : (sz) <= (1UL << 21) ? 21  \
         : (sz) <= (1UL << 22) ? 22  \
         : (sz) <= (1UL << 23) ? 23  \
         : (sz) <= (1UL << 24) ? 24  \
         : (sz) <= (1UL << 25) ? 25  \
         : (sz) <= (1UL << 26) ? 26  \
         : (sz) <= (1UL << 27) ? 27  \
         : (sz) <= (1UL << 28) ? 28  \
         : (sz) <= (1UL << 29) ? 29  \
                               : 30)

#    define _EK_TLSF_SL_LOG2_AUTO(sz) \
        ((sz) < (16UL * 1024UL) ? 2 : (sz) < (64UL * 1024UL) ? 3 : (sz) < (256UL * 1024UL) ? 4 : 5)

#    ifndef EKCFG_TLSF_FL_INDEX_MAX
#        define EKCFG_TLSF_FL_INDEX_MAX _EK_TLSF_FL_MAX_AUTO(EKCFG_HEAP_MAX_POOL_SIZE) /**< TLSF 一级索引最大值 */
#    endif
#    ifndef EKCFG_TLSF_SL_INDEX_COUNT_LOG2
#        define EKCFG_TLSF_SL_INDEX_COUNT_LOG2 _EK_TLSF_SL_LOG2_AUTO(EKCFG_HEAP_SIZE) /**< TLSF 二级索引数量的 log2 */
#    endif

#    if (EKCFG_TLSF_SL_INDEX_COUNT_LOG2) < 1 || (EKCFG_TLSF_SL_INDEX_COUNT_LOG2) > 5
#        error "EKCFG_TLSF_SL_INDEX_COUNT_LOG2 must be in [1, 5]"
#    endif
#    if (EKCFG_TLSF_FL_INDEX_MAX) < 10 || (EKCFG_TLSF_FL_INDEX_MAX) > 30
#        error "EKCFG_TLSF_FL_INDEX_MAX must be in [10, 30]"
#    endif
#    if (EKCFG_HEAP_MAX_POOL_SIZE) < (EKCFG_HEAP_SIZE)
#        error "EKCFG_HEAP_MAX_POOL_SIZE must be >= EKCFG_HEAP_SIZE"
#    endif
#    if (1UL << (EKCFG_TLSF_FL_INDEX_MAX)) < (EKCFG_HEAP_MAX_POOL_SIZE)
#        error "1UL << EKCFG_TLSF_FL_INDEX_MAX must cover EKCFG_HEAP_MAX_POOL_SIZE"
#    endif
#endif

/* ========================================================================
 * 容器与工具模块
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
#ifndef EKCFG_BITSTREAM
#    define EKCFG_BITSTREAM (0) /**< 比特流读写 */
#endif
#ifndef EKCFG_SNAPSHOT
#    define EKCFG_SNAPSHOT (0) /**< 快照 */
#endif
#ifndef EKCFG_SHARED_PTR
#    define EKCFG_SHARED_PTR (0) /**< 共享指针 */
#endif

/* ========================================================================
 * Evoke 事件调度器
 * ======================================================================== */

#ifndef EKCFG_EVOKE
#    define EKCFG_EVOKE (0) /**< 事件驱动调度器 */
#endif
#if EKCFG_EVOKE == 1
#    ifndef EKCFG_EVOKE_MIN_DEEPSLEEP_TICK
#        define EKCFG_EVOKE_MIN_DEEPSLEEP_TICK (10) /**< 进入深度睡眠最小 tick */
#    endif
#endif

/* ========================================================================
 * Picothread 微线程
 * ======================================================================== */

#ifndef EKCFG_PICOTHREAD
#    define EKCFG_PICOTHREAD (0) /**< 微线程 */
#endif
#if EKCFG_PICOTHREAD == 1
#    ifndef EKCFG_PICOTHREAD_SEM
#        define EKCFG_PICOTHREAD_SEM (0) /**< 微线程信号量 */
#    endif
#    ifndef EKCFG_PICOTHREAD_MSG
#        define EKCFG_PICOTHREAD_MSG (0) /**< 微线程消息队列 */
#    endif
#    ifndef EKCFG_PT_PRIO_LOWEST
#        define EKCFG_PT_PRIO_LOWEST (31) /**< 微线程最低优先级，就绪队列档位数为该值 + 1 */
#    endif
#    if EKCFG_PT_PRIO_LOWEST > 31
#        error "EKCFG_PT_PRIO_LOWEST must be <= 31 (ready mask is 32-bit)"
#    endif
#endif

/* ========================================================================
 * VFS 虚拟文件系统
 * ======================================================================== */

#ifndef EKCFG_VFS
#    define EKCFG_VFS (0) /**< VFS 虚拟文件系统 */
#endif
#if EKCFG_VFS == 1
#    ifndef EKCFG_VFS_MAX_FDS
#        define EKCFG_VFS_MAX_FDS (16) /**< VFS 最多可以打开的文件数目 */
#    endif
#    ifndef EKCFG_VFS_MAX_NODES
#        define EKCFG_VFS_MAX_NODES (30) /**< VFS 最大节点数量 */
#    endif
#    ifndef EKCFG_VFS_NAME_LEN
#        define EKCFG_VFS_NAME_LEN (12) /**< VFS 节点的最大名称长度 */
#    endif
#    ifndef EKCFG_VFS_TREE_DUMP_UTF8
#        define EKCFG_VFS_TREE_DUMP_UTF8 (0) /**< VFS 树输出是否启用 UTF-8 线条支持 */
#    endif
#    if EKCFG_VFS_NAME_LEN % 4
#        error "EKCFG_VFS_NAME_LEN must be 4-byte aligned (multiple of 4)"
#    endif
#    if EKCFG_VFS_MAX_FDS > 32
#        error "EKCFG_VFS_MAX_FDS must be <= 32"
#    endif
#endif

/* ========================================================================
 * Shell 模块
 * ======================================================================== */

#ifndef EKCFG_SHELL
#    define EKCFG_SHELL (0) /**< Shell 子模块 */
#endif
#if EKCFG_SHELL == 1
#    ifndef EKCFG_SHELL_BUF_SIZE
#        define EKCFG_SHELL_BUF_SIZE (64) /**< Shell 的缓冲区大小（字节） */
#    endif
#    ifndef EKCFG_SHELL_PROMPT
#        define EKCFG_SHELL_PROMPT "ek>" /**< Shell 提示符 */
#    endif
#    ifndef EKCFG_SHELL_CMD_MAX_ARGS
#        define EKCFG_SHELL_CMD_MAX_ARGS (8) /**< Shell 命令中最大参数数量 */
#    endif
#    ifndef EKCFG_SHELL_UTF8
#        define EKCFG_SHELL_UTF8 (0) /**< Shell 支持 UTF-8 处理 */
#    endif
#    ifndef EKCFG_SHELL_BUILTIN_CMDS
#        define EKCFG_SHELL_BUILTIN_CMDS (0) /**< 是否启用内置默认 Shell 命令 (1=启用, 0=禁用) */
#    endif
#endif

/* ========================================================================
 * OS / RTOS 内核模块
 * ======================================================================== */

#ifndef EKCFG_RTOS
#    define EKCFG_RTOS (0) /**< 是否使用 RTOS (0=裸机 1=内置 RTOS) */
#endif
#if EKCFG_RTOS == 1
#    ifndef EKCFG_OS_KERNEL_TIMEBASE_HZ
#        define EKCFG_OS_KERNEL_TIMEBASE_HZ (1000000U) /**< OS 内核时基频率（Hz） */
#    endif
#    ifndef EKCFG_OS_KERNEL_STACKOVERFLOW_CHECK
#        define EKCFG_OS_KERNEL_STACKOVERFLOW_CHECK (0U) /**< 0=关闭 1=边界指针检查 2=魔数染色 + 高水位线  */
#    endif
#    ifndef EKCFG_OS_THREAD_NAME_LEN
#        define EKCFG_OS_THREAD_NAME_LEN (16) /**< OS 线程名字的最大长度 */
#    endif
#    ifndef EKCFG_OS_THREADS_MAX
#        define EKCFG_OS_THREADS_MAX (16) /**< OS 最大线程数 */
#    endif
#    ifndef EKCFG_OS_THREAD_MIN_STACK
#        define EKCFG_OS_THREAD_MIN_STACK (256) /**< OS 线程最小的栈大小（字节） */
#    endif
#    ifndef EKCFG_OS_THREAD_MIN_PRIO
#        define EKCFG_OS_THREAD_MIN_PRIO (31U) /**< OS 线程最低优先级，就绪队列档位数为该值 + 1 */
#    endif
#    ifndef EKCFG_OS_IDLE_THREAD_STACK
#        define EKCFG_OS_IDLE_THREAD_STACK (512) /**< OS 空闲线程的栈大小（字节） */
#    endif
#    ifndef EKCFG_OS_ISR_WORKQ
#        define EKCFG_OS_ISR_WORKQ (0) /**< 是否启用内核内置 ISR 工作队列 (0=关闭 1=启用) */
#    endif
#    if EKCFG_OS_ISR_WORKQ == 1
#        ifndef EKCFG_OS_ISR_WORKQ_PRIO
#            define EKCFG_OS_ISR_WORKQ_PRIO (0U) /**< 内核内置 ISR 工作队列线程优先级 */
#        endif
#        ifndef EKCFG_OS_ISR_WORKQ_STACK
#            define EKCFG_OS_ISR_WORKQ_STACK (1024U) /**< 内核内置 ISR 工作队列线程栈大小（字节） */
#        endif
#        if EKCFG_OS_ISR_WORKQ_PRIO > EKCFG_OS_THREAD_MIN_PRIO
#            error "EKCFG_OS_ISR_WORKQ_PRIO must be <= EKCFG_OS_THREAD_MIN_PRIO"
#        endif
#        if EKCFG_OS_ISR_WORKQ_STACK < EKCFG_OS_THREAD_MIN_STACK
#            error "EKCFG_OS_ISR_WORKQ_STACK must be >= EKCFG_OS_THREAD_MIN_STACK"
#        endif
#    endif
#    if EKCFG_OS_THREADS_MAX > 64
#        error "EKCFG_OS_THREADS_MAX must be <= 64"
#    endif
#    if EKCFG_OS_THREAD_MIN_PRIO > 31
#        error "EKCFG_OS_THREAD_MIN_PRIO must be <= 31"
#    endif
#    if EKCFG_OS_KERNEL_STACKOVERFLOW_CHECK > 2U
#        error "EKCFG_OS_KERNEL_STACKOVERFLOW_CHECK must be 0, 1, or 2"
#    endif
#endif

/* ========================================================================
 * 跨模块依赖与互斥校验
 * ======================================================================== */

// DEPEND_ON 为真表示依赖满足：module 未开启，或 dependency 已开启。
#define DEPEND_ON(module, dependency) (!(module) || (dependency))
// MUTUAL_EXCLUSION 为真表示两个互斥模块同时启用（冲突），为假表示未冲突。
#define MUTUAL_EXCLUSION(module1, module2) ((module1) && (module2))

#if MUTUAL_EXCLUSION(EKCFG_PICOLIBC, EKCFG_NEWLIB)
#    error "EKCFG_PICOLIBC and EKCFG_NEWLIB are mutually exclusive"
#endif
#if MUTUAL_EXCLUSION(EKCFG_EVOKE, EKCFG_RTOS)
#    error "EKCFG_EVOKE and EKCFG_RTOS are mutually exclusive"
#endif

#if !DEPEND_ON(EKCFG_EVOKE, EKCFG_LIST)
#    error "EKCFG_EVOKE requires EKCFG_LIST=1"
#endif
#if !DEPEND_ON(EKCFG_EVOKE, EKCFG_RINGBUF_SPSC)
#    error "EKCFG_EVOKE requires EKCFG_RINGBUF_SPSC=1"
#endif

#if !DEPEND_ON(EKCFG_PICOTHREAD, EKCFG_LIST)
#    error "EKCFG_PICOTHREAD requires EKCFG_LIST=1"
#endif
#if EKCFG_PICOTHREAD == 1
#    if !DEPEND_ON(EKCFG_PICOTHREAD_MSG, EKCFG_RINGBUF)
#        error "EKCFG_PICOTHREAD_MSG requires EKCFG_RINGBUF=1"
#    endif
#else
#    if defined(EKCFG_PICOTHREAD_SEM) && (EKCFG_PICOTHREAD_SEM)
#        error "EKCFG_PICOTHREAD_SEM requires EKCFG_PICOTHREAD=1"
#    endif
#    if defined(EKCFG_PICOTHREAD_MSG) && (EKCFG_PICOTHREAD_MSG)
#        error "EKCFG_PICOTHREAD_MSG requires EKCFG_PICOTHREAD=1"
#    endif
#endif

#if !DEPEND_ON(EKCFG_SHELL, EKCFG_VFS)
#    error "EKCFG_SHELL requires EKCFG_VFS=1"
#endif

#if !DEPEND_ON(EKCFG_RTOS, EKCFG_ARCH)
#    error "EKCFG_RTOS requires EKCFG_ARCH=1"
#endif
#if !DEPEND_ON(EKCFG_RTOS, EKCFG_LIST)
#    error "EKCFG_RTOS requires EKCFG_LIST=1"
#endif

#undef DEPEND_ON
#undef MUTUAL_EXCLUSION

#endif /* EK_CONF_INTERNAL_H */
