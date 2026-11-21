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
 * 平台 / 运行环境
 * ======================================================================== */

/* EKCFG_PICOLIBC / EKCFG_NEWLIB 由 CMake 的 PICOLIBC 选项派生。
 * 裸机 newlib 需要 syscall port 时，在项目 ek_conf.h 中定义 EKCFG_NEWLIB 1。 */
#define EKCFG_IO_LWPRTF    (1) /**< IO 是否使用 lwprintf (1=使用, 0=不使用) */
#define EKCFG_STATIC_ALLOC (1) /**< 是否启用 EK_DEFINE_* 静态对象注册 (1=使用, 0=不使用);静态初始化函数始终可用 */

/* ========================================================================
 * Arch 架构模块
 * ======================================================================== */

#define EKCFG_ARCH (1) /**< 硬件架构抽象层 */
#if EKCFG_ARCH == 1
/* ARM 目标必须配置外部 IRQ 数量（IRQ0..N-1）。
 * 通用启动文件按此生成向量表，该宏会由汇编器直接求值，
 * 必须写成 (91) 这类纯整数表达式，不要加 U 后缀。 */
#    define EKCFG_ARCH_IRQ_COUNT /**< ARM 外部 IRQ 数量，通用启动文件按此生成向量表 */
#endif

/* ========================================================================
 * Export 自动初始化模块
 * ======================================================================== */

#define EKCFG_EXPORT (1) /**< 自动初始化机制 */

/* ========================================================================
 * Assert 断言模块
 * ======================================================================== */

#define EKCFG_ASSERT (1) /**< 断言模块（删去则跟随 Debug/Release 自动开关） */
#if EKCFG_ASSERT == 1
#    define EKCFG_ASSERT_TINY (1) /**< 使用轻量级断言模式（删去则 Debug=0 / Release=1） */
#    define EKCFG_ASSERT_LOG  (1) /**< 断言失败时输出日志（删去则跟随 Debug/Release） */
#endif

/* ========================================================================
 * Log 日志模块
 * ======================================================================== */

#define EKCFG_LOG (1) /**< 日志模块 */
#if EKCFG_LOG == 1
#    define EKCFG_LOG_DEBUG    (1) /**< 启用 DEBUG 级别日志（删去则跟随 Debug/Release） */
#    define EKCFG_LOG_COLOR    (0) /**< 启用 ANSI 彩色日志 */
#    define EKCFG_LOG_BUF_SIZE (128) /**< 日志缓冲区大小（字节） */
#endif

/* ========================================================================
 * Heap 内存堆模块
 * ======================================================================== */

#define EKCFG_HEAP_TLSF (1) /**< 内存堆使用 TLSF 分配器 (1=TLSF, 0=自定义) */
#if EKCFG_HEAP_TLSF == 1
#    define EKCFG_HEAP_SECTION ".heap" /**< 堆内存所在链接器段 (如 ".tcmram") */
#    define EKCFG_HEAP_SIZE    (16 * 1024) /**< 内存堆大小（字节） */
/* 以下参数未定义时由 conf_internal.h 按堆大小自动推导，通常无需手写 */
/* #    define EKCFG_HEAP_MAX_POOL_SIZE       (32 * 1024 * 1024) */ /**< 多内存池时的最大单池字节数 */
/* #    define EKCFG_TLSF_FL_INDEX_MAX        (24) */ /**< TLSF 一级索引最大值，最大连续块为 2 的该值次方字节 */
/* #    define EKCFG_TLSF_SL_INDEX_COUNT_LOG2 (3) */ /**< TLSF 二级索引数量的 log2，二级链表数为 2 的该值次方 */
#endif

/* ========================================================================
 * 容器与工具模块
 * ======================================================================== */

