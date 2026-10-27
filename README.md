[English](README.en.md) | 简体中文

# EmbeddedKit (ek_platform)

面向资源受限 MCU 的轻量级嵌入式 C 库，提供数据结构、内存管理、日志、事件调度和微线程等基础设施。不依赖 RTOS，可直接用于裸机项目；模块通过配置宏在编译期按需启用。

本文件只说明如何把库集成到你的项目，各模块的 API 见对应头文件。

## 特性

- 零 RTOS 依赖，可直接集成到裸机工程
- 配置驱动：所有模块通过 `ek_conf.h` 开关裁剪，关闭的模块不参与编译
- 支持 GCC、ARM Compiler 5 (armcc)、ARM Compiler 6 (armclang)
- 平台相关接口以 `__EK_WEAK` 声明，用户提供同名强函数即可覆盖

## 目录结构

```
ek_platform/
├── CMakeLists.txt
├── ek_conf_template.h             # 配置模板，复制为 ek_conf.h
├── ek_board_template.h            # 板级内存模板，复制为 ek_board.h
├── cmake/
│   ├── ek_config.cmake            # CMake 配置函数
│   ├── ek_linker.cmake            # 链接脚本生成与注入
│   ├── linker/
│   │   ├── cortexm.ld.S           # Cortex-M 链接脚本模板
│   │   └── riscv.ld.S             # RISC-V 链接脚本模板
│   ├── gcc-arm-none-eabi.cmake    # ARM 交叉编译工具链
│   └── gcc-riscv-none-elf.cmake   # RISC-V 交叉编译工具链
├── include/ek/
│   ├── ek_conf_internal.h         # 配置入口：引入 ek_conf.h、填充默认值、依赖校验
│   ├── core/                      # ek_def / ek_assert / ek_io / ek_log / ek_export
│   ├── mem/                       # ek_heap / ek_static_alloc
│   ├── container/                 # ek_list / ek_vec / ek_ringbuf / ek_stack / ek_snapshot / ek_str / ek_str_view
│   └── sched/                     # ek_evoke / ek_picothread
├── src/                           # 与 include/ek 相同的模块划分
│   ├── core/
│   ├── mem/
│   ├── container/
│   └── sched/
├── port/clib/picolibc/            # picolibc 的 stdout/stderr 适配
├── port/clib/newlib/              # newlib/nanolibc syscall 适配
├── third_party/
│   ├── tlsf/                      # O(1) 内存分配器
│   └── lwprintf/                  # 轻量 printf
├── README.md
└── README.en.md
```

## 支持平台

`PLATFORM` 用于选择工具链 CPU/ISA 参数。ARM 使用 `cmake/gcc-arm-none-eabi.cmake`，RISC-V 使用 `cmake/gcc-riscv-none-elf.cmake`：

| PLATFORM | CPU / ISA | FPU / ABI |
|----------|-----------|-----------|
| cm0 | Cortex-M0 | 无 |
| cm3 | Cortex-M3 | 无 |
| cm4 | Cortex-M4 | fpv4-sp-d16 (hard) |
| cm7 | Cortex-M7 | fpv5-d16 (hard) |
| cm23 | Cortex-M23 | 无 |
| cm33 | Cortex-M33 | fpv5-sp-d16 (hard) |
| cm55 | Cortex-M55 | auto (hard) |
| rv32imac | RISC-V RV32IMAC | ilp32 |
| rv32imafc | RISC-V RV32IMAFC | ilp32f |
| rv64imac | RISC-V RV64IMAC | lp64 |
| rv64gc | RISC-V RV64GC | lp64d |

RISC-V 编译器不在 PATH 时，用 `RISCV_TOOLCHAIN_PREFIX` 指定前缀（包含目录和末尾 `-`）：

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=lib/ek_platform/cmake/gcc-riscv-none-elf.cmake \
      -DPLATFORM=rv32imac \
      -DRISCV_TOOLCHAIN_PREFIX=/opt/xpack-riscv-none-elf-gcc-<version>/bin/riscv-none-elf- \
      -B build
