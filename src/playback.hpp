#pragma once

#include "renderer.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace pdfcube {

inline Transition transition_from_name(const std::string& effect) {
  if (effect == "c") return Transition::cube_forward;
  if (effect == "blinds") return Transition::blinds;
  if (effect == "split") return Transition::split;
  if (effect == "cover") return Transition::cover;
  if (effect == "uncover") return Transition::uncover;
  if (effect == "fly") return Transition::fly;
  if (effect == "push") return Transition::push;
  if (effect == "dissolve") return Transition::dissolve;
  if (effect == "glitter") return Transition::glitter;
  return Transition::switch_page;
}

inline Transition transition_from_page(const PageInfo& page) {
  if (page.cube_transition) return Transition::cube_forward;
  switch (page.transition_type) {
    case POPPLER_PAGE_TRANSITION_BLINDS: return Transition::blinds;
    case POPPLER_PAGE_TRANSITION_SPLIT: return Transition::split;
    case POPPLER_PAGE_TRANSITION_COVER: return Transition::cover;
    case POPPLER_PAGE_TRANSITION_UNCOVER: return Transition::uncover;
    case POPPLER_PAGE_TRANSITION_FLY: return Transition::fly;
    case POPPLER_PAGE_TRANSITION_PUSH: return Transition::push;
    case POPPLER_PAGE_TRANSITION_DISSOLVE: return Transition::dissolve;
    case POPPLER_PAGE_TRANSITION_GLITTER: return Transition::glitter;
    default: return Transition::switch_page;
  }
}

inline void apply_transition_metadata(FrameState& frame, const PageInfo& page) {
  frame.vertical_panels = page.alignment == POPPLER_PAGE_TRANSITION_VERTICAL;
  frame.split_inward = page.direction == POPPLER_PAGE_TRANSITION_INWARD;
  frame.direction_degrees = page.angle;
  frame.fly_scale = page.scale;
  frame.fly_inward = page.direction == POPPLER_PAGE_TRANSITION_INWARD;
}

// Apply the program to both animation kinds and dwell durations. Shared by
// the application and direct frame tests, so tests exercise actual dispatch.
inline void apply_program(const std::string& program, std::vector<int>& durations,
                          std::vector<Transition>& transitions, std::vector<bool>& cubes) {
  if (program.empty() || durations.empty()) return;
  std::istringstream input(program);
  std::string duration, effect;
  std::size_t page = 0;
  int last_duration = 0;
  Transition last_transition = Transition::switch_page;
  while (page < durations.size() && std::getline(input,duration,':')) {
    if (duration.empty()) break;
    last_duration = std::max(0,static_cast<int>(std::stod(duration)*1000));
    durations[page] = last_duration;
    if (std::getline(input,effect,':')) last_transition = transition_from_name(effect);
    const auto next = (page+1)%durations.size();
    transitions[next] = last_transition;
    cubes[next] = last_transition == Transition::cube_forward;
    ++page;
  }
  for (; page < durations.size(); ++page) {
    durations[page] = last_duration;
    const auto next = (page+1)%durations.size();
    transitions[next] = last_transition;
    cubes[next] = last_transition == Transition::cube_forward;
  }
}

}  // namespace pdfcube
