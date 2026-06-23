#include "webview/webview.h"
#include <iostream>
#include <string>

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

    // 1. 设置 Webview 的默认背景颜色为网页背景色 (RGB: 15, 23, 42)，避免拉伸窗口时白光闪烁
    w.set_background_color(15, 23, 42, 255);

    // 1. 设置本地静态文件夹映射
    // 将域名 "app.local" 映射到当前执行目录下的 "./resources" 目录
    w.set_assets_mapping("app.local", "./resources");

    // 2. 绑定 RPC 双向通信函数
    // 前端直接调用 `window.callCppRPC("some parameter")` 将触发该 lambda
    w.bind("callCppRPC", [&](const std::string &req) -> std::string {
      std::cout << "C++ Received: " << req << std::endl;
      // 使用自带的 json_escape 进行安全转义，生成合法 JSON 字符串
      return webview::detail::json_escape("Hello from C++! You sent: " + req);
    });

    // 3. 导航到我们的本地虚拟域名页面
    // 相对路径下的静态资源加载（CSS、JS、图片）依然通过相对路径获取
    w.navigate("app://app.local/index.html");

    w.run();
  } catch (const webview::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  return 0;
}
