#include "renderer.hpp"

#include "math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace pdfcube {
namespace {

constexpr float pi = 3.14159265358979323846F;
constexpr int legacy_frames = 30;

struct Viewport {
  int x;
  int y;
  int width;
  int height;
};

struct CubePose {
  float angle;
  float camera_y;
  float camera_z;
};

Viewport content_viewport(int width, int height, double page_aspect) {
  int content_width = width;
  int content_height = std::max(1, static_cast<int>(std::lround(width / page_aspect)));
  if (content_height > height) {
    content_height = height;
    content_width = std::max(1, static_cast<int>(std::lround(height * page_aspect)));
  }
  content_width = std::min(content_width, width);
  content_height = std::min(content_height, height);
  return {(width - content_width) / 2, (height - content_height) / 2, content_width,
          content_height};
}

CubePose legacy_cube_pose(double progress, bool forward) {
  std::array<double, legacy_frames + 1> angle{};
  std::array<double, legacy_frames + 1> camera_y{};
  std::array<double, legacy_frames + 1> camera_z{};
  std::array<double, legacy_frames> rotation_step{};
  std::array<double, legacy_frames> depth_step{};

  double rotation_sum = 0.0;
  for (int frame = 0; frame < legacy_frames; ++frame) {
    const double sine = std::sin(frame * static_cast<double>(pi) / legacy_frames);
    rotation_step[frame] = sine * sine;
    rotation_sum += rotation_step[frame];
  }

  constexpr double camera_peak = 0.4;
  constexpr double depth_peak = 0.07;
  const double camera_step = camera_peak / ((legacy_frames - 1) / 2.0);
  const double depth_increment = depth_peak / (legacy_frames / 4.0);
  for (int frame = 0; frame < legacy_frames; ++frame) {
    double horizontal = 0.0;
    if (frame < legacy_frames / 2) {
      horizontal = std::min(frame * camera_step, camera_peak);
    } else {
      horizontal = camera_peak - (frame - (legacy_frames - 1) / 2.0) * camera_step;
      if (horizontal < 0.01) horizontal = 0.0;
    }

    if (frame < legacy_frames / 4) {
      depth_step[frame] = -frame * depth_increment;
    } else if (frame < legacy_frames / 2) {
      depth_step[frame] =
          -depth_peak + (frame - legacy_frames / 4.0) * depth_increment;
    } else {
      depth_step[frame] = -depth_step[frame - legacy_frames / 2];
    }

    angle[frame + 1] = angle[frame] + rotation_step[frame] * 90.0 / rotation_sum;
    camera_y[frame + 1] = (forward ? 3.6 : 3.0) * horizontal;
    camera_z[frame + 1] =
        camera_z[frame] - (forward ? 2.4 : 2.0) * depth_step[frame];
  }

  const double position = std::clamp(progress, 0.0, 1.0) * legacy_frames;
  const int first = std::min(static_cast<int>(position), legacy_frames - 1);
  const int second = std::min(first + 1, legacy_frames);
  const double fraction = position - first;
  const auto interpolate = [&](const auto& values) {
    return static_cast<float>(values[first] + (values[second] - values[first]) * fraction);
  };
  return {interpolate(angle) * pi / 180.0F, interpolate(camera_y), interpolate(camera_z)};
}

const char* desktop_vertex_shader = R"(
#version 130
in vec3 position;
in vec2 texture_coordinate;
uniform mat4 transform;
out vec2 uv;
void main() {
  gl_Position = transform * vec4(position, 1.0);
  uv = texture_coordinate;
}
)";

const char* desktop_fragment_shader = R"(
#version 130
in vec2 uv;
uniform sampler2D page_texture;
uniform vec4 solid_color;
uniform bool textured;
out vec4 output_color;
void main() {
  output_color = textured ? texture(page_texture, uv) : solid_color;
}
)";

const char* es_vertex_shader = R"(
#version 300 es
precision highp float;
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texture_coordinate;
uniform mat4 transform;
out vec2 uv;
void main() {
  gl_Position = transform * vec4(position, 1.0);
  uv = texture_coordinate;
}
)";

const char* es_fragment_shader = R"(
#version 300 es
precision highp float;
in vec2 uv;
uniform sampler2D page_texture;
uniform vec4 solid_color;
uniform bool textured;
out vec4 output_color;
void main() {
  output_color = textured ? texture(page_texture, uv) : solid_color;
}
)";

std::vector<unsigned char> surface_pixels_rgba(cairo_surface_t* surface) {
  const int width = cairo_image_surface_get_width(surface);
  const int height = cairo_image_surface_get_height(surface);
  const int stride = cairo_image_surface_get_stride(surface);
  const unsigned char* source = cairo_image_surface_get_data(surface);
  std::vector<unsigned char> result(static_cast<std::size_t>(width * height * 4));
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      std::uint32_t argb = 0;
      std::memcpy(&argb, source + row * stride + column * 4, sizeof(argb));
      const std::size_t offset = static_cast<std::size_t>((row * width + column) * 4);
      result[offset] = static_cast<unsigned char>((argb >> 16U) & 0xffU);
      result[offset + 1] = static_cast<unsigned char>((argb >> 8U) & 0xffU);
      result[offset + 2] = static_cast<unsigned char>(argb & 0xffU);
      result[offset + 3] = static_cast<unsigned char>((argb >> 24U) & 0xffU);
    }
  }
  return result;
}

}  // namespace

