#include "page_flip.hpp"
#include <cstdlib>

namespace {
std::array<float, 3> point(const pdfcube::Matrix& m, int axis, float edge) {
  return {m.value[axis * 4] * edge + m.value[12],
          m.value[axis * 4 + 1] * edge + m.value[13],
          m.value[axis * 4 + 2] * edge + m.value[14]};
}
bool close(float a, float b) { return std::abs(a - b) < .00002F; }
}

int main() {
  using namespace pdfcube;
  for (int direction : {0, 90, 180, 270}) {
    const int axis = direction == 0 || direction == 180 ? 0 : 1;
    const float hinge = direction == 0 || direction == 90 ? 1.0F : -1.0F;
    for (int step = 0; step <= 100; ++step) {
      const auto strips = page_flip(step / 100.0, false, direction);
      const auto reversed = page_flip(1 - step / 100.0, true, (direction + 180) % 360);
      std::array<float, 3> previous{};
      previous[axis] = hinge;
      for (int i = 0; i < flip_strips; ++i) {
        const auto first = point(strips[i].model, axis, hinge);
        const auto last = point(strips[i].model, axis, -hinge);
        for (int row = 0; row < 3; ++row) {
          // A connected sheet: strips meet exactly, with a fixed outer hinge.
          if (!close(first[row], previous[row])) return EXIT_FAILURE;
        }
        for (int j = 0; j < 16; ++j) {
          if (!std::isfinite(strips[i].model.value[j]) ||
              !close(strips[i].model.value[j], reversed[i].model.value[j])) return EXIT_FAILURE;
        }
        if (first[2] < -.00001F || last[2] > 2.00001F) return EXIT_FAILURE;
        if (step == 0) {
          // The flat strip maps back to its original, unmirrored page region.
          const float uv_center = strips[i].uv[axis] + strips[i].uv[axis + 2] / 2;
          const float position = axis == 0 ? 2 * uv_center - 1 : 1 - 2 * uv_center;
          if (!close(strips[i].model.value[12 + axis], position) ||
              !close(strips[i].shade, 1)) return EXIT_FAILURE;
        }
        if (step == 100 && !close(4 * last[axis] / (4 - last[2]), hinge)) return EXIT_FAILURE;
        previous = last;
      }
    }
  }
  if (flip_direction(45) != 0 || flip_direction(-90) != 0 || flip_direction(360) != 0)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
