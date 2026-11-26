[English](README.en.md) | 简体中文

# EmbeddedKit (ek_platform)

面向资源受限 MCU 的轻量级嵌入式 C 基础库（v7），提供数据结构、TLSF 内存管理、分级日志、断言、事件驱动调度器（evoke）、协作式微线程（picothread）、VFS 虚拟文件系统、嵌入式命令行解释器（Shell），以及内置无滴答抢占式 RTOS 内核（线程、信号量、带优先级继承的互斥量、支持 Wait Morphing 的条件变量、屏障、消息队列、侵入式零拷贝 kfifo、支持延时调度与中断线程化的工作队列、栈溢出检测与高水位线）。支持裸机与内置 RTOS 两种模式，零第三方 RTOS 依赖，通过编译期宏按需裁剪。

## 快速使用（CMake）

### 1. 准备配置文件

```bash
git submodule add https://github.com/EmbeddedKitOrg/EK_Platform.git lib/ek_platform

cp lib/ek_platform/ek_conf_template.h  include/ek_conf.h
cp lib/ek_platform/ek_board_template.h include/ek_board.h
```

- `ek_conf.h`：模块开关与参数配置（未定义的宏自动取默认值）。
- `ek_board.h`：芯片 Flash/RAM 布局，用于自动生成链接脚本（`host` 构建不需要）。
- `ek_soc.h`（Cortex-M 必需）：自行创建，在其中包含芯片 CMSIS 头文件（如 `#include "gd32f4xx.h"`）。

### 2. 编写 `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/lib/ek_platform/cmake/platform.cmake)

# 1. project() 之前：选择架构、picolibc 与默认构建模式
ek_platform_prepare(ARCH cm4 PICOLIBC ON BUILD_TYPE Debug)
set(PICOLIBC_FETCH ON) # 自动下载预编译 picolibc

project(my_app LANGUAGES C ASM)

add_executable(${PROJECT_NAME} src/main.c)

# 2. add_subdirectory(ek_platform) 之前：绑定配置文件与 SoC 依赖
ek_platform_configure(
    CONF "${CMAKE_CURRENT_SOURCE_DIR}/include/ek_conf.h"
    BOARD "${CMAKE_CURRENT_SOURCE_DIR}/include/ek_board.h"
    SOC "${CMAKE_CURRENT_SOURCE_DIR}/include/ek_soc.h"
    SOC_TARGET gd32f4xx_driver # 提供芯片 CMSIS 头文件路径与宏定义的 target
)
add_subdirectory(lib/ek_platform)

target_link_libraries(${PROJECT_NAME} PRIVATE ek_platform gd32f4xx_driver)
```

### 3. 启动、中断与平台适配

- **自动初始化**：启用 `EKCFG_EXPORT=1`（默认）时，进入 `main()` 前已自动完成堆、IO、静态对象及 `EK_EXPORT_*` 导出函数的初始化，无需手动调用。
- **内置启动文件（Cortex-M）**：默认编译 `port/startup/cortexm/startup.S`，**请勿再将厂商 `startup_xxx.s` 加入工程**。需在 `ek_conf.h` 中设置外部中断数 `#define EKCFG_ARCH_IRQ_COUNT (91)`（纯整数，勿加 `U`）。
- **注册外部中断（Cortex-M）**：
  ```c
  EK_DEFINE_ARCH_IRQ(USART0_IRQn, USART0_IRQHandler);          // 静态注册
  ek_arch_irq_install(USART0_IRQn, USART0_IRQHandler);         // 或运行时动态绑定
  EK_DEFINE_OS_WORKQ_ISR(TIMER6_IRQn, top_fn, bottom_fn);      // 或中断线程化注册（EKCFG_OS_ISR_WORKQ=1）
  ```
