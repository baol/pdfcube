#pragma once

namespace pdfcube {

enum class NumberJumpKind { page, cube_section };

struct NumberJump {
  NumberJumpKind kind;
  int number;
};

constexpr NumberJump number_jump(int digit, bool control) {
  return control ? NumberJump{NumberJumpKind::cube_section, digit}
                 : NumberJump{NumberJumpKind::page, (digit - 1) * 5};
}

}  // namespace pdfcube