Renderer::Renderer(Document& document, Color background, Color top)
    : document_(document), background_(background), top_(top) {
  textures_.resize(static_cast<std::size_t>(document.page_count()));
  prepared_pages_.resize(static_cast<std::size_t>(document.page_count()));
  render_thread_ = std::thread(&Renderer::render_pages_in_background, this);
}

Renderer::~Renderer() {
  {
    std::lock_guard lock(prepared_mutex_);
    stop_render_thread_ = true;
  }
  prepared_condition_.notify_one();
  if (render_thread_.joinable()) render_thread_.join();
}

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
  const bool desktop = epoxy_is_desktop_gl();
  const GLuint vertex =
      compile_shader(GL_VERTEX_SHADER, desktop ? desktop_vertex_shader : es_vertex_shader);
  const GLuint fragment =
      compile_shader(GL_FRAGMENT_SHADER, desktop ? desktop_fragment_shader : es_fragment_shader);
  program_ = glCreateProgram();
  glAttachShader(program_, vertex);
  glAttachShader(program_, fragment);
  glBindAttribLocation(program_, 0, "position");
  glBindAttribLocation(program_, 1, "texture_coordinate");
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
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum_texture_size_);
  if (maximum_texture_size_ < 1) {
    throw std::runtime_error("OpenGL reported an invalid maximum texture size");
  }
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
}

void Renderer::release() {
  for (PageTexture& texture : textures_) {
    if (texture.name != 0) glDeleteTextures(1, &texture.name);
    texture = {};
  }
  if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
  if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
  if (program_ != 0) glDeleteProgram(program_);
  vbo_ = vao_ = program_ = 0;
}

GLuint Renderer::texture_for_page(int page, int maximum_size) {
  PageTexture& texture = textures_.at(static_cast<std::size_t>(page));
  maximum_size = std::clamp(maximum_size, 1, maximum_texture_size_);
  if (texture.name != 0 && texture.maximum_size >= maximum_size) return texture.name;

  Surface surface;
  {
    std::lock_guard lock(prepared_mutex_);
    PreparedPage& prepared = prepared_pages_.at(static_cast<std::size_t>(page));
    if (prepared.surface && prepared.maximum_size >= maximum_size) {
      surface = std::move(prepared.surface);
      prepared.maximum_size = 0;
      prepared.requested_size = 0;
    }
  }
  if (!surface) {
    std::lock_guard document_lock(document_render_mutex_);
    surface = document_.render_page(page, maximum_size);
    std::lock_guard prepared_lock(prepared_mutex_);
    PreparedPage& prepared = prepared_pages_.at(static_cast<std::size_t>(page));
    prepared.surface.reset();
    prepared.maximum_size = 0;
    prepared.requested_size = 0;
    std::erase_if(render_requests_, [page](const RenderRequest& request) {
      return request.page == page;
    });
  }
  const int width = cairo_image_surface_get_width(surface.get());
  const int height = cairo_image_surface_get_height(surface.get());
  const std::vector<unsigned char> pixels = surface_pixels_rgba(surface.get());
  if (texture.name != 0) glDeleteTextures(1, &texture.name);
  glGenTextures(1, &texture.name);
  glBindTexture(GL_TEXTURE_2D, texture.name);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels.data());
  texture.maximum_size = std::max(width, height);
  return texture.name;
}

void Renderer::schedule_page(int page, int maximum_size) {
  maximum_size = std::clamp(maximum_size, 1, maximum_texture_size_);
  if (textures_.at(static_cast<std::size_t>(page)).maximum_size >= maximum_size) return;

  {
    std::lock_guard lock(prepared_mutex_);
    PreparedPage& prepared = prepared_pages_.at(static_cast<std::size_t>(page));
    if (prepared.maximum_size >= maximum_size || prepared.requested_size >= maximum_size) return;
    prepared.requested_size = maximum_size;
    std::erase_if(render_requests_, [page](const RenderRequest& request) {
      return request.page == page;
    });
    render_requests_.push_back({page, maximum_size});
  }
  prepared_condition_.notify_one();
}

void Renderer::render_pages_in_background() {
  for (;;) {
    RenderRequest request{};
    {
      std::unique_lock lock(prepared_mutex_);
      prepared_condition_.wait(lock, [this] {
        return stop_render_thread_ || !render_requests_.empty();
      });
      if (stop_render_thread_) return;
      request = render_requests_.front();
      render_requests_.pop_front();
    }

    Surface surface;
    {
      std::lock_guard document_lock(document_render_mutex_);
      surface = document_.render_page(request.page, request.maximum_size);
    }
    std::lock_guard lock(prepared_mutex_);
    PreparedPage& prepared = prepared_pages_.at(static_cast<std::size_t>(request.page));
    if (prepared.requested_size == request.maximum_size) {
      prepared.surface = std::move(surface);
      prepared.maximum_size = request.maximum_size;
      prepared.requested_size = 0;
    }
  }
}

