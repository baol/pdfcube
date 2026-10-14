#pragma once

#include "math.hpp"
#include <algorithm>

namespace pdfcube {

constexpr int flip_strips = 64;
constexpr float flip_camera = 4;

// PDF Cover/Uncover admit the four cardinal directions; malformed values
// fall back to the PDF default (left to right), not arbitrary rotations.
inline int flip_direction(int degrees) {
  return degrees == 90 || degrees == 180 || degrees == 270 ? degrees : 0;
}

struct FlipStrip {
  Matrix model;
  std::array<float, 4> uv;
  float shade;
};

inline std::array<FlipStrip, flip_strips> page_flip(double progress, bool cover, int degrees) {
  const int direction = flip_direction(degrees);
  const bool horizontal = direction == 0 || direction == 180;
  const float travel = direction == 0 || direction == 90 ? 1.0F : -1.0F;
  const float hinge = cover ? -travel : travel;
  const float t = static_cast<float>(std::clamp(cover ? 1 - progress : progress, 0.0, 1.0));
  const float opened = t * t * (3 - 2 * t);
  constexpr float pi = 3.14159265358979323846F;
  constexpr float fraction = 1.0F / flip_strips;
  float distance = 0;
  float depth = 0;
  std::array<FlipStrip, flip_strips> strips;
  for (int i = 0; i < flip_strips; ++i) {
    const float u = (i + .5F) * fraction;
    // Integrate a gently curved sheet with constant arc length. At either
    // endpoint curvature vanishes. Fully open, the sheet projects edge-on.
    const float angle = std::atan(flip_camera) *
        (opened + .25F * std::sin(pi * opened) * (2 * u - 1));
    const float dx = 2 * fraction * std::cos(angle);
    const float dz = 2 * fraction * std::sin(angle);
    const float center = hinge * (1 - distance - dx / 2);
    const float z = depth + dz / 2;
    const Matrix model = horizontal
        ? translate(center, 0, z) * rotate_y(hinge * angle) * scale(fraction, 1, 1)
        : translate(0, center, z) * rotate_x(-hinge * angle) * scale(1, fraction, 1);
    const bool ascending_uv = horizontal ? hinge < 0 : hinge > 0;
    const float start = (ascending_uv ? i : flip_strips - i - 1) * fraction;
    strips[i] = {model, horizontal ? std::array<float, 4>{start, 0, fraction, 1}
                                 : std::array<float, 4>{0, start, 1, fraction},
                 .7F + .3F * std::cos(angle)};
    distance += dx;
    depth += dz;
  }
  return strips;
}

}  // namespace pdfcube
