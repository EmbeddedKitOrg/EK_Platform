English | [简体中文](README.md)

# EmbeddedKit (ek_platform)

A lightweight embedded C foundation library for resource-constrained MCUs, providing data structures, memory management, logging, assertions, an event scheduler (evoke), cooperative microthreads (picothread), and VFS. Zero RTOS dependency; modules are enabled on demand at compile time via configuration macros.

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

# 1. Before project(): select architecture and whether to enable picolibc
ek_platform_prepare(ARCH cm4 PICOLIBC ON)
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
- **Override Weak Functions for Hardware Adaptation**: Define strong functions with the same name as needed:
  ```c
  int ek_port_io_fputc(int ch);                                          // printf character output
  uint32_t ek_port_log_get_tick(void);                                   // Log timestamp
  void ek_assert_hook(const char *file, uint32_t line, const char *expr); // Assertion hook
  void ek_evoke_enter_critical(void);                                    // evoke critical section & sleep hooks
  void ek_evoke_exit_critical(void);
  void ek_evoke_set_timer(uint32_t xtick);
  void ek_evoke_light_sleep(void);
  void ek_evoke_deep_sleep(void);
  void *ek_malloc(size_t size);                                          // Custom heap when EKCFG_HEAP_TLSF=0
  void *ek_realloc(void *ptr, size_t size);
  void *ek_free(void *ptr);
  ```

### 4. Direct Source Integration without CMake (Optional)

- **Include Paths**: `include/`, `third_party/tlsf/`, `third_party/lwprintf/inc/`, plus the directories containing `ek_conf.h` and `ek_soc.h`.
- **Sources**: `src/**/*.c`, `third_party/tlsf/tlsf.c`, `third_party/lwprintf/lwprintf.c`, and the matching `port/arch/*`, `port/clib/*`, and `port/startup/cortexm/startup.S`.
- **Required Compile Definitions** (for both C and ASM): `-DEK_CONF_PATH="ek_conf.h"`, plus `-DEK_SOC_PATH="ek_soc.h"` on Cortex-M.

---

## Configuration Reference

### 1. CMake Options

#### `ek_platform_prepare(ARCH <arch> [PICOLIBC ON|OFF] [TOOLCHAIN <path>])`

| Argument / Variable | Values / Description |
|---|---|
| `ARCH` | **ARM**: `cm0`, `cm3`, `cm4`, `cm7`, `cm23`, `cm33`, `cm55`<br>**RISC-V**: `rv32imac`, `rv32imafc`, `rv64imac`, `rv64gc`<br>**Host**: `host`, `x86_64` |
| `PICOLIBC` | `ON` / `OFF` (default `OFF`). When enabled, select the source via one of:<br>- `PICOLIBC_FETCH=ON`: Automatically download the prebuilt package (cached under `build/_deps`)<br>- `PICOLIBC_BUILD=ON`: Build from source with meson (ARM only)<br>- `PICOLIBC_ROOT="/path"`: Use a locally installed picolibc |
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
| `EKCFG_RTOS` | `0` | `0`=bare-metal, `1`=RTOS (requires defining `EK_LOCK_*` mutex macros) |
| `EKCFG_IO_LWPRTF` | `1` | `1`=use built-in lwprintf (forced to `0` when `PICOLIBC=ON`, delegating to `printf`) |
| `EKCFG_NEWLIB` | `0` | `1`=enable bare-metal newlib/nanolibc syscall stubs (mutually exclusive with picolibc) |
| `EKCFG_STATIC_ALLOC` | `1` | Enable `EK_DEFINE_*` static object linker-section registration |
| `EKCFG_EXPORT` | `1` | Enable `EK_EXPORT_*` auto-initialization mechanism |
| `EKCFG_ASSERT` | `1` | Enable assertion module |
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
| `EKCFG_EVOKE` | `0` | Event-driven cooperative scheduler (bare-metal only) |
| `EKCFG_PICOTHREAD` | `0` | Cooperative microthreads |
| `EKCFG_PICOTHREAD_SEM` / `MSG` | `0` | Microthread semaphore / message queue (requires `EKCFG_PICOTHREAD`) |
| `EKCFG_VFS` | `0` | Virtual File System |

#### Module Parameters

| Macro | Default | Description |
|---|---|---|
| `EKCFG_ARCH_IRQ_COUNT` | None (required on ARM) | External IRQ count; evaluated by assembler, must be a plain integer like `(91)` |
| `EKCFG_HEAP_TLSF` | `1` | `1`=use built-in TLSF allocator, `0`=custom `ek_malloc/free/realloc` |
| `EKCFG_HEAP_SECTION` | `".heap"` | Linker section for heap memory (e.g., `".tcmram"`) |
| `EKCFG_HEAP_SIZE` | `(16 * 1024)` | Default heap size in bytes |
| `EKCFG_TLSF_FL_INDEX_MAX` | `(24)` | TLSF first-level index max (max block size is $2^{\text{FL}}$ bytes) |
| `EKCFG_TLSF_SL_INDEX_COUNT_LOG2` | `(3)` | $\log_2$ of TLSF second-level index count |
| `EKCFG_LOG_DEBUG` | `1` | Enable DEBUG-level log output |
| `EKCFG_LOG_COLOR` | `0` | Enable ANSI color output in logs |
| `EKCFG_LOG_BUF_SIZE` | `(128)` | Single-line log formatting buffer size in bytes |
| `EKCFG_ASSERT_TINY` | `1` | `1`=compact assertion (omits file name and expression string), `0`=full assertion |
| `EKCFG_ASSERT_LOG` | `1` | Output log message on assertion failure |
| `EKCFG_EVOKE_MIN_DEEPSLEEP_TICK` | `(10)` | Minimum idle ticks before evoke enters deep sleep |
| `EKCFG_PT_PRIO_LOWEST` | `(31)` | Lowest microthread priority value (priority levels = value + 1) |
| `EKCFG_VFS_MAX_FDS` | `(16)` | Maximum simultaneously open VFS file descriptors |
| `EKCFG_VFS_MAX_NODES` | `(30)` | VFS static node pool capacity |
| `EKCFG_VFS_NAME_LEN` | `(12)` | Maximum VFS node name length |
| `EKCFG_VFS_TREE_DUMP_UTF8` | `0` | Use UTF-8 box-drawing characters when dumping VFS directory tree |

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
