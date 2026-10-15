#include "push.hpp"

#include <cmath>
#include <cstdlib>

namespace {
bool close(float a, float b) { return std::abs(a - b) < .00002F; }
}

int main() {
  using namespace pdfcube;
  for (const int direction : {0, 90, 180, 270}) {
    float previous_contact = -2;
    for (int step = 0; step <= 100; ++step) {
      const auto pose = push_pose(step / 100.0, direction);
      if (pose.contact + .00001F < previous_contact ||
          pose.outgoing_compression < -.00001F || pose.outgoing_compression > .10501F ||
          pose.incoming_compression < -.00001F || pose.incoming_compression > .02501F ||
          pose.outgoing_stretch < .99999F || pose.outgoing_stretch > 1.04001F)
        return EXIT_FAILURE;
      previous_contact = pose.contact;
      for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(pose.outgoing.value[i]) ||
            !std::isfinite(pose.incoming.value[i])) return EXIT_FAILURE;
      }
    }
  }

  const auto start = push_pose(0, 0);
  const auto middle = push_pose(.5, 0);
  const auto end = push_pose(1, 0);
  if (!close(start.outgoing.value[12], 0) || !close(start.incoming.value[12], -2) ||
      !close(end.outgoing.value[12], 2) || !close(end.incoming.value[12], 0) ||
      middle.outgoing_compression <= middle.incoming_compression ||
      middle.outgoing_stretch <= 1 || close(middle.wobble, 0)) return EXIT_FAILURE;

  if (push_direction(-90) != 0 || push_direction(45) != 0 ||
      push_direction(90) != 90 || push_direction(270) != 270) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
