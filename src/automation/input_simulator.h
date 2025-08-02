#pragma once

#include <functional>
#include <string>
#include <windows.h>

namespace dfg {

class InputSimulator {
public:
  InputSimulator() = default;
  ~InputSimulator() = default;

  void move_to(
      int x, int y, int duration_ms = 200,
      std::function<float(float)> interp = [](float t) {
        return t < 0.5 ? 2 * t * t : 1 - pow(-2 * t + 2, 2) / 2;
      });

  void move_relative(int dx, int dy);
  POINT get_mouse_position();
  void mouse_event(DWORD event_flags, DWORD data = 0);
  void left_click();
  void right_click();
  void key_press(WORD vk_code);
  void key_release(WORD vk_code);
  void key_tap(WORD vk_code);
  void type_text(const std::string &text);
  void wheel_scroll(int delta);

private:
};

} // namespace dfg