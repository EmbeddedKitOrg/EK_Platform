# 更新日志

本文件记录 [ek_utils](https://github.com/EmbeddedKitOrg/EK_UTILS) 的版本变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [7.0.0] - 2026-10-29

### 新增

- `cmake/ek_arch.cmake`：`PLATFORM` 的唯一架构表，集中提供 FAMILY / FLAGS / TOOLCHAIN / LINKER_ARCH / ENTRY / PICOLIBC_PLATFORM；ARM、RISC-V、host 共用一份数据。
- `cmake/ek_platform.cmake`：两阶段配置入口。`ek_platform_prepare(ARCH ... PICOLIBC ...)` 在 `project()` 前选择工具链并同步 `PLATFORM` / `PICOLIBC_PLATFORM`；`ek_platform_configure(CONF ... BOARD ... LINKER ...)` 在 `add_subdirectory()` 前设置配置与链接模式。
- host 原生构建：新增 `cmake/gnu-gcc.cmake` 与 `cmake/linker/host.ld`，`ek_platform_prepare(ARCH host)` 可直接编译并链接 host 可执行文件，`host.ld` 提供 `.ek_export` / `.ek_static_alloc` 段边界。
- `cmake/ek_linker.cmake` 新增 `ek_linker_script()`：纯生成函数，不读全局、不碰 target；新增 `ek_linker_attach()` 作为唯一注入点，按 target 类型自动选择 `INTERFACE` / `PRIVATE`。
- `cmake/picolibc.cmake` 新增 `ek_picolibc_apply(target)`：探测、下载、源码构建、链接策略全部收敛到 target，不再回写 9 个全局变量。
- `test/cmake/`：arch 表断言、链接脚本 golden、host configure、缺 BOARD 错误路径的最小测试脚本。
- `ek_conf_internal.h` 新增内部 `DEPEND_ON` 宏与配套 `#error` 依赖校验。
- `ek_def.h` 内部新增 `__EK_CLEANUP` 宏，用于包装 `__attribute__((cleanup(x)))`。
- 新增 VFS 初步支持（`ek_vfs_core` / `ek_vfs_priv`），包含节点池、路径 token 解析和树操作。

### 变更

- CMake 配置收敛为 `ek_platform_prepare()` / `ek_platform_configure()` 两个函数；`PICOLIBC` 成为构建事实，`EKCFG_PICOLIBC` / `EKCFG_NEWLIB` 由 CMake 的 `PICOLIBC` 选项派生，`ek_conf_template.h` 不再重复定义。
- 链接脚本生成与注入分离：生成结果登记 conf/board/template 以及 `EK_LINKER_INCLUDE_DIRS` 下的头文件为 configure 依赖；注入按 target 类型选择 `INTERFACE` / `PRIVATE`，重复注入直接报错。
- picolibc 探测顺序保持 `PICOLIBC_SPECS` → `PICOLIBC_ROOT` → 工具链 → 系统安装 → `PICOLIBC_BUILD` → `PICOLIBC_FETCH`；下载增加 30 秒超时，删除无人设置的 `PICOLIBC_SHA256_<platform>` 分支。
- `port/clib` 按 `PICOLIBC` 选择实现：`PICOLIBC=ON` 只编译 `port/clib/picolibc/picolibc_port.c`，否则只编译 `port/clib/newlib/newlib_port.c`；newlib 端口是否实际生效仍由 `EKCFG_NEWLIB` 控制。
- 调整各个模块 `EK_EXPORT_COMPONENTS` 的优先级，不再占用 0、1 这类最高优先级。
- 优化 `ek_log` 模块的日志输出宏

### 修复

- 链接模板补齐 picolibc 的 `__bothinit_array_start` / `__bothinit_array_end`：picolibc 的 `__libc_init_array()` 使用这两个符号，缺失时不会执行 `.init_array`，导致 `ek_export_init()` 及所有导出初始化被跳过。
- `newlib_port.c` 补齐 `_init` / `_fini` / `_exit` / `_kill` / `_getpid` 弱桩，修复 newlib/nanolibc 链接时的未定义引用。
- 链接模板的 `/DISCARD/` 仅在 `EKCFG_PICOLIBC != 0` 时生成，避免 newlib/lwprintf 路径丢弃 `libc.a` / `libm.a` / `libgcc.a`。
- `._user_heap_stack` 增加 `__heap_start` / `__heap_end`，供 newlib `_sbrk` 使用；picolibc 路径下未被引用时不会生成。
- `ek_static_alloc_init()` 的导出 order 调整为 20，确保在 `ek_heap_init` / `ek_io_init` / `ek_evoke_init` / `ek_pt_init`（order 10）之后执行，避免静态对象在依赖组件初始化前注册。
- 'ek_vfs_core.c' 修复一些bug。

### 破坏性变更

- 删除 `cmake/ek_config.cmake` 和全部 `ek_set_*` 接口；消费者改用 `ek_platform_prepare()` / `ek_platform_configure()`，调用顺序不变（`project()` 前 / `add_subdirectory()` 前）。
- `EK_LINKER_AUTO` 改为 `EK_LINKER_MODE`（`auto` / `off`）；`ek_linker_attach()` 的 `BOARD_PATH` 参数改名为 `BOARD`，多 target 场景先调用 `ek_platform_configure(... LINKER off)`。
- `ek_conf_template.h` 不再定义 `EKCFG_PICOLIBC` / `EKCFG_NEWLIB`；裸机 newlib 需要 syscall port 时在项目 `ek_conf.h` 中定义 `EKCFG_NEWLIB 1`。
- 删除工具链文件中的 `TOOLCHAIN_LINK_LIBRARIES`（未被消费）。
- 枚举类型别名统一由 `_t` 改为 `_e`：`ek_pt_state_t` → `ek_pt_state_e`、`ek_evoke_state_t` → `ek_evoke_state_e`、`ek_log_level_t` → `ek_log_level_e`；`ek_vfs_node_e` 原本已使用 `_e`。不保留旧名兼容别名。

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
