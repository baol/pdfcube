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
  const auto absolute_u8 = absolute.u8string();
  const std::string absolute_utf8(absolute_u8.begin(), absolute_u8.end());
  GError* error = nullptr;
  char* uri = g_filename_to_uri(absolute_utf8.c_str(), nullptr, &error);
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

  try {
    const int count = poppler_document_get_n_pages(document_);
    if (count < 1) {
      throw std::runtime_error("The PDF contains no pages");
    }
    pages_.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
      auto page_owner = std::unique_ptr<PopplerPage, decltype(&g_object_unref)>(
          poppler_document_get_page(document_, index), &g_object_unref);
      PopplerPage* page = page_owner.get();
      if (page == nullptr) throw std::runtime_error("Cannot read PDF page " + std::to_string(index + 1));
      PageInfo info;
      poppler_page_get_size(page, &info.width, &info.height);
      if (!std::isfinite(info.width) || !std::isfinite(info.height) ||
          info.width <= 0 || info.height <= 0)
        throw std::runtime_error("Invalid dimensions on PDF page " + std::to_string(index + 1));
      const double display_seconds = poppler_page_get_duration(page);
      if (std::isfinite(display_seconds) && display_seconds > 0)
        info.duration_ms = static_cast<int>(std::min(display_seconds, 86400.0) * 1000);
      if (PopplerPageTransition* transition = poppler_page_get_transition(page)) {
        info.cube_transition = transition->type == POPPLER_PAGE_TRANSITION_BOX;
        info.transition_type = transition->type;
        info.alignment = transition->alignment;
        info.direction = transition->direction;
        info.transition_seconds = std::isfinite(transition->duration_real)
            ? std::clamp(transition->duration_real, 0.0, 86400.0) : 1.0;
        info.angle = transition->angle;
        info.scale = transition->scale;
        info.rectangular = transition->rectangular;
        poppler_page_transition_free(transition);
      }
      pages_.push_back(info);
    }
  } catch (...) {
    g_object_unref(document_);
    document_ = nullptr;
    throw;
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
  const PageInfo& info = page_info(page_number);
  auto page_owner = std::unique_ptr<PopplerPage, decltype(&g_object_unref)>(
      poppler_document_get_page(document_, page_number), &g_object_unref);
  PopplerPage* page = page_owner.get();
  if (page == nullptr) throw std::runtime_error("Cannot render PDF page " + std::to_string(page_number + 1));
  const double scale = std::max(1, maximum_size) / std::max(info.width, info.height);
  const int width = static_cast<int>(std::clamp(std::ceil(info.width * scale), 1.0, static_cast<double>(std::max(1, maximum_size))));
  const int height = static_cast<int>(std::clamp(std::ceil(info.height * scale), 1.0, static_cast<double>(std::max(1, maximum_size))));
  Surface surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height));
  if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
    throw std::runtime_error(cairo_status_to_string(cairo_surface_status(surface.get())));
  auto context_owner = std::unique_ptr<cairo_t, decltype(&cairo_destroy)>(
      cairo_create(surface.get()), &cairo_destroy);
  cairo_t* context = context_owner.get();
  cairo_set_source_rgb(context, 1.0, 1.0, 1.0);
  cairo_paint(context);
  cairo_scale(context, scale, scale);
  poppler_page_render(page, context);
  if (cairo_status(context) != CAIRO_STATUS_SUCCESS)
    throw std::runtime_error(cairo_status_to_string(cairo_status(context)));
  cairo_surface_flush(surface.get());
  return surface;
}

}  // namespace pdfcube

