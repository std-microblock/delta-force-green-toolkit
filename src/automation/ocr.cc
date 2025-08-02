#include "ocr.h"

#include "Windows.h"

#include "leptonica/allheaders.h"
#include <leptonica/pix.h>
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>

struct Pix_Box {
  l_int32 x;
  l_int32 y;
  l_int32 w;
  l_int32 h;
  l_uint32 refcount; /* reference count (1 if no clones)  */
};

namespace dfg {
OCR::OCR() { api = std::make_unique<tesseract::TessBaseAPI>(); }
OCR::~OCR() { api->End(); }
void OCR::initialize() {
  api->Init("./models/", "eng", tesseract::OEM_LSTM_ONLY);
}
static std::string charPtrToString(const char *ptr) {
  if (!ptr) {
    return "";
  }
  std::string str(ptr);
  delete[] ptr;
  return str;
}

std::optional<std::string> OCR::recognize_text_eng(const cv::Mat &image) {
  auto image_p = image.clone();

  if (cv::mean(image_p)[0] < 128) {
    cv::bitwise_not(image_p, image_p);
  }

  if (image_p.channels() > 1) {
    cv::cvtColor(image_p, image_p, cv::COLOR_BGR2GRAY);
  }

  api->SetImage(image_p.data, image_p.cols, image_p.rows, image_p.channels(),
                image_p.step[0]);
  return charPtrToString(api->GetUTF8Text());
}
} // namespace dfg