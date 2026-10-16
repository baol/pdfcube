#include "instance.hpp"

#include <cassert>

int main() {
  pdfcube::Options options;
  assert(pdfcube::application_flags(options) == G_APPLICATION_DEFAULT_FLAGS);
  options.snapshot = "frame.png";
  const auto flags = pdfcube::application_flags(options);
  assert(flags == G_APPLICATION_NON_UNIQUE);
  // Two real application registrations with the same ID must both remain
  // local. No GUI, X server, animation, or timed pause is needed to prove it.
  auto* first = g_application_new("org.pdfcube.pdfcube",flags);
  auto* second = g_application_new("org.pdfcube.pdfcube",flags);
  GError* error = nullptr;
  assert(g_application_register(first,nullptr,&error));
  assert(error == nullptr && !g_application_get_is_remote(first));
  assert(g_application_register(second,nullptr,&error));
  assert(error == nullptr && !g_application_get_is_remote(second));
  g_object_unref(second);
  g_object_unref(first);
}