- **标准 IO 适配（`ek/core/io.h`）**：
  - **开启 VFS（`EKCFG_VFS=1`）**：标准流直接路由到 VFS `fd 0/1/2`；在 `ek_conf.h` 中定义 `#define EKCFG_IO_CONSOLE_PATH "/dev/uart0"` 可在 `ek_io_init()` 启动阶段自动绑定 `stdin/stdout/stderr`，运行期可通过 `ek_vfs_bind_fd()` / `ek_vfs_bind_stdio()` 热重定向。
  - **关闭 VFS（`EKCFG_VFS=0`）**：通过 `ek_io_set_ops(&ops)` 注册 `ek_io_ops_t`（`put_ch` / `get_ch` / `write` / `read`）即可接管 `ek_printf` / `printf`。
- **接入内置 RTOS（`EKCFG_RTOS=1`）**：实现 `ek_os_timer_ops_t`（`now` 与 `set_timeout`），在定时闹钟中断中调用 `ek_os_kernel_announce_isr()`，并在 `main()` 中通过 `ek_os_kernel_start(&timer_ops)` 启动调度器。
- **覆盖弱函数适配硬件**：按需实现同名强函数覆盖默认弱实现：
  ```c
  uint32_t ek_port_log_get_tick(void);                                    // 日志时间戳
  void ek_assert_hook(const char *file, uint32_t line, const char *expr); // 断言钩子（默认调用 abort()）
  void ek_os_kernel_idle_hook(void);                                      // RTOS 空闲线程钩子
  void ek_os_kernel_stackoverflow_hook(int tid, const char *name);        // RTOS 栈溢出钩子（CHECK > 0）
  void *ek_malloc(size_t size);                                           // EKCFG_HEAP_TLSF=0 时自定义堆
  void *ek_realloc(void *ptr, size_t size);
  void ek_free(void *ptr);
  ```

### 4. 非 CMake 直接集成（可选）

- **Include 路径**：`include/`、`third_party/tlsf/`、`third_party/lwprintf/inc/`，以及 `ek_conf.h`、`ek_soc.h` 所在目录。
- **源文件**：`src/**/*.c`、`third_party/tlsf/tlsf.c`、`third_party/lwprintf/lwprintf.c`，以及对应平台的 `port/arch/*`、`port/clib/*`、`port/startup/cortexm/startup.S`。
- **必需编译宏**（C 与 ASM 均需添加）：`-DEK_CONF_PATH="ek_conf.h"`，Cortex-M 还需 `-DEK_SOC_PATH="ek_soc.h"`。

---

## 可配置项

### 1. CMake 配置项

#### `ek_platform_prepare(ARCH <arch> [PICOLIBC ON|OFF] [BUILD_TYPE Debug|Release|RelWithDebInfo|MinSizeRel] [TOOLCHAIN <path>])`

| 参数 / 变量 | 可选值 / 说明 |
|---|---|
| `ARCH` | **ARM**: `cm0`, `cm3`, `cm4`, `cm7`, `cm23`, `cm33`, `cm55`<br>**RISC-V**: `rv32imac`, `rv32imafc`, `rv64imac`, `rv64gc`<br>**Host**: `host`, `x86_64` |
| `PICOLIBC` | `ON` / `OFF`（默认 `OFF`）。开启后可通过以下变量之一指定来源：<br>- `PICOLIBC_FETCH=ON`：自动下载对应架构预编译包（缓存于 `build/_deps`）<br>- `PICOLIBC_BUILD=ON`：使用 meson 从源码构建（仅限 ARM）<br>- `PICOLIBC_ROOT="/path"`：使用本地已安装的 picolibc |
| `BUILD_TYPE` | 可选，默认 `Debug`（命令行 `-DCMAKE_BUILD_TYPE=...` 优先级更高）。自动注入 `EK_PLATFORM_DEBUG` 联动 `EKCFG_DEBUG`、`EKCFG_ASSERT` 与 `EKCFG_LOG_DEBUG` 默认值，业务代码可用 `EK_IS_DEBUG`、`EK_DEBUG_ONLY(...)`、`ek_is_debug()`、`ek_build_mode_str()` |
| `RISCV_TOOLCHAIN_PREFIX` | RISC-V 编译器不在 `PATH` 时指定前缀（如 `/opt/.../bin/riscv-none-elf-`） |

