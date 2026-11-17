English | [简体中文](README.md)

# EmbeddedKit (ek_platform)

A lightweight embedded C library for resource-constrained MCUs. It provides data structures, memory management, logging, an event scheduler, and cooperative microthreads. It has no RTOS dependency and can be used in bare-metal projects; modules are enabled at compile time through configuration macros.

This document only covers integration. See the individual headers for module APIs.

## Features

- No RTOS dependency; integrates directly into bare-metal projects
- Configuration-driven: every module is enabled or disabled through `ek_conf.h`; disabled modules are not compiled, and containers required by the schedulers are enabled automatically
- Compilers: GCC and ARM Compiler 6 (armclang) are fully supported. ARM Compiler 5 (armcc) can build the containers, schedulers and other modules, but `EKCFG_EXPORT` / `EKCFG_STATIC_ALLOC` rely on section symbols from a GNU ld linker script; Keil scatter files are not supported yet
- C99 baseline. CI builds several configurations with `-Werror` for Cortex-M0/M4/M7/M33 and RV32/RV64, and runs host unit tests under ASan/UBSan
- Platform hooks are declared `__EK_WEAK`; provide a strong function with the same name to override

## Directory layout

```
ek_platform/
├── CMakeLists.txt
├── ek_conf_template.h             # configuration template; copy to ek_conf.h
├── ek_board_template.h            # board memory template; copy to ek_board.h
├── cmake/
│   ├── ek_config.cmake            # CMake configuration helpers
│   ├── ek_linker.cmake            # linker script generation and injection
│   ├── linker/
│   │   ├── cortexm.ld.S           # Cortex-M linker script template
│   │   └── riscv.ld.S             # RISC-V linker script template
│   ├── gcc-arm-none-eabi.cmake    # ARM cross-compilation toolchain
│   └── gcc-riscv-none-elf.cmake   # RISC-V cross-compilation toolchain
├── include/ek/
│   ├── ek_conf_internal.h         # config entry: includes ek_conf.h, defaults, dependency checks
│   ├── core/                      # ek_def / ek_assert / ek_critical / ek_io / ek_log / ek_export
│   ├── mem/                       # ek_heap / ek_static_alloc
│   ├── container/                 # ek_list / ek_vec / ek_ringbuf / ek_kfifo / ek_stack / ek_snapshot / ek_str / ek_str_view
│   └── sched/                     # ek_evoke / ek_picothread
├── src/                           # same module split as include/ek
│   ├── core/
│   ├── mem/
│   ├── container/
│   └── sched/
├── port/clib/picolibc/            # picolibc stdout/stderr adapter
├── port/clib/newlib/              # newlib/nanolibc syscall adapter
├── third_party/
│   ├── tlsf/                      # O(1) memory allocator
│   └── lwprintf/                  # lightweight printf
├── tests/                         # host unit tests and multi-configuration cross-compile check
├── README.md
└── README.en.md
```

## Supported platforms

`PLATFORM` selects the toolchain CPU/ISA flags. ARM uses `cmake/gcc-arm-none-eabi.cmake`; RISC-V uses `cmake/gcc-riscv-none-elf.cmake`:

| PLATFORM | CPU / ISA | FPU / ABI |
|----------|-----------|-----------|
| cm0 | Cortex-M0 | none |
| cm3 | Cortex-M3 | none |
| cm4 | Cortex-M4 | fpv4-sp-d16 (hard) |
| cm7 | Cortex-M7 | fpv5-d16 (hard) |
| cm23 | Cortex-M23 | none |
| cm33 | Cortex-M33 | fpv5-sp-d16 (hard) |
| cm55 | Cortex-M55 | auto (hard) |
| rv32imac | RISC-V RV32IMAC | ilp32 |
| rv32imafc | RISC-V RV32IMAFC | ilp32f |
| rv64imac | RISC-V RV64IMAC | lp64 |
| rv64gc | RISC-V RV64GC | lp64d |

If the RISC-V compiler is not on `PATH`, set the prefix with `RISCV_TOOLCHAIN_PREFIX` (include the directory and trailing `-`):

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=lib/ek_platform/cmake/gcc-riscv-none-elf.cmake \
      -DPLATFORM=rv32imac \
      -DRISCV_TOOLCHAIN_PREFIX=/opt/xpack-riscv-none-elf-gcc-<version>/bin/riscv-none-elf- \
      -B build
