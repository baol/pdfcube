#!/usr/bin/env bash
set -euo pipefail

if test "$#" -ne 3; then
  echo "usage: $0 PDFCUBE.EXE VERSION OUTPUT.msi" >&2
  exit 2
fi

binary="$1"
version="$2"
output="$3"
root="$(cd "$(dirname "$0")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

python3 "${root}/packaging/windows/stage_runtime.py" \
  "${binary}" "${MINGW_PREFIX}" "${work}/stage"
magick -background none "${root}/data/org.pdfcube.pdfcube.svg" \
  -define icon:auto-resize=256,128,64,48,32,16 "${work}/pdfcube.ico"
python3 "${root}/packaging/windows/generate_wxs.py" \
  "${work}/stage" "${version}" "${work}/pdfcube.wxs" "${work}/pdfcube.ico"
wixl -a x64 -o "${output}" "${work}/pdfcube.wxs"

mkdir "${work}/extracted"
msiextract -C "${work}/extracted" "${output}"
installed="$(find "${work}/extracted" -iname pdfcube.exe -print -quit)"
test -n "${installed}"
"${installed}" --version | grep -F "${version}"
