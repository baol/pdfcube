#include "timing.hpp"
#include "performance.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>
#include <sstream>

int main() {
  struct Scheduler {
    std::vector<std::pair<std::string,unsigned>> calls;
    void cancel(unsigned id) { calls.emplace_back("cancel",id); }
    unsigned idle() { calls.emplace_back("idle",0); return 11; }
    unsigned after(unsigned milliseconds) { calls.emplace_back("after",milliseconds); return 12; }
  } scheduler;
  assert(pdfcube::schedule_advance(scheduler,0,4000,false) == 12);
  assert((scheduler.calls == std::vector<std::pair<std::string,unsigned>>{{"after",4000}}));
  scheduler.calls.clear();
  assert(pdfcube::schedule_advance(scheduler,12,1250,false) == 12);
  assert((scheduler.calls == std::vector<std::pair<std::string,unsigned>>{{"cancel",12},{"after",1250}}));
  scheduler.calls.clear();
  assert(pdfcube::schedule_advance(scheduler,12,0,false) == 0);
  assert((scheduler.calls == std::vector<std::pair<std::string,unsigned>>{{"cancel",12}}));
  scheduler.calls.clear();
  assert(pdfcube::schedule_advance(scheduler,0,0,true) == 11);
  assert((scheduler.calls == std::vector<std::pair<std::string,unsigned>>{{"idle",0}}));
  scheduler.calls.clear();
  assert(pdfcube::schedule_advance(scheduler,0,-1,false) == 0 && scheduler.calls.empty());
  for (const auto duration : {750000,1250000,2000000}) {
    assert(pdfcube::timed_progress(100,100,duration) == 0);
    assert(pdfcube::timed_progress(99,100,duration) == 0);
    assert(pdfcube::timed_progress(100+duration/2,100,duration) == .5);
    assert(pdfcube::timed_progress(100+duration,100,duration) == 1);
    assert(pdfcube::timed_progress(101+duration,100,duration) == 1);
  }
  assert(pdfcube::timed_progress(100,100,0) == 1);
  std::ostringstream output;
  const pdfcube::FrameTiming frames[]{{1000000,16667,42,3,.5,false}};
  pdfcube::write_frame_timings(output,frames);
  assert(output.str() ==
      "frame_time_us,frame_interval_us,cpu_submit_us,transition,progress,preparing\n"
      "1000000,16667,42,3,0.5,0\n");
}