#### `ek_platform_configure(...)`

| 参数 | 是否必需 | 说明 |
|---|---|---|
| `CONF` | 必需 | `ek_conf.h` 路径 |
| `BOARD` | 交叉编译必需 | `ek_board.h` 路径（`LINKER off` 或 `host` 构建可省略） |
| `SOC` | Cortex-M 必需 | `ek_soc.h` 路径 |
| `SOC_TARGET` | Cortex-M 常用 | 向 `ek_platform` 提供芯片 CMSIS 头文件路径与编译宏的 CMake target |
| `LINKER` | 可选 | `auto`（默认自动生成链接脚本）或 `off`（多目标时配合 `ek_linker_attach()` 手动指定） |
| `TEMPLATE` | 可选 | 自定义链接脚本模板路径（默认按架构选用 `cmake/linker/*.ld.S`） |

### 2. 功能与模块配置 (`ek_conf.h`)

#### 运行环境与基础服务

| 宏 | 默认值 | 说明 |
|---|---|---|
| `EKCFG_RTOS` | `0` | `0`=裸机，`1`=启用内置无滴答 RTOS 内核（依赖 `EKCFG_ARCH=1` 与 `EKCFG_LIST=1`，与 `EKCFG_EVOKE` 互斥；`ek_malloc/free/realloc` 自动提供线程安全；提供 `thread`、`sem`、`mutex`、`condvar`、`barrier`、`msgq`、`kfifo`、`workq` 原语） |
| `EKCFG_PICOLIBC` | `1` | `1`=使用 picolibc 适配层（CMake `PICOLIBC` 选项优先级更高；开启时自动关闭 `EKCFG_NEWLIB` 与 `EKCFG_IO_LWPRTF`） |
| `EKCFG_NEWLIB` | `0` | `1`=启用裸机 newlib/nanolibc 系统调用桩（与 `EKCFG_PICOLIBC` 互斥） |
| `EKCFG_IO_LWPRTF` | `0` | `1`=使用内置 lwprintf（启用 `EKCFG_PICOLIBC=1` 时自动强制为 `0` 并由标准 `printf` 接管） |
| `EKCFG_IO_CONSOLE_PATH` | 未定义（可选） | 开启 `EKCFG_VFS=1` 时指定默认控制台设备路径（如 `"/dev/uart0"`），启动时自动绑定 `stdin/stdout/stderr` |
| `EKCFG_STATIC_ALLOC` | `1` | 启用 `EK_DEFINE_*` 静态对象链接器段注册 |
| `EKCFG_EXPORT` | `1` | 启用 `EK_EXPORT_*` 自动初始化机制 |
| `EKCFG_ASSERT` | `EKCFG_DEBUG` | 启用断言模块（未显式配置时默认跟随 Debug 模式：`Debug=1`, `Release=0`） |
| `EKCFG_LOG` | `1` | 启用分级日志模块 |
| `EKCFG_ARCH` | `1` | 启用硬件架构抽象层（中断管理、临界区、VTOR 重定向等） |

#### 模块开关（`1`=启用，`0`=关闭）

