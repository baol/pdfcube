#include "zoom.hpp"
#include "math.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

int main() {
  using pdfcube::ZoomRegion;
  struct Expected { ZoomRegion region; double x; double y; double scale; };
  // Page coordinates recovered from the original camera positions and GLU
  // projection at frame 29. Test framing, not just stored offset constants.
  const std::array cases{
      Expected{ZoomRegion::upper_left, -.494, .2833333333, 2.058984},
      Expected{ZoomRegion::upper_right, .494, .2833333333, 2.058984},
      Expected{ZoomRegion::lower_left, -.494, -.4766666667, 2.058984},
      Expected{ZoomRegion::lower_right, .494, -.4766666667, 2.058984},
      Expected{ZoomRegion::center, 0, -.1444, 1.505831}};
  const auto near = [](double a, double b) { return std::abs(a - b) < 0.00001; };
  for (const auto& expected : cases) {
    const auto start = pdfcube::legacy_zoom_pose(expected.region, 0);
    const auto end = pdfcube::legacy_zoom_pose(expected.region, 1);
    const auto matrix = pdfcube::translate(end.x, end.y, 0) *
                        pdfcube::scale(end.scale, end.scale, 1);
    if (!near(start.scale, 1) || !near(start.x, 0) || !near(start.y, 0) ||
        !near(end.scale, expected.scale) ||
        !near(matrix.value[0] * expected.x + matrix.value[12], 0) ||
        !near(matrix.value[5] * expected.y + matrix.value[13], 0)) {
      std::cerr << "Legacy zoom region framing changed\n";
      return EXIT_FAILURE;
    }
    // Independently evaluate the original GLU projection at every old frame.
    for (int frame = 0; frame < 30; ++frame) {
      const auto pose = pdfcube::legacy_zoom_pose(expected.region, frame / 29.0);
      const double angle = expected.region == ZoomRegion::center
          ? 30 + 7 * (1 + std::cos(frame * std::numbers::pi / 30))
          : 21 + 23 * std::cos(frame * std::numbers::pi / 60);
      const auto projection = pdfcube::perspective(
          angle * std::numbers::pi / 180, 1, .5, 10);
      const double projected_scale = projection.value[0] *
                                     std::tan(22 * std::numbers::pi / 180);
      if (!near(pose.scale, projected_scale) ||
          !near(-pose.x / pose.scale, expected.x * frame / 29) ||
          !near(-pose.y / pose.scale, expected.y * frame / 29)) {
        std::cerr << "Legacy zoom animation trajectory changed\n";
        return EXIT_FAILURE;
      }
    }
  }
}
