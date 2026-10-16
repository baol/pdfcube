#include "wipe.hpp"

#include <cassert>
#include <cmath>

int main() {
  using namespace pdfcube;
  for (const int direction : {0,90,180,270}) {
    for (const auto size : {WipeClip{0,0,320,240},WipeClip{0,0,180,320},WipeClip{0,0,1,1}}) {
      const auto first = wipe_geometry(0,direction,size.width,size.height);
      const auto last = wipe_geometry(1,direction,size.width,size.height);
      assert(first.outgoing.width == size.width && first.outgoing.height == size.height);
      assert(last.outgoing.width == 0 || last.outgoing.height == 0);
      int previous = size.width * size.height;
      float previous_center = first.center;
      for (int step=0; step<=1000; ++step) {
        const auto pose = wipe_geometry(step/1000.0,direction,size.width,size.height);
        const auto again = wipe_geometry(step/1000.0,direction,size.width,size.height);
        const auto clip = pose.outgoing;
        assert(clip.x >= 0 && clip.y >= 0 && clip.width >= 0 && clip.height >= 0);
        assert(clip.x + clip.width <= size.width && clip.y + clip.height <= size.height);
        const int area = clip.width * clip.height;
        assert(area <= previous);
        previous = area;
        assert(pose.center == again.center && pose.half_width == again.half_width);
        assert(pose.half_width > 0 && std::isfinite(pose.center));
        if (direction == 0 || direction == 90) assert(pose.center >= previous_center);
        else assert(pose.center <= previous_center);
        previous_center = pose.center;
      }
      assert(wipe_geometry(-1,direction,size.width,size.height).center == first.center);
      assert(wipe_geometry(2,direction,size.width,size.height).center == last.center);
      // Symmetric travel and a midpoint exactly at the center, without time integration.
      const auto quarter = wipe_geometry(.25,direction,size.width,size.height);
      const auto three = wipe_geometry(.75,direction,size.width,size.height);
      assert(std::abs(quarter.center + three.center) < .00001F);
      assert(std::abs(wipe_geometry(.5,direction,size.width,size.height).center) < .00001F);
    }
  }
  for (const int invalid : {-90,45,135,360}) {
    assert(wipe_direction(invalid) == 0);
    assert(wipe_geometry(.37,invalid,320,240).center == wipe_geometry(.37,0,320,240).center);
  }
}
