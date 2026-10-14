#include "split.hpp"
#include <cstdlib>
#include <cmath>

int main() {
  using namespace pdfcube;
  const auto close = [](float a, float b) { return std::abs(a - b) < .00001F; };
  float previous = -1;
  for (int step = 0; step <= 100; ++step) {
    const double p = step / 100.0;
    const float angle = split_angle(p, false);
    if (angle < previous || !close(angle, split_angle(1 - p, true))) return EXIT_FAILURE;
    previous = angle;
    for (bool vertical : {false, true}) {
      for (int panel : {0, 1}) {
        const float side = panel == 0 ? -1.0F : 1.0F;
        const Matrix door = split_door(vertical, panel, angle);
        const int axis = vertical ? 0 : 1;
        const float hinge = vertical ? side : -side;
        // The outer edge stays fixed in all three dimensions.
        for (int row = 0; row < 3; ++row) {
          const float point = door.value[axis * 4 + row] * hinge + door.value[12 + row];
          if (!close(point, row == axis ? hinge : 0)) return EXIT_FAILURE;
        }
        if (step == 100) {
          const float inner = -hinge;
          const float x = door.value[axis * 4 + axis] * inner + door.value[12 + axis];
          const float z = door.value[axis * 4 + 2] * inner + door.value[14];
          // Fully open projects to the hinge: no residual panel or final snap.
          if (!close(4 * x / (4 - z), hinge)) return EXIT_FAILURE;
        }
      }
    }
  }
  if (!close(split_angle(-1, false), 0) ||
      !close(split_angle(2, false), std::atan(4.0F))) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
