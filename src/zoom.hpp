#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace pdfcube {

enum class ZoomRegion { none, upper_left, upper_right, lower_left, lower_right, center };

struct ZoomPose {
  float scale{1.0F};
  float x{0.0F};
  float y{0.0F};
};

// Legacy pdfcube.cc sampled frames 0..29, not 0..30. Preserve its final
// framing and interpolate the camera/FOV samples for modern frame clocks.
inline ZoomPose legacy_zoom_pose(ZoomRegion region, double progress) {
  if (region == ZoomRegion::none) return {};
  const double frame = std::clamp(progress, 0.0, 1.0) * 29.0;
  const int first = static_cast<int>(frame);
  const int second = std::min(first + 1, 29);
  const auto fov = [region](int i) {
    if (region == ZoomRegion::center)
      return 30.0 + 7.0 * (1.0 + std::cos(i * std::numbers::pi / 30.0));
    return 21.0 + 23.0 * std::cos(i * std::numbers::pi / 60.0);
  };
  const double angle = fov(first) + (fov(second) - fov(first)) * (frame - first);
  // Normalize the old 44-degree full-page projection to today's exact fit.
  const double scale = std::tan(22.0 * std::numbers::pi / 180.0) /
                       std::tan(angle * std::numbers::pi / 360.0);
  const double movement = 0.38 * frame / 29.0;
  double camera_x = 0.0;
  double camera_y = -movement * 0.38;
  if (region != ZoomRegion::center) {
    const bool left = region == ZoomRegion::upper_left || region == ZoomRegion::lower_left;
    const bool upper = region == ZoomRegion::upper_left || region == ZoomRegion::upper_right;
    camera_x = (left ? -1.3 : 1.3) * movement;
    camera_y = (upper ? movement : -movement) - 0.1 * frame / 30.0;
  }
  // Camera positions are in page coordinates; translate AFTER scaling.
  return {static_cast<float>(scale), static_cast<float>(-camera_x * scale),
          static_cast<float>(-camera_y * scale)};
}

}  // namespace pdfcube