#define EKCFG_STR          (0) /**< 动态字符串 */
#define EKCFG_STR_VIEW     (0) /**< 字符视图 */
#define EKCFG_LIST         (0) /**< 双向循环链表 */
#define EKCFG_VEC          (0) /**< 动态数组 */
#define EKCFG_RINGBUF      (0) /**< 通用环形缓冲区 */
#define EKCFG_RINGBUF_SPSC (0) /**< SPSC 无锁环形缓冲区 */
#define EKCFG_STACK        (0) /**< 通用栈 */
#define EKCFG_BITSTREAM    (0) /**< 比特流读写 */
#define EKCFG_SNAPSHOT     (0) /**< 快照 */
#define EKCFG_SHARED_PTR   (0) /**< 共享指针 */

/* ========================================================================
 * Evoke 事件调度器
 * ======================================================================== */

#define EKCFG_EVOKE (0) /**< 事件驱动调度器 (依赖 LIST / RINGBUF_SPSC，与 RTOS 互斥) */
#if EKCFG_EVOKE == 1
#    define EKCFG_EVOKE_MIN_DEEPSLEEP_TICK (10) /**< 进入深度睡眠最小 tick */
#endif

/* ========================================================================
 * Picothread 微线程
 * ======================================================================== */

#define EKCFG_PICOTHREAD (0) /**< 微线程 (依赖 LIST) */
#if EKCFG_PICOTHREAD == 1
#    define EKCFG_PICOTHREAD_SEM (0) /**< 微线程信号量 */
#    define EKCFG_PICOTHREAD_MSG (0) /**< 微线程消息队列 (依赖 RINGBUF) */
#    define EKCFG_PT_PRIO_LOWEST (31) /**< 微线程最低优先级，就绪队列档位数为该值 + 1 */
#endif

/* ========================================================================
 * VFS 虚拟文件系统
 * ======================================================================== */

#define EKCFG_VFS (0) /**< VFS 虚拟文件系统 */
#if EKCFG_VFS == 1
#    define EKCFG_VFS_MAX_FDS        (16) /**< VFS 最多可以打开的文件数目 */
#    define EKCFG_VFS_MAX_NODES      (30) /**< VFS 最大节点数量 */
#    define EKCFG_VFS_NAME_LEN       (12) /**< VFS 节点的最大名称长度 (须为 4 的倍数) */
#    define EKCFG_VFS_TREE_DUMP_UTF8 (0) /**< VFS 树输出是否启用 UTF-8 线条支持 */
#endif

/* ========================================================================
 * Shell 模块
 * ======================================================================== */

#define EKCFG_SHELL (0) /**< Shell 子模块 (依赖 VFS) */
#if EKCFG_SHELL == 1
#    define EKCFG_SHELL_BUF_SIZE     (256) /**< Shell 的缓冲区大小（字节） */
#    define EKCFG_SHELL_PROMPT       "ek>" /**< Shell 提示符 */
#    define EKCFG_SHELL_CMD_MAX_ARGS (8) /**< Shell 命令中最大参数数量 */
#    define EKCFG_SHELL_UTF8         (0) /**< Shell 支持 UTF-8 处理 */
#    define EKCFG_SHELL_BUILTIN_CMDS (0) /**< 启动几个默认的 Shell 命令 */
#endif

/* ========================================================================
 * OS / RTOS 内核模块
 * ======================================================================== */

#define EKCFG_RTOS (0) /**< 是否启用 RTOS (0=裸机 1=内置 RTOS) */
#if EKCFG_RTOS == 1
#    define EKCFG_OS_KERNEL_TIMEBASE_HZ (1000000U) /**< OS 内核频率 */
#    define EKCFG_OS_THREAD_NAME_LEN    (16) /**< OS 线程名字的最大长度 */
#    define EKCFG_OS_THREADS_MAX        (16) /**< OS 最大线程数 (<= 64) */
#    define EKCFG_OS_THREAD_MIN_STACK   (256) /**< OS 线程最小栈大小（字节） */
#    define EKCFG_OS_THREAD_MIN_PRIO    (31U) /**< OS 线程最低优先级 (< 32) */
#    define EKCFG_OS_IDLE_THREAD_STACK  (512) /**< OS 空闲线程栈大小（字节） */
#endif

#endif /* EK_CONF_H */
