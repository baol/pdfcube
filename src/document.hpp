#pragma once

#include <cairo.h>
#include <poppler.h>

#include <memory>
#include <string>
#include <vector>

namespace pdfcube {

struct PageInfo {
  double width{};
  double height{};
  bool cube_transition{false};
  int duration_ms{0};
  PopplerPageTransitionType transition_type{POPPLER_PAGE_TRANSITION_REPLACE};
  PopplerPageTransitionAlignment alignment{POPPLER_PAGE_TRANSITION_HORIZONTAL};
  PopplerPageTransitionDirection direction{POPPLER_PAGE_TRANSITION_INWARD};
  double transition_seconds{0.0};
  int angle{0};
  double scale{1.0};
  bool rectangular{false};
};

struct SurfaceDeleter {
  void operator()(cairo_surface_t* surface) const;
};
using Surface = std::unique_ptr<cairo_surface_t, SurfaceDeleter>;

class Document {
 public:
  explicit Document(const std::string& filename);
  ~Document();

  Document(const Document&) = delete;
  Document& operator=(const Document&) = delete;

  [[nodiscard]] int page_count() const;
  [[nodiscard]] const PageInfo& page_info(int page) const;
  [[nodiscard]] Surface render_page(int page, int maximum_size) const;

 private:
  PopplerDocument* document_{nullptr};
  std::vector<PageInfo> pages_;
};

}  // namespace pdfcube

