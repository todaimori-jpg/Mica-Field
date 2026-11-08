#!/usr/bin/env bash
set -euo pipefail
web_root="$(cd "$(dirname "$0")" && pwd)"
output="${1:-$web_root/app}"
mkdir -p "$output"
em++ -std=c++17 -O3 -fno-exceptions -fno-rtti \
    "$web_root/WasmBridge.cpp" "$web_root/../Source/Patch.cpp" "$web_root/../Source/Dsp.cpp" \
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=shell -sWASM_ASYNC_COMPILATION=0 \
    -sFILESYSTEM=0 -sINITIAL_MEMORY=16777216 -sSTACK_SIZE=1048576 -sALLOW_MEMORY_GROWTH=0 \
    -o "$output/engine.mjs"
"${CXX:-g++}" -std=c++17 -O2 "$web_root/Metadata.cpp" "$web_root/../Source/Patch.cpp" "$web_root/../Source/Dsp.cpp" -o "$output/metadata-generator"
"$output/metadata-generator" > "$output/meta.json"
rm "$output/metadata-generator"
python3 "$web_root/metadata.py" "$output/meta.json"
