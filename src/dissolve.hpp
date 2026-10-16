#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
  float rim{0};
};

struct GlassShard {
  std::vector<ShardVertex> vertices;
  float direction_x;
  float direction_y;
  float spin;
  float tilt_x;
  float tilt_y;
  float speed;
  float impact_angle;
  float forward_speed;
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
    // A clipped Voronoi partition makes interlocking, irregular glass pieces.
    // The impact determines their motion, not a visible pattern of spokes.
    using Point = std::array<float, 2>;
    constexpr int columns = 8;
    std::vector<Point> seeds;
    for (int row = 0; row < columns; ++row) {
      for (int column = 0; column < columns; ++column) {
        const auto id = static_cast<std::uint32_t>(row * columns + column);
        float x = -1 + (column + .5F + .9F * (shard_hash(id * 31U + 7U) - .5F)) / 4;
        float y = -1 + (row + .5F + .9F * (shard_hash(id * 53U + 3U) - .5F)) / 4;
        // Slightly denser fragments near the hit, without concentric seams.
        const float density = .65F + .35F * std::max(std::abs(x), std::abs(y));
        seeds.push_back({x * density, y * density});
      }
    }
    std::vector<GlassShard> result;
    result.reserve(seeds.size());
    std::vector<Point> shared_vertices;
    for (std::size_t cell = 0; cell < seeds.size(); ++cell) {
      std::vector<Point> polygon{{-1,-1}, {1,-1}, {1,1}, {-1,1}};
      const auto& seed = seeds[cell];
      for (std::size_t neighbor = 0; neighbor < seeds.size(); ++neighbor) {
        if (neighbor == cell) continue;
        const auto& other = seeds[neighbor];
        const float nx = other[0] - seed[0], ny = other[1] - seed[1];
        const float mx = (seed[0] + other[0]) / 2, my = (seed[1] + other[1]) / 2;
        const auto side = [&](const Point& p) { return (p[0]-mx)*nx + (p[1]-my)*ny; };
        std::vector<Point> clipped;
        Point previous = polygon.back();
        float previous_side = side(previous);
        for (const auto& point : polygon) {
          const float current_side = side(point);
          if ((previous_side <= 0) != (current_side <= 0)) {
            const float t = previous_side / (previous_side-current_side);
            clipped.push_back({previous[0] + t*(point[0]-previous[0]),
                               previous[1] + t*(point[1]-previous[1])});
          }
          if (current_side <= 0) clipped.push_back(point);
          previous = point;
          previous_side = current_side;
        }
        polygon = std::move(clipped);
        // Each seed lies strictly inside the page and owns a nonempty cell.
        if (polygon.empty()) break;
      }
      GlassShard shard{};
      for (Point point : polygon) {
        // Adjacent cells share exactly the same corners, despite independent
        // floating-point half-plane clipping.
        bool found = false;
        for (const auto& shared : shared_vertices) {
          if (std::hypot(point[0]-shared[0],point[1]-shared[1]) < .00001F) {
            point = shared;
            found = true;
            break;
          }
        }
        if (!found) shared_vertices.push_back(point);
        if (!shard.vertices.empty() &&
            shard.vertices.back().x == point[0] && shard.vertices.back().y == point[1]) continue;
        shard.vertices.push_back({point[0],point[1],0,(point[0]+1)/2,(1-point[1])/2});
      }
      if (shard.vertices.size() > 1 &&
          shard.vertices.front().x == shard.vertices.back().x &&
          shard.vertices.front().y == shard.vertices.back().y) shard.vertices.pop_back();
      if (shard.vertices.size() < 3) continue;
      const auto center = shard_center(shard);
      const float x = center[0], y = center[1], distance = std::hypot(x,y);
      const auto id = static_cast<std::uint32_t>(cell);
      const float scatter = (shard_hash(id*29U+5U)-.5F)*.16F;
      const float direction = std::atan2(y,x) + scatter;
      const float tumble = 2.1F + .5F*(1-std::min(distance,1.0F)) +
                           2.4F*shard_hash(id*47U+17U);
      shard.direction_x = std::cos(direction);
      shard.direction_y = std::sin(direction);
      shard.spin = (shard_hash(id*43U+13U)-.5F)*2.2F;
      shard.tilt_x = shard.direction_y*tumble;
      shard.tilt_y = -shard.direction_x*tumble;
      shard.impact_angle = .30F + .25F*shard_hash(id*37U+11U);
      shard.forward_speed = .08F + .57F*std::pow(std::max(0.0F,1-distance),2.0F);
      result.push_back(std::move(shard));
    }
    // The impact is strongest at the center and tapers toward the border.
    // Pick one minimum border speed that still clears every piece by 99%.
    // The initial broken pose supplies separation, not a slow crack phase.
    float blast_speed = 0;
    for (const auto& shard : result) {
      const auto center = shard_center(shard);
      // Also bound the enlarged silhouette as pieces fly toward the camera.
      const float edge = 1 + 1.6F * shard_radius(shard) + .015F;
      const float exit_x = std::abs(shard.direction_x) > .001F ?
          (edge - std::copysign(1.0F, shard.direction_x) * center[0]) /
              std::abs(shard.direction_x) : 1000;
      const float exit_y = std::abs(shard.direction_y) > .001F ?
          (edge - std::copysign(1.0F, shard.direction_y) * center[1]) /
              std::abs(shard.direction_y) : 1000;
      const float distance = std::hypot(center[0], center[1]);
      const float central_impulse = .35F * std::pow(std::max(0.0F, 1 - distance), 2.0F);
      blast_speed = std::max(blast_speed,
          (std::min(exit_x, exit_y) - .12F * distance) / .99F - central_impulse);
    }
    for (auto& shard : result) {
      const auto center = shard_center(shard);
      const float distance = std::hypot(center[0], center[1]);
      shard.speed = blast_speed + .35F * std::pow(std::max(0.0F, 1 - distance), 2.0F);
    }
    return result;
  }();
  return shards;
}

