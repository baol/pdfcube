#include "application.hpp"
#include "navigation.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace pdfcube {
namespace {

constexpr gint64 transition_duration_us = 750'000;
constexpr gint64 zoom_duration_us = 750'000;

int wrap_page(int page, int count) {
  const int wrapped = page % count;
  return wrapped < 0 ? wrapped + count : wrapped;
}

double eased(double value) {
  value = std::clamp(value, 0.0, 1.0);
  return value * value * (3.0 - 2.0 * value);
}

void require_gl_context(GtkGLArea* area) {
  gtk_gl_area_make_current(area);
  if (GError* error = gtk_gl_area_get_error(area)) {
    throw std::runtime_error(std::string("OpenGL initialization failed: ") + error->message);
  }
  if (gdk_gl_context_get_current() == nullptr) {
    throw std::runtime_error(
        "OpenGL initialization failed: GTK did not make a GLX or EGL context current");
  }
}

}  // namespace

Application::Application(Options options) : options_(std::move(options)) {}

int Application::run() {
  application_ = gtk_application_new("org.pdfcube.pdfcube", G_APPLICATION_DEFAULT_FLAGS);
  g_application_set_resource_base_path(G_APPLICATION(application_), "/org/pdfcube/pdfcube");
  g_signal_connect(application_, "activate", G_CALLBACK(activate_callback), this);
  char name[] = "pdfcube";
  char* arguments[] = {name, nullptr};
  const int status = g_application_run(G_APPLICATION(application_), 1, arguments);
  if (auto_advance_source_ != 0) g_source_remove(auto_advance_source_);
  g_object_unref(application_);
  if (!startup_error_.empty()) {
    std::cerr << startup_error_ << '\n';
    return 1;
  }
  return status;
}

void Application::activate_callback(GtkApplication* application, gpointer data) {
  static_cast<Application*>(data)->activate(application);
}

void Application::activate(GtkApplication* application) {
  window_ = gtk_application_window_new(application);
  gtk_window_set_title(GTK_WINDOW(window_), "PDFCube");
  gtk_window_set_default_size(GTK_WINDOW(window_), 960, 720);

  area_ = gtk_gl_area_new();
  gtk_gl_area_set_allowed_apis(
      GTK_GL_AREA(area_), static_cast<GdkGLAPI>(GDK_GL_API_GL | GDK_GL_API_GLES));
  gtk_gl_area_set_required_version(GTK_GL_AREA(area_), 3, 0);
  gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(area_), true);
  gtk_widget_set_hexpand(area_, true);
  gtk_widget_set_vexpand(area_, true);
  g_signal_connect(area_, "realize", G_CALLBACK(realize_callback), this);
  g_signal_connect(area_, "unrealize", G_CALLBACK(unrealize_callback), this);
  g_signal_connect(area_, "render", G_CALLBACK(render_callback), this);

  GtkEventController* keys = gtk_event_controller_key_new();
  g_signal_connect(keys, "key-pressed", G_CALLBACK(key_callback), this);
  gtk_widget_add_controller(window_, keys);
  gtk_widget_add_tick_callback(area_, tick_callback, this, nullptr);
  gtk_window_set_child(GTK_WINDOW(window_), area_);
  if (options_.fullscreen) gtk_window_fullscreen(GTK_WINDOW(window_));
  gtk_window_present(GTK_WINDOW(window_));

  if (options_.input_file.empty()) {
    choose_file();
  } else {
    open_document(options_.input_file);
  }
}

void Application::choose_file() {
  GtkFileDialog* chooser = gtk_file_dialog_new();
  gtk_file_dialog_set_title(chooser, "Choose a PDF presentation");
  GtkFileFilter* filter = gtk_file_filter_new();
  gtk_file_filter_set_name(filter, "PDF documents");
  gtk_file_filter_add_mime_type(filter, "application/pdf");
  gtk_file_dialog_set_default_filter(chooser, filter);
  g_object_unref(filter);
  gtk_file_dialog_open(chooser, GTK_WINDOW(window_), nullptr, file_open_callback, this);
  g_object_unref(chooser);
}

