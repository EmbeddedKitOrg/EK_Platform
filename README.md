# EmbeddedKit (ek_utils)

面向资源受限 MCU 环境的轻量级嵌入式 C 工具库。提供数据结构、内存管理、日志、事件调度等裸机和 RTOS 开发的通用基础设施。

## 特性

- **零依赖**：无需 RTOS，可直接集成到裸机项目
- **配置驱动**：所有模块通过 `ek_conf.h` 宏开关按需裁剪，编译期零开销
- **跨编译器**：支持 GCC、ARM Compiler 5 (armcc)、ARM Compiler 6 (armclang)
- **弱函数扩展**：平台相关接口通过 `__EK_WEAK` 弱符号声明，用户提供强函数覆盖

## 项目结构

```
ek_utils/
├── cmake/                         # 交叉编译工具链
│   └── gcc-arm-none-eabi.cmake
├── CMakeLists.txt                 # 构建入口（对象库，子模块友好）
├── ek_conf_template.h             # 配置模板 → 复制到项目改名为 ek_conf.h
├── inc/                           # 头文件
│   ├── ek_conf_internal.h         # 内部配置入口（引入用户 ek_conf.h + 默认值 + 校验）
│   ├── ek_def.h                   # 跨编译器兼容层 + 错误传播宏（EK_ERR_RETURN / EK_ERR_GOTO）
│   ├── ek_assert.h                # 断言模块
│   ├── ek_log.h                   # 分级日志模块
│   ├── ek_io.h                    # IO 输出封装（lwprintf / picolibc / 标准 libc 直通）
│   ├── ek_heap.h                  # 内存堆管理（基于 TLSF）
│   ├── ek_list.h                  # 双向循环链表（纯头文件）
│   ├── ek_vec.h                   # 类型安全动态数组（纯头文件，宏生成类型）
│   ├── ek_ringbuf.h               # 环形缓冲区（通用 + SPSC 无锁变体）
│   ├── ek_stack.h                 # 通用 LIFO 栈
│   ├── ek_snapshot.h              # 快照数据（单槽覆盖写，整块读写）
│   ├── ek_str.h                   # 动态字符串
│   ├── ek_str_view.h              # 字符串视图
│   ├── ek_export.h                # 函数自动导出初始化（类似 Linux initcall）
│   ├── ek_static_alloc.h          # 静态对象自动注册与初始化
│   ├── ek_evoke.h                 # 协作式事件驱动任务调度器
│   └── ek_picothread.h            # 微线程 + 信号量 + 消息队列
├── src/                           # 源文件
│   ├── ek_assert.c
│   ├── ek_evoke.c
│   ├── ek_export.c
│   ├── ek_heap.c
│   ├── ek_io.c
│   ├── ek_log.c
│   ├── ek_picothread.c
│   ├── ek_picolibc_port.c          # picolibc 适配层
│   ├── ek_ringbuf.c
│   ├── ek_snapshot.c
│   ├── ek_stack.c
│   ├── ek_static_alloc.c
│   ├── ek_str.c
│   └── ek_str_view.c
├── third_party/                   # 第三方代码
│   ├── tlsf/                       # TLSF 实时内存分配器（O(1) 分配/释放）
│   ├── lwprintf/                   # 轻量级 printf 实现
│   └── picolibc/                   # 轻量级 C 标准库（含 Cortex-M 预编译库）
└── README.md
```

## 模块清单

| 模块 | 配置宏 | 类型 | 说明 |
|------|--------|------|------|
| `ek_def` | 始终包含 | 基础层 | 跨编译器兼容层（`__EK_WEAK`/`__EK_PACKED`/`__EK_INLINE` 等） |
| `ek_io` | `EKCFG_IO_LWPRTF` / `EKCFG_PICOLIBC` | 核心服务 | IO 输出封装，支持 lwprintf / picolibc / 标准 libc 三种后端 |
| `ek_log` | `EKCFG_LOG` | 核心服务 | 分级日志（NONE/DEBUG/INFO/WARN/ERROR/FATAL），支持 ANSI 彩色、时间戳和按文件级别过滤 |
| `ek_assert` | `EKCFG_ASSERT` | 核心服务 | 断言：tiny 模式（死循环）和 full 模式（输出详情） |
| `ek_heap` | `EKCFG_HEAP_TLSF` | 核心服务 | 基于 TLSF 的内存堆管理，支持多内存池 |
| `ek_list` | `EKCFG_LIST` | 数据结构 | Linux 内核风格双向循环链表（纯头文件，侵入式设计） |
| `ek_vec` | `EKCFG_VEC` | 数据结构 | 类型安全动态数组（纯头文件，宏生成类型，依赖 `ek_heap`） |
| `ek_ringbuf` | `EKCFG_RINGBUF` | 数据结构 | 通用环形缓冲区（RTOS 模式下通过锁宏支持多线程安全） |
| `ek_ringbuf_spsc` | `EKCFG_RINGBUF_SPSC` | 数据结构 | SPSC 单生产者单消费者无锁环形缓冲区 |
| `ek_stack` | `EKCFG_STACK` | 数据结构 | 通用 LIFO 栈 |
| `ek_snapshot` | `EKCFG_SNAPSHOT` | 数据结构 | 单槽快照（唯一 id 覆盖写，整块读写） |
| `ek_str` | `EKCFG_STR` | 数据结构 | 自动扩容的动态字符串 |
| `ek_str_view` | `EKCFG_STR_VIEW` | 数据结构 | 无动态分配的字符串视图：裁剪、去空白、按分隔符切分 |
| `ek_export` | `EKCFG_EXPORT` | 系统服务 | 基于链接器段的自动初始化（需配合链接脚本） |
| `ek_static_alloc` | `EKCFG_STATIC_ALLOC` | 系统服务 | 静态对象自动注册（`EK_DEFINE_*` → `.ek_static_alloc` 段） |
| `ek_evoke` | `EKCFG_EVOKE` | 系统服务 | 协作式事件驱动调度器（仅裸机，`EKCFG_RTOS=0` 时可用） |
| `ek_picothread` | `EKCFG_PICOTHREAD` | 系统服务 | 协作式微线程调度器（protothread，分级 FIFO 就绪队列） |
| `ek_picothread_sem` | `EKCFG_PICOTHREAD_SEM` | 系统服务 | 微线程信号量（依赖 `ek_picothread`） |
| `ek_picothread_msg` | `EKCFG_PICOTHREAD_MSG` | 系统服务 | 微线程消息队列（依赖 `ek_picothread` + `ek_ringbuf`） |

## 快速上手

### 支持平台

| PLATFORM | CPU | FPU |
|----------|-----|-----|
| cm0 | Cortex-M0 | 无 |
| cm3 | Cortex-M3 | 无 |
| cm4 | Cortex-M4 | fpv4-sp-d16 (hard) |
| cm7 | Cortex-M7 | fpv5-d16 (hard) |
| cm23 | Cortex-M23 | 无 |
| cm33 | Cortex-M33 | fpv5-sp-d16 (hard) |
| cm55 | Cortex-M55 | fpv5-auto (hard) |

### 方式一：子模块集成（推荐）

```bash
# 在你的 MCU 项目根目录
git submodule add https://github.com/EmbeddedKitOrg/EK_UTILS.git lib/ek_utils
```

CMakeLists.txt 配置（CMake ≥ 3.12，推荐 3.20+）：

```cmake
# 1.（推荐）通过 EK_CONF_PATH 显式指定 ek_conf.h 位置。
#    必须用绝对路径：ek_conf_internal.h 里是 #include EK_CONF_PATH（引号形式），
#    相对路径会先按 ek_utils/inc/ 目录解析，容易落空。
set(EK_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_conf.h"
    CACHE FILEPATH "" FORCE)

# 2. 引入 ek_utils 对象库
add_subdirectory(lib/ek_utils)

# 3. 链接对象库 — CMake 3.12+ 允许直接链接 OBJECT 库：
#    ek_utils 的 PUBLIC include（inc/、third_party/tlsf、third_party/lwprintf/inc）
#    会自动传播给主工程，主工程无需再手动加 ek_utils/inc 等路径。
#    （旧的 $<TARGET_OBJECTS:ek_utils> 写法只搬对象文件、不带接口，不推荐。）
target_link_libraries(${PROJECT_NAME} ek_utils)
```

