[English](README.en.md) | 简体中文

# EmbeddedKit (ek_platform)

面向资源受限 MCU 的轻量级嵌入式 C 基础库，提供数据结构、内存管理、日志、断言、事件调度（evoke）、协作式微线程（picothread）和 VFS 等模块。零 RTOS 依赖，通过编译期宏按需裁剪。

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

include(${CMAKE_CURRENT_SOURCE_DIR}/lib/ek_platform/cmake/ek_platform.cmake)

# 1. project() 之前：选择架构与是否启用 picolibc
ek_platform_prepare(ARCH cm4 PICOLIBC ON)
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
- **内置启动文件（Cortex-M）**：默认编译 `port/startup/cortexm/ek_startup.S`，**请勿再将厂商 `startup_xxx.s` 加入工程**。需在 `ek_conf.h` 中设置外部中断数 `#define EKCFG_ARCH_IRQ_COUNT (91)`（纯整数，勿加 `U`）。
- **注册外部中断（Cortex-M）**：
  ```c
  EK_DEFINE_ARCH_IRQ(USART0_IRQn, USART0_IRQHandler);      // 静态注册
  ek_arch_irq_install(USART0_IRQn, USART0_IRQHandler);     // 或运行时动态绑定
  ```
- **覆盖弱函数适配硬件**：按需实现同名强函数覆盖默认弱实现：
  ```c
  int ek_port_io_fputc(int ch);                                          // printf 字符输出
  uint32_t ek_port_log_get_tick(void);                                   // 日志时间戳
  void ek_assert_hook(const char *file, uint32_t line, const char *expr); // 断言钩子
  void ek_evoke_enter_critical(void);                                    // evoke 临界区与休眠钩子
  void ek_evoke_exit_critical(void);
  void ek_evoke_set_timer(uint32_t xtick);
  void ek_evoke_light_sleep(void);
  void ek_evoke_deep_sleep(void);
  void *ek_malloc(size_t size);                                          // EKCFG_HEAP_TLSF=0 时自定义堆
  void *ek_realloc(void *ptr, size_t size);
  void *ek_free(void *ptr);
  ```

### 4. 非 CMake 直接集成（可选）

- **Include 路径**：`include/`、`third_party/tlsf/`、`third_party/lwprintf/inc/`，以及 `ek_conf.h`、`ek_soc.h` 所在目录。
- **源文件**：`src/**/*.c`、`third_party/tlsf/tlsf.c`、`third_party/lwprintf/lwprintf.c`，以及对应平台的 `port/arch/*`、`port/clib/*`、`port/startup/cortexm/ek_startup.S`。
- **必需编译宏**（C 与 ASM 均需添加）：`-DEK_CONF_PATH="ek_conf.h"`，Cortex-M 还需 `-DEK_SOC_PATH="ek_soc.h"`。

---

## 可配置项

### 1. CMake 配置项

#### `ek_platform_prepare(ARCH <arch> [PICOLIBC ON|OFF] [TOOLCHAIN <path>])`

| 参数 / 变量 | 可选值 / 说明 |
|---|---|
| `ARCH` | **ARM**: `cm0`, `cm3`, `cm4`, `cm7`, `cm23`, `cm33`, `cm55`<br>**RISC-V**: `rv32imac`, `rv32imafc`, `rv64imac`, `rv64gc`<br>**Host**: `host`, `x86_64` |
| `PICOLIBC` | `ON` / `OFF`（默认 `OFF`）。开启后可通过以下变量之一指定来源：<br>- `PICOLIBC_FETCH=ON`：自动下载对应架构预编译包（缓存于 `build/_deps`）<br>- `PICOLIBC_BUILD=ON`：使用 meson 从源码构建（仅限 ARM）<br>- `PICOLIBC_ROOT="/path"`：使用本地已安装的 picolibc |
| `RISCV_TOOLCHAIN_PREFIX` | RISC-V 编译器不在 `PATH` 时指定前缀（如 `/opt/.../bin/riscv-none-elf-`） |

#### `ek_platform_configure(...)`

