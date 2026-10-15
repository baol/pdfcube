#include "application.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace pdfcube {

struct ApplicationTest {
  static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
  }

  static void run(const char* fixture) {
    Application app(Options{});
    app.area_ = gtk_gl_area_new();
    g_object_ref_sink(app.area_);
    app.open_document(fixture);
    require(app.startup_error_.empty(), "fixture failed to open");
    app.options_.demo = true;
    app.durations_.assign(app.document_->page_count(), 60000);
    app.schedule_auto_advance();
    const guint old_timer = app.auto_advance_source_;
    require(old_timer != 0, "demo timer was not scheduled");
    require(!app.handle_key(GDK_KEY_C, GDK_CONTROL_MASK), "Ctrl-C triggered a transition");
    require(!app.handle_key(GDK_KEY_F, GDK_CONTROL_MASK), "Ctrl-F was consumed");
    require(!app.handle_key(GDK_KEY_c, GDK_ALT_MASK), "Alt-C was consumed");
    app.zoom_region_ = ZoomRegion::center;
    app.handle_key(GDK_KEY_Right, GDK_LOCK_MASK);
    require(app.current_page_ == 1 && app.zoom_region_ == ZoomRegion::none,
            "plain navigation failed to reset zoom");
    require(app.auto_advance_source_ != 0 && app.auto_advance_source_ != old_timer,
            "plain navigation kept the old timer");
    require(g_main_context_find_source_by_id(nullptr, old_timer) == nullptr,
            "old timer is still active");
    app.zoom_region_ = ZoomRegion::center;
    app.handle_key(GDK_KEY_1, GdkModifierType(0));
    require(app.current_page_ == 0 && app.zoom_region_ == ZoomRegion::none,
            "numeric jump failed to reset zoom");
    app.zoom_region_ = ZoomRegion::center;
    app.handle_key(GDK_KEY_1, GDK_CONTROL_MASK);
    require(app.current_page_ == 1 && app.zoom_region_ == ZoomRegion::none,
            "section jump failed to reset zoom");
    app.handle_key(GDK_KEY_C, GDK_LOCK_MASK);
    require(app.transition_ == Transition::cube_forward,
            "Caps Lock disabled cube shortcut");
    require(app.auto_advance_source_ == 0, "animation retained previous slide timer");
    app.transition_ = Transition::none;
    app.transition_preparing_ = false;
    app.options_.demo = false;
    app.durations_.assign(app.document_->page_count(), 0);
    app.handle_key(GDK_KEY_T, GDK_LOCK_MASK);
    require(app.options_.demo && app.auto_advance_source_ == 0,
            "demo toggle immediately advanced a page without duration");
    app.handle_key(GDK_KEY_H, GDK_LOCK_MASK);
    require(app.zoom_animating_ && app.tick_source_ != 0,
            "zoom did not restart the frame clock");
    app.zoom_animating_ = false;
    app.zoom_preparing_ = false;
    // The widget owns the registered callback; destruction removes it.
    g_object_unref(app.area_);
  }
};

}  // namespace pdfcube

int main(int argc, char** argv) {
  if (argc != 2) return EXIT_FAILURE;
  gtk_init();
  try {
    pdfcube::ApplicationTest::run(argv[1]);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
