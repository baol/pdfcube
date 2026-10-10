#include "options.hpp"

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
  output << "\npdfcube 0.1.0\n"
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
       "Cube top color as r:g:b, with values between 0.0 and 1.0.")
      ("demo", po::value<bool>()->implicit_value(true),
       "Auto-advance using PDF or programmed durations.")
      ("program", po::value<std::string>(),
       "Transition program duration:type pairs, for example 5:c:3:p.")
      ("no-fullscreen,n", po::value<bool>()->implicit_value(true),
       "Do not start in full-screen mode.");

  po::options_description hidden("Hidden options");
  hidden.add_options()
      ("input-file", po::value<std::string>(), "PDF file")
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
    po::variables_map command_values;
    po::variables_map config_values;
    po::store(po::command_line_parser(argc, argv)
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
      result.output = "pdfcube 0.1.0\n";
      return result;
    }
    if (const auto* selected = value("no-fullscreen")) {
      result.options.fullscreen = !selected->as<bool>();
    }
    if (const auto* selected = value("demo")) result.options.demo = selected->as<bool>();
    if (const auto* selected = value("program")) result.options.program = selected->as<std::string>();
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

