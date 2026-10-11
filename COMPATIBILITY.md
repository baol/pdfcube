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
| Windows | GTK UCRT64 | Build and CLI in CI | Manual rendering smoke test |
| macOS | GTK/Homebrew | Build and CLI in CI | Manual rendering smoke test |

KDE, GNOME, and other desktop environments are not separate renderer targets;
they are covered through GTK's X11 and Wayland backends. GPU and driver
combinations are only considered verified after they appear in this matrix.

## Out of scope for 0.1.0

- OpenGL 2.x and OpenGL ES 2.x
- Vulkan, Metal, and Direct3D renderers
- Native Windows installers and macOS application bundles

Those targets require separate renderer or packaging work and must not be
inferred from the portable GTK build alone.
