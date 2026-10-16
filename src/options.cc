#include "options.hpp"

#include "pdfcube-config.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace pdfcube {
namespace po = boost::program_options;

namespace {

Color parse_color(const std::string& value) {
  std::istringstream input(value);
  std::string part;
  std::vector<float> values;
  while (std::getline(input, part, ':')) {
    std::size_t consumed = 0;
    const float component = std::stof(part, &consumed);
    if (consumed != part.size() || component < 0.0F || component > 1.0F) {
      throw std::invalid_argument("components must be between 0.0 and 1.0");
    }
    values.push_back(component);
  }
  if (values.size() != 3) {
    throw std::invalid_argument("three colon-separated components are required");
  }
  return {values[0], values[1], values[2], 1.0F};
}

std::string help_text(const po::options_description& visible) {
  std::ostringstream output;
  output << "\npdfcube " PDFCUBE_VERSION "\n"
         << "=============\n"
         << "PDF presentation viewer with 3D transitions\n\n"
         << visible << "\n"
         << "Usage: pdfcube [options] [presentation.pdf]\n";
  return output.str();
}

}  // namespace

ParseResult parse_options(int argc, char** argv) {
  ParseResult result;

  po::options_description generic("Generic options");
  generic.add_options()
      ("help,h", "This help message.")
      ("version,v", "Version information");

  po::options_description options("Available options");
  options.add_options()
      ("bgcolor,b", po::value<std::string>(),
       "Background color as r:g:b, with values between 0.0 and 1.0.")
      ("top-color,t", po::value<std::string>(),
       "Cube cap and Push side color as r:g:b, with values between 0.0 and 1.0.")
      ("demo", po::value<bool>()->implicit_value(true),
       "Auto-advance using PDF or programmed durations.")
      ("experimental", po::value<bool>()->implicit_value(true),
       "Enable experimental animation details, including thin glass shard edges.")
      ("program", po::value<std::string>(),
       "Transition program duration:type pairs (c, p, blinds, split, cover, uncover, fly, push, dissolve, glitter, wipe), for example 5:c:3:p.")
      ("performance-log", po::value<std::string>(),
       "Write buffered animation frame timings to a CSV file on exit.")
      ("no-fullscreen,n", po::value<bool>()->implicit_value(true),
       "Do not start in full-screen mode.");

  po::options_description hidden("Hidden options");
  hidden.add_options()
      ("input-file,i", po::value<std::string>(), "PDF file")
      ("record-raw", po::value<std::string>(), "Export one presentation cycle as RGB24 frames")
      ("record-fps", po::value<int>(), "Export frame rate")
      ("record-width", po::value<int>(), "Export width")
      ("record-height", po::value<int>(), "Export height")
      ("snapshot", po::value<std::string>(), "Write one PNG frame and exit")
      ("snapshot-progress", po::value<double>(),
       "Transition progress used by --snapshot");

  po::options_description command_line;
  command_line.add(generic).add(options).add(hidden);
  po::options_description visible;
  visible.add(generic).add(options);
  po::positional_options_description positional;
  positional.add("input-file", 1);

  try {
    // Boost's optional bool argument otherwise consumes a following PDF path
    // as its value. Make the bare flag explicit while retaining =false and
    // normal positional-argument handling after "--".
    std::vector<std::string> arguments;
    bool positional_only = false;
    for (int i = 1; i < argc; ++i) {
      const std::string argument = argv[i];
      if (argument == "--") positional_only = true;
      const bool boolean_switch = argument == "--experimental" || argument == "--demo" ||
                                  argument == "--no-fullscreen";
      arguments.push_back(!positional_only && boolean_switch ? argument + "=true" : argument);
    }
    po::variables_map command_values;
    po::variables_map config_values;
    po::store(po::command_line_parser(arguments)
                  .options(command_line)
                  .positional(positional)
                  .run(),
              command_values);
    if (const char* home = std::getenv("HOME")) {
      std::ifstream config(std::string(home) + "/.pdfcuberc");
      if (config) {
        po::store(po::parse_config_file(config, options), config_values);
      }
    }
    po::notify(command_values);
    po::notify(config_values);

    const auto value = [&](const char* name) -> const po::variable_value* {
      if (const auto found = command_values.find(name); found != command_values.end()) {
        return &found->second;
      }
      if (const auto found = config_values.find(name); found != config_values.end()) {
        return &found->second;
      }
      return nullptr;
    };

    if (command_values.contains("help")) {
      result.should_exit = true;
      result.output = help_text(visible);
      return result;
    }
    if (command_values.contains("version")) {
      result.should_exit = true;
      result.output = "pdfcube " PDFCUBE_VERSION "\n";
      return result;
    }
    if (const auto* selected = value("no-fullscreen")) {
      result.options.fullscreen = !selected->as<bool>();
    }
    if (const auto* selected = value("demo")) result.options.demo = selected->as<bool>();
    if (const auto* selected = value("experimental")) {
      result.options.experimental = selected->as<bool>();
    }
    if (const auto* selected = value("program")) result.options.program = selected->as<std::string>();
    if (const auto* selected = value("performance-log")) {
      result.options.performance_log = selected->as<std::string>();
    }
    if (const auto* selected = value("bgcolor")) {
      result.options.background = parse_color(selected->as<std::string>());
    }
    if (const auto* selected = value("top-color")) {
      result.options.top = parse_color(selected->as<std::string>());
    }
    if (const auto* selected = value("input-file")) {
      result.options.input_file = selected->as<std::string>();
    }
    if (const auto* selected = value("record-raw")) {
      result.options.record_raw = selected->as<std::string>();
      result.options.fullscreen = false;
      result.options.demo = false;
      if (result.options.input_file.empty() || !result.options.program.empty())
        throw std::invalid_argument("recording requires a PDF and no transition program");
    }
    for (const auto& entry : std::vector<std::pair<const char*, int*>>{
             {"record-fps", &result.options.record_fps},
             {"record-width", &result.options.record_width},
             {"record-height", &result.options.record_height}}) {
      if (const auto* selected = value(entry.first)) *entry.second = selected->as<int>();
      if (*entry.second < 1 || *entry.second > 8192)
        throw std::invalid_argument("recording dimensions and rate must be in 1..8192");
    }
    if (!result.options.record_raw.empty() && value("snapshot"))
      throw std::invalid_argument("recording and snapshot are mutually exclusive");
    if (const auto* selected = value("snapshot")) {
      result.options.snapshot = selected->as<std::string>();
    }
    if (const auto* selected = value("snapshot-progress")) {
      result.options.snapshot_transition = true;
      result.options.snapshot_progress = selected->as<double>();
    }
    result.options.snapshot_progress =
        std::clamp(result.options.snapshot_progress, 0.0, 1.0);
  } catch (const std::exception& error) {
    result.should_exit = true;
    result.exit_code = 2;
    result.output = std::string("Command line option parsing error: ") + error.what() + "\n";
  }
  return result;
}

}  // namespace pdfcube

