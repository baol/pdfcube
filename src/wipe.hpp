#pragma once

#include <algorithm>
#include <cmath>

namespace pdfcube {

inline int wipe_direction(int degrees) {
  return degrees == 90 || degrees == 180 || degrees == 270 ? degrees : 0;
}

struct WipeClip { int x, y, width, height; };
struct WipeGeometry {
  WipeClip outgoing;
  bool horizontal;
  float center;  // Clip-space blade center along its travel axis.
  float half_width;
  int direction;
};

// The blade travels just beyond both edges, including its bevel. Pixel-aligned
// clipping leaves no partially sampled seam between independently fitted pages.
inline WipeGeometry wipe_geometry(double progress, int degrees, int width, int height) {
  const int direction = wipe_direction(degrees);
  const bool horizontal = direction == 0 || direction == 180;
  const bool positive = direction == 0 || direction == 90;
  const int extent = horizontal ? width : height;
  const float half_pixels = std::max(2.0F, .014F * std::min(width, height));
  const float margin = 2 * half_pixels / extent;
  const double p = std::clamp(progress, 0.0, 1.0);
  const double eased = p * p * (3 - 2 * p);
  const double front = -margin + (1 + 2 * margin) * eased;
  const int swept = std::clamp(static_cast<int>(std::lround(front * extent)), 0, extent);
  WipeClip clip{0, 0, width, height};
  if (horizontal) {
    clip.x = positive ? swept : 0;
    clip.width = width - swept;
  } else {
    clip.y = positive ? swept : 0;
    clip.height = height - swept;
  }
  return {clip, horizontal, static_cast<float>((positive ? 1 : -1) * (2 * front - 1)),
          2 * half_pixels / extent, direction};
}

}  // namespace pdfcube
