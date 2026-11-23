# 更新日志

本文件记录 [ek_platform](https://github.com/EmbeddedKitOrg/EK_Platform) 的版本变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

## [7.0.0] - 2026-11-23

### 新增

- **内置无滴答抢占式 RTOS 内核**（`EKCFG_RTOS=1`，`include/ek/os/`、`src/os/`、`port/arch/cortexm/context.S`）：
  - 调度与时间抽象：`ek_os_timer_ops_t`（`now` / `set_timeout`）、`ek_os_kernel_start()`、`ek_os_kernel_time()`、`ek_os_kernel_announce_isr()`、`ek_os_kernel_sched_lock/unlock()`、`ek_os_kernel_spin_lock/unlock()`、`ek_os_kernel_current_tid()`、`EK_OS_MSEC()` / `EK_OS_SEC()` / `EK_OS_FOREVER`、`EKCFG_OS_KERNEL_TIMEBASE_HZ`。
  - 线程生命周期管理：`ek_os_thread_create()` / `create_static()` / `EK_DEFINE_OS_THREAD`、`ek_os_thread_exit()`、`ek_os_thread_join()`、`ek_os_thread_yield()`、`ek_os_thread_sleep()`、`ek_os_thread_suspend/resume()`（支持在中断中调用 `resume`）、`ek_os_thread_set_prio/get_prio()`、`ek_os_thread_abort()`；Cortex-M4F 硬件 FPU 上下文保存与恢复及空闲线程 `WFI` 等待。
  - 同步与通信原语（均支持动态与静态创建 / `EK_DEFINE_OS_*`）：信号量（`sem.h`）、支持递归重入与级联优先级继承的互斥量（`mutex.h`）、支持 Wait Morphing 优化的条件变量（`condvar.h`）、周期同步屏障（`barrier.h`）、定长消息队列（`msgq.h`）、零拷贝侵入式 FIFO（`kfifo.h`）。
  - 栈溢出检测与钩子：`EKCFG_OS_KERNEL_STACKOVERFLOW_CHECK`（级别 `1` 边界/对齐检查、级别 `2` 魔数染色哨兵 + `ek_os_thread_stack_high_water_mark()` 高水位查询）、`ek_os_kernel_stackoverflow_hook()` 与 `ek_os_kernel_idle_hook()` 弱钩子。
- **VFS 虚拟文件系统与总线模型**（`EKCFG_VFS=1`，`include/ek/vfs/`、`src/vfs/`）：静态节点池、路径注册/注销/查找、fd 池、`read/write/lseek/ioctl/poll/close` 分发、`ek_vfs_tree_dump()`（支持 `EKCFG_VFS_TREE_DUMP_UTF8`）、标准流 `fd 0/1/2` 预留及 `ek_vfs_bind_fd()` / `ek_vfs_bind_stdio()` 运行期热重定向；`vfs_bus` 总线模型与静态设备树声明宏 `EK_DEFINE_VFS_PATH` / `EK_DEFINE_VFS_BUS` / `EK_DEFINE_VFS_BUS_DEV`。
- **嵌入式命令行解释器 Shell**（`EKCFG_SHELL=1`，`include/ek/shell/`、`src/shell/`）：基于 VFS fd 解耦 IO、`.ek_shell_cmd` 静态命令段（`EK_DEFINE_SHELL_CMD`）与动态命令注册/注销、VT100/ANSI 行编辑快捷键、UTF-8 字符列宽支持（`EKCFG_SHELL_UTF8`）与可选内置命令（`EKCFG_SHELL_BUILTIN_CMDS`：`hello`、`echo`、`tree`、`clear`、`version`、`help`）。
- **硬件架构抽象层与通用 Cortex-M 启动文件**（`EKCFG_ARCH=1`，`include/ek/core/arch.h`、`port/arch/{cortexm,riscv,host}/`、`port/startup/cortexm/startup.S`）：跨平台内存屏障、临界区、`ek_arch_in_isr()`、`ek_arch_idle()`；Cortex-M 支持启动期自动重定向 VTOR 至 SRAM、`ek_arch_irq_install()` / `EK_DEFINE_ARCH_IRQ()`、`EKCFG_ARCH_IRQ_COUNT` 边界校验、`EKLD_VECTOR_ALIGN`（默认 `0x200`）与通用启动汇编（不再依赖厂商 `startup_xxx.s`）。
- **新容器与基础工具**：`bitstream`（比特流读写）、`shared_ptr`（引用计数共享指针）、`compiler.h`（独立编译器属性与内联抽象头）；`list.h` 扩充链表操作 API；`def.h` 新增 `ek_off_t`（`int64_t`）、`__EK_CLEANUP`、`EK_INC_MOD` 软件取模与对齐宏。
- **两阶段 CMake 配置与多构建支持**：`cmake/arch.cmake`、`cmake/platform.cmake`（`ek_platform_prepare()` / `ek_platform_configure()`，支持 `SOC` / `SOC_TARGET` / `BUILD_TYPE`）、`cmake/linker.cmake`（`ek_linker_script()` / `ek_linker_attach()`）、`cmake/picolibc.cmake`（`ek_picolibc_apply()`）；支持 Debug/Release 模式联动（`EK_PLATFORM_DEBUG`、`EK_IS_DEBUG`、`EK_DEBUG_ONLY`、`ek_is_debug()`、`ek_build_mode_str()`）与 Host 原生构建（`ARCH host`、`cmake/gnu-gcc.cmake`、`cmake/linker/host.ld`）。
- **新版标准 IO 接口与配置依赖校验**：`ek/core/io.h` 新增 `EK_IO_STDIN/STDOUT/STDERR`、`ek_io_write/read/putc/getc`、`EKCFG_IO_CONSOLE_PATH` 启动自动绑定及非 VFS 模式 `ek_io_ops_t` / `ek_io_set_ops()`；`conf_internal.h` 新增模块间依赖与互斥编译期校验；`static_alloc.h` 新增 `EK_STATIC_ALLOC_ORDER_*` 初始化顺序常量。

### 变更

- 版本宏更新：`EK_VERSION_MAJOR` 6 → 7（`7.0.0`）。
- TLSF 堆增强：`ek_malloc` / `ek_realloc` / `ek_free` 在 `EKCFG_RTOS=1` 下自动提供线程安全并在中断上下文安全拒绝；`EKCFG_TLSF_FL_INDEX_MAX` 与 `EKCFG_TLSF_SL_INDEX_COUNT_LOG2` 改为按堆大小自动推导（新增 `EKCFG_HEAP_MAX_POOL_SIZE`）；`EKCFG_STATIC_ALLOC` 默认改为 `1`。
- 自动初始化阶段重构为 `BOOT` / `BOARD` / `COMPONENT` / `APP` 四阶段，新增库内顺序常量 `EK_EXPORT_ORDER_*`（板级顺序常量移由应用工程自行维护）；`ek_arch_redirect_vtor()` 与 `EK_DEFINE_ARCH_IRQ` 在 `BOOT` 阶段自动安装；`ek_io_init()` 排在 `ek_static_alloc_init()` 之后以支持启动期绑定静态 VFS 控制台设备。
- 配置模板与内部校验按模块重组：`ek_conf_template.h` 与 `conf_internal.h` 将子配置收入各模块条件块内；`EKCFG_ASSERT` / `EKCFG_ASSERT_TINY` / `EKCFG_ASSERT_LOG` / `EKCFG_LOG_DEBUG` 默认跟随 Debug/Release 构建模式；`EKCFG_PICOLIBC` / `EKCFG_NEWLIB` 由 CMake `PICOLIBC` 选项派生。
- `port/clib` 与链接脚本改进：`PICOLIBC=ON` 时仅编译 `picolibc_port.c`（独立 `stdin/stdout/stderr` 流并接入 `ek_io_*`），否则仅编译 `newlib_port.c`（`_write/_read` 整块透传并补齐 `_init/_fini/_exit/_kill/_getpid` 弱桩）；`cortexm.ld.S` / `riscv.ld.S` 增加 `EKLD_*` 参数合法性校验，`.vectors` 收集 `.ek_vectors`。
- 默认 `ek_assert_hook` 失败后改为调用 `abort()` 替代 `while (1)` 死循环；优化 `ek_log` 格式化与输出路径减少底层 IO 调用次数。
- 同步更新中英文集成文档 `README.md` 与 `README.en.md`。

### 修复

- 修复 picolibc 链接模板缺失 `__bothinit_array_start` / `__bothinit_array_end` 导致 `.init_array`（含 `ek_export_init()`）被跳过的问题。
- 修复链接模板 `/DISCARD/` 在 newlib/lwprintf 路径下误丢弃 `libc.a` / `libm.a` / `libgcc.a` 的问题，并在 `._user_heap_stack` 补齐 newlib `_sbrk` 所需 `__heap_start` / `__heap_end`。
- 修复 `EK_STATIC_ALLOC_REGISTER` 在 64 位 host 上缺少指针宽度对齐导致段遍历步进错位及启动崩溃的问题。

### 破坏性变更

- **头文件与源文件移除 `ek_` 前缀**：头文件统一为 `include/ek/<模块>/<文件>.h`（如 `#include "ek/core/log.h"`），不再导出扁平子目录 include 路径（头文件保护宏保留 `EK_` 前缀）。
- **CMake 接口替换**：删除 `cmake/ek_config.cmake` / `cmake/ek_linker.cmake` 及全部 `ek_set_*` 函数，改用 `ek_platform_prepare()` 与 `ek_platform_configure()`；`EK_LINKER_AUTO` 改为 `EK_LINKER_MODE`（`auto` / `off`）；`ek_linker_attach()` 的 `BOARD_PATH` 改名为 `BOARD`；Cortex-M 目标须提供 `SOC`（`ek_soc.h`）且不再引入厂商启动汇编；删除未使用的 `TOOLCHAIN_LINK_LIBRARIES`。
- **移除 `ek_port_io_fputc` 弱函数**：改用 VFS `fd 0/1/2`（`EKCFG_VFS=1`，可配 `EKCFG_IO_CONSOLE_PATH`）或非 VFS 模式 `ek_io_set_ops()`（`EKCFG_VFS=0`）。
- **移除旧导出宏与 `EK_LOCK_*` 外部锁宏**：删除 `EK_EXPORT_EARLIEST` / `HARDWARE` / `COMPONENTS` / `USER` 与公开 `EK_EXPORT_LEVEL`（`ek_export_item_t.level` 更名为 `phase`），改用 `EK_EXPORT_BOOT` / `BOARD` / `COMPONENT` / `APP`；移除旧 `EK_LOCK_*` 宏，`EKCFG_RTOS=1` 转为启用内置 RTOS 内核（并与 `EKCFG_EVOKE` 互斥）。
- **`evoke` 平台回调改为 ops 虚函数表**：移除原弱函数钩子，改为通过 `ek_evoke_ops_t` 传入。
- **枚举命名与断言宏统一**：枚举类型后缀统一由 `_t` 改为 `_e`（`ek_pt_state_e`、`ek_evoke_state_e`、`ek_log_level_e`），不保留兼容别名；移除 `ek_assert_param`，统一使用 `ek_assert`。
- `ek_conf_template.h` 不再定义 `EKCFG_PICOLIBC` / `EKCFG_NEWLIB`；裸机 newlib 需要 syscall 桩时在项目 `ek_conf.h` 中定义 `EKCFG_NEWLIB 1`。

## [6.2.1] - 2026-10-27

### 修复

- 修复 `ek_set_toolchain()` 在已有 CMake 缓存时误报 `must be called before project()`：`CMAKE_PROJECT_NAME` 会在二次 configure 时从缓存预加载，不能用来判断 `project()` 是否已执行。

## [6.2.0] - 2026-10-27

### 新增

- 新增 `ek_set_toolchain()`：按 `PLATFORM` 自动选择 `cmake/gcc-arm-none-eabi.cmake` 或 `cmake/gcc-riscv-none-elf.cmake`，必须在 `project()` 之前调用；顶层工程不再需要传 `-DCMAKE_TOOLCHAIN_FILE`。

## [6.1.1] - 2026-10-27

### 修复

- ARM 工具链仅在未启用 picolibc 时追加 `--specs=nano.specs`；`PICOLIBC=ON` 时由 `cmake/picolibc.cmake` 注入 picolibc specs，父工程无需再复制或修改工具链。

## [6.1.0] - 2026-10-27

### 新增

- 新增 `cmake/picolibc.cmake` 定位模块，按 `PICOLIBC_SPECS` → `PICOLIBC_ROOT` → 工具链自带 → 系统安装（`/usr/lib/picolibc/<triple>`）→ `PICOLIBC_BUILD` → `PICOLIBC_FETCH` 的顺序解析 picolibc，失败时给出可操作的错误提示。
- 新增 `EKCFG_NEWLIB` 与 `port/clib/newlib/newlib_port.c`：为裸机 newlib/nanolibc 提供弱符号 `_write` / `_read` / `_close` / `_fstat` / `_isatty` / `_lseek` / `_sbrk`，BSP 可覆盖。
- `ek_io.h` 的系统 libc 分支补全 `ek_printf` / `ek_vprintf` / `ek_sprintf` / `ek_snprintf` / `ek_vsnprintf` 宏，host 上可以直接使用 glibc，不再强制绑定 picolibc。
- `PICOLIBC_BUILD=ON`：按 `PLATFORM` 自动映射 `-mcpu` / `-mfpu` / `-mfloat-abi`，用 meson + ninja 从 picolibc 官方源码构建，安装到 `<build>/_deps/`。
- `PICOLIBC_FETCH=ON`：按 `PICOLIBC_PLATFORM` 下载预编译包，支持 http(s)、`file://` 和本地路径；下载归档、解压结果带缓存，重复 configure 不会重复下载或解压；支持自动读取 `${URL}.sha256` 校验。
- 新增配置项：`PICOLIBC_PLATFORM`、`PICOLIBC_PREFIX`、`PICOLIBC_SOURCE_DIR`、`PICOLIBC_SOURCE_URL`、`PICOLIBC_SHA256_<platform>`。
- 新增 `scripts/build-picolibc.sh`：单平台打包脚本，支持 ARM、RISC-V、x86_64，产出 `.tar.gz` + `.sha256`，并用生成的 specs 链接最小程序自检。
- 新增 `.github/workflows/picolibc-prebuilt.yml`：手动或每周检查 picolibc 最新版本，矩阵构建 12 个平台并发布到 `picolibc-<version>` Release。
- 新增 `ek_board_template.h` 与 `EK_BOARD_PATH`：板级内存布局（`EKLD_*`）从 `ek_conf.h` 分离，通过独立 `ek_board.h` 提供；库内不包含任何具体板子的配置。
- 新增 `cmake/ek_config.cmake`：`ek_set_conf_file()` / `ek_set_board_file()` / `ek_set_linker_file()` / `ek_set_arch()` / `ek_set_picolibc()` / `ek_set_linker_arch()` / `ek_set_linker_template()`；`ek_set_arch()` 和 `ek_set_picolibc()` 需在 `project()` 之前调用，文件与链接配置需在 `add_subdirectory(ek_platform)` 之前调用。
- 新增 `cmake/ek_linker.cmake` 与 `cmake/linker/cortexm.ld.S`：用 C 预处理器生成链接脚本，自动通过 `INTERFACE_LINK_OPTIONS` / `INTERFACE_LINK_DEPENDS` 注入链接目标；支持 `.ek_export`、`.ek_static_alloc`、TCMRAM、SDRAM0/SDRAM1 段，提供 `EK_LINKER_AUTO` 自动模式和 `ek_linker_attach()` 多目标模式。
- 新增 `cmake/linker/riscv.ld.S`：通用 RISC-V 裸机模板，默认入口 `_start`，保留 `.ek_export`、`.ek_static_alloc` 和 SDRAM0/SDRAM1 段；`EK_LINKER_ARCH` / `PLATFORM` 会自动选择 `cortexm.ld.S` 或 `riscv.ld.S`，`EK_LINKER_TEMPLATE` 仅作显式覆盖。
- 新增 `cmake/gcc-riscv-none-elf.cmake`：支持 `rv32imac` / `rv32imafc` / `rv64imac` / `rv64gc`；`RISCV_TOOLCHAIN_PREFIX` 可指定 xpack 或 GNU 工具链前缀（包含目录和末尾 `-`）。

### 变更

- `PICOLIBC=ON` 时不再依赖仓库内的 `third_party/picolibc`，预编译库和生成头文件不再随仓库分发。
- CMake 不再硬编码 `third_party/picolibc/libs/cortexm/<PLATFORM>` 与 `-lc -lnosys -lm`；有 specs 时使用 `--specs` + 可选的 `--picolibc-prefix`，无 specs 时回退到 `libcrt0` / `libc` / `libm` / `libnosys` 并自动查找 `picolibc.ld`。
- `PICOLIBC_SHA256` 由必填改为可选：未提供时自动尝试 `<URL>.sha256`，仍没有则跳过校验并打印实际哈希。
- 下载包解压目录与源码构建目录分开存放（`_deps/picolibc-fetch-*` / `_deps/picolibc-*`）。
- README 更新 picolibc 定位方式、系统包安装提示、缓存说明和多架构平台表；错误提示补充 Debian/Ubuntu 的 `apt install picolibc-arm-none-eabi`。
- 新增英文版集成文档 `README.en.md`，中英文 README 顶部增加语言切换链接。
- `ek_export_init()` 标记为 `__attribute__((constructor))`，由启动代码的 `__libc_init_array()` 在 `main()` 之前自动调用，使用方不再需要显式调用。
- 链接脚本改由 `ek_platform` 生成，不再需要手写 `.ld`：生成时同时读取 `ek_conf.h`（组件开关）、`ek_conf_internal.h`（默认值）和 `ek_board.h`（内存布局）；`EKCFG_EXPORT` / `EKCFG_STATIC_ALLOC` 关闭时对应段自动省略。
- 生成脚本保留 SDRAM0/SDRAM1 两个可选区域；`SDRAM` 区域更名为 `SDRAM0`，旧 `.sdram_data` 输入段仍映射到 `SDRAM0`，边界符号统一为 `_sdram0_data_start` / `_sdram0_data_end`。

### 修复

- `*_init_static()` / `*_deinit_static()` 不再受 `EKCFG_STATIC_ALLOC` 控制，始终参与编译；该宏现在只控制 `EK_DEFINE_*` / `EK_STATIC_ALLOC_REGISTER` 注册机制。修复 `EKCFG_STATIC_ALLOC=0` 时 `ek_evoke_init()` 调用 `ek_ringbuf_init_spsc_static()` 无法编译的问题，覆盖 ringbuf（普通/SPSC）、stack、snapshot、evoke、picothread（pt/sem/msg）。

### 破坏性变更

- 删除 `third_party/picolibc/`（244 个文件，约 39M）。直接引用该目录的构建脚本需要改用 `PICOLIBC_ROOT` / `PICOLIBC_SPECS` / `PICOLIBC_BUILD` / `PICOLIBC_FETCH`。
- `PICOLIBC=ON` 但系统、工具链、`PICOLIBC_ROOT` 都没有 picolibc，且未开启 `PICOLIBC_BUILD` / `PICOLIBC_FETCH` 时，configure 直接失败。
- `ek_export_init()` 改为 `__attribute__((constructor))` 自动执行；若仍显式调用，会重复执行所有导出项（含 `ek_heap_init()`）。

## [6.0.0] - 2026-10-25

### 新增

- 新增 MIT 许可证文件 `LICENSE`，README 的许可章节同步说明；`third_party/` 下的代码仍遵循各自许可证。

### 变更

- README 重写为集成指南：保留目录结构、集成方式、CMake 配置、链接脚本、配置项和弱函数说明，删除逐模块移植章节。
- 目录结构按模块重组：公共头文件由 `inc/` 移至 `include/ek/`（`core` / `mem` / `container` / `sched` 四个子目录），源文件由 `src/*.c` 同步移入对应模块目录。
- picolibc 适配层由 `src/ek_picolibc_port.c` 移至 `port/clib/picolibc/picolibc_port.c`。
- 库内头文件引用统一为带模块前缀的形式（如 `#include "ek/core/ek_def.h"`）；`EK_INCLUDE_DIRS` 增加 `include/` 根目录，同时保留原有子目录条目以兼容使用方的扁平引用（如 `#include "ek_log.h"`）。
- CMake 工程与 OBJECT 库 target 由 `ek_utils` 更名为 `ek_platform`；源文件列表按模块显式列出，替代 `file(GLOB src/*.c)`。
- 版本宏更新：`EK_VERSION_MAJOR` 5 → 6。

### 破坏性变更

- 头文件路径变更：`inc/ek_def.h` 等 → `include/ek/<module>/<name>.h`。使用方需将 `-I` 指向 `include/`，建议改用 `#include "ek/<module>/<name>.h"`。
- CMake target 更名：`target_link_libraries(<target> PRIVATE ek_utils)` → `ek_platform`。
- 直接引用源文件路径的构建脚本需同步更新：`src/ek_evoke.c` → `src/sched/ek_evoke.c`、`src/ek_heap.c` → `src/mem/ek_heap.c`、`src/ek_picolibc_port.c` → `port/clib/picolibc/picolibc_port.c`。

## [5.0.0] - 2026-10-22

### 变更

- 错误返回体系改为 POSIX errno：所有原返回 `ek_err_t` 的接口改为返回 `int`，成功返回 0（有数量语义时直接返回数量），失败返回负 errno（如 `-EINVAL`、`-ENOSPC`、`-ETIMEDOUT`）。
- 错误传播宏 `EK_ERR_RETURN` / `EK_ERR_GOTO` 移入 `ek_def.h`，改为按返回值 `< 0` 判断。
- 版本宏更新：`EK_VERSION_MAJOR` 4 → 5。
- 容器反查宏从 `ek_list.h` 移至 `ek_def.h`，由 `ek_list_container` 更名为 `EK_CONTAINER_OF`，不再依赖 `EKCFG_LIST`。

### 移除

- 移除 `ek_utils/inc/ek_err.h`：删除 `ek_err_t` 类型、`EK_OK` / `EK_ERR_*` 错误码与 `ek_strerror()`。

### 破坏性变更

- 所有返回错误码的 API 返回类型由 `ek_err_t` 变为 `int`，调用点需改用 `0` / `-E*`（`<errno.h>` 现已由 `ek_def.h` 引入）。
- `ek_static_alloc_get_init_error()` 返回 `int`，成功为 0。
- `ek_pt_t::wait_result` 改为 `int`，取值为 0 或负 errno。
- 移除 `ek_strerror()`，可改用标准 `strerror(-err)`。
- `ek_list_container(ptr, type, member)` 更名为 `EK_CONTAINER_OF(ptr, type, member)`，宏已移至 `ek_def.h`，调用点需改名。

## [4.0.0] - 2026-10-17

### 变更

- 各模块 API 不再使用断言检查入参：参数非法时返回对应错误（`EK_ERR_INVAL`/`NULL`/`false`/直接返回），内存分配失败同样返回错误值
- 模块未初始化、非任务上下文等内部状态检查仍保留断言

### 移除

- 移除句柄指针别名类型，统一改用原始指针类型：
  - `ek_evoke_task_handle_t` → `ek_evoke_task_t *`
  - `ek_evoke_event_handle_t` → `ek_evoke_event_t *`
  - `ek_pt_handle_t` → `ek_pt_t *`
  - `ek_pt_sem_handle_t` → `ek_pt_sem_t *`
  - `ek_pt_msg_handle_t` → `ek_pt_msg_t *`

## [3.1.0] - 2026-10-17

### 新增

- 新增字符串视图模块 `ek_str_view` （`EKCFG_STR_VIEW`，默认关闭)：无动态分配的 C 字符串裁剪模块。

