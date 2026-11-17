# 更新日志

本文件记录 [ek_platform](https://github.com/EmbeddedKitOrg/EK_Platform) 的版本变更（v5 及更早版本以 ek_utils / EK_UTILS 名义发布）。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### 新增

- 新增 `ek_kfifo` 侵入式零拷贝 FIFO（`EKCFG_KFIFO`）：用户结构体内嵌 `ek_kfifo_node_t`，入队/出队只链接指针，O(1)，可在 ISR 中使用；提供 `put` / `put_front` / `put_list` / `get` / `get_all` / `peek_*` / `remove` / `count`，以及 `ek_kfifo_get_entry()`、`EK_DEFINE_KFIFO()`、`EK_KFIFO_FILL()`（初始化对象池）。
- 新增 `ek_critical_enter()` / `ek_critical_exit()`（`ek/core/ek_critical.h`）：弱符号，默认在 Cortex-M 上保存/恢复 PRIMASK、在 RISC-V 上保存/恢复 mstatus.MIE，支持嵌套；`ek_kfifo` 与 `ek_evoke` 的默认临界区都基于它。
- 新增 `EKCFG_EXPORT_AUTO_INIT`（默认 1）：设为 0 时 `ek_export_init()` 不再作为 constructor 在 `main()` 之前执行，可在时钟配置完成后手动调用。
- `ek_evoke` 新增 `ek_evoke_run_once()`、`ek_evoke_get_tick()` 与弱函数 `ek_evoke_get_timer_elapsed()`。
- 新增 `ek_heap_destroy()`、`ek_heap_is_ready()`、`ek_list_node_is_linked()`，`ek_def.h` 新增 `EK_TICK_AFTER()` / `EK_TICK_AFTER_EQ()`（回绕安全的 tick 比较）、`EK_COMPILER_BARRIER()` / `EK_MEMORY_BARRIER()`、`ek_ctz32()`。
- 新增 `tests/`：主机单元测试（CMake + CTest，可选 ASan/UBSan）与 `tests/compile_check.sh` 多配置交叉编译检查；新增 `.github/workflows/ci.yml`，在 push / PR 时运行。

### 修复

- picothread：`EK_PT_SEM_GIVE` 唤醒等待者后，等待者会重新 `take` 而拿不到信号量，导致这次释放丢失、等待者一直等到超时。现在 give 直接把信号量移交给等待者。
- picothread：`EK_PT_SEM_TAKE` / `EK_PT_MSG_*` 立即成功时 `err` 会沿用上一次等待的结果（如 `-ETIMEDOUT`），调用方误以为失败而泄漏信号量。现在每次等待前清零。
- picothread：挂起或阻塞一个正在等待信号量/消息的任务后，再 give/send 会对已摘下的节点重复移除导致空指针访问。挂起现在会取消等待（结果为 `-EINTR`），销毁为 `-ECANCELED`。
- picothread：消息队列改为直接移交（发送方直接拷入等待中的接收缓冲区，接收后把等待发送者的数据补入缓冲区），不会再被其他任务"抢走"。
- picothread / evoke：tick 比较未处理 32 位回绕，1 kHz 节拍下约 49.7 天后延时任务会被提前唤醒。现在所有比较都是回绕安全的；`EK_PT_WAIT_FOREVER` 不再借用 tick 值。
- picothread：`ek_pt_schedule()` 在 `now == 0` 时返回值会与"可无限睡眠"的 0 混淆；调度结束后清空当前任务，主循环中可以销毁刚运行过的任务。
- evoke：定时器运行途中插入更早的延时请求时，重新装载丢掉了已走过的时间，已排队的请求被整体推迟。现在通过虚拟时钟累计已走过的时间（需要实现 `ek_evoke_get_timer_elapsed()`）。
- evoke：任务回调执行期间到来的 publish 只增加计数，回调返回后任务被直接挂回等待队列，积压的事件要等下一次 publish 才被处理。现在回调返回后重新订阅时会消费积压计数。
- evoke：检查"无事可做"与进入睡眠之间没有临界区，中断落在两者之间会导致请求一直等到下一个中断。现在睡眠函数在临界区内调用。
- evoke：在回调中销毁自己会访问已释放的任务；销毁事件后，仍指向它的延时请求和就绪任务会访问已释放内存。均已修复。
- evoke：ISR 请求队列实际只能存 `EK_EVOKE_MAX_ISR_REQ - 1` 个请求。
- `ek_list_remove()` 移除后把指针置 NULL，重复移除即空指针访问；现在节点指回自身，重复移除是空操作。
- `EK_LOG_*` 宏是裸 `if`，在 `if/else` 中使用时 `else` 会被宏截走；现在用 `do { } while (0)` 包裹。
- `EKCFG_ASSERT=0` 时 `ek_assert_param` 未定义、`EKCFG_LOG=0` 时 `EK_LOG_*` 未定义，导致 picothread、evoke 等模块编译失败；现在提供空实现。
- 只开 `EKCFG_PICOTHREAD` / `EKCFG_EVOKE` 而未开链表/环形缓冲区时编译失败；现在自动打开依赖模块。
- `EK_PT_YEILD` 展开为 `case __LINE__: }`（标签后直接是 `}`），C23 之前不合法，旧版 GCC 与严格编译器会报错；新宏 `EK_PT_YIELD` 补上空语句。
- 部分 C 库（ARM Compiler 的 armlib/microlib、MinGW）缺少 `ENODATA` / `ETIMEDOUT` / `ECANCELED` 等错误码，现在由 `ek_def.h` 补齐。
- `CRLF` 被定义为 `" \r\n"`（多一个空格）。
- SPSC 环形缓冲区的索引没有 `volatile` 与内存屏障，ISR 与主循环之间可能读到未写完的数据。
- 环形缓冲区每次读写用 `%` 取模，Cortex-M0 等没有硬件除法的内核上需要调用软件除法；改为比较回绕。
- `ek_snapshot_get()` 在无数据时返回而未释放锁；`ek_snapshot_set()` 未按文档拒绝 `unique == 0`。
- `ek_str_view_trim()` 把 `char` 直接传给 `isspace()`，遇到 0x80 以上字节（如 UTF-8）是未定义行为。
- `ek_heap_init()` 失败时静默死循环；现在断言失败，断言关闭时 `ek_malloc()` 返回 NULL。RTOS 模式下 TLSF 堆操作现在加锁。
- `ek_export_init()` / `ek_static_alloc_init()` 不再在栈上开 VLA、不再依赖 `qsort`，改为原地按 (level, order, 段内顺序) 选择；`ek_export_init()` 重复调用只执行一次。
- 工具链文件只识别 CMake ≥ 3.24 的 `CMakeScratch` 临时工程，CMake 3.20～3.23 下编译器检测因缺少 `PLATFORM` 而失败；现在同时识别 `CMakeTmp` 并通过 `CMAKE_TRY_COMPILE_PLATFORM_VARIABLES` 传递 `PLATFORM`。
- 库不再强制 `-Os -g`：只在未指定 `CMAKE_BUILD_TYPE` 时作为默认值，Debug/Release 等沿用主工程选项。

### 变更

- `EKCFG_PICOLIBC` / `EKCFG_IO_LWPRTF` 的内部默认值改为与模板一致（`0` / `1`）。依赖旧默认值（未在 `ek_conf.h` 中定义、实际使用 picolibc）的工程需显式设置 `EKCFG_PICOLIBC=1`。
- RTOS 模式下 ringbuf / stack / snapshot 在锁被占用时改为阻塞等待（`EK_LOCK_ACQUIRE`），不再返回 `-EBUSY`；`EK_LOCK_TRY` 不再是必填宏。
- `ek_evoke_light_sleep()` / `ek_evoke_deep_sleep()` 改为在临界区内调用（Cortex-M 上 PRIMASK=1 时挂起的中断仍能唤醒 WFI），实现中不要自行开中断；`ek_evoke_set_timer()` 也在临界区内调用。
- `ek_evoke_enter_critical()` / `ek_evoke_exit_critical()` 的默认实现从空函数改为调用 `ek_critical_*()`（关中断）。
- `ek_static_alloc_init()` 失败日志不再调用 `strerror()`，直接输出错误码。
- README 修正编译器支持说明：armcc 下 `EKCFG_EXPORT` / `EKCFG_STATIC_ALLOC` 依赖 GNU ld 链接脚本，暂不支持 scatter 文件；补全源文件直接集成时需要的 include 路径。

### 弃用

- `EK_PT_YEILD` → `EK_PT_YIELD`，`ek_heap_destory()` → `ek_heap_destroy()`，`EK_EVOKE_STATE_WAITTING` → `EK_EVOKE_STATE_WAITING`；旧名保留为兼容别名。

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
