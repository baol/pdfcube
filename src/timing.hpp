#pragma once

#include <algorithm>
#include <cstdint>

namespace pdfcube {

inline double timed_progress(std::int64_t now, std::int64_t started, std::int64_t duration) {
  if (duration <= 0) return 1;
  return std::clamp((now-started)/static_cast<double>(duration),0.0,1.0);
}

// The backend owns callback dispatch. Tests use a recording backend and a
// synthetic clock; no test waits for the requested pause to elapse.
template<class Scheduler>
unsigned schedule_advance(Scheduler& scheduler, unsigned previous, int delay_ms,
                          bool allow_immediate) {
  if (previous != 0) scheduler.cancel(previous);
  if (delay_ms <= 0) return allow_immediate ? scheduler.idle() : 0;
  return scheduler.after(static_cast<unsigned>(delay_ms));
}

}  // namespace pdfcube
