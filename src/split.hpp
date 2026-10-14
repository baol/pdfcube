#pragma once

#include "math.hpp"
#include <algorithm>

namespace pdfcube {

inline float split_angle(double progress, bool inward) {
  const float t = static_cast<float>(std::clamp(inward ? 1 - progress : progress, 0.0, 1.0));
  return std::atan(4.0F) * t * t * (3 - 2 * t);
}

inline Matrix split_door(bool vertical, int panel, float angle) {
  const float side = panel == 0 ? -1.0F : 1.0F;
  return vertical
      ? translate(side, 0, 0) * rotate_y(side * angle) *
        translate(-side / 2, 0, 0) * scale(.5F, 1, 1)
      : translate(0, -side, 0) * rotate_x(side * angle) *
        translate(0, side / 2, 0) * scale(1, .5F, 1);
}

}  // namespace pdfcube
