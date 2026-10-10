#include "renderer.hpp"

#include "math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace pdfcube {
namespace {

constexpr float pi = 3.14159265358979323846F;

const char* vertex_shader = R"(
#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texture_coordinate;
uniform mat4 transform;
out vec2 uv;
void main() {
  gl_Position = transform * vec4(position, 1.0);
  uv = texture_coordinate;
}
)";

const char* fragment_shader = R"(
#version 330 core
in vec2 uv;
uniform sampler2D page_texture;
uniform vec4 solid_color;
uniform bool textured;
out vec4 output_color;
void main() {
  output_color = textured ? texture(page_texture, uv) : solid_color;
}
)";

}  // namespace

Renderer::Renderer(Document& document, Color background, Color top)
    : document_(document), background_(background), top_(top) {
  textures_.resize(static_cast<std::size_t>(document.page_count()));
}

Renderer::~Renderer() = default;

GLuint Renderer::compile_shader(GLenum type, const char* source) {
  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint status = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string message(static_cast<std::size_t>(length), '\0');
    glGetShaderInfoLog(shader, length, nullptr, message.data());
    glDeleteShader(shader);
    throw std::runtime_error("OpenGL shader compilation failed: " + message);
  }
  return shader;
}

void Renderer::initialize() {
  const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_shader);
  const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_shader);
  program_ = glCreateProgram();
  glAttachShader(program_, vertex);
  glAttachShader(program_, fragment);
  glLinkProgram(program_);
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  GLint status = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    throw std::runtime_error("OpenGL shader program could not be linked");
  }

  constexpr std::array<float, 30> vertices = {
      -1, -1, 0, 0, 1,  1, -1, 0, 1, 1,  1, 1, 0, 1, 0,
      -1, -1, 0, 0, 1,  1,  1, 0, 1, 0, -1, 1, 0, 0, 0,
  };
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        reinterpret_cast<void*>(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  matrix_uniform_ = glGetUniformLocation(program_, "transform");
  texture_uniform_ = glGetUniformLocation(program_, "page_texture");
  color_uniform_ = glGetUniformLocation(program_, "solid_color");
  textured_uniform_ = glGetUniformLocation(program_, "textured");
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
}

void Renderer::release() {
  for (GLuint texture : textures_) {
    if (texture != 0) glDeleteTextures(1, &texture);
  }
  std::fill(textures_.begin(), textures_.end(), 0);
  if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
  if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
  if (program_ != 0) glDeleteProgram(program_);
  vbo_ = vao_ = program_ = 0;
}

GLuint Renderer::texture_for_page(int page) {
  GLuint& texture = textures_.at(static_cast<std::size_t>(page));
  if (texture != 0) return texture;
  Surface surface = document_.render_page(page);
  const int width = cairo_image_surface_get_width(surface.get());
  const int height = cairo_image_surface_get_height(surface.get());
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA,
               GL_UNSIGNED_BYTE, cairo_image_surface_get_data(surface.get()));
  return texture;
}

void Renderer::draw_page(int page, const float* matrix) {
  glUniformMatrix4fv(matrix_uniform_, 1, GL_FALSE, matrix);
  glUniform1i(textured_uniform_, GL_TRUE);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_for_page(page));
  glUniform1i(texture_uniform_, 0);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::draw_color(const Color& color, const float* matrix) {
  glUniformMatrix4fv(matrix_uniform_, 1, GL_FALSE, matrix);
  glUniform1i(textured_uniform_, GL_FALSE);
  glUniform4f(color_uniform_, color.red, color.green, color.blue, color.alpha);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::render(int width, int height, const FrameState& frame) {
  glViewport(0, 0, width, height);
  glClearColor(background_.red, background_.green, background_.blue, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(program_);
  glBindVertexArray(vao_);

  const float viewport_aspect = static_cast<float>(width) / std::max(1, height);
  const auto& page = document_.page_info(frame.current_page);
  const float page_aspect = static_cast<float>(page.width / page.height);
  const float fit_x = page_aspect > viewport_aspect ? 1.0F : page_aspect / viewport_aspect;
  const float fit_y = page_aspect > viewport_aspect ? viewport_aspect / page_aspect : 1.0F;
  const Matrix projection = perspective(45.0F * pi / 180.0F, viewport_aspect, 0.1F, 20.0F);
  const Matrix view = translate(0.0F, 0.0F, -3.0F);
  const Matrix zoom = translate(frame.offset_x, frame.offset_y, 0.0F) *
                      scale(frame.zoom, frame.zoom, 1.0F);
  const Matrix page_scale = scale(fit_x, fit_y, 1.0F);

  if (frame.transition == Transition::none || frame.transition == Transition::switch_page) {
    const Matrix matrix = projection * view * zoom * page_scale;
    draw_page(frame.current_page, matrix.value.data());
    return;
  }

  const float direction = frame.transition == Transition::cube_forward ? -1.0F : 1.0F;
  const float angle = direction * static_cast<float>(frame.progress) * pi / 2.0F;
  const Matrix animated_view =
      view * zoom * rotate_x(0.14F * std::sin(static_cast<float>(frame.progress) * pi)) *
      scale(0.78F, 0.78F, 0.78F);
  const Matrix current = projection * animated_view * rotate_y(angle) * translate(0, 0, 1) * page_scale;
  const Matrix incoming = projection * animated_view * rotate_y(angle - direction * pi / 2.0F) *
                          translate(0, 0, 1) * page_scale;
  draw_page(frame.current_page, current.value.data());
  draw_page(frame.next_page, incoming.value.data());

  const Matrix top = projection * animated_view * rotate_y(angle) * translate(0, fit_y, 0) *
                     rotate_x(-pi / 2.0F) * scale(fit_x, 1.0F, 1.0F);
  // The cap is the visible outer surface, so use painter ordering at the two
  // shared page edges instead of leaving the result to depth-buffer rounding.
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  draw_color(top_, top.value.data());
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

void Renderer::write_png(const std::string& filename, int width, int height) const {
  std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 4));
  glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
  const int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
  std::vector<unsigned char> flipped(static_cast<std::size_t>(stride * height));
  for (int row = 0; row < height; ++row) {
    std::copy_n(pixels.data() + static_cast<std::size_t>((height - row - 1) * width * 4),
                static_cast<std::size_t>(width * 4),
                flipped.data() + static_cast<std::size_t>(row * stride));
  }
  cairo_surface_t* surface = cairo_image_surface_create_for_data(
      flipped.data(), CAIRO_FORMAT_ARGB32, width, height, stride);
  const cairo_status_t status = cairo_surface_write_to_png(surface, filename.c_str());
  cairo_surface_destroy(surface);
  if (status != CAIRO_STATUS_SUCCESS) {
    throw std::runtime_error("Could not write snapshot: " +
                             std::string(cairo_status_to_string(status)));
  }
}

}  // namespace pdfcube
