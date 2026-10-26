#!/usr/bin/env bash
# 构建单个平台的 picolibc 预编译包
# 用法: build-picolibc.sh <version> <platform> <outdir>
#   ARM:    cm0 cm3 cm4 cm7 cm23 cm33 cm55
#   RISC-V: rv32imac rv32imafc rv64imac rv64gc
#   x86:    x86_64（裸机 freestanding，-no-pie）
set -euo pipefail

version="${1:?usage: $0 <version> <platform> <outdir>}"
platform="${2:?usage: $0 <version> <platform> <outdir>}"
outdir="${3:-out}"

arch=""
cc=""
ar=""
cflags=()
case "$platform" in
    cm0)  arch=arm; cc=arm-none-eabi-gcc; ar=arm-none-eabi-ar; cflags=(-mcpu=cortex-m0 -mfloat-abi=soft) ;;
    cm3)  arch=arm; cc=arm-none-eabi-gcc; ar=arm-none-eabi-ar; cflags=(-mcpu=cortex-m3 -mfloat-abi=soft) ;;
    cm4)  arch=arm; cc=arm-none-eabi-gcc; ar=arm-none-eabi-ar; cflags=(-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard) ;;
    cm7)  arch=arm; cc=arm-none-eabi-gcc; ar=arm-none-eabi-ar; cflags=(-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard) ;;
    cm23) arch=arm; cc=arm-none-eabi-gcc; ar=arm-none-eabi-ar; cflags=(-mcpu=cortex-m23 -mfloat-abi=soft) ;;
    cm33) arch=arm; cc=arm-none-eabi-gcc; ar=arm-none-eabi-ar; cflags=(-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard) ;;
    cm55) arch=arm; cc=arm-none-eabi-gcc; ar=arm-none-eabi-ar; cflags=(-mcpu=cortex-m55 -mfpu=fpv5-auto -mfloat-abi=hard) ;;
    rv32imac)  arch=riscv; cc=riscv64-unknown-elf-gcc; ar=riscv64-unknown-elf-ar; cflags=(-march=rv32imac -mabi=ilp32) ;;
    rv32imafc) arch=riscv; cc=riscv64-unknown-elf-gcc; ar=riscv64-unknown-elf-ar; cflags=(-march=rv32imafc -mabi=ilp32f) ;;
    rv64imac)  arch=riscv; cc=riscv64-unknown-elf-gcc; ar=riscv64-unknown-elf-ar; cflags=(-march=rv64imac -mabi=lp64) ;;
    rv64gc)    arch=riscv; cc=riscv64-unknown-elf-gcc; ar=riscv64-unknown-elf-ar; cflags=(-march=rv64gc -mabi=lp64d) ;;
    x86_64)    arch=x86; cc=gcc; ar=ar ;;
    *) echo "unknown platform: $platform" >&2; exit 1 ;;
esac

mkdir -p "$outdir"
outdir="$(cd "$outdir" && pwd)"

command -v "$cc" >/dev/null || { echo "$cc not found" >&2; exit 1; }
command -v meson >/dev/null || { echo "meson not found" >&2; exit 1; }
command -v ninja >/dev/null || { echo "ninja not found" >&2; exit 1; }

src_root="$outdir/src"
src="$src_root/picolibc-$version"
if [ ! -d "$src" ]; then
    mkdir -p "$src_root"
    archive="$outdir/src-$version.tar.gz"
    if [ ! -f "$archive" ]; then
        curl -fsSL "https://github.com/picolibc/picolibc/archive/refs/tags/$version.tar.gz" -o "$archive"
    fi
    tar -xzf "$archive" -C "$src_root"
fi

build="$outdir/build-$platform"
stage="$outdir/picolibc-$version-$platform"
cross="$outdir/cross-$platform.txt"
rm -rf "$build" "$stage"

if [ "$arch" = x86 ]; then
    meson setup "$build" "$src" \
        --prefix "$stage" \
        -Dpicocrt=true -Duse-stdlib=false -Dsemihost=false -Dposix-console=false \
        -Dmultilib=false -Dtests=false -Dspecsdir=lib
else
    cat > "$cross" <<EOF
[binaries]
c = ['$cc', '-nostdlib']
ar = '$ar'
[host_machine]
system = 'none'
cpu_family = '$arch'
cpu = '$arch'
endian = 'little'
[properties]
skip_sanity_check = true
EOF
    meson setup "$build" "$src" \
        --cross-file "$cross" \
        --prefix "$stage" \
        -Dmultilib=false -Dtests=false -Dspecsdir=lib \
        "-Dc_args=${cflags[*]}" \
        "-Dc_link_args=${cflags[*]}"
fi
ninja -C "$build"
ninja -C "$build" install

cp "$src/COPYING.picolibc" "$stage/"

# 用生成的 specs 链接一个最小程序，确认包可用
cat > "$outdir/check-$platform.c" <<'EOF'
#include <string.h>
int main(void) { return (int)strlen("x"); }
EOF
if [ "$arch" = x86 ]; then
    gcc -no-pie \
        --specs="$stage/lib/picolibc.specs" \
        --picolibc-prefix="$stage" \
        "$outdir/check-$platform.c" -o "$outdir/check-$platform.elf"
else
    "$cc" "${cflags[@]}" \
        --specs="$stage/lib/picolibc.specs" \
        --picolibc-prefix="$stage" \
        "$outdir/check-$platform.c" -o "$outdir/check-$platform.elf"
fi

pkg="picolibc-$version-$platform.tar.gz"
tar -czf "$outdir/$pkg" -C "$outdir" "picolibc-$version-$platform"
( cd "$outdir" && sha256sum "$pkg" > "$pkg.sha256" )

echo "built $outdir/$pkg"
