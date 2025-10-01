#pragma once

#include <deque>
#include <ftxui/component/component.hpp>

#include "nvtuner.h"

class GraphsTab {
 public:
  struct GpuGraphData {
    std::deque<int> util;
    std::deque<int> mem;
    std::deque<int> gpu_clock;
    std::deque<int> temp;
    int max_supported_gpu_clock = 9999;

    static const size_t MAX_HISTORY = 360;

    void add_sample(const GpuState& gs);
    std::vector<int> get_normalized(const std::string& type, int width,
                                    int height) const;
    std::vector<int> get_normalized_util(int width, int height) const;
    std::vector<int> get_normalized_mem(int width, int height) const;
    std::vector<int> get_normalized_gpu_clock(int width, int height) const;
    std::vector<int> get_normalized_temp(int width, int height) const;
  };

 private:
  int selected_gpu_ = 0;
  std::vector<std::string> menu_entries_;
  std::vector<GpuGraphData> gpu_history_;

  const std::vector<GpuState>& gpu_states_;

  ftxui::Component main_component_;
  ftxui::Component menu_component_;
  ftxui::Components subtab_components_;

 public:
  explicit GraphsTab(const std::vector<GpuState>& gpu_states);
  void update();
  ftxui::Component get_component() { return main_component_; };

 private:
  ftxui::Element create_chart(
      const std::string& title, const std::string& max_label,
      const std::string& min_label,
      std::function<std::vector<int>(int, int)> data_func,
      ftxui::Color chart_color);
};