> 不传 `EK_CONF_PATH` 也可以：把存放 `ek_conf.h` 的目录加进 ek_utils 的 include
> 搜索路径即可（见下文"配置说明 · 方式 B"）。

### 方式二：源文件直接集成

```
Include Paths:
  - inc/
  - third_party/tlsf/
  - third_party/lwprintf/inc/

源文件（按需选择）:
  - src/*.c（自动收集所有 .c 文件）

第三方源文件（始终编译，由 #if 守卫控制实际使用）:
  - third_party/tlsf/tlsf.c
  - third_party/lwprintf/lwprintf.c
```

## 配置说明

ek_utils 不内置默认配置文件。**用户必须创建自己的 `ek_conf.h`**，否则编译报错。

### 创建配置文件

1. 复制仓库根目录的 `ek_conf_template.h` → 你的项目 include 路径，改名为 `ek_conf.h`
2. 按硬件调整宏值（不需要改的可以删掉，未定义的宏自动取默认值）
3. 确保编译器能找到你的 `ek_conf.h`（见下文两种方式）

### 指定配置文件的两种方式

> **原理与关键约束**：ek_utils 的源文件通过 `ek_conf_internal.h` 引入配置，逻辑为：
> ```c
> #ifdef EK_CONF_PATH
> #    include EK_CONF_PATH   // 方式 A：CMake 传入的路径
> #else
> #    include "ek_conf.h"    // 方式 B：标准 include 搜索
> #endif
> ```
> ek_utils 是 **OBJECT 库**，编译自己的 `.c` 时会执行上述 `#include`，因此存放
> `ek_conf.h` 的目录必须出现在 **ek_utils 的 include 搜索路径**中——仅给主 target
> 设 `PRIVATE` include 目录**不会**传播给被 `add_subdirectory` 的子项目 ek_utils。

**方式 A：`EK_CONF_PATH` 编译宏（推荐，最省心）**

推荐在项目 CMakeLists.txt 中设置（可复现，需在 `add_subdirectory` 之前）：

```cmake
set(EK_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_conf.h"
    CACHE FILEPATH "" FORCE)
add_subdirectory(lib/ek_utils)
```

也可在命令行传入：

```bash
cmake -DEK_CONF_PATH="$(pwd)/Core/Inc/ek_conf.h" -B build
```

ek_utils 的 CMakeLists 会把 `EK_CONF_PATH` 作为编译宏传给编译器
（`target_compile_definitions`），`ek_conf_internal.h` 通过 `#include EK_CONF_PATH`
直接展开为该路径。

> **必须使用绝对路径**。`#include EK_CONF_PATH` 展开后是引号形式
> `#include "..."`，相对路径会先按包含它的文件所在目录（`ek_utils/inc/`）
> 解析，再按 `-I` 搜索，容易落空；绝对路径最稳妥。

**方式 B：include 搜索路径**

不传 `EK_CONF_PATH` 时走标准 `#include "ek_conf.h"` 搜索，需把存放 `ek_conf.h`
的目录加入 ek_utils 的 include 搜索路径：

```cmake
add_subdirectory(lib/ek_utils)
# 让 ek_utils 编译 .c 时也能找到你的 ek_conf.h
target_include_directories(ek_utils PUBLIC Core/Inc)
target_include_directories(${PROJECT_NAME} PRIVATE Core/Inc)
# 链接：target_link_libraries(${PROJECT_NAME} ek_utils)（见方式一）
```

> 无需担心"优先级"：`ek_utils/inc/` 中没有同名 `ek_conf.h`（只有
> `ek_conf_internal.h` 和 `ek_conf_template.h`），不会冲突。
> 旧文档称"把 `target_include_directories` 写在 `add_subdirectory` 之前可提高优先级"——
> 该说法对 OBJECT 库 + PRIVATE 不成立，PRIVATE 不会传播给子项目。

### 配置宏完整列表

```c
// 平台/运行环境（IO 后端三选一，详见"IO 后端选择"）
#define EKCFG_RTOS          (0)     // 是否使用 RTOS
#define EKCFG_PICOLIBC      (1)     // 是否使用 picolibc（内部默认 1；模板默认 0。=1 时自动关闭 lwprintf）
#define EKCFG_IO_LWPRTF     (0)     // IO 是否使用 lwprintf（内部默认 0；模板默认 1。两者均 0 时需在 ek_conf.h 自行定义 ek_printf 宏）
#define EKCFG_STATIC_ALLOC  (0)     // 静态对象自动初始化（EK_DEFINE_*）

// 核心服务（1=启用，0=禁用）
#define EKCFG_EXPORT (0)
#define EKCFG_ASSERT (1)
#define EKCFG_LOG    (1)

// 数据结构（1=启用，0=禁用）
#define EKCFG_STR            (0)
#define EKCFG_STR_VIEW       (0)
#define EKCFG_LIST           (0)
#define EKCFG_VEC            (0)
#define EKCFG_RINGBUF        (0)
#define EKCFG_RINGBUF_SPSC   (0)
#define EKCFG_STACK          (0)
#define EKCFG_SNAPSHOT       (0)

// 系统服务（1=启用，0=禁用）
#define EKCFG_EVOKE          (0)   // 仅裸机 (EKCFG_RTOS=0 时可用)
#define EKCFG_PICOTHREAD     (0)   // 协作式微线程调度器
#define EKCFG_PICOTHREAD_SEM (0)   // 微线程信号量 (依赖 PICOTHREAD)
#define EKCFG_PICOTHREAD_MSG (0)   // 微线程消息队列 (依赖 PICOTHREAD)

// 子配置
#define EKCFG_HEAP_TLSF                (1)           // 使用 TLSF 分配器
#define EKCFG_HEAP_SIZE                (30 * 1024)   // 堆大小（按 MCU SRAM 调整）
#define EKCFG_HEAP_SECTION             ".tcmram"     // 堆所在链接器段
#define EKCFG_TLSF_FL_INDEX_MAX        (24)          // TLSF 一级索引最大值，最大连续块为 2^N 字节
#define EKCFG_TLSF_SL_INDEX_COUNT_LOG2 (3)           // TLSF 二级索引数量的 log2
#define EKCFG_LOG_DEBUG                (1)           // 启用 DEBUG 日志
#define EKCFG_LOG_COLOR                (1)           // ANSI 彩色日志
#define EKCFG_LOG_BUF_SIZE             (256)         // 日志缓冲区
#define EKCFG_ASSERT_TINY              (1)           // 轻量级断言
#define EKCFG_ASSERT_LOG               (1)           // 断言时输出日志
#define EKCFG_EVOKE_MIN_DEEPSLEEP_TICK (10)          // 进入深度睡眠最小 tick
#define EKCFG_PT_PRIO_LOWEST           (31)          // 微线程最低优先级，就绪队列档位数为该值 + 1
```

### IO 后端选择（三种方式）

`ek_io.h` 按以下优先级选择后端：`EKCFG_IO_LWPRTF=1` → lwprintf；否则 `EKCFG_PICOLIBC=1` → picolibc；两者均为 0 → 标准 libc 直通（需用户定义宏）。

| 后端 | ek_conf.h 配置 | `ek_printf` 等宏 | 需实现 | 适用场景 |
|------|---------------|------------------|--------|----------|
| picolibc（内部默认） | `EKCFG_PICOLIBC=1` | 自动 → `printf`/`vsnprintf` 等 stdio | `int ek_port_io_fputc(int ch)` | picolibc 标准 I/O 经适配层重定向到平台字符输出 |
| lwprintf（模板默认） | `EKCFG_IO_LWPRTF=1, EKCFG_PICOLIBC=0` | 自动 → `lwprintf`/`lwvsnprintf` 等 | `int ek_port_io_fputc(int ch)` | 极简 IO，自带轻量格式化，不依赖 libc |
| 标准 libc stdio 直通 | `EKCFG_PICOLIBC=0, EKCFG_IO_LWPRTF=0` | **需在 ek_conf.h 中自行定义** | 无（直接用 libc） | 已有完整 libc（桌面/新libc/RTOS 提供 stdio），直接使用系统 printf |

**方式三（标准 libc stdio 直通）**：`ek_io.h` 在两者均为 0 时不定义
`ek_printf`/`ek_vsnprintf` 等宏，需在 `ek_conf.h` 中补全：

