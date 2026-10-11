#include "navigation.hpp"

#include <cstdlib>

int main() {
  using pdfcube::NumberJumpKind;

  for (int digit = 1; digit <= 9; ++digit) {
    const auto page = pdfcube::number_jump(digit, false);
    if (page.kind != NumberJumpKind::page || page.number != (digit - 1) * 5) {
      return EXIT_FAILURE;
    }

    const auto section = pdfcube::number_jump(digit, true);
    if (section.kind != NumberJumpKind::cube_section || section.number != digit) {
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
