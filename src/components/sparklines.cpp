#include "sparklines.h"

#include <fmt/core.h>

#include <algorithm>
#include <numeric>

using namespace ftxui;
using namespace fmt;

void GpuStateSamples::add_sample(const GpuState& gs) {
  util.push_back(gs.gpu_util_percent);
  gpu_clock.push_back(gs.gpu_clock_mhz);
  temp.push_back(gs.temperature_c);
  power.push_back(gs.power_usage_w);

  if (util.size() > MAX_SAMPLES) {
    util.pop_front();
    gpu_clock.pop_front();
    temp.pop_front();
    power.pop_front();
  }
}

Sparklines::Sparklines(const std::vector<GpuState>& gpu_states)
    : gpu_states_(gpu_states), samples_(gpu_states.size()) {
  for (size_t i = 0; i < gpu_states.size(); i++) {
    sparkline_windows_.push_back(create_window(i));
  }

  main_component_ = Container::Vertical(sparkline_windows_);
}

void Sparklines::update() {
  for (size_t i = 0; i < gpu_states_.size(); i++) {
    samples_[i].add_sample(gpu_states_[i]);
  }
}

std::string Sparklines::get_sparkline_string(const std::deque<int>& data,
                                             int min_val, int max_val) {
  // static const std::vector<std::string> BLOCK_ELEMS = {" ", "▁", "▂", "▃",
  // "▄", "▅", "▆", "▇", "█"};
  static const std::vector<std::string> BLOCK_ELEMS = {"▁", "▂", "▃", "▄",
                                                       "▅", "▆", "▇"};
  std::string result = "";
  for (auto& v : data) {
    double normalized_value = (v - min_val) * 1.0 / (max_val - min_val);
    normalized_value = std::clamp(normalized_value, 0.0, 1.0);

    result += BLOCK_ELEMS[int(normalized_value * (BLOCK_ELEMS.size() - 0.01))];
  }

  return result;
}

Component Sparklines::create_window(size_t gpu_index) {
  return Renderer([this, gpu_index](bool focused) {
    const auto& gs = gpu_states_[gpu_index];
    const auto& sample = samples_[gpu_index];

    auto text_sparkline = [](const std::deque<int>& data, int min_val,
                             int max_val) {
      return text(get_sparkline_string(data, min_val, max_val));
    };
    auto dq_max = [](const std::deque<int>& dq) {
      return *std::max_element(dq.begin(), dq.end());
    };
    auto dq_avg = [](const std::deque<int>& dq) {
      return std::accumulate(dq.begin(), dq.end(), 0) / dq.size();
    };

    return window(
               text(fmt::format("GPU {}: {}", gs.index, gs.name)),
               hbox({
                   vbox({
                       text("Metric"),
                       separator(),
                       text("Util.   [%]"),
                       text("Clock [MHz]"),
                       text("Temp.   [C]"),
                       text("Power   [W]"),
                   }),
                   separator(),
                   vbox({
                       text("Sparkline (60s)") |
                           size(WIDTH, EQUAL, GpuStateSamples::MAX_SAMPLES),
                       separator(), text_sparkline(sample.util, 0, 100),
                       text_sparkline(sample.gpu_clock, 0,
                                      gs.gpu_max_clock_mhz),
                       text_sparkline(sample.temp, 0, 100),
                       text_sparkline(sample.power, 0, 100),
                   }),
                   separator(),
                   vbox({
                       text("Now "),
                       separator(),
                       text(format("{}", sample.util.back())),
                       text(format("{}", sample.gpu_clock.back())),
                       text(format("{}", sample.temp.back())),
                       text(format("{}", sample.power.back())),
                   }),
                   separator(),
                   vbox({
                       text("Max "), separator(),
                       text(format("{}", dq_max(sample.util))),
                       text(format("{}", dq_max(sample.gpu_clock))),
                       text(format("{}", dq_max(sample.temp))),
                       text(format("{}", dq_max(sample.power))),
                   }),
                   separator(),
                   vbox({
                       text("Avg "), separator(),
                       text(format("{}", dq_avg(sample.util))),
                       text(format("{}", dq_avg(sample.gpu_clock))),
                       text(format("{}", dq_avg(sample.temp))),
                       text(format("{}", dq_avg(sample.power))),
                   }) | flex,
               })) |
           (focused ? focus : dim);
  });
}