| 宏 | 默认值 | 说明 |
|---|---|---|
| `EKCFG_STR` / `EKCFG_STR_VIEW` | `0` | 动态字符串 / 只读字符串视图 |
| `EKCFG_LIST` | `0` | 侵入式双向循环链表 |
| `EKCFG_VEC` | `0` | 类型安全动态数组 |
| `EKCFG_RINGBUF` / `EKCFG_RINGBUF_SPSC` | `0` | 通用环形缓冲区 / SPSC 无锁环形缓冲区 |
| `EKCFG_STACK` | `0` | 通用栈 |
| `EKCFG_SNAPSHOT` | `0` | 状态快照组件 |
| `EKCFG_BITSTREAM` | `0` | 比特流读写 |
| `EKCFG_SHARED_PTR` | `0` | 引用计数共享指针 |
| `EKCFG_EVOKE` | `0` | 事件驱动协作式调度器（仅裸机，依赖 `EKCFG_LIST` 与 `EKCFG_RINGBUF_SPSC`，与 `EKCFG_RTOS` 互斥；平台回调通过 `ek_evoke_ops_t` 传入） |
| `EKCFG_PICOTHREAD` | `0` | 协作式微线程（依赖 `EKCFG_LIST`） |
| `EKCFG_PICOTHREAD_SEM` / `MSG` | `0` | 微线程信号量 / 消息队列（依赖 `EKCFG_PICOTHREAD`，消息队列另依赖 `EKCFG_RINGBUF`） |
| `EKCFG_VFS` | `0` | 虚拟文件系统（含 `vfs_bus` 静态总线/设备树注册与 `ek_vfs_bind_fd` 标准流绑定） |
| `EKCFG_SHELL` | `0` | 嵌入式命令行解释器（依赖 `EKCFG_VFS=1`，支持 `.ek_shell_cmd` 静态导出、动态命令注册与 VT100 行编辑） |

#### 模块参数配置

| 宏 | 默认值 | 说明 |
|---|---|---|
| `EKCFG_ARCH_IRQ_COUNT` | 无（ARM 必填） | 外部中断数量，汇编器直接求值，须写为 `(91)` 等纯整数形式（`1..240`） |
| `EKCFG_HEAP_TLSF` | `1` | `1`=使用内置 TLSF 分配器，`0`=自定义 `ek_malloc/free/realloc` |
| `EKCFG_HEAP_SECTION` | 未定义（可选） | 定义后将默认堆内存放入指定链接器段（如 `".heap"` 或 `".tcmram"`） |
| `EKCFG_HEAP_SIZE` | `(30 * 1024)` | 默认堆大小（字节） |
| `EKCFG_HEAP_MAX_POOL_SIZE` | `EKCFG_HEAP_SIZE` | 单个内存池最大容量（字节），多池时可显式覆盖 |
| `EKCFG_TLSF_FL_INDEX_MAX` | 按 `EKCFG_HEAP_MAX_POOL_SIZE` 自动推导 | TLSF 一级索引上限，有效范围 `[10, 30]`（最大块 $2^{\text{FL}}$ 字节） |
| `EKCFG_TLSF_SL_INDEX_COUNT_LOG2` | 按 `EKCFG_HEAP_SIZE` 自动推导 | TLSF 二级索引数量 $\log_2$，有效范围 `[1, 5]` |
| `EKCFG_LOG_DEBUG` | `EKCFG_DEBUG` | 启用 DEBUG 级别日志输出（默认跟随 `EKCFG_DEBUG`） |
| `EKCFG_LOG_COLOR` | `1` | 启用 ANSI 颜色日志 |
| `EKCFG_LOG_BUF_SIZE` | `(256)` | 单条日志格式化缓冲区大小（字节） |
| `EKCFG_ASSERT_TINY` | `EKCFG_RELEASE` | `1`=精简断言（不含文件名与表达式字符串），`0`=完整断言（默认 Debug=`0`, Release=`1`） |
| `EKCFG_ASSERT_LOG` | `EKCFG_DEBUG` | 断言失败时输出日志（默认跟随 `EKCFG_DEBUG`） |
| `EKCFG_EVOKE_MIN_DEEPSLEEP_TICK` | `(10)` | evoke 进入深度睡眠的最小空闲 tick 阈值 |
| `EKCFG_PT_PRIO_LOWEST` | `(31)` | 微线程最低优先级数值（`<= 31`，优先级档位数为该值 + 1） |
| `EKCFG_VFS_MAX_FDS` | `(16)` | VFS 最大同时打开文件描述符数（`<= 32`） |
| `EKCFG_VFS_MAX_NODES` | `(30)` | VFS 静态节点池容量 |
| `EKCFG_VFS_NAME_LEN` | `(12)` | VFS 节点名称最大长度（须为 `4` 的倍数） |
| `EKCFG_VFS_TREE_DUMP_UTF8` | `0` | VFS 目录树打印是否使用 UTF-8 制表符 |
| `EKCFG_SHELL_BUF_SIZE` | `(64)` | Shell 行编辑缓冲区大小（字节） |
| `EKCFG_SHELL_PROMPT` | `"ek>"` | Shell 交互提示符 |
| `EKCFG_SHELL_CMD_MAX_ARGS` | `(8)` | 单条 Shell 命令最大参数数量（`argc` 上限） |
| `EKCFG_SHELL_UTF8` | `0` | 是否启用 Shell UTF-8 多字节显示与删除支持 |
| `EKCFG_SHELL_BUILTIN_CMDS` | `0` | 是否启用内置命令（`hello` / `echo` / `tree` / `clear` / `version` / `help`） |
| `EKCFG_OS_KERNEL_TIMEBASE_HZ` | `(1000000U)` | RTOS 内核时基频率（Hz） |
| `EKCFG_OS_KERNEL_STACKOVERFLOW_CHECK` | `(0U)` | 栈溢出检测级别：`0`=关闭，`1`=SP 越界/对齐检查，`2`=魔数染色哨兵 + 高水位线统计 |
| `EKCFG_OS_THREAD_NAME_LEN` | `(16)` | RTOS 线程名最大长度 |
| `EKCFG_OS_THREADS_MAX` | `(16)` | RTOS 最大并发线程数（`<= 64`） |
| `EKCFG_OS_THREAD_MIN_STACK` | `(256)` | RTOS 线程最小栈大小（字节） |
| `EKCFG_OS_THREAD_MIN_PRIO` | `(31U)` | RTOS 线程最低优先级（`< 32`，就绪档位数为该值 + 1） |
| `EKCFG_OS_IDLE_THREAD_STACK` | `(512)` | RTOS 空闲线程栈大小（字节） |
| `EKCFG_OS_ISR_WORKQ` | `0` | 是否启用内核内置 ISR 工作队列（`1`=在 `ek_os_kernel_start()` 自动创建 `"isr workq"` 线程，配合 `EK_DEFINE_OS_WORKQ_ISR` 使用） |
| `EKCFG_OS_ISR_WORKQ_PRIO` | `(0U)` | 内核内置 ISR 工作队列线程优先级 |
| `EKCFG_OS_ISR_WORKQ_STACK` | `(1024U)` | 内核内置 ISR 工作队列线程栈大小（字节） |