void Renderer::preload_neighbors(int page, int maximum_size) {
  if (document_.page_count() < 2) return;
  const int next = (page + 1) % document_.page_count();
  const int previous = (page + document_.page_count() - 1) % document_.page_count();
  for (int candidate = 0; candidate < document_.page_count(); ++candidate) {
    if (candidate == page || candidate == next || candidate == previous) continue;
    PageTexture& texture = textures_[static_cast<std::size_t>(candidate)];
    if (texture.name != 0) glDeleteTextures(1, &texture.name);
    texture = {};
  }
  {
    std::lock_guard lock(prepared_mutex_);
    std::erase_if(render_requests_, [page, next, previous](const RenderRequest& request) {
      return request.page != page && request.page != next && request.page != previous;
    });
    for (int candidate = 0; candidate < document_.page_count(); ++candidate) {
      if (candidate == page || candidate == next || candidate == previous) continue;
      prepared_pages_[static_cast<std::size_t>(candidate)] = {};
    }
  }
  schedule_page(next, maximum_size);
  schedule_page(previous, maximum_size);
}

void Renderer::draw_page(int page, int maximum_size, const float* matrix) {
  glUniformMatrix4fv(matrix_uniform_, 1, GL_FALSE, matrix);
  glUniform1i(textured_uniform_, GL_TRUE);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_for_page(page, maximum_size));
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

  const auto& page = document_.page_info(frame.current_page);
  const Viewport viewport = content_viewport(width, height, page.width / page.height);
  glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
  const int viewport_texture_size = std::max(viewport.width, viewport.height);
  const int texture_size = std::min(
      maximum_texture_size_,
      viewport_texture_size * static_cast<int>(std::ceil(std::max(1.0F, frame.zoom))));
  const Matrix zoom = translate(frame.offset_x, frame.offset_y, 0.0F) *
                      scale(frame.zoom, frame.zoom, 1.0F);

  if (frame.transition == Transition::none || frame.transition == Transition::switch_page) {
    draw_page(frame.current_page, texture_size, zoom.value.data());
    preload_neighbors(frame.current_page, viewport_texture_size);
    return;
  }

  const bool forward = frame.transition == Transition::cube_forward;
  const float direction = forward ? -1.0F : 1.0F;
  const CubePose pose = legacy_cube_pose(frame.progress, forward);
  constexpr float field_of_view = 44.0F * pi / 180.0F;
  const float camera_distance = 1.0F + 1.0F / std::tan(field_of_view / 2.0F);
  const Matrix projection = perspective(field_of_view, 1.0F, 0.5F, 10.0F);
  const Matrix view = look_at(0.0F, pose.camera_y, camera_distance + pose.camera_z, 0.0F,
                              0.0F, 0.0F);
  const Matrix cube = rotate_y(direction * pose.angle);
  const Matrix animated_view = projection * view * zoom * cube;
  const Matrix current = animated_view * translate(0.0F, 0.0F, 1.0F);
  const Matrix incoming = animated_view * translate(forward ? 1.0F : -1.0F, 0.0F, 0.0F) *
                          rotate_y(forward ? pi / 2.0F : -pi / 2.0F);
  draw_page(frame.current_page, texture_size, current.value.data());
  draw_page(frame.next_page, texture_size, incoming.value.data());

  const Matrix top = animated_view * translate(0.0F, 1.0F, 0.0F) * rotate_x(-pi / 2.0F);
  const Matrix bottom = animated_view * translate(0.0F, -1.0F, 0.0F) * rotate_x(pi / 2.0F);
  const Matrix back = animated_view * translate(0.0F, 0.0F, -1.0F) * rotate_y(pi);
  const Matrix other_side =
      animated_view * translate(forward ? -1.0F : 1.0F, 0.0F, 0.0F) *
      rotate_y(forward ? -pi / 2.0F : pi / 2.0F);
  draw_color(top_, top.value.data());
  draw_color(top_, bottom.value.data());
  draw_color(top_, back.value.data());
  draw_color(top_, other_side.value.data());
}

void Renderer::write_png(const std::string& filename, int width, int height) const {
  std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 4));
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  const int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
  std::vector<unsigned char> flipped(static_cast<std::size_t>(stride * height));
  for (int row = 0; row < height; ++row) {
    const unsigned char* source =
        pixels.data() + static_cast<std::size_t>((height - row - 1) * width * 4);
    unsigned char* destination =
        flipped.data() + static_cast<std::size_t>(row * stride);
    for (int column = 0; column < width; ++column) {
      const std::uint32_t argb =
          (static_cast<std::uint32_t>(source[column * 4 + 3]) << 24U) |
          (static_cast<std::uint32_t>(source[column * 4]) << 16U) |
          (static_cast<std::uint32_t>(source[column * 4 + 1]) << 8U) |
          static_cast<std::uint32_t>(source[column * 4 + 2]);
      std::memcpy(destination + column * 4, &argb, sizeof(argb));
    }
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