```

`PICOLIBC_BUILD=ON` 会按 `PLATFORM` 自动选择 `-mcpu` / `-mfpu` / `-mfloat-abi`，用 meson 构建一份对应 ABI 的 picolibc，无需预先准备预编译包。

picolibc 预编译包的平台名可以独立用 `PICOLIBC_PLATFORM` 指定（默认同 `PLATFORM`）：

| 架构 | PICOLIBC_PLATFORM |
|------|-------------------|
| ARM | cm0, cm3, cm4, cm7, cm23, cm33, cm55 |
| RISC-V | rv32imac, rv32imafc, rv64imac, rv64gc |
| x86 | x86_64（裸机 freestanding，链接需要 `-no-pie`） |

RISC-V 和 x86 目前通过 `PICOLIBC_FETCH=ON` 使用 CI 发布的预编译包；`PICOLIBC_BUILD=ON` 的本地源码构建只覆盖 ARM。

## 方式一：CMake 子模块（推荐）

### 1. 添加子模块

```bash
git submodule add https://github.com/EmbeddedKitOrg/EK_Platform.git lib/ek_platform
```

### 2. 创建 ek_conf.h 与 ek_board.h

复制两个模板到你的项目：`ek_conf.h` 只保留需要修改的宏，其余保持默认；`ek_board.h` 按芯片手册填写 `EKLD_*` 内存参数：

```bash
cp lib/ek_platform/ek_conf_template.h  Core/Inc/ek_conf.h
cp lib/ek_platform/ek_board_template.h Core/Inc/ek_board.h
```

### 3. 配置 CMake

```cmake
cmake_minimum_required(VERSION 3.20)

# 配置函数必须在 project()/add_subdirectory() 之前 include
include(${CMAKE_CURRENT_SOURCE_DIR}/lib/ek_platform/cmake/ek_config.cmake)

# 目标平台，同时决定下载的 picolibc 预编译包
ek_set_arch(cm4)

# 使用 picolibc 时开启；改用 lwprintf 或系统 libc 时关闭
ek_set_picolibc(ON)

# 自动选择 ARM/RISC-V 工具链
ek_set_toolchain()

project(my_app C)

# picolibc 定位（四选一）：
#   1) 已安装：set(PICOLIBC_ROOT "/path/to/picolibc")
#   2) 工具链自带 --specs=picolibc.specs：无需额外设置
#   3) 从源码构建（需要 meson/ninja，按 PLATFORM 自动选参数）：
#        set(PICOLIBC_BUILD ON)
#        # set(PICOLIBC_SOURCE_DIR "/path/to/picolibc")  # 可选，留空则下载源码包
#   4) 下载预编译包：
#        set(PICOLIBC_FETCH ON)
#        # set(PICOLIBC_SHA256 "<sha256>")  # 可选，提供则校验
# 也可以用 PICOLIBC_SPECS 直接指定 specs 文件。

# 装了系统 picolibc（如 apt install picolibc-arm-none-eabi）时，
# PICOLIBC=ON 会自动检测 /usr/lib/picolibc/<triple>/picolibc.specs。
# 传统 Arm GNU Toolchain / Ubuntu 的 arm-none-eabi-gcc 默认带 newlib，
# 可用 arm-none-eabi-gcc -print-file-name=picolibc.specs 确认。
# specs 默认路径与实际安装位置不同时，用 PICOLIBC_PREFIX 指定前缀。

ek_set_conf_file("${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_conf.h")
ek_set_linker_file("${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_board.h")

add_subdirectory(lib/ek_platform)

