#include "options.hpp"
#include "recording.hpp"
#include "playback.hpp"
#include "renderer.hpp"
#include "performance.hpp"

#include <epoxy/egl.h>

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
// One native renderer/context serves all requested frames. No GTK frame
// clock, X server, window startup, vsync, or wall-clock animation is involved.
struct Context {
  EGLDisplay display{EGL_NO_DISPLAY};
  EGLSurface surface{EGL_NO_SURFACE};
  EGLContext context{EGL_NO_CONTEXT};
  Context() {
    const auto platform = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!platform) throw std::runtime_error("surfaceless EGL is unavailable");
    display = platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);
    if (!eglInitialize(display,nullptr,nullptr)) throw std::runtime_error("EGL initialization failed");
    if (!eglBindAPI(EGL_OPENGL_API)) throw std::runtime_error("desktop OpenGL is unavailable");
    const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    EGLConfig config{};
    EGLint count=0;
    if (!eglChooseConfig(display,attributes,&config,1,&count) || count != 1)
      throw std::runtime_error("no EGL framebuffer configuration");
    const EGLint size[]{EGL_WIDTH,320,EGL_HEIGHT,240,EGL_NONE};
    surface = eglCreatePbufferSurface(display,config,size);
    context = eglCreateContext(display,config,EGL_NO_CONTEXT,nullptr);
    if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT ||
        !eglMakeCurrent(display,surface,surface,context)) throw std::runtime_error("EGL context failed");
  }
  ~Context() {
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(display,context);
    eglDestroySurface(display,surface);
    eglTerminate(display);
  }
};
struct Pages {
  pdfcube::Document document;
  pdfcube::Renderer renderer;
  Pages(const pdfcube::Options& options)
      : document(options.input_file),
        renderer(document,options.background,options.top,options.experimental) { renderer.initialize(); }
  ~Pages() { renderer.release(); }
};
}

int main() {
  try {
    Context context;
    std::map<std::string,std::unique_ptr<Pages>> cache;
    std::cout << "READY\n" << std::flush;
    std::string line;
    while (std::getline(std::cin,line)) {
      try {
        std::istringstream request(line);
        std::vector<std::string> arguments{"pdfcube"};
        for (std::string value; request >> std::quoted(value);) arguments.push_back(value);
        const bool sequence = arguments.size() > 1 && arguments[1] == "@sequence";
        if (sequence) arguments.erase(arguments.begin()+1);
        std::vector<char*> pointers;
        for (auto& argument : arguments) pointers.push_back(argument.data());
        const auto parsed = pdfcube::parse_options(static_cast<int>(pointers.size()),pointers.data());
        if (parsed.should_exit) throw std::runtime_error(parsed.output);
        const auto& options = parsed.options;
        std::ifstream file(options.input_file,std::ios::binary);
        if (!file) throw std::runtime_error("missing fixture: "+options.input_file);
        const std::string contents{std::istreambuf_iterator<char>(file),{}};
        std::ostringstream key;
        key << options.input_file << contents << options.experimental << options.background.red
            << options.background.green << options.background.blue << options.top.red
            << options.top.green << options.top.blue;
        auto& pages = cache[key.str()];
        if (!pages) pages = std::make_unique<Pages>(options);
        pdfcube::FrameState frame;
        if (options.snapshot_transition && pages->document.page_count() > 1) {
          const auto count = static_cast<std::size_t>(pages->document.page_count());
          std::vector<int> durations(count);
          std::vector<pdfcube::Transition> transitions(count);
          std::vector<bool> cubes(count);
          for (std::size_t i=0;i<count;++i) {
            const auto& page = pages->document.page_info(static_cast<int>(i));
            durations[i] = page.duration_ms;
            transitions[i] = pdfcube::transition_from_page(page);
            cubes[i] = page.cube_transition;
          }
          pdfcube::apply_program(options.program,durations,transitions,cubes);
          frame.next_page = 1;
          frame.transition = transitions[1];
          frame.progress = options.snapshot_progress;
          pdfcube::apply_transition_metadata(frame,pages->document.page_info(1));
          if (frame.transition == pdfcube::Transition::switch_page && frame.progress <= 0)
            frame.transition = pdfcube::Transition::none;
        }
        if (!options.record_raw.empty()) {
          std::ofstream output(options.record_raw, std::ios::binary);
          pdfcube::record_presentation(pages->document, pages->renderer, options.record_fps,
              options.record_width, options.record_height,
              [&](const std::vector<unsigned char>& pixels, int repeats) {
                for (int i=0; i<repeats; ++i)
                  output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
              });
          if (!output) throw std::runtime_error("recording output failed");
        } else if (sequence) {
          // Render every frame of a 60 Hz second directly, without PNG encoding
          // or advancing a wall clock. Readback catches deferred GL failures.
          std::vector<unsigned char> pixels(320*240*4);
          for (int index=0;index<=60;++index) {
            frame.progress = index/60.0;
            pages->renderer.render(320,240,frame);
            glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("frame rendering failed");
          }
        } else {
          pages->renderer.render(320,240,frame);
          pages->renderer.write_png(options.snapshot,320,240);
        }
        if (!options.performance_log.empty()) {
          std::ofstream output(options.performance_log);
          pdfcube::write_frame_timings(output,{});
        }
        std::cout << "OK\n" << std::flush;
      } catch (const std::exception& error) {
        std::cout << "ERROR " << error.what() << '\n' << std::flush;
      }
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
