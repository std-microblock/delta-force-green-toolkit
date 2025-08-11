#pragma once
#include "opencv2/opencv.hpp"
#include <functional>
#include <iostream>
#include <print>

#include "./automation/ocr.h"
#include "./automation/input_simulator.h"
#include "./automation/screen_capture.h"

#include "./behaviors/warehouse_manager.h"

#include <windows.h>

namespace dfg {

struct App {
  InputSimulator input_simulator;
  ScreenCapture screen_capture;
  OCR ocr;

  HWND df_window;
  // Scale is the scale from the real window size to the size when reference
  // image is taken (develop_df_width, develop_df_height). The bigger the window
  // is, the smaller the scale is. This is used to scale mouse coordinates and
  // image matching.
  float scale = 1;
  static constexpr int develop_df_width = 1920;
  static constexpr int develop_df_height = 1080;

  WarehouseManager warehouse_manager{*this};

  App();
  void init();
  // The image is scaled to the develop_df_width
  // and develop_df_height, so it can be used by image matching algorithms.
  cv::Mat capture_dfwin();
  cv::Mat load_img(std::string path);
  // Move mouse to absolute position in Delta Force window coordinates
  // This function will also process the scale factor.
  void move_to_abs(int x, int y);
  inline void move_to_abs(cv::Point p) { move_to_abs(p.x, p.y); }

  void sleep(int ms, float randomize_rate = 0.2);
  void focus_maximize_df();

  enum class RelPos {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center,
    MiddleLeft,
    MiddleRight,
    MiddleTop,
    MiddleBottom
  };

  std::optional<cv::Rect> locate_image_rect(std::string path,
                                            float threshold = 0.1f);
  std::optional<cv::Point> locate_image(std::string path,
                                        RelPos result_pos = RelPos::Center,
                                        float threshold = 0.1f);
  std::optional<cv::Rect> wait_for_image_rect(std::string path,
                                              int max_wait = 1000,
                                              float threshold = 0.7f);
  std::optional<cv::Point> wait_for_image(std::string path,
                                          RelPos result_pos = RelPos::Center,
                                          int max_wait = 1000,
                                          float threshold = 0.7f);
  cv::Point rect_to_relpos(cv::Rect rect, RelPos pos);

  void focus_df();
};
} // namespace dfg
