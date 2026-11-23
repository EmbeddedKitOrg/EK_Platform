English | [简体中文](README.md)

# EmbeddedKit (ek_platform)

A lightweight embedded C foundation library (v7) for resource-constrained MCUs, providing data structures, TLSF memory management, leveled logging, assertions, an event-driven scheduler (evoke), cooperative microthreads (picothread), a Virtual File System (VFS), an embedded command-line Shell, and a built-in tickless preemptive RTOS kernel (threads, semaphores, mutexes with priority inheritance, condition variables with Wait Morphing, barriers, message queues, zero-copy intrusive kfifo, stack overflow detection, and stack high-water-mark tracking). Supports both bare-metal and built-in RTOS modes with zero third-party RTOS dependencies, tailored on demand at compile time via configuration macros.

## Quick Start (CMake)

### 1. Prepare Configuration Files

```bash
git submodule add https://github.com/EmbeddedKitOrg/EK_Platform.git lib/ek_platform

cp lib/ek_platform/ek_conf_template.h  include/ek_conf.h
cp lib/ek_platform/ek_board_template.h include/ek_board.h
```

- `ek_conf.h`: Module switches and parameter settings (undefined macros automatically fall back to defaults).
- `ek_board.h`: Chip Flash/RAM layout used to auto-generate the linker script (not needed for `host` builds).
- `ek_soc.h` (required for Cortex-M): Create this file and include your chip's CMSIS header (e.g., `#include "gd32f4xx.h"`).

### 2. Write `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/lib/ek_platform/cmake/platform.cmake)

# 1. Before project(): select architecture, picolibc, and default build type
ek_platform_prepare(ARCH cm4 PICOLIBC ON BUILD_TYPE Debug)
set(PICOLIBC_FETCH ON) # Automatically download prebuilt picolibc

project(my_app LANGUAGES C ASM)

add_executable(${PROJECT_NAME} src/main.c)

# 2. Before add_subdirectory(ek_platform): bind config files and SoC target
ek_platform_configure(
    CONF "${CMAKE_CURRENT_SOURCE_DIR}/include/ek_conf.h"
    BOARD "${CMAKE_CURRENT_SOURCE_DIR}/include/ek_board.h"
    SOC "${CMAKE_CURRENT_SOURCE_DIR}/include/ek_soc.h"
    SOC_TARGET gd32f4xx_driver # Target providing chip CMSIS include paths and compile definitions
)
add_subdirectory(lib/ek_platform)

target_link_libraries(${PROJECT_NAME} PRIVATE ek_platform gd32f4xx_driver)
```

### 3. Startup, Interrupts, and Platform Hooks

- **Auto-initialization**: When `EKCFG_EXPORT=1` (default), the heap, IO, static objects, and `EK_EXPORT_*` functions are initialized automatically before `main()`. Do not call them manually.
- **Built-in Startup (Cortex-M)**: Compiles `port/startup/cortexm/startup.S` by default; **do not add the vendor `startup_xxx.s` to your project**. Set the external interrupt count in `ek_conf.h` via `#define EKCFG_ARCH_IRQ_COUNT (91)` (plain integer expression, no `U` suffix).
- **Register External Interrupts (Cortex-M)**:
  ```c
  EK_DEFINE_ARCH_IRQ(USART0_IRQn, USART0_IRQHandler);      // Static registration
  ek_arch_irq_install(USART0_IRQn, USART0_IRQHandler);     // Or dynamic runtime binding
  ```
- **Standard I/O Adaptation (`ek/core/io.h`)**:
  - **With VFS (`EKCFG_VFS=1`)**: Standard streams route directly to VFS `fd 0/1/2`. Defining `#define EKCFG_IO_CONSOLE_PATH "/dev/uart0"` in `ek_conf.h` automatically binds `stdin/stdout/stderr` during `ek_io_init()`, and streams can be hot-rebound at runtime via `ek_vfs_bind_fd()` / `ek_vfs_bind_stdio()`.
  - **Without VFS (`EKCFG_VFS=0`)**: Register an `ek_io_ops_t` table (`put_ch` / `get_ch` / `write` / `read`) via `ek_io_set_ops(&ops)` to handle `ek_printf` / `printf`.
