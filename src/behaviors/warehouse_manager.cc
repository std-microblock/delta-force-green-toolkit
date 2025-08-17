#include "warehouse_manager.h"
#include "../main.h"
#include "opencv2/highgui.hpp"

namespace dfg {
WarehouseManager::GridDetectionResult
WarehouseManager::detect_warehouse_grid() {
  using RelPos = App::RelPos;
  auto left_top =
      app.locate_image("warehouse/warehouse_lefttop.png", RelPos::TopRight);
  auto right_top =
      app.locate_image("warehouse/warehouse_righttop.png", RelPos::BottomLeft);

  if (!left_top || !right_top) {
    throw std::runtime_error("Warehouse grid not found");
  }

  auto wh_width = right_top->x - left_top->x;
  auto cell_width = (int)std::ceil(wh_width / 9.0f);

  return {left_top->x, left_top->y, cell_width, cell_width};
}
cv::Mat WarehouseManager::GridDetectionResult::visualize(cv::Mat on) {
  int cols = 9;
  int rows = 9;
  for (int i = 0; i < cols; ++i) {
    cv::line(on, cv::Point(start_x + i * cell_width, start_y),
             cv::Point(start_x + i * cell_width, start_y + rows * cell_height),
             cv::Scalar(0, 255, 0), 2);
  }
  for (int i = 0; i < rows; ++i) {
    cv::line(on, cv::Point(start_x, start_y + i * cell_height),
             cv::Point(start_x + cols * cell_width, start_y + i * cell_height),
             cv::Scalar(0, 255, 0), 2);
  }
  return on;
}

struct EscapePresser {
  App &app;
  bool pressed = false;

  EscapePresser(App &app) : app(app) {}

  ~EscapePresser() { press(); }

