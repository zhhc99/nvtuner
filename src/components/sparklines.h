#pragma once

#include <deque>
#include <ftxui/component/component.hpp>

#include "nvtuner.h"

struct GpuStateSamples {
  std::deque<int> util;
  std::deque<int> gpu_clock;
  std::deque<int> temp;
  std::deque<int> power;

  static const size_t MAX_SAMPLES = 60;

  void add_sample(const GpuState& gs);
};

class Sparklines {
 private:
  std::vector<GpuStateSamples> samples_;
  const std::vector<GpuState>& gpu_states_;
  ftxui::Components sparkline_window_focus_components_;  // or naming whatever
  ftxui::Components sparkline_windows_;
  ftxui::Component main_component_;

 public:
  explicit Sparklines(const std::vector<GpuState>& gpu_states);
  void update();
  ftxui::Component get_component() { return main_component_; };

  static std::string get_sparkline_string(const std::deque<int>& data,
                                          int min_val, int max_val);

 private:
  ftxui::Component create_window(size_t gpu_index);
};
