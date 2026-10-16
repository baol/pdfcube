#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace pdfcube {

struct ShardVertex {
  float x;
  float y;
  float z;
  float u;
  float v;
  float light{1};
};

struct GlassShard {
  std::vector<ShardVertex> vertices;
  float delay;
  float direction_x;
  float direction_y;
  float spin;
  float tilt_x;
  float tilt_y;
  float speed;
};

inline float shard_hash(std::uint32_t value) {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return static_cast<float>(value & 0xffffU) / 65535.0F;
}

inline std::array<float, 2> shard_center(const GlassShard& shard) {
  std::array<float, 2> center{};
  for (const auto& vertex : shard.vertices) {
    center[0] += vertex.x / shard.vertices.size();
    center[1] += vertex.y / shard.vertices.size();
  }
  return center;
}

inline float shard_radius(const GlassShard& shard) {
  const auto center = shard_center(shard);
  float radius = 0;
  for (const auto& vertex : shard.vertices)
    radius = std::max(radius, std::hypot(vertex.x - center[0], vertex.y - center[1]));
  return radius;
}

inline const std::vector<GlassShard>& glass_shards() {
  static const std::vector<GlassShard> shards = [] {
    // Shared radial cracks and irregular concentric cracks form a spiderweb.
    // Chips near the impact are small; outer splinters are larger and longer.
    constexpr int spokes = 16;
    constexpr std::array<float, 4> radii{.14F, .39F, .70F, 1};
    constexpr float pi = 3.14159265358979323846F;
    using Point = std::array<float, 2>;
    std::array<std::array<Point, spokes>, radii.size()> points{};
    for (int spoke = 0; spoke < spokes; ++spoke) {
      // Preserve corner rays so the intact mesh covers the whole square.
      const float jitter = spoke % 4 == 0 ? 0 :
          (shard_hash(spoke * 31U + 7U) - .5F) * .45F;
      const float angle = pi / 4 + (spoke + jitter) * (2 * pi / spokes);
      for (std::size_t ring = 0; ring < radii.size(); ++ring) {
        const bool outer = ring + 1 == radii.size();
        const float bend = outer ? 0 :
            (shard_hash(static_cast<std::uint32_t>(ring) * 41U + spoke * 71U) - .5F) * .16F;
        float x = std::cos(angle + bend), y = std::sin(angle + bend);
        if (outer && spoke % 4 == 0) {
          x = std::copysign(1.0F, x);
          y = std::copysign(1.0F, y);
        }
        // Interior cracks form broken circles, not nested rectangular grids.
        const float boundary = outer ? std::max(std::abs(x), std::abs(y)) : 1;
        const float radius = radii[ring] + (ring + 1 == radii.size() ? 0 :
            (shard_hash(static_cast<std::uint32_t>(ring) * 97U + spoke * 53U) - .5F) * .045F);
        points[ring][spoke] = {radius * x / boundary, radius * y / boundary};
      }
    }
    std::vector<GlassShard> result;
    result.reserve(spokes * radii.size());
    const auto add = [&](std::initializer_list<Point> polygon) {
      const auto id = static_cast<std::uint32_t>(result.size());
      const auto vertex = [](const Point& point) {
        return ShardVertex{point[0], point[1], 0, (point[0] + 1) / 2,
                           (1 - point[1]) / 2};
      };
      GlassShard shard{};
      for (const auto& point : polygon) shard.vertices.push_back(vertex(point));
      const auto center = shard_center(shard);
      const float x = center[0], y = center[1];
      const float distance = std::hypot(x, y);
      // Modest scatter breaks up perfect spokes without losing the common
      // impact direction or coherent angular momentum about a tangent axis.
      const float scatter = (shard_hash(id * 29U + 5U) - .5F) * .16F;
      const float direction_x = (x * std::cos(scatter) - y * std::sin(scatter)) / distance;
      const float direction_y = (x * std::sin(scatter) + y * std::cos(scatter)) / distance;
      // Fixed but distinct angular speeds avoid a synchronized edge-on flash.
      const float tumble = 2.1F + .5F * (1 - std::min(distance, 1.0F)) +
                           2.4F * shard_hash(id * 47U + 17U);
      shard.delay = .022F * distance;
      shard.direction_x = direction_x;
      shard.direction_y = direction_y;
      shard.spin = (shard_hash(id * 43U + 13U) - .5F) * 2.2F;
      shard.tilt_x = direction_y * tumble;
      shard.tilt_y = -direction_x * tumble;
      result.push_back(std::move(shard));
    };
    for (int spoke = 0; spoke < spokes; ++spoke) {
      const int next = (spoke + 1) % spokes;
      add({{0, 0}, points[0][spoke], points[0][next]});
      for (std::size_t ring = 1; ring < radii.size(); ++ring) {
        const auto& a = points[ring - 1][spoke];
        const auto& b = points[ring][spoke];
        const auto& c = points[ring][next];
        const auto& d = points[ring - 1][next];
        add({a, b, c, d});
      }
    }
    // One blast speed, with modest fixed variation, sets a common impulse.
    // Its lower bound clears even the last central chip by 99%; outer pieces
    // depart naturally earlier instead of slowing to a common deadline.
    float blast_speed = 0;
    for (const auto& shard : result) {
      const auto center = shard_center(shard);
      const float edge = 1 + shard_radius(shard) + .015F;
      const float exit_x = std::abs(shard.direction_x) > .001F ?
          (edge - std::copysign(1.0F, shard.direction_x) * center[0]) /
              std::abs(shard.direction_x) : 1000;
      const float exit_y = std::abs(shard.direction_y) > .001F ?
          (edge - std::copysign(1.0F, shard.direction_y) * center[1]) /
              std::abs(shard.direction_y) : 1000;
      blast_speed = std::max(blast_speed, std::min(exit_x, exit_y) / (.99F - shard.delay));
    }
    for (std::size_t i = 0; i < result.size(); ++i)
      result[i].speed = blast_speed * (1 + .18F * shard_hash(static_cast<std::uint32_t>(i) * 61U));
    return result;
  }();
  return shards;
}

