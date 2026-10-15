#!/usr/bin/env bash
set -euo pipefail

if test "$#" -ne 3; then
  echo "usage: $0 PDFCUBE_BINARY VERSION OUTPUT.dmg" >&2
  exit 2
fi

binary="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
version="$2"
output="$(cd "$(dirname "$3")" && pwd)/$(basename "$3")"
root="$(cd "$(dirname "$0")/../.." && pwd)"
work="$(mktemp -d)"
mountpoint=""
cleanup() {
  if test -n "${mountpoint}"; then
    hdiutil detach -quiet "${mountpoint}" 2>/dev/null || true
  fi
  rm -rf "${work}"
}
trap cleanup EXIT

app="${work}/PDFCube.app"
contents="${app}/Contents"
mkdir -p "${contents}/MacOS" "${contents}/Frameworks" \
  "${contents}/Resources/share/glib-2.0/schemas" \
  "${contents}/Resources/share/icons/hicolor/scalable/apps" \
  "${contents}/Resources/lib/gdk-pixbuf-2.0/2.10.0/loaders"

cp "${binary}" "${contents}/MacOS/pdfcube-bin"
cp "${root}/data/org.pdfcube.pdfcube.svg" \
  "${contents}/Resources/share/icons/hicolor/scalable/apps/"
cp "${root}/LICENSE" "${root}/README.md" "${contents}/Resources/"
schema_source="$(brew --prefix)/share/glib-2.0/schemas"
if ! find "${schema_source}" -maxdepth 1 -name '*.xml' -print -quit | grep -q .; then
  echo "No GLib schemas found in ${schema_source}" >&2
  exit 1
fi
find "${schema_source}" -maxdepth 1 -name '*.xml' \
  -exec cp {} "${contents}/Resources/share/glib-2.0/schemas/" \;
glib-compile-schemas "${contents}/Resources/share/glib-2.0/schemas"

dylibbundler -od -b -x "${contents}/MacOS/pdfcube-bin" \
  -d "${contents}/Frameworks" -p '@executable_path/../Frameworks/'

pixbuf_prefix="$(brew --prefix gdk-pixbuf)"
cp "${pixbuf_prefix}/bin/gdk-pixbuf-query-loaders" "${contents}/MacOS/"
cp "${pixbuf_prefix}/lib/gdk-pixbuf-2.0/2.10.0/loaders/"*.so \
  "${contents}/Resources/lib/gdk-pixbuf-2.0/2.10.0/loaders/"
dylibbundler -od -b -x "${contents}/MacOS/gdk-pixbuf-query-loaders" \
  -d "${contents}/Frameworks" -p '@executable_path/../Frameworks/'
for loader in "${contents}/Resources/lib/gdk-pixbuf-2.0/2.10.0/loaders/"*.so; do
  dylibbundler -od -b -x "${loader}" \
    -d "${contents}/Frameworks" -p '@executable_path/../Frameworks/'
done

cat > "${contents}/MacOS/pdfcube" <<'EOF'
#!/usr/bin/env bash
set -e
contents="$(cd "$(dirname "$0")/.." && pwd)"
export XDG_DATA_DIRS="${contents}/Resources/share${XDG_DATA_DIRS:+:${XDG_DATA_DIRS}}"
export GSETTINGS_SCHEMA_DIR="${contents}/Resources/share/glib-2.0/schemas"
export GDK_PIXBUF_MODULEDIR="${contents}/Resources/lib/gdk-pixbuf-2.0/2.10.0/loaders"
pixbuf_cache="${TMPDIR:-/tmp}/pdfcube-pixbuf-${UID}.cache"
"${contents}/MacOS/gdk-pixbuf-query-loaders" "${GDK_PIXBUF_MODULEDIR}/"*.so > "${pixbuf_cache}"
export GDK_PIXBUF_MODULE_FILE="${pixbuf_cache}"
exec "${contents}/MacOS/pdfcube-bin" "$@"
EOF
chmod +x "${contents}/MacOS/pdfcube"

iconset="${work}/PDFCube.iconset"
mkdir "${iconset}"
for size in 16 32 128 256 512; do
  rsvg-convert -w "${size}" -h "${size}" "${root}/data/org.pdfcube.pdfcube.svg" \
    > "${iconset}/icon_${size}x${size}.png"
  double=$((size * 2))
  rsvg-convert -w "${double}" -h "${double}" "${root}/data/org.pdfcube.pdfcube.svg" \
    > "${iconset}/icon_${size}x${size}@2x.png"
done
iconutil -c icns "${iconset}" -o "${contents}/Resources/PDFCube.icns"

sed -e "s/@VERSION@/${version}/g" \
  "${root}/packaging/macos/Info.plist.in" > "${contents}/Info.plist"

"${contents}/MacOS/pdfcube" --version | grep -F "${version}"
codesign --force --deep --sign - "${app}"
codesign --verify --deep --strict "${app}"

dmg_root="${work}/dmg"
mkdir "${dmg_root}"
cp -R "${app}" "${dmg_root}/"
ln -s /Applications "${dmg_root}/Applications"
rm -f "${output}"
hdiutil create -quiet -volname "PDFCube ${version}" -srcfolder "${dmg_root}" \
  -ov -format UDZO "${output}"
hdiutil verify "${output}"
mountpoint="${work}/mounted"
mkdir "${mountpoint}"
hdiutil attach -quiet -nobrowse -mountpoint "${mountpoint}" "${output}"
"${mountpoint}/PDFCube.app/Contents/MacOS/pdfcube" --version | grep -F "${version}"
hdiutil detach -quiet "${mountpoint}"
mountpoint=""
