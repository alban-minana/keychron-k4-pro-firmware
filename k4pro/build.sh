#!/usr/bin/env bash
# Local build with Docker.
#   k4pro/build.sh                 -> all 4 variants, keymap "via"
#   k4pro/build.sh iso/rgb         -> one variant
#   k4pro/build.sh iso/rgb default -> one variant, another keymap
# Output: k4pro/out/*.bin
set -euo pipefail
cd "$(dirname "$0")/.."
VARIANTS="${1:-ansi/rgb ansi/white iso/rgb iso/white}"
KM="${2:-via}"
git submodule update --init --depth 1 lib/chibios lib/chibios-contrib lib/printf lib/lufa
tar -cf - requirements.txt k4pro/Dockerfile | docker build -q -t k4pro-qmk -f k4pro/Dockerfile - >/dev/null
mkdir -p k4pro/out
SRC="$(pwd)"; command -v cygpath >/dev/null && SRC="$(cygpath -w "$SRC")"
for v in $VARIANTS; do
  MSYS_NO_PATHCONV=1 docker run --rm -v "$SRC:/qmk_firmware" -w /qmk_firmware k4pro-qmk \
    make "keychron/k4_pro/$v:$KM" SKIP_GIT=yes
  cp "keychron_k4_pro_${v/\//_}_${KM}.bin" k4pro/out/
done
ls -l k4pro/out
