#pragma once

#include "document.hpp"
#include "options.hpp"

#include <epoxy/gl.h>

#include <string>
#include <vector>

namespace pdfcube {

enum class Transition { none, cube_forward, cube_backward, switch_page };

struct FrameState {
  int current_page{0};
  int next_page{0};
  Transition transition{Transition::none};
  double progress{0.0};
  float zoom{1.0F};
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

 private:
  GLuint compile_shader(GLenum type, const char* source);
  GLuint texture_for_page(int page);
  void draw_page(int page, const float* matrix);
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
  std::vector<GLuint> textures_;
};

}  // namespace pdfcube

