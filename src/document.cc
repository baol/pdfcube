#include "document.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace pdfcube {

void SurfaceDeleter::operator()(cairo_surface_t* surface) const {
  if (surface != nullptr) {
    cairo_surface_destroy(surface);
  }
}

Document::Document(const std::string& filename) {
  const auto absolute = std::filesystem::absolute(filename);
  GError* error = nullptr;
  char* uri = g_filename_to_uri(absolute.c_str(), nullptr, &error);
  if (uri == nullptr) {
    const std::string message = error != nullptr ? error->message : "invalid filename";
    g_clear_error(&error);
    throw std::runtime_error("Cannot open PDF: " + message);
  }
  document_ = poppler_document_new_from_file(uri, nullptr, &error);
  g_free(uri);
  if (document_ == nullptr) {
    const std::string message = error != nullptr ? error->message : "invalid PDF";
    g_clear_error(&error);
    throw std::runtime_error("Cannot open PDF: " + message);
  }

  const int count = poppler_document_get_n_pages(document_);
  if (count < 1) {
    g_object_unref(document_);
    document_ = nullptr;
    throw std::runtime_error("The PDF contains no pages");
  }
  pages_.reserve(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    PopplerPage* page = poppler_document_get_page(document_, index);
    PageInfo info;
    poppler_page_get_size(page, &info.width, &info.height);
    if (PopplerPageTransition* transition = poppler_page_get_transition(page)) {
      info.cube_transition = transition->type == POPPLER_PAGE_TRANSITION_BOX;
      info.duration_ms = std::max(0, transition->duration * 1000);
      poppler_page_transition_free(transition);
    }
    pages_.push_back(info);
    g_object_unref(page);
  }
}

Document::~Document() {
  if (document_ != nullptr) {
    g_object_unref(document_);
  }
}

int Document::page_count() const { return static_cast<int>(pages_.size()); }

const PageInfo& Document::page_info(int page) const {
  return pages_.at(static_cast<std::size_t>(page));
}

Surface Document::render_page(int page_number, int maximum_size) const {
  PopplerPage* page = poppler_document_get_page(document_, page_number);
  const PageInfo& info = page_info(page_number);
  const double scale = std::max(1, maximum_size) / std::max(info.width, info.height);
  const int width = std::max(1, static_cast<int>(std::ceil(info.width * scale)));
  const int height = std::max(1, static_cast<int>(std::ceil(info.height * scale)));
  Surface surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height));
  cairo_t* context = cairo_create(surface.get());
  cairo_set_source_rgb(context, 1.0, 1.0, 1.0);
  cairo_paint(context);
  cairo_scale(context, scale, scale);
  poppler_page_render(page, context);
  cairo_destroy(context);
  g_object_unref(page);
  cairo_surface_flush(surface.get());
  return surface;
}

}  // namespace pdfcube

