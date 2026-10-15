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
    assert(std::abs(shards[index].tilt_x) <= .8F);
    assert(std::abs(shards[index].tilt_y) <= .8F);
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
}
