#include "webview/webview.h"

#include <iostream>
#include <string>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

// Returns the directory of the running executable without a trailing separator.
// On macOS this is the directory containing the executable inside the app
// bundle (e.g. Foo.app/Contents/MacOS/). On Windows and Linux it is the
// directory of the .exe / ELF binary. This lets the example locate its assets
// regardless of the process' current working directory.
static std::string exe_dir() {
#if defined(__APPLE__)
  char path[4096];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) != 0) {
    return ".";
  }
  std::string p(path);
#elif defined(_WIN32)
  wchar_t wpath[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, wpath, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return ".";
  }
  std::string p(webview::detail::narrow_string(std::wstring(wpath, len)));
#else
  char path[4096];
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len <= 0) {
    return ".";
  }
  path[len] = '\0'; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
  std::string p(path);
#endif
  auto slash = p.find_last_of("/\\");
  if (slash == std::string::npos) {
    return ".";
  }
  return p.substr(0, slash);
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE /*hInst*/, HINSTANCE /*hPrevInst*/,
                   LPSTR /*lpCmdLine*/, int /*nCmdShow*/) {
#else
int main() {
#endif
  try {
    webview::webview w(true, nullptr);
    w.set_title("Assets Mapping & RPC Example");
    w.set_size(800, 600, WEBVIEW_HINT_NONE);

    // 1. Set the webview background to match the page background (RGB: 15, 23,
    //    42) so resizing the window does not flash white under a dark theme.
    w.set_background_color(15, 23, 42, 255);

    // 2. Map the virtual host "app.local" to the local assets folder shipped
    //    with the example. The URL scheme is fixed to app://.
    //    - macOS: assets live in the app bundle at Contents/Resources/
    //      (the executable is in Contents/MacOS/).
    //    - Windows/Linux: assets live in a "resources" folder next to the
    //      executable.
#if defined(__APPLE__)
    std::string resources = exe_dir() + "/../Resources";
#else
    std::string resources = exe_dir() + "/resources";
#endif
    w.set_assets_mapping("app.local", resources);

    // 3. Register an RPC handler reusing the existing bind() API. The front end
    //    can call it via window.callCppRPC("some parameter").
    w.bind("callCppRPC", [&](const std::string &req) -> std::string {
      std::cout << "C++ Received: " << req << std::endl;
      // json_escape produces a valid JSON string literal.
      return webview::detail::json_escape("Hello from C++! You sent: " + req);
    });

    // 4. Navigate to the virtual host. Relative asset references in the page
    //    (CSS, JS, images) are resolved through the same app:// mapping.
    w.navigate("app://app.local/index.html");

    w.run();
  } catch (const webview::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  return 0;
}
