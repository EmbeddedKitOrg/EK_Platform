English | [简体中文](README.md)

# EmbeddedKit (ek_platform)

A lightweight embedded C library for resource-constrained MCUs. It provides data structures, memory management, logging, an event scheduler, and cooperative microthreads. It has no RTOS dependency and can be used in bare-metal projects; modules are enabled at compile time through configuration macros.

This document only covers integration. See the individual headers for module APIs.

## Features

- No RTOS dependency; integrates directly into bare-metal projects
- Configuration-driven: every module is enabled or disabled through `ek_conf.h`; disabled modules are not compiled
- Supports GCC, ARM Compiler 5 (armcc), and ARM Compiler 6 (armclang)
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
│   ├── core/                      # ek_def / ek_assert / ek_io / ek_log / ek_export
│   ├── mem/                       # ek_heap / ek_static_alloc
│   ├── container/                 # ek_list / ek_vec / ek_ringbuf / ek_stack / ek_snapshot / ek_str / ek_str_view
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

For RISC-V, set `PLATFORM=rv...` or call `ek_set_linker_arch(riscv)`; the library then selects `cmake/linker/riscv.ld.S` automatically. Use `ek_set_linker_template(...)` only for a custom template. That template uses `_start` as the entry point, and TCMRAM is optional (`EKLD_TCMRAM_LENGTH` of 0 omits it).

For multiple targets or boards, disable the automatic mode and generate per target:

```cmake
set(EK_LINKER_AUTO OFF)
...
ek_linker_attach(app BOARD_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/ek_board_a.h")
```

Without CMake, preprocess `cmake/linker/cortexm.ld.S` manually with `${CMAKE_C_COMPILER} -E -P -x c`, or reuse the generated result.

### 5. Initialization

With `EKCFG_EXPORT=1`, `ek_export_init()` is marked `__attribute__((constructor))` and runs automatically from `__libc_init_array()` before `main()`. The heap, IO, scheduler, and other modules are initialized in `level → order` order. Do not call it again:

```c
int main(void)
{
    // ek_export_init() already runs from __libc_init_array()
    // ...
}
```

Without export, call the initializers manually in dependency order: `ek_heap_init()`, then `ek_io_init()` and the other `*_init()` functions, and finally `ek_static_alloc_init()`.

## Option 2: compile sources directly

Without CMake, add these include paths and the required sources:

```
Include paths:
  - include/
  - third_party/tlsf/
  - third_party/lwprintf/inc/
  - <picolibc install prefix>/include/     (when using picolibc; provided by you)

Sources (choose what you need):
  - src/core/*.c
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
| picolibc (library default) | `EKCFG_PICOLIBC=1` | mapped to `printf` / `vsnprintf` | `int ek_port_io_fputc(int ch)` |
| lwprintf (template default) | `EKCFG_IO_LWPRTF=1`, `EKCFG_PICOLIBC=0` | mapped to `lwprintf` / `lwvsnprintf` | `int ek_port_io_fputc(int ch)` |
| Bare-metal newlib/nanolibc | `EKCFG_NEWLIB=1`, `EKCFG_PICOLIBC=0`, `EKCFG_IO_LWPRTF=0` | mapped to the system `printf` | `_write` etc. from `port/clib/newlib/newlib_port.c`; `ek_port_io_fputc` can be overridden |
| System libc (host) | `EKCFG_PICOLIBC=0`, `EKCFG_IO_LWPRTF=0`, `EKCFG_NEWLIB=0` | mapped to the system `printf` | nothing |

`EKCFG_PICOLIBC` and `EKCFG_NEWLIB` are mutually exclusive. The bare-metal newlib syscall port provides weak `_write` / `_read` / `_close` / `_fstat` / `_isatty` / `_lseek` / `_sbrk`; override them as needed in the BSP. `_sbrk` uses `__heap_start` / `__heap_end` from the linker script.

When `EKCFG_PICOLIBC=1`, `EKCFG_IO_LWPRTF` is forced to 0; no manual change is required.

### Configuration macros

See `ek_conf_template.h` for the full list and defaults. Macros not defined in `ek_conf.h` take their defaults, and unmet dependencies are reported at compile time (for example, enabling `EKCFG_PICOTHREAD_SEM` without `EKCFG_PICOTHREAD`, or enabling `EKCFG_EVOKE` in RTOS mode).

## Platform adaptation: weak functions

The following functions have weak implementations. Provide strong functions with the same names as needed:

```c
// ek_io: character output (lwprintf / picolibc backends)
int ek_port_io_fputc(int ch);

// ek_log: system timestamp
uint32_t ek_port_log_get_tick(void);

// ek_evoke: critical sections, timer, sleep
void ek_evoke_enter_critical(void);
void ek_evoke_exit_critical(void);
void ek_evoke_set_timer(uint32_t xtick);
void ek_evoke_light_sleep(void);
void ek_evoke_deep_sleep(void);

// ek_assert: assertion hook (full mode)
void ek_assert_hook(const char *file, uint32_t line, const char *expr);

// ek_heap: only needed when EKCFG_HEAP_TLSF=0
void *ek_malloc(size_t size);
void *ek_realloc(void *ptr, size_t size);
void ek_free(void *ptr);
```

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

## Third-party dependencies

| Library | Purpose | License |
|---------|---------|---------|
| [TLSF](http://tlsf.baisoku.org) v3.1 | real-time memory allocator (O(1) malloc/free) | BSD-style |
| [lwprintf](https://github.com/MaJerle/lwprintf) | lightweight printf | MIT |
| [picolibc](https://github.com/picolibc/picolibc) | embedded C standard library (optional, not bundled) | BSD |

## License

Original code in this project is licensed under MIT; see [LICENSE](LICENSE). Code under `third_party/` follows its own licenses.