- **Built-in RTOS Integration (`EKCFG_RTOS=1`)**: Implement `ek_os_timer_ops_t` (`now` and `set_timeout`), call `ek_os_kernel_announce_isr()` from your hardware timer alarm ISR, and start the scheduler with `ek_os_kernel_start(&timer_ops)` in `main()`.
- **Override Weak Functions for Hardware Adaptation**: Define strong functions with the same name as needed:
  ```c
  uint32_t ek_port_log_get_tick(void);                                    // Log timestamp
  void ek_assert_hook(const char *file, uint32_t line, const char *expr); // Assertion hook (calls abort() by default)
  void ek_os_kernel_idle_hook(void);                                      // RTOS idle thread hook
  void ek_os_kernel_stackoverflow_hook(int tid, const char *name);        // RTOS stack overflow hook (CHECK > 0)
  void *ek_malloc(size_t size);                                           // Custom heap when EKCFG_HEAP_TLSF=0
  void *ek_realloc(void *ptr, size_t size);
  void ek_free(void *ptr);
  ```

### 4. Direct Source Integration without CMake (Optional)

- **Include Paths**: `include/`, `third_party/tlsf/`, `third_party/lwprintf/inc/`, plus the directories containing `ek_conf.h` and `ek_soc.h`.
- **Sources**: `src/**/*.c`, `third_party/tlsf/tlsf.c`, `third_party/lwprintf/lwprintf.c`, and the matching `port/arch/*`, `port/clib/*`, and `port/startup/cortexm/startup.S`.
- **Required Compile Definitions** (for both C and ASM): `-DEK_CONF_PATH="ek_conf.h"`, plus `-DEK_SOC_PATH="ek_soc.h"` on Cortex-M.

---

## Configuration Reference

### 1. CMake Options

#### `ek_platform_prepare(ARCH <arch> [PICOLIBC ON|OFF] [BUILD_TYPE Debug|Release|RelWithDebInfo|MinSizeRel] [TOOLCHAIN <path>])`

| Argument / Variable | Values / Description |
|---|---|
| `ARCH` | **ARM**: `cm0`, `cm3`, `cm4`, `cm7`, `cm23`, `cm33`, `cm55`<br>**RISC-V**: `rv32imac`, `rv32imafc`, `rv64imac`, `rv64gc`<br>**Host**: `host`, `x86_64` |
| `PICOLIBC` | `ON` / `OFF` (default `OFF`). When enabled, select the source via one of:<br>- `PICOLIBC_FETCH=ON`: Automatically download the prebuilt package (cached under `build/_deps`)<br>- `PICOLIBC_BUILD=ON`: Build from source with meson (ARM only)<br>- `PICOLIBC_ROOT="/path"`: Use a locally installed picolibc |
| `BUILD_TYPE` | Optional, defaults to `Debug` (CLI `-DCMAKE_BUILD_TYPE=...` takes precedence). Injects `EK_PLATFORM_DEBUG` to drive `EKCFG_DEBUG`, `EKCFG_ASSERT`, and `EKCFG_LOG_DEBUG` defaults; application code can use `EK_IS_DEBUG`, `EK_DEBUG_ONLY(...)`, `ek_is_debug()`, and `ek_build_mode_str()` |
| `RISCV_TOOLCHAIN_PREFIX` | Toolchain prefix when the RISC-V compiler is not in `PATH` (e.g., `/opt/.../bin/riscv-none-elf-`) |

#### `ek_platform_configure(...)`

| Argument | Required | Description |
|---|---|---|
| `CONF` | Yes | Path to `ek_conf.h` |
| `BOARD` | Cross builds | Path to `ek_board.h` (optional when `LINKER off` or building for `host`) |
| `SOC` | Cortex-M | Path to `ek_soc.h` |
| `SOC_TARGET` | Cortex-M | CMake target providing chip CMSIS include paths and compile definitions to `ek_platform` |
| `LINKER` | Optional | `auto` (default, auto-generates linker script) or `off` (attach per target with `ek_linker_attach()`) |
| `TEMPLATE` | Optional | Custom linker script template path (defaults to `cmake/linker/*.ld.S` by architecture) |

### 2. Feature & Module Macros (`ek_conf.h`)

#### Runtime Environment & Core Services