```c
#define EKCFG_PICOLIBC  (0)   /* 不使用 picolibc */
#define EKCFG_IO_LWPRTF (0)   /* 不使用 lwprintf */

#include <stdio.h>

#define ek_printf    printf
#define ek_vprintf   vprintf
#define ek_sprintf   sprintf
#define ek_snprintf  snprintf
#define ek_vsnprintf vsnprintf
```

> 核心约束：只要 `EKCFG_IO_LWPRTF=0` 且 `EKCFG_PICOLIBC=0`，用到 `ek_printf` 的模块
> （`ek_log`、`ek_str`）编译时就需要上述宏，**必须**由用户在 `ek_conf.h` 中补全，
> 否则编译报错（`ek_io.h` 源码注释："如果不需要使用 lwprintf 需要补全下列的宏"）。
> `EKCFG_PICOLIBC=1` 时 `EKCFG_IO_LWPRTF` 会被强制置 0（ek_conf_internal 自动处理）。

### 原理解释

```
库源文件
  └→ #include "ek_conf_internal.h"
       ├→ #include 用户的 ek_conf.h     （用户定义的宏）
       ├→ 填充用户未定义的宏（#ifndef 守卫）
       └→ 依赖校验（picolibc 冲突、evoke+RTOS 冲突等）
```

用户只需定义要改的宏，其余自动取默认值。

## 平台适配

### 弱函数（Weak Functions）

所有需要平台或用户定制的函数以 `__EK_WEAK` 声明，用户提供同名强函数即可覆盖：

```c
// ========== ek_io — 字符输出 ==========
// lwprintf / picolibc 后端：
int ek_port_io_fputc(int ch)             // 字符输出（UART 等）
// 标准 libc 直通后端无需实现

// ========== ek_log — 时间戳 ==========
uint32_t ek_port_log_get_tick(void)       // 系统时间戳

// ========== ek_evoke — 临界区 & 睡眠（共 5 个）==========
void ek_evoke_enter_critical(void)       // 进入临界区（关中断）
void ek_evoke_exit_critical(void)        // 退出临界区（开中断）
void ek_evoke_set_timer(uint32_t xtick)  // 设置单次定时器
void ek_evoke_light_sleep(void)          // 浅睡眠（WFI）
void ek_evoke_deep_sleep(void)           // 深度睡眠（低功耗模式）

// ========== ek_assert — 断言失败钩子（仅 full 模式）==========
void ek_assert_hook(const char *file, uint32_t line, const char *expr)  // 断言失败时调用（死循环前）

// ========== ek_heap — 自定义分配器（仅在 EKCFG_HEAP_TLSF=0 时）==========
void *ek_malloc(size_t size)
void ek_free(void *ptr)
void *ek_realloc(void *ptr, size_t size)
```

### 在 STM32 项目中的典型实现

```c
// ek_port.c
#include "ek_io.h"
#include "ek_log.h"
// lwprintf 与 picolibc 后端需要实现此函数

int ek_port_io_fputc(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

uint32_t ek_port_log_get_tick(void)
{
    return HAL_GetTick();
}
```

## 设计约定

### 错误返回约定

所有操作函数返回 `int`：成功返回 `0`（有数量语义时直接返回数量），失败返回负 errno
（如 `-EINVAL`、`-ENOSPC`、`-ETIMEDOUT`）。`<errno.h>` 已由 `ek_def.h` 引入：

```c
int ek_ringbuf_write(ek_ringbuf_t *rb, const void *item);
// 返回 0 成功，-ENOSPC 满，-EBUSY 忙

// 错误传播宏（定义在 ek_def.h，按返回值 < 0 判断）
EK_ERR_RETURN(ek_str_append(s, "hello"));  // 返回 < 0 则直接 return
EK_ERR_GOTO(ek_foo(), cleanup);             // 返回 < 0 则 goto cleanup
```

`bool` 返回值仅用于纯查询函数（`ek_ringbuf_full`、`ek_stack_empty` 等）。错误码转字符串用标准库
`strerror(-err)`。

### _safely 释放宏

释放后将指针置 NULL，防止悬空指针重用：

```c
ek_free_safely(ptr);                    // ptr 释放后自动 = NULL
ek_ringbuf_destroy_safely(rb);          // rb 释放后自动 = NULL
ek_stack_destroy_safely(sk);            // sk 释放后自动 = NULL
```

### 命名规范

- 公共 API：`ek_` 前缀（`ek_malloc`、`ek_ringbuf_create`）
- 全局变量：`g_` 前缀（`g_ek_default_heap`）
- 静态变量：`s_` 前缀（`s_lock`、`s_log_buffer`）
- 内部静态函数：`_` 前缀 + 文件顶部前向声明 + 文件底部实现
- 配置宏：`EKCFG_` 前缀，正逻辑命名（1=启用，0=禁用）

### 侵入式数据结构

`ek_list` 和 `ek_evoke` 使用侵入式链表节点（将 `ek_list_node_t` 嵌入用户结构体），避免额外内存分配。

## 第三方依赖