void Application::file_open_callback(GObject* source, GAsyncResult* result, gpointer data) {
  auto* self = static_cast<Application*>(data);
  GError* error = nullptr;
  GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
  if (file != nullptr) {
    char* path = g_file_get_path(file);
    if (path != nullptr) self->open_document(path);
    g_free(path);
    g_object_unref(file);
  } else if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    self->startup_error_ = std::string("Could not open PDF: ") + error->message;
    g_application_quit(G_APPLICATION(self->application_));
  }
  g_clear_error(&error);
}

void Application::open_document(const std::string& filename) {
  try {
    document_ = std::make_unique<Document>(filename);
    cube_transitions_.resize(static_cast<std::size_t>(document_->page_count()));
    durations_.resize(static_cast<std::size_t>(document_->page_count()));
    for (int page = 0; page < document_->page_count(); ++page) {
      const PageInfo& info = document_->page_info(page);
      cube_transitions_[static_cast<std::size_t>(page)] = info.cube_transition;
      durations_[static_cast<std::size_t>(page)] = info.duration_ms;
    }
    configure_program();
    if (!options_.snapshot.empty() && options_.snapshot_progress > 0.0 &&
        document_->page_count() > 1) {
      next_page_ = 1;
      transition_ = cube_transitions_[1] ? Transition::cube_forward : Transition::switch_page;
      transition_started_ = g_get_monotonic_time();
    }
    gtk_gl_area_queue_render(GTK_GL_AREA(area_));
    if (options_.demo) schedule_auto_advance(true);
  } catch (const std::exception& error) {
    startup_error_ = error.what();
    g_application_quit(G_APPLICATION(application_));
  }
}

void Application::configure_program() {
  if (options_.program.empty() || !document_) return;
  std::istringstream input(options_.program);
  std::string duration;
  std::string effect;
  int page = 0;
  int last_duration = 0;
  bool last_cube = false;
  while (page < document_->page_count() && std::getline(input, duration, ':')) {
    if (duration.empty()) break;
    last_duration = std::max(0, static_cast<int>(std::stod(duration) * 1000.0));
    durations_[static_cast<std::size_t>(page)] = last_duration;
    if (std::getline(input, effect, ':')) last_cube = effect == "c";
    cube_transitions_[static_cast<std::size_t>((page + 1) % document_->page_count())] = last_cube;
    ++page;
  }
  for (; page < document_->page_count(); ++page) {
    durations_[static_cast<std::size_t>(page)] = last_duration;
    cube_transitions_[static_cast<std::size_t>((page + 1) % document_->page_count())] = last_cube;
  }
}

void Application::realize_callback(GtkGLArea* area, gpointer data) {
  auto* self = static_cast<Application*>(data);
  gtk_gl_area_make_current(area);
  if (GError* error = gtk_gl_area_get_error(area)) {
    self->startup_error_ = std::string("OpenGL initialization failed: ") + error->message;
    g_application_quit(G_APPLICATION(self->application_));
  }
}

void Application::unrealize_callback(GtkGLArea* area, gpointer data) {
  auto* self = static_cast<Application*>(data);
  if (self->renderer_) {
    try {
      require_gl_context(area);
      self->renderer_->release();
    } catch (const std::exception&) {
      // The context has already gone away, so its resources go with it.
    }
  }
  self->renderer_.reset();
}

gboolean Application::render_callback(GtkGLArea* area, GdkGLContext* context, gpointer data) {
  auto* self = static_cast<Application*>(data);
  if (!self->document_) return TRUE;
  try {
    gdk_gl_context_make_current(context);
    require_gl_context(area);
    if (self->reload_renderer_ && self->renderer_) {
      self->renderer_->release();
      self->renderer_.reset();
      self->reload_renderer_ = false;
    }
    if (!self->renderer_) {
      self->renderer_ = std::make_unique<Renderer>(
          *self->document_, self->options_.background, self->options_.top);
      self->renderer_->initialize();
    }
    const int scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));
    const int width = gtk_widget_get_width(GTK_WIDGET(area)) * scale;
    const int height = gtk_widget_get_height(GTK_WIDGET(area)) * scale;
    const FrameState state = self->frame_state();
    self->renderer_->render(width, height, state);
    if (!self->options_.snapshot.empty() && !self->snapshot_written_) {
      self->renderer_->write_png(self->options_.snapshot, width, height);
      self->snapshot_written_ = true;
      g_application_quit(G_APPLICATION(self->application_));
    }
  } catch (const std::exception& error) {
    self->startup_error_ = error.what();
    g_application_quit(G_APPLICATION(self->application_));
  }
  return TRUE;
}