```

`PICOLIBC_BUILD=ON` maps `PLATFORM` to `-mcpu` / `-mfpu` / `-mfloat-abi` and builds a matching picolibc with meson; no prebuilt package is required.

The picolibc package name can be selected independently with `PICOLIBC_PLATFORM` (defaults to `PLATFORM`):

| Architecture | PICOLIBC_PLATFORM |
|--------------|-------------------|
| ARM | cm0, cm3, cm4, cm7, cm23, cm33, cm55 |
| RISC-V | rv32imac, rv32imafc, rv64imac, rv64gc |
| x86 | x86_64 (freestanding; link with `-no-pie`) |

RISC-V and x86 currently use the prebuilt packages published by CI through `PICOLIBC_FETCH=ON`; the local `PICOLIBC_BUILD=ON` source build only covers ARM.

## Option 1: CMake submodule (recommended)

### 1. Add the submodule

```bash
git submodule add https://github.com/EmbeddedKitOrg/EK_Platform.git lib/ek_platform
```

### 2. Create ek_conf.h and ek_board.h

Copy both templates into your project: keep only the `ek_conf.h` overrides you need, and fill in the `EKLD_*` memory values in `ek_board.h` for your chip:

```bash
cp lib/ek_platform/ek_conf_template.h  Core/Inc/ek_conf.h
cp lib/ek_platform/ek_board_template.h Core/Inc/ek_board.h
```

### 3. Configure CMake

```cmake
cmake_minimum_required(VERSION 3.20)

# The config helpers must be included before project()/add_subdirectory()
include(${CMAKE_CURRENT_SOURCE_DIR}/lib/ek_platform/cmake/ek_config.cmake)

# Target platform; also selects the prebuilt picolibc package
ek_set_arch(cm4)

# Enable when using picolibc; turn it off for lwprintf or the system libc
ek_set_picolibc(ON)

# Select the ARM/RISC-V toolchain automatically
ek_set_toolchain()

project(my_app C)

# picolibc lookup (choose one):
#   1) Installed: set(PICOLIBC_ROOT "/path/to/picolibc")
#   2) Toolchain provides --specs=picolibc.specs: nothing else needed
#   3) Build from source (requires meson/ninja; uses PLATFORM):
#        set(PICOLIBC_BUILD ON)
#        # set(PICOLIBC_SOURCE_DIR "/path/to/picolibc")  # optional; downloads if empty
#   4) Download a prebuilt package:
#        set(PICOLIBC_FETCH ON)
#        # set(PICOLIBC_SHA256 "<sha256>")  # optional; verified when set
# You can also point PICOLIBC_SPECS directly at a specs file.

# With a system picolibc (for example apt install picolibc-arm-none-eabi),
# PICOLIBC=ON automatically finds /usr/lib/picolibc/<triple>/picolibc.specs.
# Traditional Arm GNU Toolchain / Ubuntu arm-none-eabi-gcc ship newlib, not
# picolibc; check with: arm-none-eabi-gcc -print-file-name=picolibc.specs
# If the specs default path differs from the install location, set PICOLIBC_PREFIX.

# Point at the files before add_subdirectory
ek_set_conf_file("${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_conf.h")
ek_set_linker_file("${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_board.h")

add_subdirectory(lib/ek_platform)

