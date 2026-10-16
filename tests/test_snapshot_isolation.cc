#include "instance.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <thread>

namespace {
int child() {
  pdfcube::Options options;
  options.snapshot = "frame.png";
  auto* application = g_application_new("org.pdfcube.pdfcube",pdfcube::application_flags(options));
  GError* error = nullptr;
  assert(g_application_register(application,nullptr,&error));
  assert(error == nullptr && !g_application_get_is_remote(application));
#ifdef __linux__
  // Meson supplies a private session bus. Do not silently exercise only the
  // no-bus fallback, which hid the original test bug in the cloud environment.
  assert(g_application_get_dbus_connection(application) != nullptr);
#endif
  auto* loop = g_main_loop_new(nullptr,false);
  std::thread release([loop] {
    std::string line;
    std::getline(std::cin,line);
    g_idle_add(+[](gpointer data) -> gboolean {
      g_main_loop_quit(static_cast<GMainLoop*>(data));
      return G_SOURCE_REMOVE;
    },loop);
  });
  std::cout << "READY\n" << std::flush;
  g_main_loop_run(loop);
  release.join();
  g_main_loop_unref(loop);
  g_object_unref(application);
  return 0;
}

struct Snapshot {
  GSubprocess* process;
  explicit Snapshot(const char* executable) {
    GError* error = nullptr;
    process = g_subprocess_new(static_cast<GSubprocessFlags>(
        G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE),
        &error,executable,"--child",nullptr);
    assert(process != nullptr && error == nullptr);
    auto* output = g_data_input_stream_new(g_subprocess_get_stdout_pipe(process));
    auto* line = g_data_input_stream_read_line(output,nullptr,nullptr,&error);
    assert(error == nullptr && line != nullptr && std::string(line) == "READY");
    g_free(line);
    g_object_unref(output);
  }
  ~Snapshot() {
    GError* error = nullptr;
    auto* input = g_subprocess_get_stdin_pipe(process);
    assert(g_output_stream_write_all(input,"\n",1,nullptr,nullptr,&error));
    assert(g_output_stream_close(input,nullptr,&error));
    assert(g_subprocess_wait_check(process,nullptr,&error));
    assert(error == nullptr);
    g_object_unref(process);
  }
};
}

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--child") return child();
  pdfcube::Options options;
  assert(pdfcube::application_flags(options) == G_APPLICATION_DEFAULT_FLAGS);
  options.snapshot = "frame.png";
  const auto flags = pdfcube::application_flags(options);
  assert(flags == G_APPLICATION_NON_UNIQUE);
  // Real snapshot invocations are separate processes/connections, not two
  // GApplications exporting the same object path on one shared connection.
  // Keep the first registered and dispatching until the second is also ready.
  Snapshot first(argv[0]);
  Snapshot second(argv[0]);
}
