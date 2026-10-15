#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pdfcube {

struct ShardVertex {
  float x;
  float y;
  float z;
  float u;
  float v;
};

struct GlassShard {
  std::array<ShardVertex, 3> vertices;
  float delay;
  float direction_x;
  float direction_y;
  float spin;
  float tilt_x;
  float tilt_y;
};

inline float shard_hash(std::uint32_t value) {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return static_cast<float>(value & 0xffffU) / 65535.0F;
}

inline const std::vector<GlassShard>& glass_shards() {
  static const std::vector<GlassShard> shards = [] {
    constexpr int columns = 10;
    constexpr int rows = 6;
    std::array<std::array<std::array<float, 2>, columns + 1>, rows + 1> points{};
    for (int row = 0; row <= rows; ++row) {
      for (int column = 0; column <= columns; ++column) {
        float x = -1.0F + 2.0F * column / columns;
        float y = -1.0F + 2.0F * row / rows;
        if (column > 0 && column < columns)
          x += (shard_hash(row * 97U + column * 31U) - .5F) * .09F;
        if (row > 0 && row < rows)
          y += (shard_hash(row * 53U + column * 71U + 11U) - .5F) * .12F;
        points[row][column] = {x, y};
      }
    }

    std::vector<GlassShard> result;
    result.reserve(columns * rows * 4);
    for (int row = 0; row < rows; ++row) {
      for (int column = 0; column < columns; ++column) {
        const auto& bottom_left = points[row][column];
        const auto& bottom_right = points[row][column + 1];
        const auto& top_right = points[row + 1][column + 1];
        const auto& top_left = points[row + 1][column];
        const std::uint32_t cell = static_cast<std::uint32_t>(row * columns + column);
        const std::array<float, 2> center = {
            (bottom_left[0] + bottom_right[0] + top_right[0] + top_left[0]) / 4 +
                (shard_hash(cell * 19U + 3U) - .5F) * .07F,
            (bottom_left[1] + bottom_right[1] + top_right[1] + top_left[1]) / 4 +
                (shard_hash(cell * 23U + 5U) - .5F) * .09F,
        };
        const std::array<std::array<float, 2>, 5> ring = {
            bottom_left, bottom_right, top_right, top_left, bottom_left};
        for (int triangle = 0; triangle < 4; ++triangle) {
          const std::uint32_t id = cell * 4U + static_cast<std::uint32_t>(triangle);
          const auto vertex = [](const std::array<float, 2>& point) {
            return ShardVertex{point[0], point[1], 0, (point[0] + 1) / 2,
                               (1 - point[1]) / 2};
          };
          const float centroid_x =
              (ring[triangle][0] + ring[triangle + 1][0] + center[0]) / 3;
          const float centroid_y =
              (ring[triangle][1] + ring[triangle + 1][1] + center[1]) / 3;
          // A single central impact sends a fast fracture wave outwards.
          float direction_x = centroid_x;
          float direction_y = centroid_y;
          const float length = std::hypot(direction_x, direction_y);
          if (length > .001F) {
            direction_x /= length;
            direction_y /= length;
          }
          result.push_back({
              {vertex(ring[triangle]), vertex(ring[triangle + 1]), vertex(center)},
              .025F * length, direction_x, direction_y,
              (shard_hash(id * 43U + 13U) - .5F) * 1.2F,
              direction_y * (2.4F + shard_hash(id * 47U + 17U)),
              -direction_x * (2.4F + shard_hash(id * 47U + 17U))});
        }
      }
    }
    return result;
  }();
  return shards;
}

inline void update_glass_shard_vertices(double progress, std::vector<ShardVertex>& result) {
  const float position = std::clamp(static_cast<float>(progress), 0.0F, 1.0F);
  const float crack = std::clamp(position / .10F, 0.0F, 1.0F);
  result.clear();
  const auto& shards = glass_shards();
  result.reserve(shards.size() * 3);
  for (const GlassShard& shard : shards) {
    float center_x = 0;
    float center_y = 0;
    for (const ShardVertex& vertex : shard.vertices) {
      center_x += vertex.x / 3;
      center_y += vertex.y / 3;
    }
    const float raw = std::clamp((position - shard.delay) /
                                     std::max(.01F, 1 - shard.delay),
                                 0.0F, 1.0F);
    // Immediate impulse followed by drag; gravity acts after the impact.
    const float motion = (1 - std::exp(-.8F * raw)) / (1 - std::exp(-.8F));
    // The impact supplies a fixed angular momentum about an axis tangent to
    // the radial flight direction. Rotate rigidly about that axis throughout
    // flight, independently of translational drag.
    const float speed = std::sqrt(shard.tilt_x * shard.tilt_x +
                                  shard.tilt_y * shard.tilt_y + shard.spin * shard.spin);
    const float axis_x = shard.tilt_x / speed;
    const float axis_y = shard.tilt_y / speed;
    const float axis_z = shard.spin / speed;
    const float angle = speed * raw;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float separation = .0045F * crack + .018F * motion;
    const float travel = (1.15F + .35F * std::hypot(center_x, center_y)) * motion;
    const float fall = .65F * raw * raw;
    for (const ShardVertex& source : shard.vertices) {
      const float local_x = source.x - center_x;
      const float local_y = source.y - center_y;
      const float dot = axis_x * local_x + axis_y * local_y;
      const float turned_x = local_x * cosine - axis_z * local_y * sine +
                             axis_x * dot * (1 - cosine);
      const float turned_y = local_y * cosine + axis_z * local_x * sine +
                             axis_y * dot * (1 - cosine);
      const float turned_z = (axis_x * local_y - axis_y * local_x) * sine +
                             axis_z * dot * (1 - cosine);
      result.push_back({
          center_x + turned_x + shard.direction_x * (separation + travel),
          center_y + turned_y + shard.direction_y * (separation + travel) - fall,
          .72F * motion + turned_z, source.u, source.v});
    }
  }
}

}  // namespace pdfcube