inline std::size_t glass_shard_vertex_count(bool experimental = false) {
  std::size_t count = 0;
  for (const auto& shard : glass_shards()) {
    const auto faces = 3 * (shard.vertices.size() - 2);
    count += experimental ? 2 * faces + 6 * shard.vertices.size() : faces;
  }
  return count;
}

inline void update_glass_shard_vertices(double progress, std::vector<ShardVertex>& result,
                                       bool experimental = false) {
  const float position = std::clamp(static_cast<float>(progress), 0.0F, 1.0F);
  // Preserve the launch velocity, then increase it continuously by 15% over
  // the flight. There is no delayed ramp or change to angular momentum.
  const float flight = position + .075F * position * position;
  result.clear();
  const auto& shards = glass_shards();
  result.reserve(glass_shard_vertex_count(experimental));
  for (const GlassShard& shard : shards) {
    const auto center = shard_center(shard);
    // The impact supplies a fixed angular momentum about an axis tangent to
    // the radial flight direction. Rotate rigidly about that axis throughout
    // flight, independently of translation.
    const float angular_speed = std::sqrt(shard.tilt_x * shard.tilt_x +
                                          shard.tilt_y * shard.tilt_y + shard.spin * shard.spin);
    const float axis_x = shard.tilt_x / angular_speed;
    const float axis_y = shard.tilt_y / angular_speed;
    const float axis_z = shard.spin / angular_speed;
    // The impact is a cut from the intact panel to separated, tilted pieces.
    // After that first broken pose, rotation keeps its fixed angular speed.
    const float angle = position == 0 ? 0 : shard.impact_angle + angular_speed * position;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    // Both faces catch light while tumbling. The unbroken page stays neutral.
    const float normal_z = cosine + axis_z * axis_z * (1 - cosine);
    const float light = .58F + .42F * std::abs(normal_z) +
                        .10F * std::pow(std::sin(2 * angle), 8.0F);
    const float impact = position == 0 ? 0 : .12F * std::hypot(center[0], center[1]);
    const float travel = impact + shard.speed * flight;
    const float depth = shard.forward_speed * flight;
    // The central blow sends glass toward the viewer. Compensate the pivot's
    // perspective magnification so its visible outward acceleration stays mild;
    // the rigid splinter itself still grows naturally as it approaches.
    const float inverse_projection = 1 - depth / 4;
    const auto rotate = [&](float local_x, float local_y, float local_z) {
      const float dot = axis_x * local_x + axis_y * local_y + axis_z * local_z;
      const float turned_x = local_x * cosine + (axis_y * local_z - axis_z * local_y) * sine +
                             axis_x * dot * (1 - cosine);
      const float turned_y = local_y * cosine + (axis_z * local_x - axis_x * local_z) * sine +
                             axis_y * dot * (1 - cosine);
      const float turned_z = local_z * cosine + (axis_x * local_y - axis_y * local_x) * sine +
                             axis_z * dot * (1 - cosine);
      return std::array<float, 3>{turned_x, turned_y, turned_z};
    };
    const auto transform = [&](const ShardVertex& source, float local_z,
                               float lighting, float rim) {
      const auto turned = rotate(source.x - center[0], source.y - center[1], local_z);
      result.push_back({
          (center[0] + shard.direction_x * travel) * inverse_projection + turned[0],
          (center[1] + shard.direction_y * travel) * inverse_projection + turned[1],
          depth + turned[2], source.u, source.v, lighting, rim});
    };
    constexpr float half_thickness = .002F;
    const float front = experimental ? half_thickness : 0;
    // A polygon is one rigid splinter, even when rasterized as two triangles.
    for (std::size_t i = 1; i + 1 < shard.vertices.size(); ++i) {
      transform(shard.vertices[0], front, light, 0);
      transform(shard.vertices[i], front, light, 0);
      transform(shard.vertices[i + 1], front, light, 0);
    }
    if (!experimental) continue;
    for (std::size_t i = 1; i + 1 < shard.vertices.size(); ++i) {
      transform(shard.vertices[0], -front, light, 0);
      transform(shard.vertices[i + 1], -front, light, 0);
      transform(shard.vertices[i], -front, light, 0);
    }
    // Close the wafer with a tiny pale rim. Texture coordinates stay on the
    // original edge; a separate material keeps the rim from stretching text.
    for (std::size_t i = 0; i < shard.vertices.size(); ++i) {
      const auto& a = shard.vertices[i];
      const auto& b = shard.vertices[(i + 1) % shard.vertices.size()];
      const float length = std::hypot(b.x - a.x, b.y - a.y);
      const auto normal = rotate((b.y - a.y) / length, -(b.x - a.x) / length, 0);
      const float rim_light = .65F + .35F * std::abs(normal[2]);
      transform(a, front, rim_light, 1);
      transform(a, -front, rim_light, 1);
      transform(b, -front, rim_light, 1);
      transform(a, front, rim_light, 1);
      transform(b, -front, rim_light, 1);
      transform(b, front, rim_light, 1);
    }
  }
}

}  // namespace pdfcube
