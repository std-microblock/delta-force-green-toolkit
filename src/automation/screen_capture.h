#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp> 

namespace dfg {

class ScreenCapture {
public:
    ScreenCapture() = default;
    ~ScreenCapture() = default;

    cv::Mat capture_screen();

    cv::Mat capture_window(HWND hwnd);

private:
    
};

} 