## [3.0.0] - 2026-10-17

### 变更

- `ek_err.h` 将原本用于表达函数正常执行的 `EK_ERR_NONE` 变更为 `EK_OK`

## [2.2.0] - 2026-10-09

### 新增

- `ek_def.h` 新增基础工具宏：`EK_DIV_CEIL(x, div)`（向上取整除法，除数为 0 时安全返回 `x`）、`EK_MAX(v1, v2)`、`EK_MIN(v1, v2)`。
- `EK_CLAMP(val, min, max)` 防止重复求值：内部改用 `__typeof__` 临时变量，各参数仅求值一次。

## [2.1.0] - 2026-08-30

相对 [v2.0.0rc](https://github.com/EmbeddedKitOrg/EK_UTILS/releases/tag/v2.0.0rc) 的正式版本。

### 新增

- 快照数据模块 `ek_snapshot`（`EKCFG_SNAPSHOT`，默认关闭）：单槽快照，生产者以唯一 id（`unique`，0 表示无数据）覆盖写入，消费者随时整块读取最新值；控制块与数据单次分配（柔性数组内联），支持动态创建与 `EK_SNAPSHOT_DEFINE(handle, size)` 静态定义。

### 变更

- 容器静态分配改为 **union 容器**模式：`EK_DEFINE_*` 展开为 union（控制块与数据物理连续），`handle` 从对象变为 `T *const` 指针，用法与动态创建返回值统一（`&handle` → `handle`）；容器结构体的 `data[]` 柔性数组成为唯一数据位置，动态（malloc 尾部）与静态（union 尾部）读写代码零区分。
- 容器结构体删除 `buffer` 字段：`ek_ringbuf_t` / `ek_ringbuf_spsc_t` / `ek_stack_t` / `ek_snapshot_t` / `ek_pt_msg_t` 不再保存数据区指针，读写直接访问 `data[]`，相比 2.0.0rc 每对象减少 4 字节并少一次堆操作。
- `*_init_static()` 签名删除 buffer 参数（数据内嵌于 union，无需外部挂载）：`ek_ringbuf_init_static(rb, item_size, item_amount)`、`ek_ringbuf_init_spsc_static(rb, item_size, amount)`、`ek_stack_init_static(sk, item_size, item_amount)`、`ek_pt_msg_init_static(msg, item_size, item_amount)`、`ek_snapshot_init_static(snapshot, data_size)`。
- `ek_pt_msg_t` 成员重排：`recv_wait` / `send_wait` 在前，`ek_ringbuf_t rb` 置尾，`rb.data` 即消息数据区。
- evoke 内部 ISR FIFO 同步改为 union 容器（对外 API 无变化）。
- 版本宏更新：`EK_VERSION_MINOR` 0 → 1。

### 破坏性变更

从 2.0.0rc 升级必须改调用点，无兼容别名：

- `EK_DEFINE_*` 展开的句柄从对象变为 `const` 指针：所有 `&handle` 用法改为 `handle`。
- 直接访问容器 `.buffer` 字段的代码改为 `data[]`（数据始终紧跟控制头）。
- 静态初始化函数调用的实参需按新签名去除 buffer/storage 实参。
- 依赖 `ek_pt_msg_t` 字段偏移的代码需按新布局（`recv_wait` / `send_wait` / `rb`）调整。

## [2.0.0rc] - 2026-08-14

相对 [v1.2.0](https://github.com/EmbeddedKitOrg/EK_UTILS/releases/tag/v1.2.0) 的预发布版本。包含破坏性 API 变更，请勿当作稳定版依赖。

### 新增

- 静态对象自动注册模块 `ek_static_alloc`（`EKCFG_STATIC_ALLOC`，默认关闭）。`EK_DEFINE_*` 把对象登记到链接器段 `.ek_static_alloc`，由 `ek_static_alloc_init()` 按 `order` 升序初始化；`EKCFG_EXPORT=1` 时经 `EK_EXPORT_COMPONENTS` 在 `ek_export_init()` 中自动调用，否则须在堆初始化之后手动调用。失败时记录首个错误（`ek_static_alloc_get_init_error()`）并继续后续对象。
- 静态定义宏与对应 `*_init_static` / `*_deinit_static`：
  - 容器（order=10）：`EK_DEFINE_STACK(handle, type, amount)`、`EK_DEFINE_RINGBUF(handle, type, amount)`、`EK_DEFINE_RINGBUF_SPSC(handle, type, amount)`
  - 调度对象（order=20）：`EK_DEFINE_PT(handle, cb, prio, arg)`、`EK_DEFINE_PT_SEM(handle, count)`、`EK_DEFINE_PT_MSG(handle, type, amount)`、`EK_DEFINE_EVOKE_TASK(handle, cb, arg)`、`EK_DEFINE_EVOKE_EVENT(handle, init_count)`
- TLSF 索引改为配置宏，无需改第三方源码：`EKCFG_TLSF_FL_INDEX_MAX`（默认 24）、`EKCFG_TLSF_SL_INDEX_COUNT_LOG2`（默认 3）。
- 第三方源码按配置空编译：`third_party/tlsf` 由 `EKCFG_HEAP_TLSF==1` 守卫，`third_party/lwprintf` 由 `EKCFG_IO_LWPRTF==1` 守卫。
- 微线程最低优先级可配：`EKCFG_PT_PRIO_LOWEST`（默认 31，就绪队列档位数为该值 + 1）。
- `ek_io.h` 在非 lwprintf/picolibc 路径下补 `#include <stdio.h>`，方便用户把 `ek_printf` 映射到标准 libc。
- `ek_def.h` 增加软件版本宏：`EK_VERSION_MAJOR` / `EK_VERSION_MINOR` / `EK_VERSION_PATCH`（当前 2.0.0）。

### 变更

- `ek_pt_msg_t.rb` 从 `ek_ringbuf_t *` 改为内嵌 `ek_ringbuf_t`，静态消息队列不再额外声明独立 ringbuf 对象。
- README / `ek_conf_template.h` / `ek_conf_internal.h` 同步静态分配、TLSF 配置宏与 IO 后端说明。

### 破坏性变更

从 v1.2.0 升级必须改调用点，无兼容别名：

- `ek_evoke_task_create(const char *name, ek_evoke_cb_t cb, void *arg)` → `ek_evoke_task_create(ek_evoke_cb_t cb, void *arg)`
- `ek_evoke_event_create(const char *name, uint32_t init)` → `ek_evoke_event_create(uint32_t init)`
- `ek_pt_create(const char *name, ek_pt_cb_t cb, uint8_t prio, void *arg)` → `ek_pt_create(ek_pt_cb_t cb, uint8_t prio, void *arg)`
- `ek_evoke_task` / `ek_evoke_event` / `ek_pt` 去掉 `const char *name` 字段。
- 直接读写 `ek_pt_msg_t.rb` 指针的代码改为使用内嵌 `ek_ringbuf_t`。
- 启用静态分配时，链接脚本必须增加 `.ek_static_alloc` 段（`KEEP(*(SORT(.ek_static_alloc*)))`，符号 `_ek_static_alloc_start` / `_ek_static_alloc_end`）。

[2.0.0rc]: https://github.com/EmbeddedKitOrg/EK_UTILS/compare/v1.2.0...v2.0.0rc
[2.2.0]: https://github.com/EmbeddedKitOrg/EK_UTILS/compare/v2.1.0...v2.2.0
[2.1.0]: https://github.com/EmbeddedKitOrg/EK_UTILS/compare/v2.0.0rc...v2.1.0
[5.0.0]: https://github.com/EmbeddedKitOrg/EK_UTILS/compare/v4.0.0...v5.0.0
[6.1.0]: https://github.com/EmbeddedKitOrg/EK_Platform/compare/v6.0.0...v6.1.0
[6.1.1]: https://github.com/EmbeddedKitOrg/EK_Platform/compare/v6.1.0...v6.1.1
[6.2.0]: https://github.com/EmbeddedKitOrg/EK_Platform/compare/v6.1.1...v6.2.0
[6.2.1]: https://github.com/EmbeddedKitOrg/EK_Platform/compare/v6.2.0...v6.2.1
[7.0.0]: https://github.com/EmbeddedKitOrg/EK_Platform/compare/v6.2.1...v7.0.0
[Unreleased]: https://github.com/EmbeddedKitOrg/EK_Platform/compare/v7.0.0...HEAD
