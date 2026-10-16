#pragma once

#include "playback.hpp"

#include <cmath>
#include <functional>

namespace pdfcube {

// Fixed frame steps: no GUI clock, timers, or real-time pauses. The same
// sequence is used by the application and the shared headless renderer tests.
template <class Emit>
void record_presentation(Document& document, Renderer& renderer, int fps,
                         int width, int height, Emit emit) {
  FrameState state;
  auto render = [&](int repeats) {
    renderer.render(width, height, state);
    emit(renderer.read_rgb(width, height), repeats);
  };
  auto dwell = [&] {
    const auto& page = document.page_info(state.current_page);
    const double seconds = page.duration_ms > 0 ? page.duration_ms / 1000.0 : 5.0;
    render(std::max(1, static_cast<int>(std::round(seconds * fps))));
  };
  dwell();
  for (int page = 1; page <= document.page_count(); ++page) {
    state.next_page = page % document.page_count();
    const auto& info = document.page_info(state.next_page);
    state.transition = transition_from_page(info);
    apply_transition_metadata(state, info);
    if (state.transition != Transition::switch_page && info.transition_seconds > 0) {
      const int frames = std::max(1, static_cast<int>(std::round(info.transition_seconds * fps)));
      for (int frame = 0; frame < frames; ++frame) {
        state.progress = frame / static_cast<double>(frames);
        render(1);
      }
    }
    state.current_page = state.next_page;
    state.transition = Transition::none;
    if (page < document.page_count()) dwell();
  }
  render(1);
}

}  // namespace pdfcube
