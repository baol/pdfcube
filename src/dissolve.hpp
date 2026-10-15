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
              (shard_hash(id * 43U + 13U) - .5F) * 2.2F,
              (shard_hash(id * 47U + 17U) - .5F) * 1.6F,
              (shard_hash(id * 59U + 23U) - .5F) * 1.6F});
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
    const float motion = (1 - std::exp(-3 * raw)) / (1 - std::exp(-3.0F));
    const float angle = shard.spin * motion;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float tilt_x = shard.tilt_x * motion;
    const float tilt_y = shard.tilt_y * motion;
    const float cosine_x = std::cos(tilt_x);
    const float sine_x = std::sin(tilt_x);
    const float cosine_y = std::cos(tilt_y);
    const float sine_y = std::sin(tilt_y);
    const float separation = .0045F * crack + .018F * motion;
    const float travel = 3.2F * motion;
    const float fall = .65F * raw * raw;
    const float shrink = 1 - .14F * motion;
    for (const ShardVertex& source : shard.vertices) {
      const float local_x = (source.x - center_x) * shrink;
      const float local_y = (source.y - center_y) * shrink;
      const float spun_x = local_x * cosine - local_y * sine;
      const float spun_y = local_x * sine + local_y * cosine;
      const float pitched_y = spun_y * cosine_x;
      const float pitched_z = spun_y * sine_x;
      const float turned_x = spun_x * cosine_y + pitched_z * sine_y;
      const float turned_z = -spun_x * sine_y + pitched_z * cosine_y;
      result.push_back({
          center_x + turned_x + shard.direction_x * (separation + travel),
          center_y + pitched_y + shard.direction_y * (separation + travel) - fall,
          .72F * motion + turned_z, source.u, source.v});
    }
  }
}

}  // namespace pdfcube
