# Releasing PDFCube

1. Set the release version in `meson.build`. The executable and CLI tests
   derive their version from the Meson project value.
2. Add the release notes and date to `NEWS` and
   `org.pdfcube.pdfcube.metainfo.xml`.
3. Update `debian/changelog`, including its package revision and date.
4. Run the complete release checks:

   ```sh
   meson setup build --buildtype=release -Dwerror=true
   meson compile -C build
   meson test -C build --print-errorlogs
   desktop-file-validate org.pdfcube.pdfcube.desktop
   appstreamcli validate --pedantic --no-net org.pdfcube.pdfcube.metainfo.xml
   meson dist -C build
   dpkg-buildpackage -us -uc -b
   lintian ../pdfcube_*.changes
   ```

5. Inspect the source archive and Debian package contents. Install and run the
   package on both Wayland and X11 with a hardware OpenGL driver.
6. Commit the release preparation, then create and push an annotated tag:

   ```sh
   git tag -s v0.1.0 -m "PDFCube 0.1.0"
   git push origin v0.1.0
   ```

7. Create the GitHub release from the tag, use the matching `NEWS` entry as
   its notes, and attach the Meson source archive and Debian binary package.
