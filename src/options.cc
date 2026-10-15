#include "options.hpp"

#include "pdfcube-config.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <limits>
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
    if (consumed != part.size() || !std::isfinite(component) || component < 0.0F || component > 1.0F) {
      throw std::invalid_argument("components must be between 0.0 and 1.0");
    }
    values.push_back(component);
  }
  if (values.size() != 3) {
    throw std::invalid_argument("three colon-separated components are required");
  }
  return {values[0], values[1], values[2], 1.0F};
}

std::vector<ProgramStep> parse_program(const std::string& value) {
  std::vector<ProgramStep> steps;
  std::istringstream input(value);
  std::string duration, effect;
  while (std::getline(input, duration, ':')) {
    if (duration.empty() || !std::getline(input, effect, ':') || effect.empty())
      throw std::invalid_argument("--program requires duration:effect pairs");
    double seconds;
    std::size_t consumed = 0;
    try { seconds = std::stod(duration, &consumed); }
    catch (const std::exception&) {
      throw std::invalid_argument("Invalid --program duration: " + duration);
    }
    if (consumed != duration.size() || !std::isfinite(seconds) || seconds < 0 ||
        seconds > std::numeric_limits<int>::max() / 1000.0)
      throw std::invalid_argument("--program duration is outside the supported range: " + duration);
    if (effect != "c" && effect != "p" && effect != "blinds" && effect != "split" &&
        effect != "cover" && effect != "uncover" && effect != "fly" && effect != "push")
      throw std::invalid_argument("Unknown --program effect: " + effect);
    steps.push_back({static_cast<int>(seconds * 1000.0), effect});
  }
  if (steps.empty() || value.back() == ':')
    throw std::invalid_argument("--program requires duration:effect pairs");
  return steps;
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
      ("program", po::value<std::string>(),
       "Transition program duration:type pairs (c, p, blinds, split, cover, uncover, fly, push), for example 5:c:3:p.")
      ("performance-log", po::value<std::string>(),
       "Write buffered animation frame timings to a CSV file on exit.")
      ("no-fullscreen,n", po::value<bool>()->implicit_value(true),
       "Do not start in full-screen mode.");

  po::options_description hidden("Hidden options");
  hidden.add_options()
      ("input-file,i", po::value<std::string>(), "PDF file")
      ("snapshot", po::value<std::string>(), "Write one PNG frame and exit")
      ("snapshot-progress", po::value<double>(),
       "Transition progress used by --snapshot");

  po::options_description command_line;
  command_line.add(generic).add(options).add(hidden);
  po::options_description visible;
  visible.add(generic).add(options);
  po::positional_options_description positional;
  positional.add("input-file", 1);

  std::string error_source = "Command line option parsing error: ";
  try {
    po::variables_map command_values;
    po::variables_map config_values;
    // Boost's optional bool value otherwise consumes a following PDF filename.
    // Preserve explicit boolean values while making bare switches unambiguous.
    std::vector<std::string> arguments;
    bool positional_only = false;
    for (int index = 1; index < argc; ++index) {
      std::string argument = argv[index];
      if (argument == "--") positional_only = true;
      if (!positional_only && (argument == "--no-fullscreen" || argument == "-n" || argument == "--demo")) {
        const std::string next = index + 1 < argc ? argv[index + 1] : "";
        const bool explicit_bool = next == "true" || next == "false" || next == "1" ||
            next == "0" || next == "yes" || next == "no" || next == "on" || next == "off";
        if (!explicit_bool) argument = (argument == "-n" ? "--no-fullscreen" : argument) + "=true";
      }
      arguments.push_back(std::move(argument));
    }
    po::store(po::command_line_parser(arguments)
                  .options(command_line)
                  .positional(positional)
                  .run(),
              command_values);
    po::notify(command_values);

    const auto value = [&](const char* name) -> const po::variable_value* {
      if (const auto found = command_values.find(name); found != command_values.end()) {
        error_source = "Command line option parsing error: ";
        return &found->second;
      }
      if (const auto found = config_values.find(name); found != config_values.end()) {
        error_source = "Configuration file parsing error: ";
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
    const char* home = std::getenv("HOME");
    if (home == nullptr) home = std::getenv("USERPROFILE");
    if (home != nullptr) {
      std::ifstream config(std::string(home) + "/.pdfcuberc");
      if (config) {
        try {
          po::store(po::parse_config_file(config, options), config_values);
        } catch (const std::exception& error) {
          result.should_exit = true;
          result.exit_code = 2;
          result.output = std::string("Configuration file parsing error: ") + error.what() + "\n";
          return result;
        }
      }
    }
    error_source = "Configuration file parsing error: ";
    po::notify(config_values);
    if (const auto* selected = value("no-fullscreen")) {
      result.options.fullscreen = !selected->as<bool>();
    }
    if (const auto* selected = value("demo")) result.options.demo = selected->as<bool>();
    if (const auto* selected = value("program")) {
      result.options.program = selected->as<std::string>();
      result.options.program_steps = parse_program(result.options.program);
    }
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
    if (const auto* selected = value("snapshot")) {
      result.options.snapshot = selected->as<std::string>();
    }
    if (const auto* selected = value("snapshot-progress")) {
      result.options.snapshot_transition = true;
      result.options.snapshot_progress = selected->as<double>();
    }
    if (!std::isfinite(result.options.snapshot_progress))
      throw std::invalid_argument("--snapshot-progress must be finite");
    result.options.snapshot_progress =
        std::clamp(result.options.snapshot_progress, 0.0, 1.0);
  } catch (const std::exception& error) {
    result.should_exit = true;
    result.exit_code = 2;
    result.output = error_source + error.what() + "\n";
  }
  return result;
}

}  // namespace pdfcube

