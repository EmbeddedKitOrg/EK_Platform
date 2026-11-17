#!/usr/bin/env bash
# 多配置交叉编译检查：每套 tests/configs/*.h × 每个目标 CPU，把库的全部源文件编译一遍。
# 库自身源码使用 -Werror；third_party 只要求能编译。
#
# 用法：
#   tests/compile_check.sh                                  # 默认 arm-none-eabi-gcc，Cortex-M0/M4/M33
#   CC=riscv64-unknown-elf-gcc \
#   TARGETS="-march=rv32imac_zicsr -mabi=ilp32" tests/compile_check.sh
#
# TARGETS 用 ';' 分隔多组目标参数。

set -u

# Git Bash/MSYS 下 pwd -W 给出 E:/... 形式，原生 Windows 工具链才能识别
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && (pwd -W 2>/dev/null || pwd))"
CC="${CC:-arm-none-eabi-gcc}"
TARGETS="${TARGETS:--mcpu=cortex-m0 -mthumb;-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard;-mcpu=cortex-m33 -mthumb}"
STD="${STD:-c99}"

COMMON_FLAGS=(-std="$STD" -Os -ffunction-sections -fdata-sections -Wall -Wextra)
INCLUDES=(
    -I"$ROOT/include"
    -I"$ROOT/include/ek"
    -I"$ROOT/include/ek/core"
    -I"$ROOT/include/ek/mem"
    -I"$ROOT/include/ek/container"
    -I"$ROOT/include/ek/sched"
    -I"$ROOT/third_party/tlsf"
    -I"$ROOT/third_party/lwprintf/inc"
)

mapfile -t OWN_SRCS < <(find "$ROOT/src" "$ROOT/port" -name '*.c' | sort)
mapfile -t TP_SRCS < <(find "$ROOT/third_party" -name '*.c' | sort)

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

fail=0
total=0
IFS=';' read -r -a TARGET_LIST <<< "$TARGETS"

echo "compiler: $("$CC" --version | head -n1)"
for conf in "$ROOT"/tests/configs/*.h; do
    for target in "${TARGET_LIST[@]}"; do
        # shellcheck disable=SC2206
        target_flags=($target)
        label="$(basename "$conf") [$target]"
        errors=""
        for src in "${OWN_SRCS[@]}" "${TP_SRCS[@]}"; do
            extra=()
            case "$src" in "$ROOT"/src/* | "$ROOT"/port/*) extra=(-Werror) ;; esac
            total=$((total + 1))
            if ! msg=$("$CC" "${target_flags[@]}" "${COMMON_FLAGS[@]}" "${extra[@]}" "${INCLUDES[@]}" \
                "-DEK_CONF_PATH=\"$conf\"" -c "$src" -o "$OUT/obj.o" 2>&1); then
                errors+=$'\n'"  ${src#"$ROOT"/}"$'\n'"$msg"
            fi
        done
        if [ -n "$errors" ]; then
            echo "FAIL  $label$errors"
            fail=$((fail + 1))
        else
            echo "ok    $label"
        fi
    done
done

echo "compiled $total translation units, $fail failing configuration(s)"
[ "$fail" -eq 0 ]
