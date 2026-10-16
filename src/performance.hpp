#pragma once

#include <cstdint>
#include <ostream>
#include <span>

namespace pdfcube {
struct FrameTiming {
  std::int64_t frame_time_us;
  std::int64_t frame_interval_us;
  std::int64_t cpu_submit_us;
  int transition;
  double progress;
  bool preparing;
};
inline void write_frame_timings(std::ostream& output, std::span<const FrameTiming> frames) {
  output << "frame_time_us,frame_interval_us,cpu_submit_us,transition,progress,preparing\n";
  for (const auto& frame : frames)
    output << frame.frame_time_us << ',' << frame.frame_interval_us << ','
           << frame.cpu_submit_us << ',' << frame.transition << ','
           << frame.progress << ',' << frame.preparing << '\n';
}
}
