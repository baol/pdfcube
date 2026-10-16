#pragma once

#include "options.hpp"
#include <gio/gio.h>

namespace pdfcube {
inline GApplicationFlags application_flags(const Options& options) {
  return options.snapshot.empty() && options.record_raw.empty() ? G_APPLICATION_DEFAULT_FLAGS : G_APPLICATION_NON_UNIQUE;
}
}