| Macro | Default | Description |
|---|---|---|
| `EKCFG_RTOS` | `0` | `0`=bare-metal, `1`=enable built-in tickless RTOS kernel (requires `EKCFG_ARCH=1` and `EKCFG_LIST=1`, mutually exclusive with `EKCFG_EVOKE`; `ek_malloc/free/realloc` are automatically thread-safe; provides `thread`, `sem`, `mutex`, `condvar`, `barrier`, `msgq`, `kfifo`) |
| `EKCFG_PICOLIBC` | `1` | `1`=use picolibc adaptation layer (overridden by CMake `PICOLIBC`; forces `EKCFG_NEWLIB=0` and `EKCFG_IO_LWPRTF=0` when enabled) |
| `EKCFG_NEWLIB` | `0` | `1`=enable bare-metal newlib/nanolibc syscall stubs (mutually exclusive with `EKCFG_PICOLIBC`) |
| `EKCFG_IO_LWPRTF` | `0` | `1`=use built-in lwprintf (forced to `0` when `EKCFG_PICOLIBC=1`, delegating to standard `printf`) |
| `EKCFG_IO_CONSOLE_PATH` | Undefined (optional) | When `EKCFG_VFS=1`, specifies the default console device path (e.g., `"/dev/uart0"`) to auto-bind `stdin/stdout/stderr` on boot |
| `EKCFG_STATIC_ALLOC` | `1` | Enable `EK_DEFINE_*` static object linker-section registration |
| `EKCFG_EXPORT` | `1` | Enable `EK_EXPORT_*` auto-initialization mechanism |
| `EKCFG_ASSERT` | `EKCFG_DEBUG` | Enable assertion module (follows `EKCFG_DEBUG` by default: `Debug=1`, `Release=0`) |
| `EKCFG_LOG` | `1` | Enable leveled logging module |
| `EKCFG_ARCH` | `1` | Enable hardware architecture abstraction layer (IRQ management, critical sections, VTOR redirection) |

#### Module Switches (`1`=enable, `0`=disable)

| Macro | Default | Description |
|---|---|---|
| `EKCFG_STR` / `EKCFG_STR_VIEW` | `0` | Dynamic string / Read-only string view |
| `EKCFG_LIST` | `0` | Intrusive doubly circular linked list |
| `EKCFG_VEC` | `0` | Type-safe dynamic array |
| `EKCFG_RINGBUF` / `EKCFG_RINGBUF_SPSC` | `0` | General ring buffer / SPSC lock-free ring buffer |
| `EKCFG_STACK` | `0` | General stack |
| `EKCFG_SNAPSHOT` | `0` | State snapshot component |
| `EKCFG_BITSTREAM` | `0` | Bitstream reader/writer |
| `EKCFG_SHARED_PTR` | `0` | Reference-counted shared pointer |
| `EKCFG_EVOKE` | `0` | Event-driven cooperative scheduler (bare-metal only; requires `EKCFG_LIST` and `EKCFG_RINGBUF_SPSC`, mutually exclusive with `EKCFG_RTOS`; hooks supplied via `ek_evoke_ops_t`) |
| `EKCFG_PICOTHREAD` | `0` | Cooperative microthreads (requires `EKCFG_LIST`) |
| `EKCFG_PICOTHREAD_SEM` / `MSG` | `0` | Microthread semaphore / message queue (requires `EKCFG_PICOTHREAD`; message queue also requires `EKCFG_RINGBUF`) |
| `EKCFG_VFS` | `0` | Virtual File System (includes `vfs_bus` static bus/device tree registration and `ek_vfs_bind_fd` stdio binding) |
| `EKCFG_SHELL` | `0` | Embedded command-line Shell (requires `EKCFG_VFS=1`; supports `.ek_shell_cmd` static export, dynamic command registration, and VT100 line editing) |

#### Module Parameters

