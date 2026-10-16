#include "options.hpp"
#include "pdfcube-config.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {
pdfcube::ParseResult parse(std::initializer_list<std::string> arguments) {
  std::vector<std::string> values{"pdfcube"};
  values.insert(values.end(),arguments.begin(),arguments.end());
  std::vector<char*> pointers;
  for (auto& value : values) pointers.push_back(value.data());
  return pdfcube::parse_options(static_cast<int>(pointers.size()),pointers.data());
}
void set_home(const std::optional<std::string>& home) {
#ifdef _WIN32
  _putenv_s("HOME",home ? home->c_str() : "");
#else
  if (home) setenv("HOME",home->c_str(),1); else unsetenv("HOME");
#endif
}
}

int main() {
  const char* original = std::getenv("HOME");
  const std::optional<std::string> original_home = original ? std::optional<std::string>(original) : std::nullopt;
  const auto root = std::filesystem::temp_directory_path() /
      ("pdfcube-options-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  set_home(root.string());
  const auto help = parse({"--help"});
  assert(help.should_exit && help.exit_code == 0);
  for (const char* option : {"--bgcolor","--top-color","--demo","--program",
                            "--performance-log","--no-fullscreen","--experimental"})
    assert(help.output.find(option) != std::string::npos);
  assert(parse({"-h"}).output == help.output);
  const auto version = parse({"--version"});
  assert(version.should_exit && version.exit_code == 0);
  assert(version.output == "pdfcube " PDFCUBE_VERSION "\n");
  assert(parse({"-v"}).output == version.output);
  for (const char* flag : {"--experimental","--demo","--no-fullscreen"}) {
    const auto on = parse({flag,"deck.pdf"});
    assert(!on.should_exit && on.options.input_file == "deck.pdf");
    const auto off = parse({std::string(flag)+"=false","deck.pdf"});
    assert(!off.should_exit && off.options.input_file == "deck.pdf");
    if (std::string(flag) == "--experimental") assert(on.options.experimental && !off.options.experimental);
    if (std::string(flag) == "--demo") assert(on.options.demo && !off.options.demo);
    if (std::string(flag) == "--no-fullscreen") assert(!on.options.fullscreen && off.options.fullscreen);
  }
  const auto literal = parse({"--","--experimental"});
  assert(!literal.should_exit && !literal.options.experimental && literal.options.input_file == "--experimental");
  const auto capture = parse({"--no-fullscreen","--snapshot","frame.png","--snapshot-progress","0",
                              "--experimental","deck.pdf"});
  assert(!capture.should_exit && capture.options.experimental && capture.options.input_file == "deck.pdf");
  assert(capture.options.snapshot == "frame.png" && capture.options.snapshot_transition);
  assert(capture.options.snapshot_progress == 0);
  const auto colors = parse({"-b","0:0:1","-t","0.7:0.6:0.6","-i","deck.pdf"});
  assert(!colors.should_exit && colors.options.background.blue == 1 && colors.options.top.red == .7F);
  for (const auto& invalid : {parse({"--unknown"}),parse({"--bgcolor","1:2"}),parse({"--top-color","2:0:0"})}) {
    assert(invalid.should_exit && invalid.exit_code == 2);
    assert(invalid.output.find("Command line option parsing error") != std::string::npos);
  }
  for (const auto& invalid : {parse({"--record-raw","out.rgb"}),
      parse({"--record-raw","out.rgb","--snapshot","out.png","deck.pdf"}),
      parse({"--record-fps","0"}),parse({"--record-width","-1"})}) {
    assert(invalid.should_exit && invalid.exit_code == 2);
  }
  const auto recording = parse({"--record-raw","out.rgb","--demo","deck.pdf"});
  assert(!recording.should_exit && !recording.options.demo && !recording.options.fullscreen);
  assert(recording.options.input_file == "deck.pdf" && recording.options.record_fps == 30);
  std::ofstream(root/".pdfcuberc") << "bgcolor=0.1:0.2:0.3\nno-fullscreen=true\nexperimental=true\n";
  const auto configured = parse({"deck.pdf"});
  assert(!configured.should_exit && configured.options.experimental && !configured.options.fullscreen);
  assert(configured.options.background.red == .1F);
  const auto overridden = parse({"--experimental=false","--no-fullscreen=false","--bgcolor","0:0:1","deck.pdf"});
  assert(!overridden.should_exit && !overridden.options.experimental && overridden.options.fullscreen);
  assert(overridden.options.background.blue == 1 && overridden.options.background.red == 0);
  set_home(original_home);
  std::filesystem::remove(root/".pdfcuberc");
  std::filesystem::remove(root);
}
