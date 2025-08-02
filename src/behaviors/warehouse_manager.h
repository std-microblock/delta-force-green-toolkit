#pragma once
#include "opencv2/opencv.hpp"

#include "../utils/derive_format.hpp"
namespace dfg {
struct App;

struct WarehouseManager {
  App &app;

  struct GridDetectionResult {
    int start_x, start_y;
    int cell_width, cell_height;

    cv::Mat visualize(cv::Mat on);
  };

  GridDetectionResult detect_warehouse_grid();
  
  struct ItemInfo: derive_format {
    // Item position in warehouse grid
    int x, y;
    int width, height;

    // Item quality
    // 0 - unknown | 1 - white | 2 - green | 3 - blue | 4 - purple | 5 - orange | 6 - red
    int quality;

    // 军需处回收价格
    int price_system_buy;

    // 交易行税后价格
    int price_market;
    bool can_sell_in_market = false;
  };

  std::vector<ItemInfo> get_items();
};
} // namespace dfg
