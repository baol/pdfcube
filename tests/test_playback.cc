#include "playback.hpp"
#include "timing.hpp"

#include <cassert>
#include <vector>

int main() {
  const std::pair<PopplerPageTransitionType,const char*> kinds[]{
      {POPPLER_PAGE_TRANSITION_BLINDS,"blinds"}, {POPPLER_PAGE_TRANSITION_SPLIT,"split"},
      {POPPLER_PAGE_TRANSITION_COVER,"cover"}, {POPPLER_PAGE_TRANSITION_UNCOVER,"uncover"},
      {POPPLER_PAGE_TRANSITION_FLY,"fly"}, {POPPLER_PAGE_TRANSITION_PUSH,"push"},
      {POPPLER_PAGE_TRANSITION_DISSOLVE,"dissolve"}};
  for (const auto& [kind,name] : kinds) {
    pdfcube::PageInfo page;
    page.transition_type=kind;
    page.alignment=POPPLER_PAGE_TRANSITION_VERTICAL;
    page.direction=POPPLER_PAGE_TRANSITION_OUTWARD;
    page.angle=135;
    page.scale=.35;
    const auto transition = pdfcube::transition_from_page(page);
    assert(transition == pdfcube::transition_from_name(name));
    pdfcube::FrameState frame;
    pdfcube::apply_transition_metadata(frame,page);
    assert(frame.vertical_panels && !frame.split_inward && !frame.fly_inward);
    assert(frame.direction_degrees == 135 && frame.fly_scale == .35F);
    page.cube_transition=true;
    assert(pdfcube::transition_from_page(page) == pdfcube::Transition::cube_forward);
  }
  std::vector<int> durations(4,4000);
  std::vector<pdfcube::Transition> transitions(4,pdfcube::Transition::none);
  std::vector<bool> cubes(4);
  pdfcube::apply_program("1.25:dissolve:4:c",durations,transitions,cubes);
  assert((durations == std::vector<int>{1250,4000,4000,4000}));
  assert(transitions[1] == pdfcube::Transition::dissolve && !cubes[1]);
  assert(transitions[2] == pdfcube::Transition::cube_forward && cubes[2]);
  assert(transitions[3] == transitions[2] && transitions[0] == transitions[2]);
  struct Scheduler {
    unsigned requested=0;
    void cancel(unsigned) { assert(false); }
    unsigned idle() { assert(false); return 0; }
    unsigned after(unsigned milliseconds) { requested=milliseconds; return 1; }
  } scheduler;
  assert(pdfcube::schedule_advance(scheduler,0,durations[0],false) == 1);
  assert(scheduler.requested == 1250);
  pdfcube::apply_program("0:p",durations,transitions,cubes);
  for (std::size_t i=0;i<durations.size();++i) {
    assert(durations[i] == 0 && transitions[i] == pdfcube::Transition::switch_page && !cubes[i]);
  }
}
