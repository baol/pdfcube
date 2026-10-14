#include "document.hpp"

#include <cairo.h>

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 2) return EXIT_FAILURE;
  pdfcube::Document document(argv[1]);

  if (document.page_info(0).cube_transition || document.page_info(0).duration_ms != 0 ||
      !document.page_info(1).cube_transition || document.page_info(1).duration_ms != 0 ||
      document.page_info(1).transition_seconds != 2.0) {
    std::cerr << "PDF transition metadata was not preserved\n";
    return EXIT_FAILURE;
  }

  const auto expect_size = [&](int requested, int expected_width, int expected_height) {
    const pdfcube::Surface surface = document.render_page(0, requested);
    const int width = cairo_image_surface_get_width(surface.get());
    const int height = cairo_image_surface_get_height(surface.get());
    if (width != expected_width || height != expected_height) {
      std::cerr << "expected " << expected_width << 'x' << expected_height << ", got " << width
                << 'x' << height << '\n';
      return false;
    }
    return true;
  };
  if (!expect_size(640, 640, 360) || !expect_size(1920, 1920, 1080) ||
      !expect_size(2560, 2560, 1440) || !expect_size(3840, 3840, 2160)) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
