#pragma once

#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace pdfcube {

struct GlitterPiece {
  Matrix transform;
  float u, v, width, height;
  float glow;
  float opacity;
};

inline int glitter_direction(int degrees) {
  return degrees == 270 || degrees == 315 ? degrees : 0;
}

inline std::vector<GlitterPiece> glitter_pieces(double progress, int degrees) {
  constexpr int columns = 24, rows = 14;
  constexpr float pi = 3.14159265358979323846F;
  const float p = static_cast<float>(std::clamp(progress, 0.0, 1.0));
  const int direction = glitter_direction(degrees);
  const float dx = direction == 270 ? 0 : direction == 315 ? .70710678F : 1;
  const float dy = direction == 0 ? 0 : direction == 315 ? -.70710678F : -1;
  std::vector<GlitterPiece> result;
  result.reserve(columns * rows);
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      const float u = static_cast<float>(column) / columns;
      const float v = static_cast<float>(row) / rows;
      const float x = 2 * (u + .5F / columns) - 1;
      const float y = 1 - 2 * (v + .5F / rows);
      const float sweep = direction == 0 ? u : direction == 270 ? v : (u + v) / 2;
      // A fixed per-cell variation breaks the front into a narrow glitter band.
      const float variation = static_cast<float>((column * 73 + row * 37) % 101) / 100;
      const float release = .72F * sweep + .035F * variation;
      const float age = std::clamp((p - release) / .24F, 0.0F, 1.0F);
      if (p >= 1 || age >= 1) continue;
      const float flight = .32F * age;
      const float sideways = (variation - .5F) * .20F * age;
      const float size = 1 - age;
      const float angle = (variation - .5F) * 4 * pi * age;
      result.push_back({
          translate(x + dx * flight - dy * sideways,
                    y + dy * flight + dx * sideways - .08F * age * age, 0) *
              rotate_z(angle) * rotate_y(age * (2 + variation)) *
              scale(size / columns, size / rows, 1),
          u, v, 1.0F / columns, 1.0F / rows,
          p >= release ? std::max(0.0F, 1 - age * 5) : 0,
          1 - age});
    }
  }
  return result;
}

}  // namespace pdfcube
