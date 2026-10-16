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
  assert(pdfcube::glass_shard_vertex_count() > 3*shards.size());
  using Point = std::pair<float, float>;
  std::map<std::pair<Point, Point>, int> edges;
  double area = 0;
  float smallest = 10, largest = 0, slowest = 100, fastest = 0;
  std::vector<std::size_t> offsets;
  std::size_t offset = 0;
  for (const auto& shard : shards) {
    offsets.push_back(offset);
    offset += 3 * (shard.vertices.size() - 2);
    assert(shard.vertices.size() >= 3);
    assert(shard.impact_angle >= .30F && shard.impact_angle <= .55F);
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
  assert(largest > 2 * smallest);
  assert(slowest > 0 && fastest > slowest);
  // The impulse weakens with distance from the hit, rather than speeding up
  // the border pieces or making all pieces reach the edge simultaneously.
  for (const auto& a : shards) {
    const auto ca = pdfcube::shard_center(a);
    const float ra = std::hypot(ca[0],ca[1]);
    for (const auto& b : shards) {
      const auto cb = pdfcube::shard_center(b);
      if (ra < std::hypot(cb[0],cb[1])) {
        assert(a.speed >= b.speed);
        assert(a.forward_speed >= b.forward_speed);
      }
    }
  }
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

  // Recover each polygon's unique corners from the GPU triangle fan.
  // Internal triangles must never give one piece independent rotations.
  const auto corners = [&](const Frame& frame, std::size_t i) {
    Frame result{frame[offsets[i]], frame[offsets[i]+1], frame[offsets[i]+2]};
    for (std::size_t j = 3; j < shards[i].vertices.size(); ++j)
      result.push_back(frame[offsets[i]+3*(j-2)+2]);
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
  const auto screen_center = [&](const Frame& vertices) {
    auto c = center(vertices);
    const float projection = 4 / (4 - c[2]);
    c[0] *= projection;
    c[1] *= projection;
    return c;
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
  // The very first moving frame must already show an impact, not a nearly
  // intact panel waiting for cosine foreshortening to open cracks.
  Frame impact;
  pdfcube::update_glass_shard_vertices(.000001, impact);
  for (std::size_t i = 0; i < shards.size(); ++i) {
    const auto p = screen_center(corners(impact,i));
    const auto c = screen_center(corners(intact,i));
    const auto n = normal(corners(impact,i));
    assert(std::hypot(p[0]-c[0],p[1]-c[1]) >
           .119F * std::hypot(c[0],c[1]));
    assert(n[2] < .97F);
  }
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
  std::vector<float> first_step(shards.size()), last_step(shards.size()), last_gain(shards.size());
  int visible_25 = 0, visible_50 = 0;
  for (int step = 11; step <= 100; ++step) {
    Frame current, repeated;
    pdfcube::update_glass_shard_vertices(step / 100.0, current);
    pdfcube::update_glass_shard_vertices(step / 100.0, repeated);
    int visible = 0;
    for (std::size_t i = 0; i < shards.size(); ++i) {
      const auto unique = corners(current,i);
      const auto c = screen_center(unique), p = screen_center(corners(previous,i));
      // A small, uniform acceleration throughout flight: no slow-down or
      // sudden late boost. Total speed gain must remain below twenty percent.
      const float displacement = std::hypot(c[0]-p[0],c[1]-p[1]);
      if (step == 11) first_step[i] = displacement;
      if (step >= 12) {
        const float gain = displacement-last_step[i];
        assert(gain > 0);
        // Third differences amplify float rounding in projected centroids.
        if (step >= 13) assert(std::abs(gain-last_gain[i]) < .00001F);
        last_gain[i] = gain;
      }
      if (step == 100) {
        assert(displacement > first_step[i]*1.10F);
        assert(displacement < first_step[i]*1.20F);
      }
      last_step[i] = displacement;
      assert(c[2] > p[2]);
      assert((c[0]-p[0])*shards[i].direction_x + (c[1]-p[1])*shards[i].direction_y > 0);
      // All pairwise distances, including both polygon diagonals, are rigid.
      for (std::size_t a = 0; a < unique.size(); ++a)
        for (std::size_t b = a+1; b < unique.size(); ++b)
          assert(std::abs(distance(unique[a],unique[b]) -
                          distance(shards[i].vertices[a],shards[i].vertices[b])) < .00001F);
      float min_x=100, min_y=100, max_x=-100, max_y=-100;
      for (const auto& vertex : unique) {
        assert(vertex.light >= .58F && vertex.light <= 1.1F);
        const float projection = 4 / (4 - vertex.z);
        min_x=std::min(min_x,vertex.x*projection); max_x=std::max(max_x,vertex.x*projection);
        min_y=std::min(min_y,vertex.y*projection); max_y=std::max(max_y,vertex.y*projection);
      }
      const bool outside = max_x < -1 || min_x > 1 || max_y < -1 || min_y > 1;
      if (!outside) ++visible;
      if (step >= 99) assert(outside);
      for (std::size_t j = 1; j + 2 < unique.size(); ++j) {
        assert(distance(current[offsets[i]],current[offsets[i]+3*j]) == 0);
        assert(distance(current[offsets[i]+3*j-1],current[offsets[i]+3*j+1]) == 0);
      }
    }
    for (std::size_t i = 0; i < current.size(); ++i) {
      assert(current[i].u == intact[i].u && current[i].v == intact[i].v);
      assert(distance(current[i],repeated[i]) == 0);
      assert(current[i].light == repeated[i].light);
    }
    if (step == 25) visible_25 = visible;
    if (step == 50) visible_50 = visible;
    previous = std::move(current);
  }
  // Pieces leave at different times, rather than clearing in one final sweep.
  assert(visible_25 > visible_50 && visible_50 > 0);

  // The launch itself retains its previous speed; acceleration grows from
  // there rather than replacing the initial impulse with an ease-in.
  Frame launch_a, launch_b;
  pdfcube::update_glass_shard_vertices(.001,launch_a);
  pdfcube::update_glass_shard_vertices(.002,launch_b);
  for (std::size_t i = 0; i < shards.size(); ++i) {
    const auto a = screen_center(corners(launch_a,i));
    const auto b = screen_center(corners(launch_b,i));
    const float speed = std::hypot(b[0]-a[0],b[1]-a[1])/.001F;
    assert(std::abs(speed-shards[i].speed) < shards[i].speed*.002F);
  }

  // Optional wafer geometry has two textured faces joined by a pale rim.
  // The front/back separation remains fixed while each whole piece tumbles.
  assert(pdfcube::glass_shard_vertex_count(true) > 2*pdfcube::glass_shard_vertex_count());
  for (double progress : {.01, .25, .50, .99, 1.0}) {
    Frame thick;
    pdfcube::update_glass_shard_vertices(progress, thick, true);
    assert(thick.size() == pdfcube::glass_shard_vertex_count(true));
    std::size_t base = 0;
    for (const auto& shard : shards) {
      const auto faces = 3 * (shard.vertices.size()-2);
      for (std::size_t i = 0; i < faces; i += 3) {
        for (std::size_t j = 0; j < 3; ++j) {
          const std::size_t back = base + faces + i + (j == 0 ? 0 : 3-j);
          assert(std::abs(distance(thick[base+i+j],thick[back]) - .004F) < .000001F);
          assert(thick[base+i+j].rim == 0 && thick[back].rim == 0);
        }
      }
      const auto end = base + 2*faces + 6*shard.vertices.size();
      for (std::size_t i = base+2*faces; i < end; ++i) assert(thick[i].rim == 1);
      if (progress >= .99) {
        float lx=100,ly=100,hx=-100,hy=-100;
        for (std::size_t i = base; i < end; ++i) {
          const float projection = 4/(4-thick[i].z);
          lx=std::min(lx,thick[i].x*projection); hx=std::max(hx,thick[i].x*projection);
          ly=std::min(ly,thick[i].y*projection); hy=std::max(hy,thick[i].y*projection);
        }
        assert(hx < -1 || lx > 1 || hy < -1 || ly > 1);
      }
      base = end;
    }
  }
}
