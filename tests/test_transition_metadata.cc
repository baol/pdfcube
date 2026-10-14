#include "document.hpp"
#include <cstdlib>

int main(int argc, char** argv) {
  if (argc != 3) return EXIT_FAILURE;
  pdfcube::Document document(argv[1]);
  const auto& page = document.page_info(1);
  return page.transition_type == POPPLER_PAGE_TRANSITION_BLINDS &&
         page.alignment == (argv[2][0] == 'V' ? POPPLER_PAGE_TRANSITION_VERTICAL :
                                               POPPLER_PAGE_TRANSITION_HORIZONTAL) &&
         page.transition_seconds == 1.25 && page.duration_ms == 4000 &&
         !page.cube_transition ? EXIT_SUCCESS : EXIT_FAILURE;
}