target_link_libraries(${PROJECT_NAME} PRIVATE ek_platform)
```

`ek_set_arch()` / `ek_set_picolibc()` must be called before `project()`; `ek_set_conf_file()` / `ek_set_linker_file()` must be called before `add_subdirectory(ek_platform)`.

`ek_platform` is an OBJECT library. Its PUBLIC include directories and link items propagate to the main project automatically. Do not add `include/` manually, and do not replace `target_link_libraries` with `$<TARGET_OBJECTS:ek_platform>`.

Downloaded archives and source-build products are cached under `<build>/_deps/`: an existing archive (with a matching SHA) is not downloaded again, a complete extraction directory is not extracted again, and a finished source build is not rebuilt. Delete the corresponding directory to force a refresh.

### 4. Linker script

The linker script is generated by `ek_platform`; do not write one by hand. The `ek_board.h` copied in step 2 supplies the memory layout, and CMake points at it through `EK_BOARD_PATH` (already set by `ek_set_linker_file()` above).

`EK_LINKER_AUTO` defaults to ON: the library preprocesses `ek_conf.h`, `ek_conf_internal.h`, `ek_board.h`, and `cmake/linker/cortexm.ld.S` into `${build}/ek_platform/ek_platform.ld`, then propagates it through `INTERFACE_LINK_OPTIONS` / `INTERFACE_LINK_DEPENDS` to any target that links `ek_platform`. The generated script already contains `.ek_export`, `.ek_static_alloc`, TCMRAM, and SDRAM0/SDRAM1 sections; sections are omitted when `EKCFG_EXPORT` / `EKCFG_STATIC_ALLOC` is disabled.

If `ek_conf.h` includes other headers for an RTOS setup, add those directories to `EK_LINKER_INCLUDE_DIRS`; preprocessing still keeps only macros and never writes C declarations into the `.ld`.

For RISC-V, call `ek_set_arch(rv32imac)` followed by `ek_set_toolchain()`; the library then selects `cmake/gcc-riscv-none-elf.cmake` and `cmake/linker/riscv.ld.S` automatically. Use `ek_set_linker_template(...)` only for a custom template. That template uses `_start` as the entry point, and TCMRAM is optional (`EKLD_TCMRAM_LENGTH` of 0 omits it).

For multiple targets or boards, disable the automatic mode and generate per target:

```cmake
set(EK_LINKER_AUTO OFF)
...
ek_linker_attach(app BOARD_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_board_a.h")
```

Without CMake, preprocess `cmake/linker/cortexm.ld.S` manually with `${CMAKE_C_COMPILER} -E -P -x c`, or reuse the generated result.

### 5. Initialization

With `EKCFG_EXPORT=1`, `ek_export_init()` is marked `__attribute__((constructor))` by default and runs automatically from `__libc_init_array()` before `main()`. The heap, IO, scheduler, and other modules are initialized in `level → order` order. `ek_export_init()` runs only once; further calls return immediately:

```c
int main(void)
{
    // ek_export_init() already runs from __libc_init_array()
    // ...
}
```

Note that the constructor runs before `HAL_Init()` and the clock setup, so functions exported at `EK_EXPORT_HARDWARE` and similar levels must not depend on clocked peripherals. To run it after the clocks are configured, set `EKCFG_EXPORT_AUTO_INIT=0` in `ek_conf.h` and call it yourself:

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    ek_export_init(); // manual call when EKCFG_EXPORT_AUTO_INIT=0
    // ...
}
```

Without export, call the initializers manually in dependency order: `ek_heap_init()`, then `ek_io_init()` and the other `*_init()` functions, and finally `ek_static_alloc_init()`.

## Option 2: compile sources directly

Without CMake, add these include paths and the required sources:

```
Include paths:
  - include/
  - include/ek/                          (third_party headers include "ek_conf_internal.h" directly)
  - include/ek/core/                     (tlsf.c includes "ek_io.h" directly)
  - third_party/tlsf/
  - third_party/lwprintf/inc/
  - <picolibc install prefix>/include/     (when using picolibc; provided by you)

Sources (choose what you need):
  - src/core/*.c                         (ek_critical.c is used by ek_kfifo / ek_evoke)
  - src/mem/*.c
  - src/container/*.c
  - src/sched/*.c
  - port/clib/picolibc/picolibc_port.c   (when using picolibc)
  - port/clib/newlib/newlib_port.c       (when using newlib/nanolibc)

Third-party sources (always compiled; actual use is guarded by #if):
  - third_party/tlsf/tlsf.c
  - third_party/lwprintf/lwprintf.c
```

picolibc is no longer bundled with the repository. With CMake use `PICOLIBC_ROOT` / `PICOLIBC_FETCH` as shown above. For a manual build, use `--specs=picolibc.specs --picolibc-prefix=<install prefix>`, or add `-I` / `-L` yourself.

Every source includes `#include "ek/ek_conf_internal.h"`, so `include/` must be on the include path.

## Configuration

### Two ways to provide ek_conf.h

The include logic in `ek_conf_internal.h`:

```c
#ifdef EK_CONF_PATH
#    include EK_CONF_PATH   // option A: path supplied by the build system
#else
#    include "ek_conf.h"    // option B: normal include search
#endif
```

**Option A: `ek_set_conf_file()` / `EK_CONF_PATH` (recommended)**

Include the config helpers first, then point at the file before `add_subdirectory`:

```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/lib/ek_platform/cmake/ek_config.cmake)
ek_set_conf_file("${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_conf.h")
add_subdirectory(lib/ek_platform)
```

`ek_set_conf_file()` writes the `EK_CONF_PATH` cache entry; the equivalent lower-level form is:

```cmake
set(EK_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_conf.h"
    CACHE STRING "" FORCE)
add_subdirectory(lib/ek_platform)
```

The library CMakeLists passes `EK_CONF_PATH` to the compiler as a definition.

It can also be passed on the command line:

```bash
cmake -DEK_CONF_PATH="$(pwd)/Core/Inc/ek_conf.h" -B build
```

Always use an absolute path. `#include EK_CONF_PATH` expands to a quoted path, and a relative path is first resolved under `include/ek/`, so it can easily fail to resolve.

**Option B: include search path**

When `EK_CONF_PATH` is not set, add the directory containing `ek_conf.h` to the library target. The OBJECT library also needs it while compiling its own sources, so use `PUBLIC`:

```cmake
add_subdirectory(lib/ek_platform)
target_include_directories(ek_platform PUBLIC Core/Inc)
```

Setting `PRIVATE` on the main target only does not propagate to the subproject.

### IO backend selection

`ek_io.h` selects the backend in this order: `EKCFG_IO_LWPRTF=1` → lwprintf; otherwise `EKCFG_PICOLIBC=1` → picolibc; otherwise the system libc (`printf` from glibc/newlib).

| Backend | ek_conf.h settings | `ek_printf` and friends | What to provide |
|---------|-------------------|--------------------------|-----------------|
| lwprintf (default) | `EKCFG_IO_LWPRTF=1`, `EKCFG_PICOLIBC=0` | mapped to `lwprintf` / `lwvsnprintf` | `int ek_port_io_fputc(int ch)` |
| picolibc | `EKCFG_PICOLIBC=1` | mapped to `printf` / `vsnprintf` | `int ek_port_io_fputc(int ch)` |
| Bare-metal newlib/nanolibc | `EKCFG_NEWLIB=1`, `EKCFG_PICOLIBC=0`, `EKCFG_IO_LWPRTF=0` | mapped to the system `printf` | `_write` etc. from `port/clib/newlib/newlib_port.c`; `ek_port_io_fputc` can be overridden |
| System libc (host) | `EKCFG_PICOLIBC=0`, `EKCFG_IO_LWPRTF=0`, `EKCFG_NEWLIB=0` | mapped to the system `printf` | nothing |

When `ek_conf.h` defines neither macro, the internal defaults match the template: `EKCFG_PICOLIBC=0`, `EKCFG_IO_LWPRTF=1`.

`EKCFG_PICOLIBC` and `EKCFG_NEWLIB` are mutually exclusive. The bare-metal newlib syscall port provides weak `_write` / `_read` / `_close` / `_fstat` / `_isatty` / `_lseek` / `_sbrk`; override them as needed in the BSP. `_sbrk` uses `__heap_start` / `__heap_end` from the linker script.

When `EKCFG_PICOLIBC=1`, `EKCFG_IO_LWPRTF` is forced to 0; no manual change is required.

### Configuration macros

See `ek_conf_template.h` for the full list and defaults. Macros not defined in `ek_conf.h` take their defaults, and unmet dependencies are reported at compile time (for example, enabling `EKCFG_PICOTHREAD_SEM` without `EKCFG_PICOTHREAD`, or enabling `EKCFG_EVOKE` in RTOS mode). Containers used internally by the schedulers are enabled automatically: `EKCFG_PICOTHREAD` / `EKCFG_EVOKE` → `EKCFG_LIST`, `EKCFG_PICOTHREAD_MSG` → `EKCFG_RINGBUF`, `EKCFG_EVOKE` → `EKCFG_RINGBUF_SPSC`.

With `EKCFG_ASSERT=0` / `EKCFG_LOG=0`, `ek_assert_param()` and `EK_LOG_*()` expand to empty statements (arguments are not evaluated), so neither the library nor user code needs extra `#if` guards.

In RTOS mode (`EKCFG_RTOS=1`) define `EK_LOCK_TYPE` / `EK_LOCK_INIT` / `EK_LOCK_DEINIT` / `EK_LOCK_ACQUIRE` / `EK_LOCK_RELEASE`. Containers and the TLSF heap block while the lock is held instead of returning `-EBUSY`; `EK_LOCK_TRY` is no longer used.

## Intrusive zero-copy FIFO (ek_kfifo)

Enabled with `EKCFG_KFIFO=1`. Elements are your own structs with an embedded `ek_kfifo_node_t`; put/get only link pointers — no copying, no allocation, O(1) — inside a very short critical section, so ISRs and the main loop can produce and consume in any combination.

```c
typedef struct {
    ek_kfifo_node_t node;   // embedded node
    uint16_t len;
    uint8_t  buf[256];
} frame_t;

static frame_t frames[8];
EK_DEFINE_KFIFO(free_q);    // free pool
EK_DEFINE_KFIFO(rx_q);      // ready queue

EK_KFIFO_FILL(&free_q, frames, node);          // init: 8 empty frames into the pool

// ISR: take a free frame → fill it → post it (pointer only)
frame_t *f = ek_kfifo_get_entry(&free_q, frame_t, node);
if (f) { f->len = read_dma(f->buf); ek_kfifo_put(&rx_q, &f->node); }

// main loop: process → return to the pool
while ((f = ek_kfifo_get_entry(&rx_q, frame_t, node)) != NULL) {
    handle(f->buf, f->len);
    ek_kfifo_put(&free_q, &f->node);
}
```

Also available: `ek_kfifo_put_front()` (jump the queue), `ek_kfifo_get_all()` (detach the whole chain with a single critical section), `ek_kfifo_put_list()` (append a pre-linked chain), and `ek_kfifo_remove()` (cancel a pending node). The critical section defaults to `ek_critical_enter()/exit()`; if every FIFO is used from a single context, define `EK_KFIFO_CRITICAL_ENTER()` / `EK_KFIFO_CRITICAL_EXIT()` as empty in `ek_conf.h`.

## Platform adaptation: weak functions

The following functions have weak implementations. Provide strong functions with the same names as needed:

```c
// ek_io: character output (lwprintf / picolibc backends)
int ek_port_io_fputc(int ch);

// ek_log: system timestamp
uint32_t ek_port_log_get_tick(void);

// ek_critical: interrupt-masking critical section (used by ek_kfifo / ek_evoke)
// Defaults: save/restore PRIMASK on Cortex-M, mstatus.MIE on RISC-V, nestable.
// Provide your own for multi-core or priority-based (BASEPRI) masking.
void ek_critical_enter(void);
void ek_critical_exit(void);

// ek_evoke: critical sections, timer, sleep
void ek_evoke_enter_critical(void);          // defaults to ek_critical_enter()
void ek_evoke_exit_critical(void);           // defaults to ek_critical_exit()
void ek_evoke_set_timer(uint32_t xtick);     // arm the one-shot timer; clear any stale expiry flag
uint32_t ek_evoke_get_timer_elapsed(void);   // ticks since the last set_timer (read the LPTIM counter)
void ek_evoke_light_sleep(void);             // called inside the critical section: just WFI, don't re-enable IRQs
void ek_evoke_deep_sleep(void);              // same; restore clocks here after wake-up if needed

// ek_assert: assertion hook (full mode)
void ek_assert_hook(const char *file, uint32_t line, const char *expr);

// ek_heap: only needed when EKCFG_HEAP_TLSF=0
void *ek_malloc(size_t size);
void *ek_realloc(void *ptr, size_t size);
void ek_free(void *ptr);
```

`ek_evoke` calls the sleep hooks inside its critical section, so no interrupt can slip in between "nothing to do" and going to sleep. On Cortex-M a pending interrupt still wakes WFI while PRIMASK=1; the scheduler then leaves the critical section and the ISR runs. A custom `ek_evoke_enter_critical()` therefore must not mask the wake-up sources via BASEPRI.

`ek_evoke_get_timer_elapsed()` returns 0 by default. The scheduler clock then only advances when the timer expires, and a delayed request added while the timer is running is measured from the last time the timer was armed. Implement it for accurate delays.

Example for STM32:

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

## Testing

```bash
# Host unit tests (Linux/macOS; add -DEK_TEST_SANITIZE=ON for ASan/UBSan)
cmake -S tests -B build/host-tests -G Ninja
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure

# Multi-configuration cross-compile check (tests/configs/*.h × several CPUs, library sources with -Werror)
bash tests/compile_check.sh
CC=riscv64-unknown-elf-gcc TARGETS="--specs=picolibc.specs -march=rv32imac -mabi=ilp32" bash tests/compile_check.sh
```

The MinGW linker on Windows cannot resolve weak default implementations across object files, so run the host tests on Linux, WSL or in a container; `compile_check.sh` works from Git Bash. CI (`.github/workflows/ci.yml`) runs both checks on every push and pull request.

## Third-party dependencies

| Library | Purpose | License |
|---------|---------|---------|
| [TLSF](http://tlsf.baisoku.org) v3.1 | real-time memory allocator (O(1) malloc/free) | BSD-style |
| [lwprintf](https://github.com/MaJerle/lwprintf) | lightweight printf | MIT |
| [picolibc](https://github.com/picolibc/picolibc) | embedded C standard library (optional, not bundled) | BSD |

## License

Original code in this project is licensed under MIT; see [LICENSE](LICENSE). Code under `third_party/` follows its own licenses.