inline std::size_t glass_shard_vertex_count() {
  std::size_t count = 0;
  for (const auto& shard : glass_shards()) count += 3 * (shard.vertices.size() - 2);
  return count;
}

inline void update_glass_shard_vertices(double progress, std::vector<ShardVertex>& result) {
  const float position = std::clamp(static_cast<float>(progress), 0.0F, 1.0F);
  result.clear();
  const auto& shards = glass_shards();
  result.reserve(glass_shard_vertex_count());
  for (const GlassShard& shard : shards) {
    const auto center = shard_center(shard);
    const float age = std::max(0.0F, position - shard.delay);
    // The impact supplies a fixed angular momentum about an axis tangent to
    // the radial flight direction. Rotate rigidly about that axis throughout
    // flight, independently of translation.
    const float angular_speed = std::sqrt(shard.tilt_x * shard.tilt_x +
                                          shard.tilt_y * shard.tilt_y + shard.spin * shard.spin);
    const float axis_x = shard.tilt_x / angular_speed;
    const float axis_y = shard.tilt_y / angular_speed;
    const float axis_z = shard.spin / angular_speed;
    const float angle = angular_speed * age;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    // Both faces catch light while tumbling. The unbroken page stays neutral.
    const float normal_z = cosine + axis_z * axis_z * (1 - cosine);
    const float light = .58F + .42F * std::abs(normal_z) +
                        .10F * std::pow(std::sin(2 * angle), 8.0F);
    const float travel = shard.speed * age;
    const auto transform = [&](const ShardVertex& source) {
      const float local_x = source.x - center[0];
      const float local_y = source.y - center[1];
      const float dot = axis_x * local_x + axis_y * local_y;
      const float turned_x = local_x * cosine - axis_z * local_y * sine +
                             axis_x * dot * (1 - cosine);
      const float turned_y = local_y * cosine + axis_z * local_x * sine +
                             axis_y * dot * (1 - cosine);
      const float turned_z = (axis_x * local_y - axis_y * local_x) * sine +
                             axis_z * dot * (1 - cosine);
      result.push_back({
          center[0] + turned_x + shard.direction_x * travel,
          center[1] + turned_y + shard.direction_y * travel,
          turned_z, source.u, source.v, light});
    };
    // A polygon is one rigid splinter, even when rasterized as two triangles.
    for (std::size_t i = 1; i + 1 < shard.vertices.size(); ++i) {
      transform(shard.vertices[0]);
      transform(shard.vertices[i]);
      transform(shard.vertices[i + 1]);
    }
  }
}

}  // namespace pdfcube