### 3. 板级内存布局 (`ek_board.h`)

| 宏 | 说明 |
|---|---|
| `EKLD_FLASH_ORIGIN` / `EKLD_FLASH_LENGTH` | Flash 起始地址与大小 |
| `EKLD_RAM_ORIGIN` / `EKLD_RAM_LENGTH` | 主 RAM 起始地址与大小 |
| `EKLD_TCMRAM_ORIGIN` / `EKLD_TCMRAM_LENGTH` | TCMRAM 起始地址与大小（无则将 `LENGTH` 设为 `0`） |
| `EKLD_SDRAM0_ORIGIN` / `EKLD_SDRAM0_LENGTH` | 外部 SDRAM0 起始地址与大小（无则设为 `0`） |
| `EKLD_SDRAM1_ORIGIN` / `EKLD_SDRAM1_LENGTH` | 外部 SDRAM1 起始地址与大小（无则设为 `0`） |
| `EKLD_STACK_SIZE` / `EKLD_MIN_HEAP_SIZE` | 主栈大小与系统最小堆大小 |

## 第三方依赖

| 库 | 用途 | 许可证 |
|----|------|--------|
| [TLSF](http://tlsf.baisoku.org) v3.1 | 实时内存分配器（O(1) 分配/释放） | BSD-style |
| [lwprintf](https://github.com/MaJerle/lwprintf) | 轻量 printf 实现 | MIT |
| [picolibc](https://github.com/picolibc/picolibc) | 嵌入式 C 标准库 | BSD |

## 许可

本项目原创代码采用 MIT 许可证，见 [LICENSE](LICENSE)。`third_party/` 下的代码遵循各自许可证。
