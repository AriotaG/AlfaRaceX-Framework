#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/build-arm-object-check}"
CC="${ARX_ARM_CLANG:-clang}"
SIZE_TOOL="${ARX_SIZE_TOOL:-size}"

rm -rf "$OUT"
mkdir -p "$OUT/obj"

COMMON=(
  --target=arm-none-eabi
  -mcpu=cortex-m0
  -mthumb
  -std=c11
  -ffreestanding
  -fno-builtin
  -ffunction-sections
  -fdata-sections
  -Os
  -Wall -Wextra -Wpedantic -Werror
  -I"$ROOT/tools/arm_object_check/include"
  -I"$ROOT/include"
  -I"$ROOT/targets/stm32f072_common/include"
)

mapfile -t SOURCES < <(find "$ROOT/src" -type f -name '*.c' | sort)
SOURCES+=("$ROOT/targets/stm32f072_common/src/arx_stm32f072_target.c")

printf 'AlfaRaceX Cortex-M0 object gate\n' > "$OUT/report.txt"
printf 'Compiler: %s\n\n' "$($CC --version | head -1)" >> "$OUT/report.txt"

for src in "${SOURCES[@]}"; do
  rel="${src#$ROOT/}"
  obj="$OUT/obj/${rel//\//_}.o"
  "$CC" "${COMMON[@]}" -c "$src" -o "$obj"
done

"$SIZE_TOOL" "$OUT"/obj/*.o > "$OUT/size.txt"
awk 'NR>1 { text+=$1; data+=$2; bss+=$3 } END {
  printf "Portable object sum: text=%d data=%d bss=%d\n", text,data,bss;
  printf "Approx. object flash contribution: %d bytes\n", text+data;
  printf "Approx. static RAM contribution: %d bytes\n", data+bss;
}' "$OUT/size.txt" | tee -a "$OUT/report.txt"

printf 'Objects compiled: %d\n' "${#SOURCES[@]}" | tee -a "$OUT/report.txt"
printf 'Gate result: PASS\n' | tee -a "$OUT/report.txt"