| Macro | Default | Description |
|---|---|---|
| `EKCFG_ARCH_IRQ_COUNT` | None (required on ARM) | External IRQ count (`1..240`); evaluated by assembler, must be a plain integer like `(91)` |
| `EKCFG_HEAP_TLSF` | `1` | `1`=use built-in TLSF allocator, `0`=custom `ek_malloc/free/realloc` |
| `EKCFG_HEAP_SECTION` | Undefined (optional) | When defined, places the default heap in the specified linker section (e.g., `".heap"` or `".tcmram"`) |
| `EKCFG_HEAP_SIZE` | `(30 * 1024)` | Default heap size in bytes |
| `EKCFG_HEAP_MAX_POOL_SIZE` | `EKCFG_HEAP_SIZE` | Maximum single pool size in bytes (override explicitly for multi-pool setups) |
| `EKCFG_TLSF_FL_INDEX_MAX` | Auto-derived from `EKCFG_HEAP_MAX_POOL_SIZE` | TLSF first-level index max in `[10, 30]` (max block size is $2^{\text{FL}}$ bytes) |
| `EKCFG_TLSF_SL_INDEX_COUNT_LOG2` | Auto-derived from `EKCFG_HEAP_SIZE` | $\log_2$ of TLSF second-level index count in `[1, 5]` |
| `EKCFG_LOG_DEBUG` | `EKCFG_DEBUG` | Enable DEBUG-level log output (follows `EKCFG_DEBUG` by default) |
| `EKCFG_LOG_COLOR` | `1` | Enable ANSI color output in logs |
| `EKCFG_LOG_BUF_SIZE` | `(256)` | Single-line log formatting buffer size in bytes |
| `EKCFG_ASSERT_TINY` | `EKCFG_RELEASE` | `1`=compact assertion (omits file name and expression string), `0`=full assertion (`Debug=0`, `Release=1` by default) |
| `EKCFG_ASSERT_LOG` | `EKCFG_DEBUG` | Output log message on assertion failure (follows `EKCFG_DEBUG` by default) |
| `EKCFG_EVOKE_MIN_DEEPSLEEP_TICK` | `(10)` | Minimum idle ticks before evoke enters deep sleep |
| `EKCFG_PT_PRIO_LOWEST` | `(31)` | Lowest microthread priority value (`<= 31`; priority levels = value + 1) |
| `EKCFG_VFS_MAX_FDS` | `(16)` | Maximum simultaneously open VFS file descriptors (`<= 32`) |
| `EKCFG_VFS_MAX_NODES` | `(30)` | VFS static node pool capacity |
| `EKCFG_VFS_NAME_LEN` | `(12)` | Maximum VFS node name length (must be a multiple of `4`) |
| `EKCFG_VFS_TREE_DUMP_UTF8` | `0` | Use UTF-8 box-drawing characters when dumping VFS directory tree |
| `EKCFG_SHELL_BUF_SIZE` | `(64)` | Shell line-editing buffer size in bytes |
| `EKCFG_SHELL_PROMPT` | `"ek>"` | Shell interactive prompt string |
| `EKCFG_SHELL_CMD_MAX_ARGS` | `(8)` | Maximum argument count (`argc` limit) per Shell command |
| `EKCFG_SHELL_UTF8` | `0` | Enable UTF-8 multibyte width and editing support in Shell |
| `EKCFG_SHELL_BUILTIN_CMDS` | `0` | Enable built-in Shell commands (`hello` / `echo` / `tree` / `clear` / `version` / `help`) |
| `EKCFG_OS_KERNEL_TIMEBASE_HZ` | `(1000000U)` | RTOS kernel timebase frequency in Hz |
| `EKCFG_OS_KERNEL_STACKOVERFLOW_CHECK` | `(0U)` | Stack overflow check level: `0`=off, `1`=SP bounds/alignment check, `2`=magic canary paint + high-water-mark tracking |
| `EKCFG_OS_THREAD_NAME_LEN` | `(16)` | Maximum RTOS thread name length |
| `EKCFG_OS_THREADS_MAX` | `(16)` | Maximum concurrent RTOS threads (`<= 64`) |
| `EKCFG_OS_THREAD_MIN_STACK` | `(256)` | Minimum RTOS thread stack size in bytes |
| `EKCFG_OS_THREAD_MIN_PRIO` | `(31U)` | Lowest RTOS thread priority (`< 32`; priority levels = value + 1) |
| `EKCFG_OS_IDLE_THREAD_STACK` | `(512)` | RTOS idle thread stack size in bytes |

### 3. Board Memory Layout (`ek_board.h`)

| Macro | Description |
|---|---|
| `EKLD_FLASH_ORIGIN` / `EKLD_FLASH_LENGTH` | Flash start address and size |
| `EKLD_RAM_ORIGIN` / `EKLD_RAM_LENGTH` | Main RAM start address and size |
| `EKLD_TCMRAM_ORIGIN` / `EKLD_TCMRAM_LENGTH` | TCMRAM start address and size (set `LENGTH` to `0` if unused) |
| `EKLD_SDRAM0_ORIGIN` / `EKLD_SDRAM0_LENGTH` | External SDRAM0 start address and size (set to `0` if unused) |
| `EKLD_SDRAM1_ORIGIN` / `EKLD_SDRAM1_LENGTH` | External SDRAM1 start address and size (set to `0` if unused) |
| `EKLD_STACK_SIZE` / `EKLD_MIN_HEAP_SIZE` | Main stack size and minimum system heap size |

## Third-Party Dependencies

| Library | Purpose | License |
|---------|---------|---------|
| [TLSF](http://tlsf.baisoku.org) v3.1 | Real-time memory allocator (O(1) malloc/free) | BSD-style |
| [lwprintf](https://github.com/MaJerle/lwprintf) | Lightweight printf implementation | MIT |
| [picolibc](https://github.com/picolibc/picolibc) | Embedded C standard library | BSD |

## License

Original code in this project is licensed under MIT; see [LICENSE](LICENSE). Code under `third_party/` follows its respective licenses.
