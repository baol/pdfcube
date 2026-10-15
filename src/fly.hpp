#pragma once

#include "math.hpp"
#include <algorithm>
#include <numbers>
#include <cmath>

namespace pdfcube {

struct FlyPose {
  Matrix model;
  float shade;
};

inline float fly_start_scale(double value) {
  return std::isfinite(value) && value > 0 && value <= 1
      ? static_cast<float>(value) : 1.0F;
}

inline FlyPose fly_pose(double progress, bool inward, int degrees, double start_scale) {
  constexpr float pi = std::numbers::pi_v<float>;
  const float p = static_cast<float>(std::clamp(progress, 0.0, 1.0));
  const float eased = p * p * (3 - 2 * p);
  const float radians = static_cast<float>(degrees) * pi / 180;
  const float dx = std::cos(radians);
  const float dy = std::sin(radians);
  // A gust brings the leaf into view promptly, then leaves ample time for a
  // quiet landing. The cubic is exactly reversible for outward motion.
  const float flight = inward ? std::pow(1 - p, 3) : std::pow(p, 3);
  const float distance = (inward ? -2.4F : 2.4F) * flight;
  const float arc = .32F * std::sin(pi * p);
  const float bank = (inward ? 1.0F : -1.0F) * .16F * std::sin(pi * p);
  const float flutter = (inward ? 1.0F : -1.0F) * .055F *
                        std::sin(2 * pi * p) * std::sin(pi * p);
  const float initial = fly_start_scale(start_scale);
  const float size = inward ? initial + (1 - initial) * eased
                            : 1 - (1 - initial) * eased;
  const Matrix tilt = rotate_z(flutter) * rotate_z(radians) *
                      rotate_y(bank) * rotate_z(-radians);
  return {translate(dx * distance, dy * distance, arc) * tilt * scale(size, size, 1),
          .82F + .18F * std::cos(bank)};
}

}  // namespace pdfcube
