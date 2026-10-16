# Compatibility

PDFCube targets GTK 4.12 or newer on Linux, Windows, and macOS. The rendering
baseline is either desktop OpenGL 3.0 with GLSL 1.30 or OpenGL ES 3.0 with
GLSL ES 3.00. GTK selects GLX, EGL, WGL, or the native platform integration;
PDFCube selects the matching shader dialect after the context is current.

## Test matrix

| Platform | Window system | Automated coverage | Release requirement |
| --- | --- | --- | --- |
| Linux | X11, Mesa software GL | Build, CLI, deterministic rendering, sanitizers | Required in CI |
| Linux | Wayland, Mesa or NVIDIA | Build and CLI in CI | Manual rendering smoke test |
| Linux | X11, hardware GL | Build and CLI in CI | Manual rendering smoke test |
| Windows | GTK UCRT64 | Build, CLI, MSI assembly and extraction smoke test | Manual installed-MSI rendering test |
| macOS | GTK/Homebrew | Build, CLI, DMG assembly and signature verification | Manual installed-DMG rendering test |

KDE, GNOME, and other desktop environments use GTK's X11 and Wayland backends.

Native packages use GTK's platform integration and bundle the supporting runtime
libraries. Automated package checks cover assembly and basic execution; installed
packages receive the rendering smoke tests listed above.
