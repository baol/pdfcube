#include "dissolve.hpp"

#include <cassert>
#include <cmath>

int main() {
  const auto& shards = pdfcube::glass_shards();
  assert(shards.size() == 240);
  const auto& repeated = pdfcube::glass_shards();
  assert(repeated.size() == shards.size());
  for (std::size_t index = 0; index < shards.size(); ++index) {
    assert(shards[index].delay == repeated[index].delay);
    assert(shards[index].spin == repeated[index].spin);
    assert(shards[index].tilt_x == repeated[index].tilt_x);
    assert(shards[index].tilt_y == repeated[index].tilt_y);
    assert(shards[index].delay >= 0 && shards[index].delay <= .3F);
    assert(std::abs(shards[index].spin) <= 1.1F);
    assert(std::abs(shards[index].tilt_x) <= 3.4F);
    assert(std::abs(shards[index].tilt_y) <= 3.4F);
    assert(std::abs(shards[index].tilt_x * shards[index].direction_x +
                    shards[index].tilt_y * shards[index].direction_y) < .00001F);
    assert(std::abs(std::hypot(shards[index].direction_x,
                               shards[index].direction_y) - 1) < .001F);
  }

  std::vector<pdfcube::ShardVertex> intact;
  std::vector<pdfcube::ShardVertex> cracked;
  std::vector<pdfcube::ShardVertex> departed;
  pdfcube::update_glass_shard_vertices(0, intact);
  pdfcube::update_glass_shard_vertices(.06, cracked);
  pdfcube::update_glass_shard_vertices(.95, departed);
  assert(intact.size() == shards.size() * 3);
  double intact_distance = 0;
  double cracked_distance = 0;
  double departed_distance = 0;
  float maximum_depth = 0;
  for (std::size_t index = 0; index < intact.size(); ++index) {
    const auto distance = [](const auto& vertex) {
      return std::hypot(vertex.x, vertex.y);
    };
    intact_distance += distance(intact[index]);
    cracked_distance += distance(cracked[index]);
    departed_distance += distance(departed[index]);
    maximum_depth = std::max(maximum_depth, departed[index].z);
    assert(intact[index].u >= 0 && intact[index].u <= 1);
    assert(intact[index].v >= 0 && intact[index].v <= 1);
  }
  assert(cracked_distance > intact_distance);
  assert(departed_distance > cracked_distance * 1.5);
  assert(maximum_depth > .5F);

  // A central impact must launch every shard outward promptly, with a
  // stronger initial impulse than its later drift.
  std::vector<pdfcube::ShardVertex> early, later, drift;
  pdfcube::update_glass_shard_vertices(.10, early);
  pdfcube::update_glass_shard_vertices(.20, later);
  pdfcube::update_glass_shard_vertices(.30, drift);
  for (std::size_t i = 0; i < shards.size(); ++i) {
    float x = 0, y = 0, ex = 0, ey = 0, lx = 0, ly = 0, dx = 0, dy = 0;
    for (std::size_t j = 0; j < 3; ++j) {
      x += intact[3 * i + j].x / 3;
      y += intact[3 * i + j].y / 3;
      ex += early[3 * i + j].x / 3;
      ey += early[3 * i + j].y / 3;
      lx += later[3 * i + j].x / 3;
      ly += later[3 * i + j].y / 3;
      dx += drift[3 * i + j].x / 3;
      dy += drift[3 * i + j].y / 3;
    }
    assert(x * shards[i].direction_x + y * shards[i].direction_y > 0);
    assert(std::hypot(ex - x, ey - y) > .2F);
    assert(std::hypot(ex - x, ey - y) < .4F);
    assert(std::hypot(lx - ex, ly - ey) > std::hypot(dx - lx, dy - ly));
    // Tumbling preserves all three edge lengths (no shrinking or shearing).
    const auto length = [](const auto& a, const auto& b) {
      return std::sqrt((a.x - b.x) * (a.x - b.x) +
                       (a.y - b.y) * (a.y - b.y) +
                       (a.z - b.z) * (a.z - b.z));
    };
    for (std::size_t j = 0; j < 3; ++j) {
      const auto a = 3 * i + j;
      const auto b = 3 * i + (j + 1) % 3;
      assert(std::abs(length(intact[a], intact[b]) -
                      length(drift[a], drift[b])) < .00001F);
    }
  }
}
