#include "main.h"
#include "cpptrace/basic.hpp"
#include "opencv2/highgui.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <exception>
#include <filesystem>
#include <unordered_map>

namespace dfg {
cv::Mat App::capture_dfwin() {
  if (!df_window) {
    throw std::runtime_error("Delta Force window not initialized");
  }
  auto res = screen_capture.capture_window(df_window);
  if (res.empty()) {
    throw std::runtime_error("Failed to capture Delta Force window");
  }

  if (scale != 1) {
    cv::resize(res, res, cv::Size(), scale, scale, cv::INTER_LINEAR);
  }

  return res;
}

void App::init() {
  ocr.initialize();

  df_window = FindWindowW(L"UnrealWindow", nullptr);
  if (!df_window) {
    throw std::runtime_error("Delta Force window not found");
  }

  RECT rect;
  if (!GetWindowRect(df_window, &rect)) {
    throw std::runtime_error("Failed to get Delta Force window rect");
  }

  int width = rect.right - rect.left;
  int height = rect.bottom - rect.top;
  if (width <= 0 || height <= 0) {
    throw std::runtime_error("Invalid Delta Force window size");
  }

  // unreal engine uses shorter side to calculate scale
  scale = std::min(develop_df_width, develop_df_height) /
          (float)std::min(width, height);
  std::println("[app] Delta Force Window size: {}x{}, scale: {:.2f}",
               width, height, scale);
}

App::App() {}
cv::Mat App::load_img(std::string path) {
  static std::unordered_map<std::string, cv::Mat> img_cache;
  if (img_cache.find(path) == img_cache.end()) {
    cv::Mat img = cv::imread("./images/" + path, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
      throw std::runtime_error("Failed to load image: " + path);
    }

    img_cache[path] = img;
  }
  return img_cache[path];
}
void App::move_to_abs(int x, int y) {
  RECT rect;
  if (!GetWindowRect(df_window, &rect)) {
    throw std::runtime_error("Failed to get Delta Force window rect");
  }

  // scale back
  x = static_cast<int>(x / scale) + rect.left;
  y = static_cast<int>(y / scale) + rect.top;
  input_simulator.move_to(x, y, 50);
}
std::optional<cv::Rect> App::locate_image_rect(std::string path,
                                               float threshold) {
  cv::Mat img = load_img(path);
  cv::Mat screen = capture_dfwin();
  if (screen.empty()) {
    throw std::runtime_error("Failed to capture Delta Force window");
  }

  // grayscale the images
  cv::Mat img_gray, screen_gray;
  cv::cvtColor(img, img_gray, cv::COLOR_BGR2GRAY);
  cv::cvtColor(screen, screen_gray, cv::COLOR_BGR2GRAY);
  // match template
  cv::Mat result;
  cv::matchTemplate(screen_gray, img_gray, result, cv::TM_CCOEFF_NORMED);
  // find locations with high enough correlation
  cv::Point match_loc;
  double max_val;
  cv::minMaxLoc(result, nullptr, &max_val, nullptr, &match_loc);
  if (max_val < threshold) {
    return {};
  }

  return cv::Rect(match_loc.x, match_loc.y, img.cols, img.rows);
}
cv::Point App::rect_to_relpos(cv::Rect rect, RelPos pos) {
  switch (pos) {
  case RelPos::TopLeft:
    return {rect.x, rect.y};
  case RelPos::TopRight:
    return {rect.x + rect.width, rect.y};
  case RelPos::BottomLeft:
    return {rect.x, rect.y + rect.height};
  case RelPos::BottomRight:
    return {rect.x + rect.width, rect.y + rect.height};
  case RelPos::Center:
    return {rect.x + rect.width / 2, rect.y + rect.height / 2};
  case RelPos::MiddleLeft:
    return {rect.x, rect.y + rect.height / 2};
  case RelPos::MiddleRight:
    return {rect.x + rect.width, rect.y + rect.height / 2};
  case RelPos::MiddleTop:
    return {rect.x + rect.width / 2, rect.y};
  case RelPos::MiddleBottom:
    return {rect.x + rect.width / 2, rect.y + rect.height};
  }
  return {0, 0};
}
void App::sleep(int ms, float randomize_rate) {
  int sleep_time = ms;
  if (randomize_rate > 0) {
    sleep_time +=
        static_cast<int>(ms * randomize_rate * (rand() % 100 / 100.0f));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
}
void App::focus_df() {
  if (!df_window) {
    throw std::runtime_error("Delta Force window not found");
  }
  SetForegroundWindow(df_window);
  SetFocus(df_window);
}
std::optional<cv::Rect> App::wait_for_image_rect(std::string path, int max_wait,
                                                 float threshold) {
  auto start_time = std::chrono::steady_clock::now();
  while (true) {
    auto rect = locate_image_rect(path, threshold);
    if (rect) {
      return rect;
    }
    if (std::chrono::steady_clock::now() - start_time >
        std::chrono::milliseconds(max_wait)) {
      return {};
    }
    sleep(10);
  }
}
std::optional<cv::Point> App::locate_image(std::string path, RelPos result_pos,
                                           float threshold) {
  auto rect = locate_image_rect(path, threshold);
  if (!rect) {
    return {};
  }
  return rect_to_relpos(*rect, result_pos);
}
std::optional<cv::Point> App::wait_for_image(std::string path,
                                             RelPos result_pos, int max_wait,
                                             float threshold) {
  auto rect = wait_for_image_rect(path, max_wait, threshold);
  if (!rect) {
    return std::optional<cv::Point>();
  }
  return rect_to_relpos(*rect, result_pos);
}
} // namespace dfg

int onOpenCVError(int status, const char *func_name, const char *err_msg,
                  const char *file_name, int line, void *userdata) {
  throw std::runtime_error(std::string("OpenCV Error: ") + err_msg + " in " +
                           func_name + ", file " + file_name + ", line " +
                           std::to_string(line));
}

int main(int argc, char *argv[]) {
  SetConsoleOutputCP(CP_UTF8);
  cv::redirectError(onOpenCVError);
  std::string exe_path(argv[0]);
  std::filesystem::current_path(std::filesystem::path(exe_path).parent_path());

  std::set_terminate([]() { cpptrace::stacktrace::current().print(); });

  dfg::App app;
  CPPTRACE_TRY {
    app.init();
    auto mat = app.capture_dfwin();
    std::println("items: {}", app.warehouse_manager.get_items());
  }
  CPPTRACE_CATCH(const std::runtime_error &e) {
    std::cerr << e.what() << std::endl;
    cpptrace::from_current_exception().print();
    return 1;
  }
}