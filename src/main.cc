#include "application.hpp"
#include "options.hpp"

#include <iostream>

int main(int argc, char** argv) {
  pdfcube::ParseResult parsed = pdfcube::parse_options(argc, argv);
  if (parsed.should_exit) {
    std::ostream& output = parsed.exit_code == 0 ? std::cout : std::cerr;
    output << parsed.output;
    return parsed.exit_code;
  }
  pdfcube::Application application(std::move(parsed.options));
  return application.run();
}

