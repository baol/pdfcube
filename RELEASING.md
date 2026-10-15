# Releasing PDFCube

Stable Git tags remain the release-version source of truth. The release
workflow creates them automatically after a successful push to `master`.
`MIN_VERSION` provides the version floor. Ordinary releases increment the
highest stable tag's patch component; raise the major or minor component in
`MIN_VERSION` manually when that release series should change.

1. Add the release notes to `NEWS`. Update the release description and date in
   `org.pdfcube.pdfcube.metainfo.xml` when appropriate; its version placeholder
   is filled automatically.
2. Run the complete local checks:

   ```sh
   meson setup build --buildtype=release -Dwerror=true
   meson compile -C build
   meson compile -C build doc
   meson test -C build --print-errorlogs
   desktop-file-validate org.pdfcube.pdfcube.desktop
   appstreamcli validate --pedantic --no-net build/org.pdfcube.pdfcube.metainfo.xml
   meson dist -C build --formats gztar
   dpkg-buildpackage -us -uc -b
   lintian --profile ubuntu ../pdfcube_*.changes
   ```

3. Inspect the source archive and native package contents. The Debian package
   and AppImage must include `showcase.pdf`, `showcase.tex` and `README.md` under
   `usr/share/doc/pdfcube/examples/`. Packaging requires TeX Live with Beamer
   and PGF/TikZ (`texlive-latex-recommended` and `texlive-pictures`). Complete every
   manual rendering smoke test marked as a release requirement in
   `COMPATIBILITY.md`, including Wayland/NVIDIA, X11, Windows, and macOS.
   Confirm that the DMG opens, its application runs outside Homebrew, the MSI
   installs and uninstalls cleanly, and both packages display the PDFCube icon.
4. Commit and merge the release preparation to `master`. Do not create the tag
   manually. For a new release series, change `MIN_VERSION` in the same pull
   request, for example:

   ```sh
   printf '1.2.0\n' > MIN_VERSION
   ```

The `master` push starts `.github/workflows/release.yml`. It calculates the
version, builds and tests on the latest Ubuntu LTS runner, and creates the
annotated tag only after every artifact succeeds. It then publishes a GitHub
Release with a `.tar.gz` source archive, an amd64 `.deb`, an x86-64 AppImage,
a macOS `.dmg`, a Windows x86-64 `.msi`, `pdfcube-VERSION-showcase.pdf`, and
`SHA256SUMS`. GitHub Actions also retains the same files as a workflow artifact.

Linux, macOS, and Windows packages build independently. The publish job waits
for all three platforms, verifies every expected artifact, and only then creates
the automatic tag and GitHub Release. Pull requests build and smoke-test the DMG
and MSI as downloadable workflow artifacts, preventing release-only failures.
The current automated packages are ad-hoc signed on macOS and unsigned on
Windows; notarization and Authenticode signing require project-owned signing
identities and should be added before presenting either package as trusted by
the operating-system vendor.

The release build explicitly enables `-Dinstall_docs=true` and invokes the
Meson `doc` target before staging the AppImage. Debian's build rules enable
that option and invoke `doc` independently, so packages do not depend on a PDF
left over from another build. The standalone PDF is copied into `release-assets`
before checksums are generated. Missing documentation fails the package or
release checks.
