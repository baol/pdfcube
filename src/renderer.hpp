#pragma once

#include "document.hpp"
#include "options.hpp"

#include <epoxy/gl.h>

#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace pdfcube {

enum class Transition {
  none, cube_forward, cube_backward, switch_page, blinds, split, cover, uncover, fly, push
};

inline bool page_effect(Transition transition) {
  return transition == Transition::blinds || transition == Transition::split ||
         transition == Transition::cover || transition == Transition::uncover ||
         transition == Transition::fly || transition == Transition::push;
}

struct FrameState {
  int current_page{0};
  int next_page{0};
  Transition transition{Transition::none};
  double progress{0.0};
  bool vertical_panels{false};
  bool inward{true};
  int direction_degrees{0};
  float fly_scale{1};
  float zoom{1.0F};
  float texture_zoom{1.0F};
  float offset_x{0.0F};
  float offset_y{0.0F};
};

class Renderer {
 public:
  Renderer(Document& document, Color background, Color top);
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void initialize();
  void release();
  void render(int width, int height, const FrameState& frame);
  void write_png(const std::string& filename, int width, int height) const;
  [[nodiscard]] bool idle_preparation_pending() const;

 private:
  struct PageTexture {
    GLuint name{0};
    int maximum_size{0};
  };

  struct PreparedPage {
    std::vector<unsigned char> pixels;
    int width{0};
    int height{0};
    int maximum_size{0};
    int requested_size{0};
  };

  struct RenderRequest {
    int page;
    int maximum_size;
  };

  GLuint compile_shader(GLenum type, const char* source);
  GLuint upload_surface(int page, Surface surface);
  GLuint upload_pixels(int page, int width, int height, const std::vector<unsigned char>& pixels);
  bool upload_prepared_page(int page, int maximum_size);
  GLuint texture_for_page(int page, int maximum_size);
  void schedule_page(int page, int maximum_size);
  void render_pages_in_background();
  void preload_neighbors(int page, int maximum_size);
  void draw_page(int page, int maximum_size, const float* matrix);
  void draw_color(const Color& color, const float* matrix);

  Document& document_;
  Color background_;
  Color top_;
  GLuint program_{0};
  GLuint vao_{0};
  GLuint vbo_{0};
  GLint matrix_uniform_{-1};
  GLint texture_uniform_{-1};
  GLint color_uniform_{-1};
  GLint textured_uniform_{-1};
  GLint uv_rect_uniform_{-1};
  GLint shade_uniform_{-1};
  GLint maximum_texture_size_{0};
  std::vector<PageTexture> textures_;
  std::vector<PreparedPage> prepared_pages_;
  std::deque<RenderRequest> render_requests_;
  std::mutex prepared_mutex_;
  std::mutex document_render_mutex_;
  std::condition_variable prepared_condition_;
  std::thread render_thread_;
  std::exception_ptr preparation_error_;
  bool stop_render_thread_{false};
  bool idle_preparation_pending_{false};
};

}  // namespace pdfcube