| 库 | 用途 | 许可证 |
|----|------|--------|
| [TLSF](http://tlsf.baisoku.org) v3.1 | 实时内存分配器（O(1) 分配/释放） | BSD-style |
| [lwprintf](https://github.com/MaJerle/lwprintf) | 轻量级 printf 实现 | MIT |
| [picolibc](https://github.com/picolibc/picolibc) | 嵌入式 C 标准库，含 Cortex-M0/M3/M4/M7/M23/M33/M55 预编译库 | BSD |

## 许可

本项目为原创代码，第三方代码遵循各自许可证。

## 模块移植指南

本章按移植顺序逐一说明 ek_utils 全部模块的集成方式：需要的源文件、依赖关系、配置宏、用户需实现的接口、链接脚本变更、初始化方式和注意事项。

### 移植总览

|序号|模块|类型|关键移植点|
|---|---|---|---|
|1|ek_def|基础层|零移植，编译器自动适配|
|2|ek_conf_internal|基础层|用户创建 `ek_conf.h` 即可|
|3|ek_io|核心服务|lwprintf / picolibc 后端实现 `ek_port_io_fputc()`；标准 libc 直通无需实现|
|4|ek_log|核心服务|实现 `ek_port_log_get_tick()` — 系统时间戳|
|5|ek_assert|核心服务|tiny 模式零移植；full 模式依赖 `ek_log`，可选覆盖 `ek_assert_hook`|
|6|ek_heap|核心服务|决定堆段位置（`EKCFG_HEAP_SECTION`），TLSF 索引由 `EKCFG_TLSF_*` 配置|
|7|ek_list|数据结构|零移植，纯头文件|
|8|ek_vec|数据结构|纯头文件，依赖 `ek_heap`|
|9|ek_ringbuf|数据结构|依赖 `ek_heap`（动态分配）；`EKCFG_STATIC_ALLOC=1` 时可用 `EK_DEFINE_RINGBUF*`|
|10|ek_stack|数据结构|依赖 `ek_heap`（动态分配）；`EKCFG_STATIC_ALLOC=1` 时可用 `EK_DEFINE_STACK`|
|11|ek_snapshot|数据结构|依赖 `ek_heap`（动态分配）；`EKCFG_STATIC_ALLOC=1` 时可用 `EK_SNAPSHOT_DEFINE`|
|12|ek_str|数据结构|依赖 `ek_heap` + `ek_io`（格式化）|
|13|ek_str_view|数据结构|零移植，无堆/IO 依赖（纯指针裁剪）|
|14|ek_export|系统服务|**必须修改链接脚本**（`.ek_export_fn` 段）|
|15|ek_static_alloc|系统服务|`EKCFG_STATIC_ALLOC=1` 时需增加 `.ek_static_alloc` 段|
|16|ek_evoke|系统服务|仅裸机；实现 5 个弱函数（临界区/定时器/睡眠）；ISR FIFO 为文件内静态缓冲|
|17|ek_picothread|系统服务|无弱函数，纯调度器；主循环需配合定时器；就绪队列为分级 FIFO|
|18|ek_picothread_sem|系统服务|依赖 `ek_picothread`，子模块无额外移植|
|19|ek_picothread_msg|系统服务|依赖 `ek_picothread` + `ek_ringbuf`|

---

### ek_def — 跨编译器兼容层

**功能概述**：跨编译器宏定义（`__EK_WEAK`/`__EK_PACKED`/`__EK_INLINE` 等）和通用工具宏（`EK_ARRAY_LEN`/`EK_CLAMP`/`EK_FREQ_M` 等）。

**源文件**：
- `ek_utils/inc/ek_def.h`（纯头文件，无 .c）

**依赖**：
- `<stdlib.h>`、`<stdint.h>`、`<inttypes.h>`（标准 C 头文件）
- ek_conf_internal（决定是否包含 `<stdarg.h>`）

**配置宏**：无 — 始终可用。宏值根据编译器自动选择（GCC/ARMCC5/ARMCC6/其他）。

**用户需实现的接口**：无。

**链接脚本变更**：无。

**初始化**：无需初始化。

**注意事项**：
- 定义了 `bool`/`true`/`false`（`#include <stdbool.h>`）用于 `ek_list` 等模块
- `CRLF` 宏在非 Linux 平台为 `\r\n`，可在 `ek_conf.h` 中覆盖

---

### ek_conf_internal — 配置系统

**功能概述**：配置入口——引入用户 `ek_conf.h`，填充未定义宏的默认值，执行依赖校验。

**源文件**：
- `ek_utils/inc/ek_conf_internal.h`（纯头文件，无 .c）
- **用户创建**：`Core/Inc/ek_conf.h`

**依赖**：无（ek_conf_internal 是配置层起点，其他模块都依赖它）。

**配置宏**：所有 `EKCFG_*` 宏均有默认值（详见"配置宏完整列表"章节）。

**用户需实现的接口**：无 — 只需创建 `ek_conf.h` 文件。

**链接脚本变更**：无。

**初始化**：无需初始化 — 配置在编译期解析为常量。

**注意事项**：
- **用户必须创建 `ek_conf.h`**，否则 `ek_conf_internal.h` 中的 `#include "ek_conf.h"` 会编译失败
- 复制 `ek_utils/ek_conf_template.h` 改名为 `ek_conf.h`，仅保留需要覆盖的宏即可
- 依赖校验规则：`picolibc=1` 自动关闭 lwprintf；`evoke=1` 且 `RTOS=1` 编译报错；sem/msg 开启但 picothread 未开时报错；`EKCFG_PT_PRIO_LOWEST` 必须 ≤ 31
- IO 后端规则：`EKCFG_PICOLIBC=1`（内部默认）走 picolibc；`EKCFG_IO_LWPRTF=1` 走 lwprintf；两者均为 0 时 `ek_printf` 等宏未定义，**必须在 `ek_conf.h` 中自行补全**（标准 libc 直通，见"IO 后端选择")

---

### ek_io — IO 输出封装

**功能概述**：封装底层 printf 输出，支持 lwprintf、picolibc、标准 libc 三种后端，提供统一的 `ek_printf` 宏。后端选择见上文"IO 后端选择（三种方式）"。

**源文件**：
- `ek_utils/inc/ek_io.h`
- `ek_utils/src/ek_io.c`
- lwprintf 后端：`ek_utils/third_party/lwprintf/lwprintf.c`
- picolibc 后端：`ek_utils/third_party/picolibc/*.a`（预编译库）

**依赖**：
- ek_conf_internal、ek_def
- lwprintf 模式：需要 `lwprintf.h` 和 `lwprintf.c`
- picolibc 模式：需要 `<stdio.h>` 和预编译 `libpicolibc.a`

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_IO_LWPRTF` | 0 | 使用 lwprintf 后端（与 PICOLIBC 互斥） |
| `EKCFG_PICOLIBC` | 1 | 使用 picolibc 后端（开启时自动关闭 lwprintf） |
| （两者均为 0） | — | 标准 libc 直通：`ek_printf` 等宏**未定义**，需在 `ek_conf.h` 中自行定义 |

**用户需实现的接口**：
- lwprintf / picolibc 后端：`int ek_port_io_fputc(int ch)` — 底层单字符输出（picolibc 由 `ek_picolibc_port.c` 重定向 `stdout`/`stderr`）
- 标准 libc 直通后端：无需实现

```c
int ek_port_io_fputc(int ch)
{
    bsp_uart_send_data(ch);
    return ch;
}
```

**链接脚本变更**：无。

**初始化**：`ek_io_init()` — lwprintf 模式调用 `lwprintf_init()`；picolibc 模式为空函数；也可通过 `EK_EXPORT_COMPONENTS` 自动调用。

**注意事项**：
- `EKCFG_PICOLIBC=1` 时 `EKCFG_IO_LWPRTF` 被强制设为 0（ek_conf_internal 自动处理）
- lwprintf 与 picolibc 后端需要实现 `ek_port_io_fputc`；默认弱实现直接返回 `ch`
- picolibc 后端由 `ek_picolibc_port.c` 使用 `ek_port_io_fputc` 重定向 `stdout`/`stderr`，`ek_io_init()` 为空函数
- 标准 libc 直通（两者均为 0）：`ek_printf`/`ek_vsnprintf` 等宏未定义，必须在 `ek_conf.h` 中补全（参见"IO 后端选择"），`ek_io_init()` 为空函数

---

### ek_log — 分级日志

**功能概述**：多级日志输出（NONE/DEBUG/INFO/WARN/ERROR/FATAL），支持 ANSI 彩色、时间戳、文件名/行号输出，以及通过 `EK_LOG_MODULE` 实现的按文件最低日志级别过滤。

**源文件**：
- `ek_utils/inc/ek_log.h`
- `ek_utils/src/ek_log.c`

**依赖**：
- ek_io、ek_def、ek_conf_internal

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_LOG` | 1 | 日志模块总开关 |
| `EKCFG_LOG_DEBUG` | 1 | 启用 DEBUG 级别（关闭可减小代码体积） |
| `EKCFG_LOG_COLOR` | 1 | 启用 ANSI 彩色输出 |
| `EKCFG_LOG_BUF_SIZE` | 256 | 单条日志缓冲区大小（字节） |

**用户需实现的接口**：
- `uint32_t ek_port_log_get_tick(void)` — 返回系统时间戳（毫秒）

```c
uint32_t ek_port_log_get_tick(void)
{
    return HAL_GetTick();
}
```

**链接脚本变更**：无。

**初始化**：无需显式初始化。在每个使用日志的 `.c` 文件开头声明文件标签和级别。

**文件声明**：有两种方式：
- `EK_LOG_MODULE(tag, level)` — **推荐**，声明文件标签并设置该文件的最低输出级别
- `EK_LOG_FILE_TAG(tag)` — 兼容旧接口，等价于 `EK_LOG_MODULE(tag, EK_LOG_LEVEL_NONE)`（所有级别均输出）

**使用示例**：
```c
// 声明文件标签，只输出 WARN 及以上级别（WARN/ERROR/FATAL）
EK_LOG_MODULE("main.c", EK_LOG_LEVEL_WARN);

// 或兼容方式：所有级别均输出
EK_LOG_FILE_TAG("main.c");

EK_LOG_DEBUG("value = %d", val);    // DEBUG 级别（受 EKCFG_LOG_DEBUG 总开关控制）
EK_LOG_INFO("system started");      // INFO 级别
EK_LOG_WARN("voltage low: %dmV", mv); // WARN 级别
EK_LOG_ERROR("fatal: code=%d", err);  // ERROR 级别
EK_LOG_FATAL("system halted");       // FATAL 级别，输出后死循环
EK_LOG("plain output");             // 无级别输出（级别为 NONE，受 EK_LOG_MODULE 阈值影响）
```

**注意事项**：
- `EK_LOG_FATAL` 输出后进入 `while(1)` 死循环，适用于不可恢复的致命错误
- 级别过滤由文件内的 `static const ek_log_level_t _EK_LOG_MIN_LEVEL_` 控制；编译器可在常量级别确定时消除低于阈值的日志调用
- `EK_LOG(...)` 不标记级别，但同样受 `EK_LOG_MODULE` 设定的阈值影响（阈值 = `EK_LOG_LEVEL_NONE` 时始终输出）

---

### ek_assert — 断言

**功能概述**：提供两种断言模式——tiny（死循环，适合资源受限环境）和 full（输出文件名/行号/表达式，依赖 ek_log）。

**源文件**：
- `ek_utils/inc/ek_assert.h`
- `ek_utils/src/ek_assert.c`（仅 full 模式需要）

**依赖**：
- ek_conf_internal、ek_def
- full 模式：依赖 ek_log（`EKCFG_ASSERT_LOG=1` 时输出日志）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_ASSERT` | 1 | 断言模块总开关 |
| `EKCFG_ASSERT_TINY` | 1 | 使用轻量级断言（死循环）；0=full 模式 |
| `EKCFG_ASSERT_LOG` | 1 | 断言失败时输出日志（需要 EKCFG_LOG） |

**用户需实现的接口**：无强制项。`ek_assert_fault()` 有默认实现（输出信息后死循环）。可选覆盖 `ek_assert_hook()` 弱函数——它在日志输出后、死循环前被调用，用于插入自定义处理（记录到 flash、触发复位等）。

**链接脚本变更**：无。

**初始化**：无需初始化。通过 `ek_assert_param(expr)` 宏在代码中直接使用。

**注意事项**：
- tiny 模式（默认）：断言失败直接 `while(1)` 死循环，零 RAM/ROM 开销
- full 模式：会调用 `ek_assert_fault()`，可覆盖此函数实现自定义处理（如复位芯片）
- 所有 ek_utils 内部函数参数检查均使用 `ek_assert_param`
- full 模式下，`ek_assert_fault` 在死循环前会调用 `__EK_WEAK void ek_assert_hook(file, line, expr)`（默认空实现）；用户可提供同名强函数覆盖，插入自定义逻辑。注意：hook 返回后仍进入死循环，如需复位须在 hook 内直接触发

---

### ek_heap — 内存堆管理

**功能概述**：基于 TLSF（O(1) 分配/释放）的内存堆管理，支持多内存池，提供 `ek_malloc`/`ek_free`/`ek_realloc` 和 `_safely` 安全释放宏。

- `ek_utils/inc/ek_heap.h`
- `ek_utils/src/ek_heap.c`
- `ek_utils/third_party/tlsf/tlsf.c`
- `ek_utils/third_party/tlsf/tlsf.h`

**依赖**：
- ek_def、ek_conf_internal
- ek_export（`ek_heap_init` 通过 `EK_EXPORT_EARLIEST(fn, 0)` 自动初始化）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_HEAP_TLSF` | 1 | 使用 TLSF 分配器 |
| `EKCFG_HEAP_SIZE` | 30×1024 | 默认堆大小（字节） |
| `EKCFG_HEAP_SECTION` | 未定义 | 堆所在链接器段（如 `".tcmram"`），不定义则放 `.bss` |
| `EKCFG_TLSF_FL_INDEX_MAX` | 24 | TLSF 一级索引最大值，最大连续块为 2 的该值次方字节 |
| `EKCFG_TLSF_SL_INDEX_COUNT_LOG2` | 3 | TLSF 二级索引数量的 log2，二级链表数为 2 的该值次方 |

**用户需实现的接口**（仅在 `EKCFG_HEAP_TLSF=0` 时）：
- `void *ek_malloc(size_t size)`
- `void ek_free(void *ptr)`
- `void *ek_realloc(void *ptr, size_t size)`

**链接脚本变更**：如果定义 `EKCFG_HEAP_SECTION`，需确保链接脚本中有对应的段（如 `.tcmram`）。默认无需变更。

**初始化**：`ek_heap_init()` 通过 `EK_EXPORT_EARLIEST(fn, 0)`（level=0）自动初始化，前提是已启用 `EKCFG_EXPORT=1` 并配置链接脚本段。否则需在 `main()` 起始手动调用。

**注意事项**：
- TLSF 控制结构开销由 `EKCFG_TLSF_FL_INDEX_MAX` 和 `EKCFG_TLSF_SL_INDEX_COUNT_LOG2` 决定；默认 24/3 适合最大 16 MiB 的内存池
- 较小内存池可降低 `EKCFG_TLSF_FL_INDEX_MAX` 以减少控制结构占用
- `ek_free_safely(&ptr)` 释放后自动将 `ptr` 置 NULL
- `ek_heap_add_pool()` 支持运行时添加额外内存池（如 SDRAM 区域）

---

### ek_list — 双向循环链表

**功能概述**：Linux 内核风格的侵入式双向循环链表，所有操作 O(1)，零额外内存分配。

**源文件**：
- `ek_utils/inc/ek_list.h`（纯头文件，无 .c）

**依赖**：
- ek_def（`bool`/`offsetof`/`__EK_STATIC_INLINE`）
- ek_conf_internal（`EKCFG_LIST` 守卫）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_LIST` | 0 | 模块开关 |

**用户需实现的接口**：无。

**链接脚本变更**：无。

**初始化**：`ek_list_init(&head)` 初始化链表头。

**使用示例**：
```c
typedef struct {
    int data;
    ek_list_node_t node; // 嵌入链表节点
} my_item_t;

ek_list_node_t head;
ek_list_init(&head);

my_item_t a = { .data = 42 };
ek_list_insert_tail(&head, &a.node);

// 遍历
ek_list_node_t *pos;
ek_list_foreach(pos, &head) {
    my_item_t *item = EK_CONTAINER_OF(pos, my_item_t, node);
}
```

**注意事项**：
- 节点必须嵌入用户结构体，通过 `EK_CONTAINER_OF` 反查父结构体地址
- `ek_list_remove` 后将节点 prev/next 置 NULL，可用于检测节点是否在链表中
- 删除遍历时使用 `ek_list_foreach_safe`

---

### ek_vec — 动态数组

**功能概述**：类型安全的动态数组，通过宏生成类型化结构体和操作，自动扩容。

**源文件**：
- `ek_utils/inc/ek_vec.h`（纯头文件，无 .c）

**依赖**：
- ek_def、ek_heap（`ek_realloc`）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_VEC` | 0 | 模块开关 |

**用户需实现的接口**：无。

**链接脚本变更**：无。

**初始化**：`ek_vec_init(v)` 初始化（零开销，仅将指针和计数器置零）。

**使用示例**：
```c
EK_VEC_IMPLEMENT(int);          // 生成 ek_vec_int_t 类型
ek_vec_t(int) v;                // 声明数组
ek_vec_init(v);                 // 初始化
ek_vec_append(v, 42);           // 追加

uint32_t pos;
ek_vec_foreach(pos, v) {
    // 访问 v.items[pos]
}
ek_vec_remove(v, 0);            // 按索引移除
ek_vec_destroy(v);              // 释放内存
```

**注意事项**：
- 扩容策略：容量 <32 时翻倍，≥32 时加 1/2
- `ek_vec_append` 内部分配失败时 `break` 跳出（不追加），需自行检测 `v.amount` 变化
- `EK_VEC_IMPLEMENT` 必须在全局作用域使用
- 正向遍历使用 `ek_vec_foreach(pos, v)`，从指定索引开始遍历使用 `ek_vec_foreach_from(pos, index, v)`

---

### ek_ringbuf — 环形缓冲区

**功能概述**：任意类型的环形缓冲区（通用版 `ek_ringbuf_t` + SPSC 版 `ek_ringbuf_spsc_t`），RTOS 模式下通用版通过锁宏支持多线程安全。

- `ek_utils/inc/ek_ringbuf.h`
- `ek_utils/src/ek_ringbuf.c`

**依赖**：
- ek_conf_internal
- ek_heap（动态分配）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_RINGBUF` | 0 | 通用环形缓冲区 |
| `EKCFG_RINGBUF_SPSC` | 0 | SPSC 无锁环形缓冲区 |
| `EKCFG_STATIC_ALLOC` | 0 | 启用后可使用 `EK_DEFINE_RINGBUF` / `EK_DEFINE_RINGBUF_SPSC` |

**用户需实现的接口**：无。

**链接脚本变更**：使用静态定义宏时需配置 `.ek_static_alloc` 段。

**初始化**：
```c
// 动态：amount = 实际可存元素数
ek_ringbuf_t *rb = ek_ringbuf_create(sizeof(my_data_t), 10);

// SPSC 动态：amount = 底层槽位数，实际可存 amount - 1 个元素
ek_ringbuf_spsc_t *rb_spsc = ek_ringbuf_create_spsc(sizeof(my_data_t), 11);

// 静态：ek_export_init() 后对象已初始化，不走堆；句柄即指针，直接传入 API（无需 &）
EK_DEFINE_RINGBUF(static_rb, my_data_t, 10);
EK_DEFINE_RINGBUF_SPSC(static_spsc, my_data_t, 11);
ek_ringbuf_write(static_rb, &d); // 例：静态句柄用法与动态指针一致
```

**注意事项**：
- 通用版在 RTOS 模式下通过 `ek_conf_internal.h` 中的锁宏保护（用户在 `ek_conf.h` 中定义映射到 RTOS mutex/semaphore）
- SPSC 版无锁，适合 ISR→主循环 或 单生产者单消费者场景（evoke 内部使用）
- SPSC 版满判据：`(write_idx + 1) % cap == read_idx`，`amount` 是槽位数
- 静态宏展开为 union（控制块与数据连续），句柄是指针，直接传入 API（无需 `&`）；静态对象只走 `*_init_static()` / `*_deinit_static()`，deinit 不释放内存
- 使用 `ek_ringbuf_destroy_safely(&rb)` 安全释放动态对象

---

### ek_stack — 通用栈

**功能概述**：LIFO 栈，支持任意数据类型，动态分配底层缓冲区。

**源文件**：
- `ek_utils/inc/ek_stack.h`
- `ek_utils/src/ek_stack.c`

**依赖**：
- ek_conf_internal
- ek_heap（动态分配）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_STACK` | 0 | 模块开关 |
| `EKCFG_STATIC_ALLOC` | 0 | 启用后可使用 `EK_DEFINE_STACK` |

**用户需实现的接口**：无。

**链接脚本变更**：使用静态定义宏时需配置 `.ek_static_alloc` 段。

**初始化**：
```c
ek_stack_t *sk = ek_stack_create(sizeof(int), 20);
ek_stack_push(sk, &val);
ek_stack_pop(sk, &val);
ek_stack_destroy_safely(&sk);

// 静态：ek_export_init() 后对象已初始化，不走堆
EK_DEFINE_STACK(static_sk, int, 20);
```

**注意事项**：
- RTOS 模式下通过锁宏保护（`EK_LOCK_TRY`/`EK_LOCK_RELEASE`，用户在 `ek_conf.h` 中定义映射）
- 所有操作按 `item_size` 字节复制数据（值语义）
- 静态宏展开为 union（控制块与数据连续），句柄是指针，直接传入 API（无需 `&`）；静态对象只走 `ek_stack_init_static()` / `ek_stack_deinit_static()`，deinit 不释放内存

---

### ek_snapshot — 快照数据

**功能概述**：单槽快照数据结构：生产者以唯一 id（`unique`，可用时间戳/递增序号）覆盖写入最新一份数据，消费者随时整块读取。控制块与数据单次分配（柔性数组内联），无扩容。

**源文件**：
- `ek_utils/inc/ek_snapshot.h`
- `ek_utils/src/ek_snapshot.c`

**依赖**：
- ek_conf_internal、ek_def
- ek_heap（动态分配）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_SNAPSHOT` | 0 | 模块开关 |
| `EKCFG_STATIC_ALLOC` | 0 | 启用后可使用 `EK_SNAPSHOT_DEFINE` |

**用户需实现的接口**：无。

**链接脚本变更**：使用静态定义宏时需配置 `.ek_static_alloc` 段。

**初始化**：
```c
// 动态：控制块与数据单次分配
ek_snapshot_t *snap = ek_snapshot_create(sizeof(my_data_t));

// 静态：ek_export_init() 后对象已初始化，不走堆；句柄即指针
EK_SNAPSHOT_DEFINE(static_snap, sizeof(my_data_t));
```

**使用示例**：
```c
my_data_t tx = { /* 最新采样 */ };
my_data_t rx;

// 写入快照：unique 为递增序号/时间戳，不得为 0 或与当前 id 相同
if (ek_snapshot_set(snap, &tx, ++seq) == 0) {
    // 快照已更新
}

// 读取快照：无数据时返回 -ENODATA
if (ek_snapshot_get(snap, &rx) == 0) {
    // rx 即最新快照
}

// 查询当前快照 id（0 表示无数据）
uint32_t id = ek_snapshot_get_unique(snap);
```

**注意事项**：
- `unique` 为 0 表示无数据：新建快照 `get` 返回 `-ENODATA`，写入 `unique=0` 会被拒绝
- `ek_snapshot_set` 传入与当前相同的 `unique` 返回 `-EINVAL`，防止旧数据倒灌覆盖新快照
- 读写按 `data_size` 整块 `memcpy`，数据类型由用户保证，无类型检查、无扩容
- 适用于"生产者高频更新、消费者随机读取最新值"的场景（如传感器最新采样、状态快照）
- 静态宏展开为 union（控制块与数据连续），句柄是指针，直接传入 API（无需 `&`）；静态对象只走 `ek_snapshot_init_static()` / `ek_snapshot_deinit_static()`，deinit 不释放内存


### ek_str — 动态字符串

**功能概述**：自动扩容的动态字符串，支持追加、切片、格式化输出、比较、翻转。

**源文件**：
- `ek_utils/inc/ek_str.h`
- `ek_utils/src/ek_str.c`

**依赖**：
- ek_conf_internal
- ek_heap（`ek_malloc`/`ek_realloc`）
- ek_io（`ek_vsnprintf` — `ek_str_append_fmt` 需要）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_STR` | 0 | 模块开关 |

**用户需实现的接口**：无直接接口。`ek_str_append_fmt` 间接依赖 `ek_io` 的 `ek_vsnprintf`；lwprintf 与 picolibc 后端需实现 `ek_port_io_fputc`，标准 libc 直通后端无需实现（见"IO 后端选择"）。

**链接脚本变更**：无。

**初始化**：
```c
ek_str_t *s = ek_str_create("hello");
ek_str_append(s, " world");
ek_str_append_fmt(s, "(%d)", 42);
const char *cs = ek_str_get_cstring(s); // "hello world(42)"
ek_str_free(s);
```

**注意事项**：
- 扩容策略：当前容量的 1.5 倍
- `ek_str_get_cstring` 返回内部缓冲区指针，不要手动释放
- `ek_str_slice` 创建新字符串对象，需单独释放

---

### ek_str_view — 字符串视图

**功能概述**：非拥有型字符串视图：只记录字符串指针与长度，不复制内容、不分配内存。支持左/右裁剪、去除两端空白、按分隔符逐段切分。

**源文件**：
- `ek_utils/inc/ek_str_view.h`
- `ek_utils/src/ek_str_view.c`

**依赖**：
- ek_conf_internal、ek_def

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_STR_VIEW` | 0 | 模块开关 |

**用户需实现的接口**：无。

**链接脚本变更**：无。

**初始化**：无需初始化。视图不拥有内存，底层字符串在视图使用期间必须保持有效。

**使用示例**：
```c
ek_str_view_t sv = ek_str_view_create("key = value");

ek_str_view_t key = ek_str_view_split(&sv, '='); // key = "key "，sv 前进到 " value"
ek_str_view_trim(&key);                           // key = "key"
ek_str_view_t val = ek_str_view_split(&sv, '='); // 无分隔符：val = " value"，sv 被清空
ek_str_view_trim(&val);                           // val = "value"
```

**注意事项**：
- 视图不复制内容、不分配内存；底层字符串在视图使用期间必须保持有效
- `chop_left`/`chop_right` 的字符数超过视图长度时按全部切掉处理
- `ek_str_view_split` 未找到分隔符时返回剩余全部内容，并将原视图清空
- `create`/`split` 不检查 NULL 参数（传入属未定义行为）；`chop_left`/`chop_right`/`trim` 传入 NULL 返回 `-EINVAL`

---

### ek_export — 自动初始化

**功能概述**：基于链接器段的函数自动导出和初始化，类似 Linux `initcall`。通过 `EK_EXPORT_LEVEL(fn, level, order)` 将函数指针放入 `.ek_export_fn` 段，`ek_export_init()` 运行时按 `level`（层级）和 `order`（层内优先级）排序后逐个调用——排序通过 `qsort` 在运行时完成，不依赖链接器行为。

**源文件**：
- `ek_utils/inc/ek_export.h`
- `ek_utils/src/ek_export.c`

**依赖**：
- ek_conf_internal

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_EXPORT` | 0 | 模块开关 |

**用户需实现的接口**：无。

**链接脚本变更**：**必须添加 `.ek_export` 段**，否则导出的初始化函数不会被执行：

```ld
/* 在链接脚本 (.ld) 中添加 */
.ek_export :
{
    . = ALIGN(4);
    _ek_export_fn_start = .;
    KEEP(*(.ek_export_fn*))
    . = ALIGN(4);
    _ek_export_fn_end = .;
} > FLASH
```

**初始化**：在 `main()` 起始调用 `ek_export_init()`：

```c
int main(void) {
    ek_export_init(); // 按 level → order 排序后调用所有导出函数
    // ...
}
```

**使用示例**：
```c
void my_init(void) {
    // 初始化代码
}
EK_EXPORT_LEVEL(my_init, 3, 0); // level=3（应用层），order=0

// 预定义层级别名（等价于 EK_EXPORT_LEVEL(fn, level, order)）
EK_EXPORT_EARLIEST(heap_init, 0);    // level=0 — 堆初始化
EK_EXPORT_HARDWARE(hw_init, 0);      // level=1 — 硬件初始化
EK_EXPORT_COMPONENTS(comp_init, 0);  // level=2 — 组件初始化
EK_EXPORT_APP(app_init, 0);          // level=3 — 应用初始化
EK_EXPORT_USER(user_init, 0);        // level=4 — 用户初始化
```

**注意事项**：
- 排序规则：先按 `level`（层级，0–4）升序，同层级内按 `order`（层内优先级）升序；通过 `qsort` 在运行时完成
- `KEEP(*(.ek_export_fn*))` 保证即使 `--gc-sections` 也不会移除导出函数
- `EKCFG_EXPORT=0` 时所有导出宏展开为空
- ek_utils 内部模块（heap/evoke/picothread/static_alloc）自身也通过此机制自动初始化

---

### ek_static_alloc — 静态对象自动初始化

**功能概述**：把 `EK_DEFINE_*` 展开出的静态对象登记到 `.ek_static_alloc` 段。`ek_static_alloc_init()` 复制注册项、按 `order` 升序排序后逐个调用初始化回调。某项失败时记录第一个错误并继续处理后续对象。

**源文件**：
- `ek_utils/inc/ek_static_alloc.h`
- `ek_utils/src/ek_static_alloc.c`

**依赖**：
- ek_conf_internal、ek_def
- ek_export（`EKCFG_EXPORT=1` 时自动调用）
- ek_log（可选，失败时输出错误）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_STATIC_ALLOC` | 0 | 模块开关；关闭后所有 `EK_DEFINE_*` 不可用 |

**用户需实现的接口**：无。

**链接脚本变更**：**必须添加 `.ek_static_alloc` 段**，否则静态对象不会被初始化：

```ld
.ek_static_alloc :
{
    . = ALIGN(4);
    _ek_static_alloc_start = .;
    KEEP(*(SORT(.ek_static_alloc*)))
    . = ALIGN(4);
    _ek_static_alloc_end = .;
} > FLASH
```

**初始化**：
- `EKCFG_EXPORT=1` 时，`ek_static_alloc_init()` 通过 `EK_EXPORT_COMPONENTS(..., 1)` 在 `ek_pt_init` / `ek_evoke_init` 之后自动执行
- 未启用 `ek_export` 时，必须在堆初始化之后手动调用 `ek_static_alloc_init()`

**使用示例**：
```c
EK_DEFINE_STACK(static_sk, int, 4);
EK_DEFINE_RINGBUF(static_rb, int, 4);
EK_DEFINE_RINGBUF_SPSC(static_spsc, int, 5);
EK_DEFINE_PT(static_pt, led_task, 10, NULL);
EK_DEFINE_PT_SEM(static_sem, 1);
EK_DEFINE_PT_MSG(static_msg, int, 3);
EK_DEFINE_EVOKE_TASK(static_task, led_cb, NULL);
EK_DEFINE_EVOKE_EVENT(static_event, 0);

int main(void) {
    ek_export_init(); // 未启用 export 时改为手动 ek_heap_init(); ek_static_alloc_init();
    if (ek_static_alloc_get_init_error() != 0) {
        // 查看日志中的失败项
    }
}
```

**注意事项**：
- `EK_DEFINE_*` 只能在文件作用域使用，因为它会展开为 `static` 对象、私有回调和链接器段描述符
- 同一翻译单元内各宏的 `handle` 必须互不相同
- 静态宏展开为 union（控制块与数据连续），句柄是指针，直接传入 API（无需 `&`）；静态对象只走 `*_init_static()` / `*_deinit_static()`，deinit 不释放内存
- 容器类注册 order 为 10，调度对象注册 order 为 20

---

### ek_evoke — 事件驱动调度器

**功能概述**：协作式事件驱动任务调度器。ISR 通过文件内静态 SPSC FIFO 向主循环推送事件请求（发布/广播/延迟），主循环从 FIFO 取出请求、执行就绪任务、管理延迟事件池和睡眠策略。仅裸机可用。

**源文件**：
- `ek_utils/inc/ek_evoke.h`
- `ek_utils/src/ek_evoke.c`

**依赖**：
- ek_def、ek_ringbuf、ek_heap、ek_export、ek_log、ek_assert
- ek_ringbuf_spsc（ISR FIFO 通信）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_EVOKE` | 0 | 模块开关（需 `EKCFG_RTOS=0`） |
| `EKCFG_EVOKE_MIN_DEEPSLEEP_TICK` | 10 | 进入深度睡眠的最小空闲 tick |
| `EKCFG_STATIC_ALLOC` | 0 | 启用后可使用 `EK_DEFINE_EVOKE_TASK` / `EK_DEFINE_EVOKE_EVENT` |

**编译期常量（可覆盖）**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EK_EVOKE_MAX_ISR_REQ` | 10 | ISR 请求队列容量（文件内静态缓冲槽位数） |
| `EK_EVOKE_MAX_DEFER_REQ` | 10 | 延迟请求池容量 |

**用户需实现的接口（5 个弱函数）**：

| 函数 | 作用 | 典型实现 |
|------|------|---------|
| `ek_evoke_enter_critical()` | 进入临界区 | `__disable_irq()` |
| `ek_evoke_exit_critical()` | 退出临界区 | `__enable_irq()` |
| `ek_evoke_set_timer(uint32_t xtick)` | 设置单次定时器 | 设置硬件定时器在 xtick 后触发中断，中断中调用 `ek_evoke_delay_timer_callback()` |
| `ek_evoke_light_sleep()` | 浅睡眠 | `__WFI()` |
| `ek_evoke_deep_sleep()` | 深度睡眠 | 进入低功耗模式（STOP/STANDBY），需配置唤醒源 |

**链接脚本变更**：使用 `EK_EXPORT_LEVEL` 自动初始化则需配置 `.ek_export` 段；使用静态任务/事件宏则需配置 `.ek_static_alloc` 段。

**初始化**：`ek_evoke_init()` 通过 `EK_EXPORT_COMPONENTS` 自动初始化，或手动调用。ISR FIFO 在 `ek_evoke_init()` 中填入文件内静态缓冲，不再动态分配。初始化后通过 `ek_evoke_event_loop()` 进入主循环（永不返回）。

**典型主循环模式**：
```c
int main(void) {
    ek_export_init();         // 自动初始化 heap → evoke

    ek_evoke_task_t *tsk = ek_evoke_task_create(led_cb, NULL);
    ek_evoke_event_t *evt = ek_evoke_event_create(0);
    ek_evoke_event_subscribe(tsk, evt);

    ek_evoke_event_loop();   // 永不返回
}

// 或静态定义，ek_export_init() 后直接使用
EK_DEFINE_EVOKE_TASK(static_task, led_cb, NULL);
EK_DEFINE_EVOKE_EVENT(static_event, 0);
```

**注意事项**：
- `ek_evoke_event_loop()` 永不返回，调用前必须完成所有初始化
- ISR 中的 `*_from_isr` 函数内部使用 `ek_evoke_enter/exit_critical` 保护 FIFO 写入
- 延迟事件通过 `ek_evoke_set_timer()` 设置硬件定时器唤醒
- 睡眠锁（`ek_evoke_sleep_lock/unlock`）用于在关键操作期间禁止深度睡眠
- 任务和事件对象不再保存名字字符串；静态宏第一个参数是 `handle`，不是调试名

---

### ek_picothread — 微线程调度器

**功能概述**：基于 protothread 的协作式微线程调度器。通过 `EK_PT_BEGIN/YEILD/END` 宏在单函数内实现协程式逻辑，支持分级 FIFO 优先级调度和定时阻塞。

**源文件**：
- `ek_utils/inc/ek_picothread.h`
- `ek_utils/src/ek_picothread.c`

**依赖**：
- ek_list、ek_heap、ek_assert、ek_export
- ek_ringbuf（仅在启用 `picothread_msg` 时需要）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_PICOTHREAD` | 0 | 微线程调度器总开关 |
| `EKCFG_PICOTHREAD_SEM` | 0 | 信号量子模块（依赖 PICOTHREAD） |
| `EKCFG_PICOTHREAD_MSG` | 0 | 消息队列子模块（依赖 PICOTHREAD + RINGBUF） |
| `EKCFG_PT_PRIO_LOWEST` | 31 | 最低优先级，就绪队列档位数为该值 + 1，必须 ≤ 31 |
| `EKCFG_STATIC_ALLOC` | 0 | 启用后可使用 `EK_DEFINE_PT` / `EK_DEFINE_PT_SEM` / `EK_DEFINE_PT_MSG` |

**用户需实现的接口**：无 — 无弱函数，纯调度器。

**链接脚本变更**：使用 `EK_EXPORT_LEVEL` 自动初始化则需配置 `.ek_export` 段；使用静态宏则需配置 `.ek_static_alloc` 段。

**初始化**：`ek_pt_init()` 通过 `EK_EXPORT_COMPONENTS`（level=2, order=0）自动初始化，或手动调用。静态任务在 `ek_static_alloc_init()` 中入队，因此必须先完成 `ek_pt_init()`。

**主循环模式**：

每次 `ek_pt_schedule(now)` 只运行一个就绪任务。返回 `uint32_t`，有三种情况：

| 返回值 | 含义 | 主循环行为 |
|--------|------|-----------|
| `== now` | 有就绪任务，不应睡眠 | 继续循环执行调度 |
| `> now` | 返回最近阻塞任务的唤醒 tick | 设置定时器在 `ret - now` tick 后唤醒 |
| `== 0` | 无任何任务 | 可深度睡眠（等待外部中断唤醒） |

```c
static void led_task(ek_pt_t *pt, void *arg) {
    EK_PT_BEGIN(pt);
    while (1) {
        led_toggle();
        EK_PT_DELAY(500); // 阻塞 500 ticks
    }
    EK_PT_END(pt);
}

int main(void) {
    ek_export_init();
    ek_pt_create(led_task, 0, NULL);
    // 或：EK_DEFINE_PT(led_pt, led_task, 0, NULL);

    while (1) {
        uint32_t now = get_tick();
        uint32_t next = ek_pt_schedule(now);
        if (!next) {
            enter_deep_sleep(); // 无任务，深度睡眠
        } else if (next > now) {
            set_timer_once(next - now); // 定时唤醒
            enter_light_sleep();
        }
        // next == now：继续循环
    }
}
```

**注意事项**：
- `EK_PT_BEGIN`/`EK_PT_END` 必须在回调函数体内配对使用，利用 `switch-case` 实现协程恢复
- `EK_PT_YEILD` 保存行号后 `return`，下次调度从此处恢复 — 确保局部变量不跨 `YEILD` 保存状态（用 `static` 变量或结构体成员）
- 优先级数值越小越高，合法范围是 `0 .. EKCFG_PT_PRIO_LOWEST`
- 就绪队列是每级一条 FIFO：入队到 `s_ready_list[prio]` 尾部，用 32-bit 位图取最高优先级队头，同级不会互相饿死
- 阻塞链表按唤醒 tick（绝对时间）排序，tick 相同时按静态优先级排序
- 信号量/消息等待队列仍按静态优先级插入，不走就绪位图
- 任务对象不再保存名字字符串；`ek_pt_create(cb, prio, arg)` / `EK_DEFINE_PT(handle, cb, prio, arg)`
- `ek_pt_destroy` 不能在任务正在运行（`s_cur_pt`）时销毁自身

---

### ek_picothread_sem — 微线程信号量

**功能概述**：微线程间的同步原语，支持计数信号量和超时等待。

**源文件**：无需额外文件 — 实现在 `ek_picothread.h`/`.c` 中，由 `EKCFG_PICOTHREAD_SEM` 守卫控制编译。

**依赖**：
- ek_picothread（`EKCFG_PICOTHREAD=1`）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_PICOTHREAD_SEM` | 0 | 信号量子模块开关 |
| `EKCFG_STATIC_ALLOC` | 0 | 启用后可使用 `EK_DEFINE_PT_SEM` |

**用户需实现的接口**：无。

**链接脚本变更**：无（继承 ek_picothread 的配置）。

**初始化**：
```c
ek_pt_sem_t *sem = ek_pt_sem_create(1); // 初始计数 = 1（二值信号量）
// 或：EK_DEFINE_PT_SEM(static_sem, 1);
```

**使用示例**：
```c
static void producer(ek_pt_t *pt, void *arg) {
    ek_pt_sem_t *sem = (ek_pt_sem_t *)arg;
    int err;
    EK_PT_BEGIN(pt);
    // 获取信号量，超时 1000 ticks
    EK_PT_SEM_TAKE(sem, 1000, err);
    if (err == 0) {
        // 临界区操作
    }
    EK_PT_SEM_GIVE(sem);
    EK_PT_END(pt);
}
```

**注意事项**：
- `EK_PT_SEM_TAKE` 在信号量不可用时阻塞当前任务，超时后 `err == -ETIMEDOUT`
- `EK_PT_SEM_GIVE` 优先唤醒等待队列中优先级最高的任务
- `ek_pt_sem_destroy` 会唤醒所有等待任务，`wait_result` 设为 `-ECANCELED`
- 静态信号量使用 `EK_DEFINE_PT_SEM(handle, count)`，不走堆

---

### ek_picothread_msg — 微线程消息队列

**功能概述**：微线程间的消息传递机制，内部复用内嵌的 `ek_ringbuf_t` 存储消息数据，支持发送/接收超时等待。

**源文件**：无需额外文件 — 实现在 `ek_picothread.h`/`.c` 中，由 `EKCFG_PICOTHREAD_MSG` 守卫控制编译。

**依赖**：
- ek_picothread（`EKCFG_PICOTHREAD=1`）
- ek_ringbuf（`EKCFG_RINGBUF=1`）

**配置宏**：
| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EKCFG_PICOTHREAD_MSG` | 0 | 消息队列子模块开关 |
| `EKCFG_STATIC_ALLOC` | 0 | 启用后可使用 `EK_DEFINE_PT_MSG` |

**用户需实现的接口**：无。

**链接脚本变更**：无（继承 ek_picothread 的配置）。

**初始化**：
```c
ek_pt_msg_t *msg = ek_pt_msg_create(sizeof(my_data_t), 8); // 8 条消息容量
// 或：EK_DEFINE_PT_MSG(static_msg, my_data_t, 8);
```

**使用示例**：
```c
typedef struct { int id; float val; } sensor_data_t;
static void sender(ek_pt_t *pt, void *arg) {
    ek_pt_msg_t *mq = (ek_pt_msg_t *)arg;
    int err;
    sensor_data_t data = { .id = 1, .val = 3.14f };
    EK_PT_BEGIN(pt);
    EK_PT_MSG_SEND(mq, &data, 500, err);
    if (err == -ETIMEDOUT) { /* 超时处理 */ }
    EK_PT_END(pt);
}

static void receiver(ek_pt_t *pt, void *arg) {
    ek_pt_msg_t *mq = (ek_pt_msg_t *)arg;
    int err;
    sensor_data_t data;
    EK_PT_BEGIN(pt);
    EK_PT_MSG_RECV(mq, &data, 1000, err);
    if (err == 0) { /* 处理 data */ }
    EK_PT_END(pt);
}
```

**注意事项**：
- 消息队列满时发送任务阻塞在 send_wait 队列；空时接收任务阻塞在 recv_wait 队列
- `ek_pt_msg_destroy` 会唤醒所有等待任务，`wait_result` 设为 `-ECANCELED`
- 数据按 `item_size` 字节复制（值语义），与 `ek_ringbuf` 行为一致
- 静态消息队列使用 `EK_DEFINE_PT_MSG(handle, type, amount)`，ringbuf 内嵌在 `ek_pt_msg_t` 中，不额外声明独立 ringbuf 对象
