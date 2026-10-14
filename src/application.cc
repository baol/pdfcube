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
    transitions_.resize(static_cast<std::size_t>(document_->page_count()));
    durations_.resize(static_cast<std::size_t>(document_->page_count()));
    for (int page = 0; page < document_->page_count(); ++page) {
      const PageInfo& info = document_->page_info(page);
      cube_transitions_[static_cast<std::size_t>(page)] = info.cube_transition;
      transitions_[page] = info.cube_transition ? Transition::cube_forward :
          info.transition_type == POPPLER_PAGE_TRANSITION_BLINDS ? Transition::blinds :
          info.transition_type == POPPLER_PAGE_TRANSITION_SPLIT ? Transition::split :
          info.transition_type == POPPLER_PAGE_TRANSITION_COVER ? Transition::cover :
          info.transition_type == POPPLER_PAGE_TRANSITION_UNCOVER ? Transition::uncover :
          Transition::switch_page;
      durations_[static_cast<std::size_t>(page)] = info.duration_ms;
    }
    configure_program();
    if (!options_.snapshot.empty() && options_.snapshot_transition &&
        document_->page_count() > 1) {
      next_page_ = 1;
      transition_ = transitions_[1];
      if (transition_ == Transition::switch_page && options_.snapshot_progress <= 0)
        transition_ = Transition::none;
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
  Transition last_transition = Transition::switch_page;
  while (page < document_->page_count() && std::getline(input, duration, ':')) {
    if (duration.empty()) break;
    last_duration = std::max(0, static_cast<int>(std::stod(duration) * 1000.0));
    durations_[static_cast<std::size_t>(page)] = last_duration;
    if (std::getline(input, effect, ':')) {
      last_cube = effect == "c";
      last_transition = last_cube ? Transition::cube_forward :
          effect == "blinds" ? Transition::blinds :
          effect == "split" ? Transition::split :
          effect == "cover" ? Transition::cover :
          effect == "uncover" ? Transition::uncover : Transition::switch_page;
    }
    cube_transitions_[static_cast<std::size_t>((page + 1) % document_->page_count())] = last_cube;
    transitions_[(page + 1) % document_->page_count()] = last_transition;
    ++page;
  }
  for (; page < document_->page_count(); ++page) {
    durations_[static_cast<std::size_t>(page)] = last_duration;
    cube_transitions_[static_cast<std::size_t>((page + 1) % document_->page_count())] = last_cube;
    transitions_[(page + 1) % document_->page_count()] = last_transition;
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
    if (self->transition_preparing_) {
      self->transition_started_ = g_get_monotonic_time();
      self->transition_preparing_ = false;
    }
    if (self->zoom_preparing_) {
      // Start the clock only after the full zoom-resolution texture is ready.
      self->zoom_started_ = g_get_monotonic_time();
      self->zoom_preparing_ = false;
    }
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
  if (transition_preparing_) return 0.0;
  return std::clamp((g_get_monotonic_time() - transition_started_) /
                        static_cast<double>(transition_length_us_),
                    0.0, 1.0);
}

FrameState Application::frame_state() const {
  FrameState state;
  state.current_page = current_page_;
  state.next_page = next_page_;
  state.transition = transition_;
  state.progress = animation_progress();
  if (document_) state.vertical_panels =
      document_->page_info(next_page_).alignment == POPPLER_PAGE_TRANSITION_VERTICAL;
  if (document_) state.split_inward =
      document_->page_info(next_page_).direction == POPPLER_PAGE_TRANSITION_INWARD;
  if (document_) state.direction_degrees = document_->page_info(next_page_).angle;
  if (page_effect(transition_)) return state;
  double progress = zoom_animating_ ? std::clamp(
      (g_get_monotonic_time() - zoom_started_) / static_cast<double>(zoom_duration_us),
      0.0, 1.0) : 1.0;
  if (zoom_reversing_) progress = 1.0 - progress;
  if (zoom_preparing_) progress = 0.0;
  const auto pose = legacy_zoom_pose(zoom_region_, progress);
  state.texture_zoom = legacy_zoom_pose(zoom_region_, 1.0).scale;
  state.zoom = pose.scale;
  state.offset_x = pose.x;
  state.offset_y = pose.y;
  return state;
}

gboolean Application::tick_callback(GtkWidget* widget, GdkFrameClock*, gpointer data) {
  auto* self = static_cast<Application*>(data);
  bool settled_transition = false;
  if (self->transition_ != Transition::none && self->animation_progress() >= 1.0 &&
      self->options_.snapshot.empty()) {
    self->current_page_ = self->next_page_;
    self->transition_ = Transition::none;
    self->zoom_region_ = ZoomRegion::none;
    settled_transition = true;
    if (self->options_.demo) self->schedule_auto_advance();
  }
  if (self->zoom_animating_ && !self->zoom_preparing_ &&
      g_get_monotonic_time() - self->zoom_started_ >= zoom_duration_us) {
    if (self->zoom_reversing_) self->zoom_region_ = ZoomRegion::none;
    self->zoom_reversing_ = false;
    self->zoom_animating_ = false;
    settled_transition = true;
  }
  if (settled_transition || self->transition_ != Transition::none || self->zoom_animating_) {
    gtk_gl_area_queue_render(GTK_GL_AREA(widget));
  }
  return G_SOURCE_CONTINUE;
}

void Application::start_page_change(int delta, bool cube, bool programmed) {
  if (!document_ || busy()) return;
  next_page_ = wrap_page(current_page_ + delta, document_->page_count());
  transition_ = cube ? (delta > 0 ? Transition::cube_forward : Transition::cube_backward)
                     : Transition::switch_page;
  transition_length_us_ = transition_duration_us;
  if (programmed) {
    transition_ = transitions_[next_page_];
    if (options_.program.empty()) {
      transition_length_us_ = static_cast<gint64>(
          document_->page_info(next_page_).transition_seconds * 1000000);
      if (transition_length_us_ <= 0) transition_ = Transition::switch_page;
    }
  }
  transition_preparing_ = page_effect(transition_);
  transition_started_ = g_get_monotonic_time();
  if (transition_ == Transition::switch_page) {
    current_page_ = next_page_;
    transition_ = Transition::none;
    zoom_region_ = ZoomRegion::none;
  }
  gtk_gl_area_queue_render(GTK_GL_AREA(area_));
}

void Application::toggle_zoom(ZoomRegion region) {
  if (busy()) return;
  if (zoom_region_ != ZoomRegion::none) {
    reset_zoom();
    return;
  }
  zoom_region_ = region;
  zoom_reversing_ = false;
  zoom_started_ = g_get_monotonic_time();
  zoom_animating_ = true;
  zoom_preparing_ = true;
}

void Application::reset_zoom() {
  if (busy() || zoom_region_ == ZoomRegion::none) return;
  zoom_reversing_ = true;
  zoom_started_ = g_get_monotonic_time();
  zoom_animating_ = true;
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
      zoom_region_ = ZoomRegion::none;
      zoom_reversing_ = false;
      zoom_animating_ = false;
      zoom_preparing_ = false;
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
      start_page_change(1, cube, true);
      return true;
    }
    case GDK_KEY_g: reset_zoom(); return true;
    case GDK_KEY_h: toggle_zoom(ZoomRegion::upper_left); return true;
    case GDK_KEY_j: toggle_zoom(ZoomRegion::upper_right); return true;
    case GDK_KEY_k: toggle_zoom(ZoomRegion::lower_left); return true;
    case GDK_KEY_l: toggle_zoom(ZoomRegion::lower_right); return true;
    case GDK_KEY_z: toggle_zoom(ZoomRegion::center); return true;
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
  self->start_page_change(1, self->cube_transitions_[static_cast<std::size_t>(destination)], true);
  if (self->transition_ == Transition::none && self->options_.demo) {
    self->schedule_auto_advance();
  }
  return G_SOURCE_REMOVE;
}

}  // namespace pdfcube

