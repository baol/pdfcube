#include "document.hpp"
#include <cstdlib>

int main(int argc, char** argv) {
  if (argc != 3 && argc != 5) return EXIT_FAILURE;
  pdfcube::Document document(argv[1]);
  const auto& page = document.page_info(1);
  const bool split = argc == 5 && argv[3][0] == 'S';
  return page.transition_type == (split ? POPPLER_PAGE_TRANSITION_SPLIT : POPPLER_PAGE_TRANSITION_BLINDS) &&
         (!split || page.direction == (argv[4][0] == 'O' ? POPPLER_PAGE_TRANSITION_OUTWARD :
                                                          POPPLER_PAGE_TRANSITION_INWARD)) &&
         page.alignment == (argv[2][0] == 'V' ? POPPLER_PAGE_TRANSITION_VERTICAL :
                                               POPPLER_PAGE_TRANSITION_HORIZONTAL) &&
         page.transition_seconds == 1.25 && page.duration_ms == 4000 &&
         !page.cube_transition ? EXIT_SUCCESS : EXIT_FAILURE;
}
