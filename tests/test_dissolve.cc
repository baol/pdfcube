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
  assert(departed_distance > cracked_distance);
  assert(maximum_depth > .05F);

  // A central impact must launch every shard outward promptly at constant
  // screen speed, while rotation remains independent of translation.
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
    assert(std::hypot(ex - x, ey - y) > .005F);
    assert(std::hypot(ex - x, ey - y) < .4F);
    assert(std::abs((lx - ex) - (dx - lx)) < .00001F);
    assert(std::abs((ly - ey) - (dy - ly)) < .00001F);
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
  // Check the entire flight, including the final approach to the edge, so a
  // late acceleration cannot pass just by matching the first few samples.
  std::vector<pdfcube::ShardVertex> previous = early;
  for (int step = 11; step <= 99; ++step) {
    std::vector<pdfcube::ShardVertex> current;
    pdfcube::update_glass_shard_vertices(step / 100.0, current);
    for (std::size_t i = 0; i < shards.size(); ++i) {
      float vx = 0, vy = 0, reference_x = 0, reference_y = 0;
      for (std::size_t j = 0; j < 3; ++j) {
        const auto k = 3 * i + j;
        vx += (current[k].x - previous[k].x) / 3;
        vy += (current[k].y - previous[k].y) / 3;
        reference_x += (later[k].x - early[k].x) / 30;
        reference_y += (later[k].y - early[k].y) / 30;
      }
      assert(std::abs(vx - reference_x) < .00001F);
      assert(std::abs(vy - reference_y) < .00001F);
      assert(vx * shards[i].direction_x + vy * shards[i].direction_y > 0);
    }
    previous = std::move(current);
  }
  // Interior projected vertices guarantee visible geometry during the tumble.
  for (double progress : {.5, .75}) {
    std::vector<pdfcube::ShardVertex> visible;
    pdfcube::update_glass_shard_vertices(progress, visible);
    int count = 0;
    for (const auto& vertex : visible) {
      if (std::abs(vertex.x) < .98F && std::abs(vertex.y) < .98F) ++count;
    }
    assert(count > 0);
  }
  // Every triangle must be completely outside the view before the final
  // static frame. A disjoint projected bounding box is a conservative test.
  for (int step = 99; step <= 100; ++step) {
    std::vector<pdfcube::ShardVertex> exited;
    pdfcube::update_glass_shard_vertices(step / 100.0, exited);
    for (std::size_t i = 0; i < exited.size(); i += 3) {
      float min_x = 100, min_y = 100, max_x = -100, max_y = -100;
      for (std::size_t j = i; j < i + 3; ++j) {
        const auto& vertex = exited[j];
        min_x = std::min(min_x, vertex.x);
        max_x = std::max(max_x, vertex.x);
        min_y = std::min(min_y, vertex.y);
        max_y = std::max(max_y, vertex.y);
      }
      assert(max_x < -1 || min_x > 1 || max_y < -1 || min_y > 1);
    }
  }
}
