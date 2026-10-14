#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace pdfcube {
constexpr int blinds_count = 12;
// The cascade occupies 22% of the transition; every slat lands by t=1.
inline float blinds_angle(int slat, double progress) {
  const double delay = .22 * slat / (blinds_count - 1);
  const double t = std::clamp((progress - delay) / .78, 0.0, 1.0);
  return static_cast<float>(std::numbers::pi * t * t * (3 - 2 * t));
}
}
