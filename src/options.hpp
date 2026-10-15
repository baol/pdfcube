#pragma once

#include <optional>
#include <string>

namespace pdfcube {

struct Color {
  float red{};
  float green{};
  float blue{};
  float alpha{1.0F};
};

struct Options {
  Color background{0.6F, 0.0F, 0.0F, 1.0F};
  Color top{0.7F, 0.6F, 0.6F, 1.0F};
  bool demo{false};
  bool fullscreen{true};
  std::string program;
  std::string performance_log;
  std::string input_file;
  std::string snapshot;
  double snapshot_progress{0.0};
  bool snapshot_transition{false};
};

struct ParseResult {
  Options options;
  bool should_exit{false};
  int exit_code{0};
  std::string output;
};

ParseResult parse_options(int argc, char** argv);

}  // namespace pdfcube

