#pragma once

#include "math.hpp"

#include <algorithm>
#include <cmath>

namespace pdfcube {

struct PushPose {
  Matrix outgoing;
  Matrix incoming;
  float compression;
  float contact;
};

// PDF Push uses the four cardinal directions. Treat malformed values like the
// PDF default (left to right), matching Cover and Uncover.
inline int push_direction(int degrees) {
  return degrees == 90 || degrees == 180 || degrees == 270 ? degrees : 0;
}

inline PushPose push_pose(double progress, int degrees) {
  constexpr float pi = 3.14159265358979323846F;
  const float p = static_cast<float>(std::clamp(progress, 0.0, 1.0));
  const float eased = p * p * (3 - 2 * p);
  const float radians = push_direction(degrees) * pi / 180;
  const float dx = std::cos(radians);
  const float dy = std::sin(radians);

  // The shared contact plane travels across the screen. Both padded blocks
  // compress toward that plane, then recover completely at either endpoint.
  const float impact = std::pow(std::sin(pi * p), 2);
  const float half_extent = 1 - .075F * impact;
  const float contact = -1 + 2 * eased;
  const float outgoing_center = contact + half_extent;
  const float incoming_center = contact - half_extent;
  const float bulge = .11F * impact;
  const float tilt = .055F * std::sin(pi * p);
  const Matrix direction = rotate_z(radians);
  const Matrix restore = rotate_z(-radians);
  const Matrix outgoing_shape =
      direction * rotate_y(-tilt) * scale(half_extent, 1, 1) * restore;
  const Matrix incoming_shape =
      direction * rotate_y(tilt) * scale(half_extent, 1, 1) * restore;
  return {
      translate(dx * outgoing_center, dy * outgoing_center, bulge) * outgoing_shape,
      translate(dx * incoming_center, dy * incoming_center, bulge) * incoming_shape,
      1 - half_extent,
      contact,
  };
}

}  // namespace pdfcube
