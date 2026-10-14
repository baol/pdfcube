#include "fly.hpp"
#include <cstdlib>

namespace {
bool close(float a, float b) { return std::abs(a - b) < .00002F; }
}

int main() {
  using namespace pdfcube;
  for (int direction = 0; direction < 360; direction += 45) {
    const auto landed = fly_pose(1, true, direction, .35);
    const auto departed = fly_pose(0, false, direction, .35);
    for (int i = 0; i < 16; ++i) {
      if (!close(landed.model.value[i], Matrix::identity().value[i]) ||
          !close(departed.model.value[i], Matrix::identity().value[i])) return EXIT_FAILURE;
    }
    float previous = 10;
    for (int step = 0; step <= 100; ++step) {
      const double progress = step / 100.0;
      const auto incoming = fly_pose(progress, true, direction, .35);
      const auto reverse = fly_pose(1 - progress, false, direction + 180, .35);
      for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(incoming.model.value[i]) ||
            !close(incoming.model.value[i], reverse.model.value[i])) return EXIT_FAILURE;
      }
      const float distance = std::hypot(incoming.model.value[12], incoming.model.value[13]);
      if (distance > previous + .00001F || incoming.model.value[14] < -.00001F ||
          incoming.model.value[14] > .32001F) return EXIT_FAILURE;
      previous = distance;
    }
  }
  if (!close(fly_start_scale(0), 1) || !close(fly_start_scale(-1), 1) ||
      !close(fly_start_scale(2), 1) || !close(fly_start_scale(.35), .35F))
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