  void press() {
    if (!pressed) {
      std::println("[warehouse] pressing escape to exit item view");
      app.sleep(300);
      app.input_simulator.key_tap(VK_ESCAPE);
      pressed = true;
    }
  }
};

static constexpr int max_sell_quality = 4;
static constexpr int min_delta_price_to_sell_on_market = 6000;
static constexpr int max_down_price = 7;

int color_similarity_lab(const cv::Vec3b &a, const cv::Vec3b &b) {
  cv::Mat lab_a, lab_b;
  cv::cvtColor(cv::Mat(1, 1, CV_8UC3, a), lab_a, cv::COLOR_BGR2Lab);
  cv::cvtColor(cv::Mat(1, 1, CV_8UC3, b), lab_b, cv::COLOR_BGR2Lab);
  return cv::norm(lab_a, lab_b, cv::NORM_L2);
}

std::vector<WarehouseManager::ItemInfo> WarehouseManager::get_items() {
  app.focus_df();
  std::vector<ItemInfo> items;
  auto grid_info = detect_warehouse_grid();
  std::vector<std::vector<bool>> grid(100, std::vector<bool>(9, false));

  auto pointInGrid = cv::Point{grid_info.start_x + grid_info.cell_width / 2,
                               grid_info.start_y + grid_info.cell_height / 2};

  auto pointOutOfGrid =
      cv::Point{grid_info.start_x - grid_info.cell_width / 2,
                grid_info.start_y - grid_info.cell_height / 2};

  app.move_to_abs(pointInGrid);

  app.input_simulator.wheel_scroll(10000);
  app.sleep(300);
  grid_info = detect_warehouse_grid();

  // cv::imshow("Warehouse Grid", grid_info.visualize(app.capture_dfwin()));
  // cv::waitKey(0);

  int current_y_grid = 1, warehouse_size = 0;
  bool in_batch_sell_mode = false;
  auto recognize_current_scrollbar = [&]() {
    auto cap = app.capture_dfwin();
    constexpr int scrollarea_height_normal = 848;
    constexpr int scrollarea_height_batchsellmode = 803;
    int scrollarea_height = in_batch_sell_mode ? scrollarea_height_batchsellmode
                                               : scrollarea_height_normal;

    auto analyze_scrollbar = [&](const cv::Mat &img) {
      auto scrollbar_area =
          img(cv::Rect{grid_info.start_x + 9 * grid_info.cell_width, 130, 25,
                       scrollarea_height});
      auto gray_scrollbar_area = cv::Mat();
      cv::cvtColor(scrollbar_area, gray_scrollbar_area, cv::COLOR_BGR2GRAY);
      auto gray_binary_max_scrollbar_area = cv::Mat();
      cv::threshold(gray_scrollbar_area, gray_binary_max_scrollbar_area, 95,
                    130, cv::THRESH_BINARY);

      // extract the scrollbar position and height
      int scrollbar_height = 0;
      int scrollbar_y = 0;
      for (int y = 0; y < gray_binary_max_scrollbar_area.rows; y++) {
        if (cv::countNonZero(gray_binary_max_scrollbar_area.row(y)) > 2) {
          scrollbar_height++;
          if (scrollbar_y == 0)
            scrollbar_y = y;

        } else if (scrollbar_height > 0) {
          break;
        }
      }

      return std::make_tuple(scrollbar_y, scrollbar_height);
    };

    static std::optional<float> sb_scale_normal, sb_scale_batchsellmode;
    auto analyze_scroll_scale = [&]() {
      auto cap1 = app.capture_dfwin();
      auto [y1, h1] = analyze_scrollbar(cap1);
      app.move_to_abs(pointInGrid);
      for (int i = 0; i < 5; ++i) {
        app.input_simulator.wheel_scroll(y1 < 100 ? -WHEEL_DELTA : WHEEL_DELTA);
        app.sleep(100);
      }
      auto cap2 = app.capture_dfwin();
      auto [y2, h2] = analyze_scrollbar(cap2);
      for (int i = 0; i < 5; ++i) {
        app.input_simulator.wheel_scroll(y1 > 100 ? -WHEEL_DELTA : WHEEL_DELTA);
        app.sleep(100);
      }

      std::println("[warehouse] anal: {} {} {} {}", y1, h1, y2, h2);

      return std::abs((float)grid_info.cell_height * 5 / (y1 - y2));
    };

    if (in_batch_sell_mode && !sb_scale_batchsellmode)
      sb_scale_batchsellmode = analyze_scroll_scale();
    if (!in_batch_sell_mode && !sb_scale_normal)
      sb_scale_normal = analyze_scroll_scale();

    float sb_scale = in_batch_sell_mode ? sb_scale_batchsellmode.value()
                                        : sb_scale_normal.value();

    auto [scrollbar_y, scrollbar_height] = analyze_scrollbar(cap);

    warehouse_size = (scrollarea_height - scrollbar_height) * sb_scale /
                         grid_info.cell_height +
                     13 - 1;
    current_y_grid = scrollbar_y * sb_scale / grid_info.cell_height;

    std::println("[warehouse] recognized current_y_grid: {}, warehouse_size: "
                 "{} (scrolly {} h {} ht {})",
                 current_y_grid, warehouse_size, scrollbar_y, scrollbar_height,
                 scrollarea_height);
  };

  // move the y-th grid to the top
  auto scroll_to_y = [&](int y) {
    recognize_current_scrollbar();
  retry:
    if (current_y_grid == y) {
      return;
    }
    std::println("[warehouse] scrolling to y: {}", y);
    int delta = y - current_y_grid;
    app.move_to_abs(pointInGrid);
    for (int i = 0; i < std::abs(delta); ++i) {
      app.input_simulator.wheel_scroll(delta < 0 ? WHEEL_DELTA : -WHEEL_DELTA);
      app.sleep(90);
    }
    recognize_current_scrollbar();
    if (current_y_grid != y) {
      goto retry;
    }
  };

  auto grid_rect = [&](int x, int y) {
    return cv::Rect{grid_info.start_x + x * grid_info.cell_width,
                    grid_info.start_y +
                        (y - current_y_grid) * grid_info.cell_height,
                    grid_info.cell_width, grid_info.cell_height};
  };

  auto grid_center = [this, &grid_rect](int x, int y) {
    return app.rect_to_relpos(grid_rect(x, y), App::RelPos::Center);
  };

  auto reach_grid = [&](int x, int y) {
    recognize_current_scrollbar();
    constexpr static int max_cell_rows = 10;
    if (y >= current_y_grid + max_cell_rows || y < current_y_grid) {
      scroll_to_y(y);
    }

    return grid_center(x, y);
  };
  scroll_to_y(0);
  for (int y = 0; y < warehouse_size - 14; y++) {
    scroll_to_y(y < 2 ? y : y - 1);
    app.sleep(100);

    app.move_to_abs(pointOutOfGrid);
    app.sleep(60);
    auto img_no_highlight = app.capture_dfwin();
    for (int x = 0; x < 9; x++) {
      if (grid[y][x]) {
        continue;
      }

      std::println("[warehouse] proceeding: {} {}", x, y);

      // do a canny to determine if it's an empty slot fast
      auto rect = grid_rect(x, y);
      auto slot = img_no_highlight(rect);
      cv::Mat gray_slot;
      cv::cvtColor(slot, gray_slot, cv::COLOR_BGR2GRAY);
      cv::Mat edges;
      cv::Canny(gray_slot, edges, 50, 150);

      if (cv::countNonZero(edges) < 7) {
        std::println("[warehouse] slot {} {} is empty", x, y);
        continue;
      }

      app.move_to_abs(reach_grid(x, y));
      app.sleep(30);

      auto img_highlight = app.capture_dfwin();
      // check the slots of the item
      cv::Mat diff;
      cv::absdiff(img_no_highlight, img_highlight, diff);
      cv::cvtColor(diff, diff, cv::COLOR_BGR2GRAY);
      cv::threshold(diff, diff, 5, 30, cv::THRESH_BINARY);
      // cv::imshow("diff", diff);
      // cv::waitKey(0);
      std::vector<std::pair<int, int>> slots_thisitem;
      for (int x_slot = 0; x_slot < 6; ++x_slot) {
        for (int y_slot = 0; y_slot < 6; ++y_slot) {
          auto rect = grid_rect(x_slot + x, y_slot + y);
          try {
            auto slot = diff(rect);
            if (cv::countNonZero(slot) > 500) {
              slots_thisitem.emplace_back(x_slot + x, y_slot + y);
            }
          } catch (const std::exception &e) {
          }
        }
      }

      if (slots_thisitem.empty()) {
        continue;
      }

      int left = 1000, right = -1, top = 1000, bottom = -1;
      for (const auto &slot : slots_thisitem) {
        left = std::min(left, slot.first);
        right = std::max(right, slot.first);
        top = std::min(top, slot.second);
        bottom = std::max(bottom, slot.second);
      }

      for (int i = left; i <= right; ++i) {
        for (int j = top; j <= bottom; ++j) {
          grid[j][i] = true;
        }
      }

      cv::Rect item_rect = cv::Rect{
          grid_info.start_x + left * grid_info.cell_width,
          grid_info.start_y + (top - current_y_grid) * grid_info.cell_height,
          (right - left + 1) * grid_info.cell_width,
          (bottom - top + 1) * grid_info.cell_height};

      std::println("[warehouse] item pos {},{} grid: {}x{}", left, top,
                   right - left + 1, bottom - top + 1);

      auto item_img = img_no_highlight(item_rect);
      // cv::imshow("item", item_img);
      // cv::waitKey(0);
      // determine quality by color
      static std::vector<std::pair<uint64_t, uint8_t>> color_quality_map = {
          {0x1a1f22, 1}, {0x1a2824, 2}, {0x22313d, 3},
          {0x262634, 4}, {0x352b24, 5}, {0x3c2224, 6}};

      uint8_t quality = 0;
      int mindiff = 0xffff;
      auto color = item_img.at<cv::Vec4b>(item_img.rows / 2, item_img.cols - 3);
      int r1 = color[2];
      int g1 = color[1];
      int b1 = color[0];

      // item_img.at<cv::Vec4b>(3, item_img.cols - 3) = cv::Vec4b{0, 0, 255,
      // 255}; cv::imshow("item color", item_img); cv::waitKey(0);

      std::println("[warehouse] item color: #{:02x}{:02x}{:02x}", r1, g1, b1);

      for (const auto &pair : color_quality_map) {
        int r2 = (pair.first >> 16) & 0xFF;
        int g2 = (pair.first >> 8) & 0xFF;
        int b2 = pair.first & 0xFF;

        int diff = color_similarity_lab(
            cv::Vec3b{(uint8_t)b1, (uint8_t)g1, (uint8_t)r1},
            cv::Vec3b{(uint8_t)b2, (uint8_t)g2, (uint8_t)r2});

        if (diff < mindiff) {
          quality = pair.second;
          mindiff = diff;
        }
      }

      if (quality > max_sell_quality) {
        continue;
      }

      app.input_simulator.left_click();
      app.sleep(100);

      EscapePresser escape_presser(app);
      auto btn_sell = app.wait_for_image("warehouse/btn_sell.png");
      if (btn_sell) {
        app.move_to_abs(btn_sell.value());
        app.input_simulator.left_click();
        auto system_price_line =
            app.wait_for_image_rect("warehouse/sell_ui/text_system_price.png");
        auto market_price_line =
            app.wait_for_image_rect("warehouse/sell_ui/text_market_price.png");

        // screenshot from the price line to the end of the line
        auto system_price_rect =
            cv::Rect(system_price_line->x + system_price_line->width,
                     system_price_line->y, 400, system_price_line->height);
        auto market_price_rect =
            cv::Rect(market_price_line->x + market_price_line->width,
                     market_price_line->y, 400, market_price_line->height);

        auto screenshot = app.capture_dfwin();
        auto system_price_img = screenshot(system_price_rect);
        auto market_price_img = screenshot(market_price_rect);

        auto system_price_text = app.ocr.recognize_text_eng(system_price_img);
        auto market_price_text = app.ocr.recognize_text_eng(market_price_img);

        if (system_price_text && market_price_text) {
          ItemInfo item;
          item.x = left;
          item.y = top;
          item.width = right - left + 1;
          item.height = bottom - top + 1;

          // parse the price
          try {
            auto extractNumber = [](const std::string &text) {
              std::string num_str;
              for (char c : text) {
                if (isdigit(c)) {
                  num_str += c;
                }
              }
              return std::stoi(num_str);
            };

            item.price_system_buy = extractNumber(system_price_text.value());
            item.price_market = extractNumber(market_price_text.value());

            auto rect_sell_in_market = screenshot(
                app.locate_image_rect("warehouse/sell_ui/btn_sell_market.png")
                    .value());

            // if the button is green, it can be sold in market
            // else it is gray, it cannot
            cv::Scalar avg_color = cv::mean(rect_sell_in_market);
            std::println("[warehouse] avg color: {} {} {}", avg_color[0],
                         avg_color[1], avg_color[2]);
            item.can_sell_in_market = std::abs(avg_color[0] - avg_color[1]) < 3;

            escape_presser.press();

            item.quality = quality;
          } catch (const std::exception &e) {
            item.price_system_buy = 0;
            item.price_market = 0;
            item.can_sell_in_market = false;
          }

          std::println("[warehouse] item found: {}", item);

          items.push_back(item);
        }
      }

      app.sleep(100);
    }
  }

  std::vector<ItemInfo> items_to_sell_in_market;
  std::vector<ItemInfo> items_to_sell_system;

  for (const auto &item : items) {
    if (item.quality <= max_sell_quality) {
      if (item.can_sell_in_market && item.price_market - item.price_system_buy >
                                         min_delta_price_to_sell_on_market) {
        items_to_sell_in_market.push_back(item);
      } else {
        items_to_sell_system.push_back(item);
      }
    }
  }

  // batch sell items to be sold to system
  if (items_to_sell_system.size() > 1) {
    in_batch_sell_mode = true;
    app.move_to_abs(app.wait_for_image("warehouse/btn_batch_sell.png").value());
    app.input_simulator.left_click();
    app.sleep(100);

    for (const auto &item : items_to_sell_system) {
      app.move_to_abs(reach_grid(item.x, item.y));
      app.sleep(50);
      app.input_simulator.left_click();
      app.sleep(80);
    }
    app.sleep(300);
    app.move_to_abs(
        app.wait_for_image("warehouse/btn_batch_sell_sell.png").value());
    app.input_simulator.left_click();
    app.sleep(500);
    // app.move_to_abs(
    //     app.wait_for_image("warehouse/btn_batch_sell_confirm.png").value());
    // app.input_simulator.left_click();
    // app.sleep(500);
    while (app.locate_image("warehouse/btn_batch_sell_confirm.png",
                            App::RelPos::Center, 0.8f)) {
      app.sleep(100);
    }

    app.sleep(400);
    app.input_simulator.key_tap(VK_ESCAPE);
    in_batch_sell_mode = false;
  } else if (items_to_sell_system.size() == 1) {
    auto item = items_to_sell_system[0];
    app.move_to_abs(reach_grid(item.x, item.y));
    app.input_simulator.left_click();
    app.sleep(100);

    auto btn_sell = app.wait_for_image("warehouse/btn_sell.png");
    if (btn_sell) {
      app.move_to_abs(btn_sell.value());
      app.input_simulator.left_click();
      app.sleep(100);

      auto btn_sell_system =
          app.wait_for_image("warehouse/sell_ui/btn_sell_system.png");
      if (btn_sell_system) {
        app.move_to_abs(btn_sell_system.value());
        app.sleep(500);
        app.input_simulator.left_click();
      }
    }
  }

  app.sleep(500);

  // sell items to market one by one
  for (const auto &item : items_to_sell_in_market) {
    app.move_to_abs(reach_grid(item.x, item.y));
    app.input_simulator.left_click();
    app.sleep(100);

    auto btn_sell = app.wait_for_image("warehouse/btn_sell.png");
    if (btn_sell) {
      app.move_to_abs(btn_sell.value());
      app.input_simulator.left_click();
      app.sleep(100);

      auto btn_sell_market =
          app.wait_for_image("warehouse/sell_ui/btn_sell_market.png");
      if (btn_sell_market) {
        app.move_to_abs(btn_sell_market.value());
        app.sleep(400);
        app.input_simulator.left_click();
        app.sleep(100);
        app.move_to_abs(100, 100);
        app.sleep(200);

        auto btn_minus =
            app.wait_for_image("warehouse/btn_sell_market_minus_price.png");
        if (btn_minus) {
          app.move_to_abs(btn_minus.value());
          app.sleep(100);
          auto start = app.capture_dfwin();
          int iDownPrice = 0;
          while (true) {
            if (iDownPrice++ > max_down_price) {
              break;
            }
            app.input_simulator.left_click();
            app.sleep(50);
            auto end = app.capture_dfwin();
            cv::Mat diff;
            cv::absdiff(start, end, diff);
            cv::cvtColor(diff, diff, cv::COLOR_BGR2GRAY);
            cv::threshold(diff, diff, 5, 30, cv::THRESH_BINARY);
            // only compare the lower 65%
            if (cv::countNonZero(
                    diff(cv::Rect(0, diff.rows * 0.35, diff.cols * 0.57,
                                  diff.rows * 0.65 - 1))) > 600) {
              break;
            }
          }

          auto btn_upshelf =
              app.wait_for_image("warehouse/btn_sell_market_upshelf.png");

          if (btn_upshelf) {
            app.move_to_abs(btn_upshelf.value());
            app.input_simulator.left_click();
            app.sleep(300);
            continue;
          }
        }
      }
    }

    std::println("[warehouse] failed to sell item: {}", item);
    app.move_to_abs(10, 10);
    app.input_simulator.left_click();
    app.sleep(300);
  }

  return items;
}

} // namespace dfg