#pragma once

#include "math.hpp"

#include <algorithm>
#include <cmath>

namespace pdfcube {

struct PushPose {
  Matrix outgoing;
  Matrix incoming;
  float outgoing_compression;
  float incoming_compression;
  float outgoing_stretch;
  float wobble;
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

  // The shared contact plane travels across the screen. The outgoing block is
  // softer: it squashes along the impact axis, bulges across it, and rings once
  // after contact. The incoming block stays firmer. Every term vanishes at the
  // endpoints, keeping the static frames exact and replay deterministic.
  const float impact = std::pow(std::sin(pi * p), 2);
  const float settling = std::max(0.0F, (p - .35F) / .65F);
  const float wobble = .018F * std::sin(3 * pi * settling) *
                       std::pow(1 - settling, 2) * impact;
  const float outgoing_compression = .09F * impact + wobble;
  const float incoming_compression = .025F * impact;
  const float outgoing_half_extent = 1 - outgoing_compression;
  const float incoming_half_extent = 1 - incoming_compression;
  const float outgoing_stretch = 1 + .035F * impact - .3F * wobble;
  const float contact = -1 + 2 * eased;
  const float outgoing_center = contact + outgoing_half_extent;
  const float incoming_center = contact - incoming_half_extent;
  const float bulge = .11F * impact;
  const float tilt = .045F * std::sin(pi * p) + .45F * wobble;
  const Matrix direction = rotate_z(radians);
  const Matrix restore = rotate_z(-radians);
  const Matrix outgoing_shape =
      direction * rotate_y(-tilt) *
      scale(outgoing_half_extent, outgoing_stretch, 1) * restore;
  const Matrix incoming_shape =
      direction * rotate_y(tilt * .55F) * scale(incoming_half_extent, 1, 1) * restore;
  return {
      translate(dx * outgoing_center, dy * outgoing_center, bulge) * outgoing_shape,
      translate(dx * incoming_center, dy * incoming_center, bulge * .45F) * incoming_shape,
      outgoing_compression,
      incoming_compression,
      outgoing_stretch,
      wobble,
      contact,
  };
}

}  // namespace pdfcube
