#!/usr/bin/env bash
# macOS/Linux equivalent of build.ps1. Compatible with macOS's Bash 3.2.
set -euo pipefail

action="${1:-Build}"
configuration="${2:-Debug}"
case "$action" in
    Build|Clean|Flash|Doctor) ;;
    *) echo 'Usage: bash build.sh [Build|Clean|Flash|Doctor] [Debug|Release]' >&2; exit 2 ;;
esac
case "$configuration" in
    Debug|Release) ;;
    *) echo 'Configuration must be Debug or Release.' >&2; exit 2 ;;
esac
cd "$(dirname "${BASH_SOURCE[0]}")"
project_dir="$PWD"

bundle_roots=(
    "${STM32CUBE_BUNDLE_ROOT:-}"
    "${CUBE_BUNDLE_PATH:-}"
    "$HOME/Library/Application Support/stm32cube/bundles"
    "${XDG_DATA_HOME:-$HOME/.local/share}/stm32cube/bundles"
)

find_tool() {
    local name="$1" bundle="$2" root candidate selected=''
    if command -v "$name" >/dev/null 2>&1; then
        command -v "$name"
        return
    fi
    for root in "${bundle_roots[@]}"; do
        [[ -n "$root" && -d "$root/$bundle" ]] || continue
        for candidate in "$root/$bundle"/*/bin/"$name"; do
            [[ -x "$candidate" && ! -d "$candidate" ]] || continue
            if [[ -z "$selected" || "$candidate" -nt "$selected" ]]; then
                selected="$candidate"
            fi
        done
        if [[ -n "$selected" ]]; then
            printf '%s\n' "$selected"
            return
        fi
    done
    echo "Missing $name. Install the STM32Cube tools, add them to PATH, or set STM32CUBE_BUNDLE_ROOT to the bundles folder." >&2
    return 1
}

cmake_tool="$(find_tool cmake cmake)"
ninja_tool="$(find_tool ninja ninja)"
compiler="$(find_tool arm-none-eabi-gcc gnu-tools-for-stm32)"
export PATH="$(dirname "$compiler"):$(dirname "$cmake_tool"):$(dirname "$ninja_tool"):$PATH"

if [[ "$action" == Doctor ]]; then
    printf 'CMake:    %s\nNinja:    %s\nCompiler: %s\n' "$cmake_tool" "$ninja_tool" "$compiler"
    "$cmake_tool" --version
    "$ninja_tool" --version
    "$compiler" --version
    if programmer="$(find_tool STM32_Programmer_CLI programmer 2>/dev/null)"; then
        printf 'Flasher:  %s\n' "$programmer"
    else
        echo 'Flasher: not installed (only needed to program the board).'
    fi
    exit 0
fi

"$cmake_tool" --preset "$configuration"
if [[ "$action" == Clean ]]; then
    "$cmake_tool" --build --preset "$configuration" --target clean
    exit 0
fi

"$cmake_tool" --build --preset "$configuration" --parallel
firmware="$project_dir/build/$configuration/ECHO.elf"
printf 'Firmware: %s\n' "$firmware"
if [[ "$action" == Flash ]]; then
    programmer="$(find_tool STM32_Programmer_CLI programmer)"
    "$programmer" -c port=SWD -w "$firmware" -v -rst
fi