| 参数 | 是否必需 | 说明 |
|---|---|---|
| `CONF` | 必需 | `ek_conf.h` 路径 |
| `BOARD` | 交叉编译必需 | `ek_board.h` 路径（`LINKER off` 或 `host` 构建可省略） |
| `SOC` | Cortex-M 必需 | `ek_soc.h` 路径 |
| `SOC_TARGET` | Cortex-M 常用 | 向 `ek_platform` 提供芯片 CMSIS 头文件与编译宏的 CMake target |
| `LINKER` | 可选 | `auto`（默认自动生成链接脚本）或 `off`（多目标时配合 `ek_linker_attach()` 手动指定） |
| `TEMPLATE` | 可选 | 自定义链接脚本模板路径（默认按架构选用 `cmake/linker/*.ld.S`） |

### 2. 功能与模块配置 (`ek_conf.h`)

#### 运行环境与基础服务

| 宏 | 默认值 | 说明 |
|---|---|---|
| `EKCFG_RTOS` | `0` | `0`=裸机，`1`=RTOS（需同时定义 `EK_LOCK_*` 互斥锁宏） |
| `EKCFG_IO_LWPRTF` | `1` | `1`=使用内置 lwprintf（启用 `PICOLIBC=ON` 时自动强制为 `0` 并接管 `printf`） |
| `EKCFG_NEWLIB` | `0` | `1`=启用裸机 newlib/nanolibc 系统调用桩（与 picolibc 互斥） |
| `EKCFG_STATIC_ALLOC` | `1` | 启用 `EK_DEFINE_*` 静态对象链接器段注册 |
| `EKCFG_EXPORT` | `1` | 启用 `EK_EXPORT_*` 自动初始化机制 |
| `EKCFG_ASSERT` | `1` | 启用断言模块 |
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
| `EKCFG_EVOKE` | `0` | 事件驱动协作式调度器（仅裸机） |
| `EKCFG_PICOTHREAD` | `0` | 协作式微线程 |
| `EKCFG_PICOTHREAD_SEM` / `MSG` | `0` | 微线程信号量 / 消息队列（依赖 `EKCFG_PICOTHREAD`） |
| `EKCFG_VFS` | `0` | 虚拟文件系统 |

#### 模块参数配置

| 宏 | 默认值 | 说明 |
|---|---|---|
| `EKCFG_ARCH_IRQ_COUNT` | 无（ARM 必填） | 外部中断数量，汇编器直接求值，须写为 `(91)` 等纯整数形式 |
| `EKCFG_HEAP_TLSF` | `1` | `1`=使用内置 TLSF 分配器，`0`=自定义 `ek_malloc/free/realloc` |
| `EKCFG_HEAP_SECTION` | `".heap"` | 堆内存所在链接器段（如 `".tcmram"`） |
| `EKCFG_HEAP_SIZE` | `(16 * 1024)` | 默认堆大小（字节） |
| `EKCFG_TLSF_FL_INDEX_MAX` | `(24)` | TLSF 一级索引上限（最大块 $2^{\text{FL}}$ 字节） |
| `EKCFG_TLSF_SL_INDEX_COUNT_LOG2` | `(3)` | TLSF 二级索引数量 $\log_2$ |
| `EKCFG_LOG_DEBUG` | `1` | 启用 DEBUG 级别日志输出 |
| `EKCFG_LOG_COLOR` | `0` | 启用 ANSI 颜色日志 |
| `EKCFG_LOG_BUF_SIZE` | `(128)` | 单条日志格式化缓冲区大小（字节） |
| `EKCFG_ASSERT_TINY` | `1` | `1`=精简断言（不含文件名与表达式字符串），`0`=完整断言 |
| `EKCFG_ASSERT_LOG` | `1` | 断言失败时输出日志 |
| `EKCFG_EVOKE_MIN_DEEPSLEEP_TICK` | `(10)` | evoke 进入深度睡眠的最小空闲 tick 阈值 |
| `EKCFG_PT_PRIO_LOWEST` | `(31)` | 微线程最低优先级数值（优先级档位数为该值 + 1） |
| `EKCFG_VFS_MAX_FDS` | `(16)` | VFS 最大同时打开文件描述符数 |
| `EKCFG_VFS_MAX_NODES` | `(30)` | VFS 静态节点池容量 |
| `EKCFG_VFS_NAME_LEN` | `(12)` | VFS 节点名称最大长度 |
| `EKCFG_VFS_TREE_DUMP_UTF8` | `0` | VFS 目录树打印是否使用 UTF-8 制表符 |

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
