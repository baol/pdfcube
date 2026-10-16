#include "dissolve.hpp"

#include <cassert>
#include <cmath>
#include <map>
#include <utility>

using Vertex = pdfcube::ShardVertex;
using Frame = std::vector<Vertex>;

int main() {
  const auto& shards = pdfcube::glass_shards();
  assert(shards.size() == 64);
  assert(pdfcube::glass_shard_vertex_count() == 336);
  using Point = std::pair<float, float>;
  std::map<std::pair<Point, Point>, int> edges;
  double area = 0;
  float smallest = 10, largest = 0, slowest = 100, fastest = 0;
  std::vector<std::size_t> offsets;
  std::size_t offset = 0;
  for (const auto& shard : shards) {
    offsets.push_back(offset);
    offset += 3 * (shard.vertices.size() - 2);
    assert(shard.vertices.size() == 3 || shard.vertices.size() == 4);
    assert(shard.delay >= 0 && shard.delay < .04F);
    assert(std::abs(std::hypot(shard.direction_x, shard.direction_y) - 1) < .00001F);
    assert(std::abs(shard.tilt_x * shard.direction_x +
                    shard.tilt_y * shard.direction_y) < .00001F);
    const auto center = pdfcube::shard_center(shard);
    assert(center[0] * shard.direction_x + center[1] * shard.direction_y > 0);
    slowest = std::min(slowest, shard.speed);
    fastest = std::max(fastest, shard.speed);
    float piece_area = 0;
    const auto& a = shard.vertices[0];
    for (std::size_t i = 1; i + 1 < shard.vertices.size(); ++i) {
      const auto& b = shard.vertices[i];
      const auto& c = shard.vertices[i + 1];
      const float triangle_area = ((b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x)) / 2;
      assert(triangle_area > 0);
      piece_area += triangle_area;
    }
    area += piece_area;
    smallest = std::min(smallest, piece_area);
    largest = std::max(largest, piece_area);
    for (std::size_t j = 0; j < shard.vertices.size(); ++j) {
      const auto& a = shard.vertices[j];
      const auto& b = shard.vertices[(j+1) % shard.vertices.size()];
      Point p{a.x, a.y}, q{b.x, b.y};
      if (q < p) std::swap(p, q);
      ++edges[{p,q}];
    }
  }
  // The fracture partitions the panel, with no internal cracks or overlap at
  // rest. Large outer splinters and small impact chips have distinct scales.
  assert(std::abs(area - 4) < .00001);
  assert(largest > 15 * smallest);
  assert(slowest > 0 && fastest <= 1.181F * slowest);
  for (const auto& [edge, count] : edges) {
    assert(count == 1 || count == 2);
    if (count == 1) {
      const auto& [a,b] = edge;
      assert((std::abs(std::abs(a.first)-1) < .00001F &&
              std::abs(a.first-b.first) < .00001F) ||
             (std::abs(std::abs(a.second)-1) < .00001F &&
              std::abs(a.second-b.second) < .00001F));
    }
  }

  // Recover the unique corners from the GPU triangle fan. The duplicate
  // corners of a quad must never give it two independent rotations.
  const auto corners = [&](const Frame& frame, std::size_t i) {
    Frame result{frame[offsets[i]], frame[offsets[i]+1], frame[offsets[i]+2]};
    if (shards[i].vertices.size() == 4) result.push_back(frame[offsets[i]+5]);
    return result;
  };
  const auto center = [](const Frame& vertices) {
    std::array<float,3> c{};
    for (const auto& v : vertices) {
      c[0] += v.x / vertices.size();
      c[1] += v.y / vertices.size();
      c[2] += v.z / vertices.size();
    }
    return c;
  };
  const auto distance = [](const Vertex& a, const Vertex& b) {
    return std::sqrt((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y) + (a.z-b.z)*(a.z-b.z));
  };
  const auto normal = [](const Frame& vertices) {
    const auto& a = vertices[0];
    const auto& b = vertices[1];
    const auto& c = vertices[2];
    std::array<float,3> n{
      (b.y-a.y)*(c.z-a.z) - (b.z-a.z)*(c.y-a.y),
      (b.z-a.z)*(c.x-a.x) - (b.x-a.x)*(c.z-a.z),
      (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x)};
    const float length = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    for (float& value : n) value /= length;
    return n;
  };
  Frame intact, previous, early, later;
  pdfcube::update_glass_shard_vertices(0, intact);
  pdfcube::update_glass_shard_vertices(.10, early);
  pdfcube::update_glass_shard_vertices(.20, later);
  assert(intact.size() == pdfcube::glass_shard_vertex_count());
  for (std::size_t i = 0; i < shards.size(); ++i) {
    const auto unique = corners(intact,i);
    for (std::size_t j = 0; j < unique.size(); ++j) {
      assert(distance(unique[j], shards[i].vertices[j]) < .000001F);
      assert(unique[j].light == 1);
    }
  }
  Frame third;
  pdfcube::update_glass_shard_vertices(.30, third);
  for (std::size_t i = 0; i < shards.size(); ++i) {
    const auto a = normal(corners(early,i));
    const auto b = normal(corners(later,i));
    const auto c = normal(corners(third,i));
    float first_angle = 0, second_angle = 0;
    for (std::size_t j = 0; j < 3; ++j) {
      first_angle += a[j]*b[j];
      second_angle += b[j]*c[j];
    }
    // Angular momentum is coherent: no changing rotation axis or spin-up.
    assert(std::abs(first_angle-second_angle) < .00002F);
    assert(first_angle < .99F);
  }

  previous = early;
  int visible_25 = 0, visible_75 = 0;
  for (int step = 11; step <= 100; ++step) {
    Frame current, repeated;
    pdfcube::update_glass_shard_vertices(step / 100.0, current);
    pdfcube::update_glass_shard_vertices(step / 100.0, repeated);
    int visible = 0;
    for (std::size_t i = 0; i < shards.size(); ++i) {
      const auto unique = corners(current,i);
      const auto c = center(unique), p = center(corners(previous,i));
      const auto e = center(corners(early,i)), l = center(corners(later,i));
      // Equal progress intervals mean equal screen displacement for the
      // whole flight, including the final approach to the viewport edge.
      assert(std::abs((c[0]-p[0]) - (l[0]-e[0])/10) < .00001F);
      assert(std::abs((c[1]-p[1]) - (l[1]-e[1])/10) < .00001F);
      assert(std::abs(c[2]) < .00001F);
      assert((c[0]-p[0])*shards[i].direction_x + (c[1]-p[1])*shards[i].direction_y > 0);
      // All pairwise distances, including both polygon diagonals, are rigid.
      for (std::size_t a = 0; a < unique.size(); ++a)
        for (std::size_t b = a+1; b < unique.size(); ++b)
          assert(std::abs(distance(unique[a],unique[b]) -
                          distance(shards[i].vertices[a],shards[i].vertices[b])) < .00001F);
      float min_x=100, min_y=100, max_x=-100, max_y=-100;
      for (const auto& vertex : unique) {
        assert(vertex.light >= .58F && vertex.light <= 1.1F);
        min_x=std::min(min_x,vertex.x); max_x=std::max(max_x,vertex.x);
        min_y=std::min(min_y,vertex.y); max_y=std::max(max_y,vertex.y);
      }
      const bool outside = max_x < -1 || min_x > 1 || max_y < -1 || min_y > 1;
      if (!outside) ++visible;
      if (step >= 99) assert(outside);
      if (unique.size() == 4) {
        assert(distance(current[offsets[i]],current[offsets[i]+3]) == 0);
        assert(distance(current[offsets[i]+2],current[offsets[i]+4]) == 0);
      }
    }
    for (std::size_t i = 0; i < current.size(); ++i) {
      assert(current[i].u == intact[i].u && current[i].v == intact[i].v);
      assert(distance(current[i],repeated[i]) == 0);
      assert(current[i].light == repeated[i].light);
    }
    if (step == 25) visible_25 = visible;
    if (step == 75) visible_75 = visible;
    previous = std::move(current);
  }
  // Pieces leave at different times; a small tail remains to tumble late.
  assert(visible_25 > visible_75 && visible_75 > 0);
}
