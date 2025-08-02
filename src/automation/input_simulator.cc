#include "input_simulator.h"
#include <chrono>
#include <thread>

namespace dfg {

void InputSimulator::move_to(int x, int y, int duration_ms,
                             std::function<float(float)> interp) {
  POINT current_pos = get_mouse_position();
  int start_x = current_pos.x;
  int start_y = current_pos.y;

  if (duration_ms <= 0) {

    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.dx = (x * 65535) / GetSystemMetrics(SM_CXSCREEN);
    input.mi.dy = (y * 65535) / GetSystemMetrics(SM_CYSCREEN);
    SendInput(1, &input, sizeof(INPUT));
    return;
  }

  auto start_time = std::chrono::high_resolution_clock::now();
  auto end_time = start_time + std::chrono::milliseconds(duration_ms);

  while (std::chrono::high_resolution_clock::now() < end_time) {
    auto now = std::chrono::high_resolution_clock::now();
    float t = static_cast<float>(
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      now - start_time)
                      .count()) /
              duration_ms;
    t = std::min(1.0f, std::max(0.0f, t));

    float interpolated_t = interp(t);

    int current_x = static_cast<int>(start_x + (x - start_x) * interpolated_t);
    int current_y = static_cast<int>(start_y + (y - start_y) * interpolated_t);

    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.dx = (current_x * 65535) / GetSystemMetrics(SM_CXSCREEN);
    input.mi.dy = (current_y * 65535) / GetSystemMetrics(SM_CYSCREEN);
    SendInput(1, &input, sizeof(INPUT));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  INPUT input = {0};
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
  input.mi.dx = (x * 65535) / GetSystemMetrics(SM_CXSCREEN);
  input.mi.dy = (y * 65535) / GetSystemMetrics(SM_CYSCREEN);
  SendInput(1, &input, sizeof(INPUT));
}

void InputSimulator::move_relative(int dx, int dy) {
  INPUT input = {0};
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = MOUSEEVENTF_MOVE;
  input.mi.dx = dx;
  input.mi.dy = dy;
  SendInput(1, &input, sizeof(INPUT));
}

POINT InputSimulator::get_mouse_position() {
  POINT p;
  GetCursorPos(&p);
  return p;
}

void InputSimulator::mouse_event(DWORD event_flags, DWORD data) {
  INPUT input = {0};
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = event_flags;
  input.mi.mouseData = data;
  SendInput(1, &input, sizeof(INPUT));
}

void InputSimulator::left_click() {
  mouse_event(MOUSEEVENTF_LEFTDOWN);
  mouse_event(MOUSEEVENTF_LEFTUP);
}

void InputSimulator::right_click() {
  mouse_event(MOUSEEVENTF_RIGHTDOWN);
  mouse_event(MOUSEEVENTF_RIGHTUP);
}

void InputSimulator::key_press(WORD vk_code) {
  INPUT input = {0};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = 0;
  input.ki.wScan = MapVirtualKeyA(vk_code, 0);
  input.ki.dwFlags = KEYEVENTF_SCANCODE;
  SendInput(1, &input, sizeof(INPUT));
}

void InputSimulator::key_release(WORD vk_code) {
  INPUT input = {0};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = 0;
  input.ki.wScan = MapVirtualKeyA(vk_code, 0);
  input.ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_SCANCODE;
  SendInput(1, &input, sizeof(INPUT));
}

void InputSimulator::key_tap(WORD vk_code) {
  key_press(vk_code);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  key_release(vk_code);
}

void InputSimulator::type_text(const std::string &text) {
  for (char c : text) {
    if (c >= 32 && c <= 126) {
      SHORT vk = VkKeyScanA(c);
      if (vk != -1) {
        WORD vk_code = LOBYTE(vk);
        bool shift_pressed = HIBYTE(vk) & 1;

        if (shift_pressed) {
          key_press(VK_LSHIFT);
        }
        key_tap(vk_code);
        if (shift_pressed) {
          key_release(VK_LSHIFT);
        }
      }
    } else {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

void InputSimulator::wheel_scroll(int delta) {
  INPUT input = {0};
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = MOUSEEVENTF_WHEEL;
  input.mi.mouseData = delta;
  SendInput(1, &input, sizeof(INPUT));
}
} // namespace dfg