target_link_libraries(${PROJECT_NAME} PRIVATE ek_platform)
```

`ek_set_arch()` / `ek_set_picolibc()` 必须在 `project()` 之前调用；`ek_set_conf_file()` / `ek_set_linker_file()` 必须在 `add_subdirectory(ek_platform)` 之前调用。

`ek_platform` 是 OBJECT 库，它的 PUBLIC include 目录和链接项会自动传给主工程，不需要手动添加 `include/` 等路径，也不要用 `$<TARGET_OBJECTS:ek_platform>` 代替 `target_link_libraries`。

下载和源码构建的产物都缓存在 `<build>/_deps/`：归档已存在（且 SHA 匹配）时不再下载，解压目录完整时不再解压，源码构建已完成时不再重新构建。删除对应目录即可强制刷新。

### 4. 链接脚本

链接脚本由 `ek_platform` 自动生成，不需要手写。第 2 步复制出的 `ek_board.h` 提供内存布局，CMake 里通过 `EK_BOARD_PATH` 指向它（上面的 `ek_set_linker_file()` 已设置）。

`EK_LINKER_AUTO` 默认开启：库会用 C 预处理器把 `ek_conf.h`、`ek_conf_internal.h`、`ek_board.h` 和 `cmake/linker/cortexm.ld.S` 合成为 `${build}/ek_platform/ek_platform.ld`，并通过 `INTERFACE_LINK_OPTIONS` / `INTERFACE_LINK_DEPENDS` 自动挂到链接 `ek_platform` 的目标上。生成脚本已经包含 `.ek_export`、`.ek_static_alloc`、TCMRAM 以及 SDRAM0/SDRAM1 段，`EKCFG_EXPORT` / `EKCFG_STATIC_ALLOC` 关闭时对应段会自动省略。

若 `ek_conf.h` 在 RTOS 配置里 include 了其他头，把对应目录加入 `EK_LINKER_INCLUDE_DIRS`；预处理器仍只保留宏，不会把 C 声明写进 `.ld`。

RISC-V 项目调用 `ek_set_arch(rv32imac)` 后再调用 `ek_set_toolchain()`，库会自动选择 `cmake/gcc-riscv-none-elf.cmake` 和 `cmake/linker/riscv.ld.S`；需要自定义链接模板时才调用 `ek_set_linker_template(...)`。该模板默认入口是 `_start`，TCMRAM 为可选段（`EKLD_TCMRAM_LENGTH` 为 0 时不生成）。

多 target 或多板子时关闭自动模式，给每个目标单独生成：

```cmake
set(EK_LINKER_AUTO OFF)
...
ek_linker_attach(app BOARD_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_board_a.h")
```

不使用 CMake 集成时，可以用 `${CMAKE_C_COMPILER} -E -P -x c` 手动预处理 `cmake/linker/cortexm.ld.S`，或直接参考生成结果。

### 5. 初始化

启用 `EKCFG_EXPORT=1` 时，`ek_export_init()` 标记为 `__attribute__((constructor))`，由启动代码的 `__libc_init_array()` 在 `main()` 之前自动调用，堆、IO、调度器等模块会按 `level → order` 自动初始化。不要重复调用：

```c
int main(void)
{
    // ek_export_init() 已由 __libc_init_array() 自动调用
    // ...
}
```

未启用 export 时，按依赖顺序手动调用：先 `ek_heap_init()`，再 `ek_io_init()` 及其他模块的 `*_init()`，最后 `ek_static_alloc_init()`。

## 方式二：源文件直接集成

不使用 CMake 时，把下面的路径加入 include 搜索，按需把源文件加入编译：

```
Include Paths:
  - include/
  - third_party/tlsf/
  - third_party/lwprintf/inc/
  - <picolibc 安装目录>/include/          （使用 picolibc 时，由你自己提供）

源文件（按需选择）:
  - src/core/*.c
  - src/mem/*.c
  - src/container/*.c
  - src/sched/*.c
  - port/clib/picolibc/picolibc_port.c   （使用 picolibc 时）

第三方源文件（始终编译，由 #if 守卫控制实际使用）:
  - third_party/tlsf/tlsf.c
  - third_party/lwprintf/lwprintf.c
```

picolibc 不再随仓库分发。用 CMake 时见上方 `PICOLIBC_ROOT` / `PICOLIBC_FETCH`；手工编译时用 `--specs=picolibc.specs --picolibc-prefix=<安装前缀>`，或自行添加 `-I`/`-L`。

源文件通过 `#include "ek/ek_conf_internal.h"` 引入配置，因此 `include/` 这一级必须在搜索路径中。

## 配置

### 指定 ek_conf.h 的两种方式

`ek_conf_internal.h` 的引入逻辑：

```c
#ifdef EK_CONF_PATH
#    include EK_CONF_PATH   // 方式 A：构建系统传入的路径
#else
#    include "ek_conf.h"    // 方式 B：标准 include 搜索
#endif
```

**方式 A：`ek_set_conf_file()` / `EK_CONF_PATH`（推荐）**

先 include 配置函数，再在 `add_subdirectory` 之前指定：

```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/lib/ek_platform/cmake/ek_config.cmake)
ek_set_conf_file("${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_conf.h")
add_subdirectory(lib/ek_platform)
```

`ek_set_conf_file()` 写入的就是 `EK_CONF_PATH` cache；等价的底层写法：

```cmake
set(EK_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_conf.h"
    CACHE STRING "" FORCE)
add_subdirectory(lib/ek_platform)
```

库的 CMakeLists 会把 `EK_CONF_PATH` 作为编译宏传给编译器。

也可以用命令行传入：

```bash
cmake -DEK_CONF_PATH="$(pwd)/Core/Inc/ek_conf.h" -B build
```

务必使用绝对路径。`#include EK_CONF_PATH` 展开为引号形式后，相对路径会先按 `include/ek/` 解析，容易找不到文件。

**方式 B：include 搜索路径**

不设置 `EK_CONF_PATH` 时，把存放 `ek_conf.h` 的目录加给库目标。OBJECT 库编译自己的源文件时也需要这个路径，因此要用 `PUBLIC`：

```cmake
add_subdirectory(lib/ek_platform)
target_include_directories(ek_platform PUBLIC Core/Inc)
```

只给主 target 设置 `PRIVATE` include 不会传播给子项目。

### IO 后端选择

`ek_io.h` 按以下优先级选择后端：`EKCFG_IO_LWPRTF=1` → lwprintf；否则 `EKCFG_PICOLIBC=1` → picolibc；否则使用系统 libc（glibc/newlib 的 `printf`）。

| 后端 | ek_conf.h 配置 | `ek_printf` 等宏 | 需要实现 |
|------|---------------|------------------|----------|
| picolibc（内部默认） | `EKCFG_PICOLIBC=1` | 自动映射到 `printf` / `vsnprintf` | `int ek_port_io_fputc(int ch)` |
| lwprintf（模板默认） | `EKCFG_IO_LWPRTF=1`、`EKCFG_PICOLIBC=0` | 自动映射到 `lwprintf` / `lwvsnprintf` | `int ek_port_io_fputc(int ch)` |
| 裸机 newlib/nanolibc | `EKCFG_NEWLIB=1`、`EKCFG_PICOLIBC=0`、`EKCFG_IO_LWPRTF=0` | 自动映射到系统 `printf` | `_write` 等由 `port/clib/newlib/newlib_port.c` 提供，可覆盖 `ek_port_io_fputc` |
| 系统 libc（host） | `EKCFG_PICOLIBC=0`、`EKCFG_IO_LWPRTF=0`、`EKCFG_NEWLIB=0` | 自动映射到系统 `printf` | 无 |

`EKCFG_PICOLIBC` 与 `EKCFG_NEWLIB` 互斥。裸机 newlib 的 syscall port 提供弱符号 `_write` / `_read` / `_close` / `_fstat` / `_isatty` / `_lseek` / `_sbrk`，BSP 可按需覆盖；`_sbrk` 使用链接脚本的 `__heap_start` / `__heap_end`。

`EKCFG_PICOLIBC=1` 时，`EKCFG_IO_LWPRTF` 会被强制设为 0，不需要手动关闭。

### 所有配置宏

完整列表和默认值见 `ek_conf_template.h`。未在 `ek_conf.h` 中定义的宏会自动取默认值，未满足的依赖关系会在编译期报错（例如开启 `EKCFG_PICOTHREAD_SEM` 但没有开启 `EKCFG_PICOTHREAD`，或在 RTOS 模式下开启 `EKCFG_EVOKE`）。

## 平台适配：需要实现的弱函数

以下函数都有弱实现，按使用的后端和模块提供同名强函数即可：

```c
// ek_io：字符输出（lwprintf / picolibc 后端需要）
int ek_port_io_fputc(int ch);

// ek_log：系统时间戳
uint32_t ek_port_log_get_tick(void);

// ek_evoke：临界区、定时器和睡眠
void ek_evoke_enter_critical(void);
void ek_evoke_exit_critical(void);
void ek_evoke_set_timer(uint32_t xtick);
void ek_evoke_light_sleep(void);
void ek_evoke_deep_sleep(void);

// ek_assert：断言失败钩子（full 模式）
void ek_assert_hook(const char *file, uint32_t line, const char *expr);

// ek_heap：仅在 EKCFG_HEAP_TLSF=0 时需要
void *ek_malloc(size_t size);
void *ek_realloc(void *ptr, size_t size);
void ek_free(void *ptr);
```

以 STM32 为例：

```c
// ek_port.c
#include "ek/core/ek_io.h"
#include "ek/core/ek_log.h"

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

## 第三方依赖

| 库 | 用途 | 许可证 |
|----|------|--------|
| [TLSF](http://tlsf.baisoku.org) v3.1 | 实时内存分配器（O(1) 分配/释放） | BSD-style |
| [lwprintf](https://github.com/MaJerle/lwprintf) | 轻量 printf 实现 | MIT |
| [picolibc](https://github.com/picolibc/picolibc) | 嵌入式 C 标准库，含 Cortex-M 预编译库 | BSD |

## 许可

本项目原创代码采用 MIT 许可证，见 [LICENSE](LICENSE)。`third_party/` 下的代码遵循各自许可证。
