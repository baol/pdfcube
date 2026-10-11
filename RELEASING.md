# Releasing PDFCube

Git tags are the release-version source of truth. A release tag must use the
form `vMAJOR.MINOR.PATCH`; the build derives the executable, manual, AppStream
metadata, archive, package, and artifact versions from it.

1. Add the release notes to `NEWS`. Update the release description and date in
   `org.pdfcube.pdfcube.metainfo.xml` when appropriate; its version placeholder
   is filled automatically.
2. Run the complete local checks:

   ```sh
   meson setup build --buildtype=release -Dwerror=true
   meson compile -C build
   meson test -C build --print-errorlogs
   desktop-file-validate org.pdfcube.pdfcube.desktop
   appstreamcli validate --pedantic --no-net build/org.pdfcube.pdfcube.metainfo.xml
   meson dist -C build --formats gztar
   dpkg-buildpackage -us -uc -b
   lintian --profile ubuntu ../pdfcube_*.changes
   ```

3. Inspect the source archive and Debian package contents. Complete every
   manual rendering smoke test marked as a release requirement in
   `COMPATIBILITY.md`, including Wayland/NVIDIA, X11, Windows, and macOS.
4. Commit and merge the release preparation to `master`, then create and push
   a signed tag. For example:

   ```sh
   git tag -s v1.2.3 -m "PDFCube 1.2.3"
   git push origin v1.2.3
   ```

The tag starts `.github/workflows/release.yml`. It verifies that the derived
version matches the tag, builds and tests on the latest Ubuntu LTS runner, then
publishes a GitHub Release with a `.tar.gz` source archive, an amd64 `.deb`, an
x86-64 AppImage, and `SHA256SUMS`. GitHub Actions also retains the same files
as a workflow artifact.