double Application::animation_progress() const {
  if (transition_ == Transition::none) return 1.0;
  if (!options_.snapshot.empty()) return options_.snapshot_progress;
  return std::clamp((g_get_monotonic_time() - transition_started_) /
                        static_cast<double>(transition_duration_us),
                    0.0, 1.0);
}

FrameState Application::frame_state() const {
  FrameState state;
  state.current_page = current_page_;
  state.next_page = next_page_;
  state.transition = transition_;
  state.progress = animation_progress();
  if (zoom_animating_) {
    const double progress = eased(std::clamp(
        (g_get_monotonic_time() - zoom_started_) / static_cast<double>(zoom_duration_us), 0.0, 1.0));
    state.zoom = zoom_from_ + (zoom_to_ - zoom_from_) * static_cast<float>(progress);
    state.offset_x = x_from_ + (x_to_ - x_from_) * static_cast<float>(progress);
    state.offset_y = y_from_ + (y_to_ - y_from_) * static_cast<float>(progress);
  } else {
    state.zoom = zoom_;
    state.offset_x = offset_x_;
    state.offset_y = offset_y_;
  }
  return state;
}

gboolean Application::tick_callback(GtkWidget* widget, GdkFrameClock*, gpointer data) {
  auto* self = static_cast<Application*>(data);
  bool settled_transition = false;
  if (self->transition_ != Transition::none && self->animation_progress() >= 1.0 &&
      self->options_.snapshot.empty()) {
    self->current_page_ = self->next_page_;
    self->transition_ = Transition::none;
    settled_transition = true;
    if (self->options_.demo) self->schedule_auto_advance();
  }
  if (self->zoom_animating_ && g_get_monotonic_time() - self->zoom_started_ >= zoom_duration_us) {
    self->zoom_ = self->zoom_to_;
    self->offset_x_ = self->x_to_;
    self->offset_y_ = self->y_to_;
    self->zoom_animating_ = false;
  }
  if (settled_transition || self->transition_ != Transition::none || self->zoom_animating_) {
    gtk_gl_area_queue_render(GTK_GL_AREA(widget));
  }
  return G_SOURCE_CONTINUE;
}

void Application::start_page_change(int delta, bool cube) {
  if (!document_ || busy()) return;
  next_page_ = wrap_page(current_page_ + delta, document_->page_count());
  transition_ = cube ? (delta > 0 ? Transition::cube_forward : Transition::cube_backward)
                     : Transition::switch_page;
  transition_started_ = g_get_monotonic_time();
  if (!cube) {
    current_page_ = next_page_;
    transition_ = Transition::none;
  }
  gtk_gl_area_queue_render(GTK_GL_AREA(area_));
}

void Application::start_zoom(float x, float y, float zoom) {
  if (busy()) return;
  const FrameState current = frame_state();
  zoom_from_ = current.zoom;
  x_from_ = current.offset_x;
  y_from_ = current.offset_y;
  zoom_to_ = zoom;
  x_to_ = x;
  y_to_ = y;
  zoom_started_ = g_get_monotonic_time();
  zoom_animating_ = true;
}

void Application::toggle_zoom(float x, float y, float zoom) {
  if (busy()) return;
  if (std::abs(zoom_ - 1.0F) > 0.001F) reset_zoom();
  else start_zoom(x, y, zoom);
}

void Application::reset_zoom() {
  if (!busy() && std::abs(zoom_ - 1.0F) > 0.001F) start_zoom(0.0F, 0.0F, 1.0F);
}

bool Application::busy() const {
  return transition_ != Transition::none || zoom_animating_;
}

void Application::jump_to_section(int section) {
  if (!document_ || busy()) return;
  for (int page = 0; page < document_->page_count(); ++page) {
    if (cube_transitions_[static_cast<std::size_t>(page)] && --section == 0) {
      current_page_ = page;
      next_page_ = page;
      gtk_gl_area_queue_render(GTK_GL_AREA(area_));
      return;
    }
  }
}

gboolean Application::key_callback(GtkEventControllerKey*, guint keyval, guint,
                                   GdkModifierType state, gpointer data) {
  return static_cast<Application*>(data)->handle_key(keyval, state);
}

