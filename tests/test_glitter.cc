#include "glitter.hpp"
#include <cassert>
#include <cmath>

int main() {
  using namespace pdfcube;
  for (int direction : {0, 270, 315}) {
    assert(glitter_pieces(0, direction).size() == 336);
    assert(glitter_pieces(1, direction).empty());
    const auto pieces = glitter_pieces(.5, direction);
    const auto repeat = glitter_pieces(.5, direction);
    assert(!pieces.empty() && pieces.size() < 336);
    assert(pieces.size() == repeat.size());
    for (std::size_t i = 0; i < pieces.size(); ++i) {
      const auto& piece = pieces[i];
      assert(piece.glow >= 0 && piece.glow <= 1);
      assert(piece.opacity >= 0 && piece.opacity <= 1);
      assert(piece.u >= 0 && piece.u + piece.width <= 1.00001F);
      assert(piece.v >= 0 && piece.v + piece.height <= 1.00001F);
      for (int j = 0; j < 16; ++j) {
        assert(std::isfinite(piece.transform.value[j]));
        assert(piece.transform.value[j] == repeat[i].transform.value[j]);
      }
      // Released cells trail behind the front, intact cells remain ahead.
      if (piece.opacity == 1) {
        const float sweep = direction == 0 ? piece.u : direction == 270 ? piece.v :
                            (piece.u + piece.v) / 2;
        assert(sweep > .6F);
      }
    }
  }
  assert(glitter_direction(90) == 0);
  assert(glitter_direction(315) == 315);
}
