#pragma once

#include "document.hpp"
#include "options.hpp"
#include "renderer.hpp"

#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <vector>

namespace pdfcube {

class Application {
 public:
  explicit Application(Options options);
  int run();

 private:
  static void activate_callback(GtkApplication* application, gpointer data);
  static void file_open_callback(GObject* source, GAsyncResult* result, gpointer data);
  static void realize_callback(GtkGLArea* area, gpointer data);
  static void unrealize_callback(GtkGLArea* area, gpointer data);
  static gboolean render_callback(GtkGLArea* area, GdkGLContext* context, gpointer data);
  static gboolean key_callback(GtkEventControllerKey* controller, guint keyval,
                               guint keycode, GdkModifierType state, gpointer data);
  static gboolean tick_callback(GtkWidget* widget, GdkFrameClock* clock, gpointer data);
  static gboolean auto_advance_callback(gpointer data);

  void activate(GtkApplication* application);
  void choose_file();
  void open_document(const std::string& filename);
  void configure_program();
  void jump_to_section(int section);
  void start_page_change(int delta, bool cube);
  void start_zoom(float x, float y, float zoom);
  void reset_zoom();
  void schedule_auto_advance();
  FrameState frame_state() const;
  bool handle_key(guint keyval, GdkModifierType state);
  double animation_progress() const;

  Options options_;
  GtkApplication* application_{nullptr};
  GtkWidget* window_{nullptr};
  GtkWidget* area_{nullptr};
  std::unique_ptr<Document> document_;
  std::unique_ptr<Renderer> renderer_;
  std::vector<bool> cube_transitions_;
  std::vector<int> durations_;
  int current_page_{0};
  int next_page_{0};
  Transition transition_{Transition::none};
  gint64 transition_started_{0};
  float zoom_{1.0F};
  float offset_x_{0.0F};
  float offset_y_{0.0F};
  float zoom_from_{1.0F};
  float zoom_to_{1.0F};
  float x_from_{0.0F};
  float x_to_{0.0F};
  float y_from_{0.0F};
  float y_to_{0.0F};
  bool zoom_animating_{false};
  gint64 zoom_started_{0};
  guint auto_advance_source_{0};
  bool snapshot_written_{false};
  std::string startup_error_;
};

}  // namespace pdfcube