bool Application::handle_key(guint keyval, GdkModifierType state) {
  if ((state & GDK_CONTROL_MASK) != 0) {
    if (keyval == GDK_KEY_q) {
      g_application_quit(G_APPLICATION(application_));
      return true;
    }
    if (keyval == GDK_KEY_l) {
      zoom_ = zoom_from_ = zoom_to_ = 1.0F;
      offset_x_ = offset_y_ = x_from_ = x_to_ = y_from_ = y_to_ = 0.0F;
      zoom_animating_ = false;
      if (renderer_) {
        reload_renderer_ = true;
        gtk_gl_area_queue_render(GTK_GL_AREA(area_));
      }
      return true;
    }
  }

  if (keyval >= GDK_KEY_1 && keyval <= GDK_KEY_9 && document_) {
    const NumberJump jump = number_jump(
        static_cast<int>(keyval - GDK_KEY_1 + 1), (state & GDK_CONTROL_MASK) != 0);
    if (jump.kind == NumberJumpKind::cube_section) {
      jump_to_section(jump.number);
    } else if (!busy() && jump.number < document_->page_count()) {
      current_page_ = jump.number;
      next_page_ = jump.number;
      gtk_gl_area_queue_render(GTK_GL_AREA(area_));
    }
    return true;
  }

  switch (keyval) {
    case GDK_KEY_Escape:
      g_application_quit(G_APPLICATION(application_));
      return true;
    case GDK_KEY_c: start_page_change(1, true); return true;
    case GDK_KEY_a: start_page_change(-1, true); return true;
    case GDK_KEY_Right: start_page_change(1, false); return true;
    case GDK_KEY_Left:
    case GDK_KEY_Page_Up: start_page_change(-1, false); return true;
    case GDK_KEY_space:
    case GDK_KEY_Page_Down: {
      const int destination = document_ ? wrap_page(current_page_ + 1, document_->page_count()) : 0;
      const bool cube = document_ && cube_transitions_[static_cast<std::size_t>(destination)];
      start_page_change(1, cube);
      return true;
    }
    case GDK_KEY_g: reset_zoom(); return true;
    case GDK_KEY_h: toggle_zoom(0.45F, -0.35F, 2.0F); return true;
    case GDK_KEY_j: toggle_zoom(-0.45F, -0.35F, 2.0F); return true;
    case GDK_KEY_k: toggle_zoom(0.45F, 0.35F, 2.0F); return true;
    case GDK_KEY_l: toggle_zoom(-0.45F, 0.35F, 2.0F); return true;
    case GDK_KEY_z: toggle_zoom(0.0F, 0.1F, 1.5F); return true;
    case GDK_KEY_f:
      if (gtk_window_is_fullscreen(GTK_WINDOW(window_))) gtk_window_unfullscreen(GTK_WINDOW(window_));
      else gtk_window_fullscreen(GTK_WINDOW(window_));
      return true;
    case GDK_KEY_t:
      if (busy()) return true;
      options_.demo = !options_.demo;
      if (options_.demo) schedule_auto_advance(true);
      else if (auto_advance_source_ != 0) {
        g_source_remove(auto_advance_source_);
        auto_advance_source_ = 0;
      }
      return true;
    default: return false;
  }
}

void Application::schedule_auto_advance(bool allow_immediate) {
  if (!document_ || !options_.demo) return;
  if (auto_advance_source_ != 0) g_source_remove(auto_advance_source_);
  const int delay = durations_[static_cast<std::size_t>(current_page_)];
  if (delay <= 0) {
    auto_advance_source_ = allow_immediate ? g_idle_add(auto_advance_callback, this) : 0;
    return;
  }
  auto_advance_source_ = g_timeout_add(delay, auto_advance_callback, this);
}

gboolean Application::auto_advance_callback(gpointer data) {
  auto* self = static_cast<Application*>(data);
  self->auto_advance_source_ = 0;
  if (!self->document_) return G_SOURCE_REMOVE;
  const int destination = wrap_page(self->current_page_ + 1, self->document_->page_count());
  self->start_page_change(1, self->cube_transitions_[static_cast<std::size_t>(destination)]);
  if (self->transition_ == Transition::none && self->options_.demo) {
    self->schedule_auto_advance();
  }
  return G_SOURCE_REMOVE;
}

}  // namespace pdfcube

