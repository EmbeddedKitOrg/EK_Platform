# 更新日志

本文件记录 [ek_utils](https://github.com/EmbeddedKitOrg/EK_UTILS) 的版本变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### 新增

- 新增 `cmake/picolibc.cmake` 定位模块，按 `PICOLIBC_SPECS` → `PICOLIBC_ROOT` → 工具链自带 → 系统安装（`/usr/lib/picolibc/<triple>`）→ `PICOLIBC_BUILD` → `PICOLIBC_FETCH` 的顺序解析 picolibc，失败时给出可操作的错误提示。
- 新增 `EKCFG_NEWLIB` 与 `port/clib/newlib/newlib_port.c`：为裸机 newlib/nanolibc 提供弱符号 `_write` / `_read` / `_close` / `_fstat` / `_isatty` / `_lseek` / `_sbrk`，BSP 可覆盖。
- `ek_io.h` 的系统 libc 分支补全 `ek_printf` / `ek_vprintf` / `ek_sprintf` / `ek_snprintf` / `ek_vsnprintf` 宏，host 上可以直接使用 glibc，不再强制绑定 picolibc。
- `PICOLIBC_BUILD=ON`：按 `PLATFORM` 自动映射 `-mcpu` / `-mfpu` / `-mfloat-abi`，用 meson + ninja 从 picolibc 官方源码构建，安装到 `<build>/_deps/`。
- `PICOLIBC_FETCH=ON`：按 `PICOLIBC_PLATFORM` 下载预编译包，支持 http(s)、`file://` 和本地路径；下载归档、解压结果带缓存，重复 configure 不会重复下载或解压；支持自动读取 `${URL}.sha256` 校验。
- 新增配置项：`PICOLIBC_PLATFORM`、`PICOLIBC_PREFIX`、`PICOLIBC_SOURCE_DIR`、`PICOLIBC_SOURCE_URL`、`PICOLIBC_SHA256_<platform>`。
- 新增 `scripts/build-picolibc.sh`：单平台打包脚本，支持 ARM、RISC-V、x86_64，产出 `.tar.gz` + `.sha256`，并用生成的 specs 链接最小程序自检。
- 新增 `.github/workflows/picolibc-prebuilt.yml`：手动或每周检查 picolibc 最新版本，矩阵构建 12 个平台并发布到 `picolibc-<version>` Release。

### 变更

- `PICOLIBC=ON` 时不再依赖仓库内的 `third_party/picolibc`，预编译库和生成头文件不再随仓库分发。
- CMake 不再硬编码 `third_party/picolibc/libs/cortexm/<PLATFORM>` 与 `-lc -lnosys -lm`；有 specs 时使用 `--specs` + 可选的 `--picolibc-prefix`，无 specs 时回退到 `libcrt0` / `libc` / `libm` / `libnosys` 并自动查找 `picolibc.ld`。
- `PICOLIBC_SHA256` 由必填改为可选：未提供时自动尝试 `<URL>.sha256`，仍没有则跳过校验并打印实际哈希。
- 下载包解压目录与源码构建目录分开存放（`_deps/picolibc-fetch-*` / `_deps/picolibc-*`）。
- README 更新 picolibc 定位方式、系统包安装提示、缓存说明和多架构平台表；错误提示补充 Debian/Ubuntu 的 `apt install picolibc-arm-none-eabi`。

### 破坏性变更

- 删除 `third_party/picolibc/`（244 个文件，约 39M）。直接引用该目录的构建脚本需要改用 `PICOLIBC_ROOT` / `PICOLIBC_SPECS` / `PICOLIBC_BUILD` / `PICOLIBC_FETCH`。
- `PICOLIBC=ON` 但系统、工具链、`PICOLIBC_ROOT` 都没有 picolibc，且未开启 `PICOLIBC_BUILD` / `PICOLIBC_FETCH` 时，configure 直接失败。

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
