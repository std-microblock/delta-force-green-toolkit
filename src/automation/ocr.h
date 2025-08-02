#pragma once
#include "tesseract/baseapi.h"
#include "tesseract/publictypes.h"
#include "opencv2/opencv.hpp"

namespace dfg {
struct OCR {
  OCR();
  ~OCR();
  void initialize();
  std::optional<std::string> recognize_text_eng(const cv::Mat &image);

private:
  std::unique_ptr<tesseract::TessBaseAPI> api;
};
} // namespace dfg