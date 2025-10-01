#include "graphs_tab.h"

#include <fmt/core.h>

#include <algorithm>
#include <iostream>

using namespace ftxui;

// -----------------------------------------------------------------------------
// GpuGraphData
// -----------------------------------------------------------------------------

void GraphsTab::GpuGraphData::add_sample(const GpuState& gs) {
  util.push_back(gs.gpu_util_percent);
  mem.push_back(gs.mem_util_percent);
  gpu_clock.push_back(gs.gpu_clock_mhz);
  temp.push_back(gs.temperature_c);

  if (util.size() > MAX_HISTORY) {
    util.pop_front();
    mem.pop_front();
    gpu_clock.pop_front();
    temp.pop_front();
  }
}

std::vector<int> GraphsTab::GpuGraphData::get_normalized(
    const std::string& type, int width, int height) const {
  std::vector<int> result(width, 0);
  const auto& data = (type == "util")        ? util
                     : (type == "mem")       ? mem
                     : (type == "gpu_clock") ? gpu_clock
                                             : temp;

  if (data.empty()) return result;

  size_t data_size = data.size();
  std::vector<int> processed_data(width, 0);
  size_t copy_size = std::min(static_cast<size_t>(width), data.size());
  size_t start_pos = width - copy_size;

  if (data_size <= width) {
    processed_data.assign(data.begin(), data.end());
  } else {
    processed_data.assign(data.end() - width, data.end());
  }
  processed_data.resize(width, 0);

  int scale_min = 0;
  int scale_max = (type == "util")        ? 100
                  : (type == "mem")       ? 100
                  : (type == "gpu_clock") ? max_supported_gpu_clock
                                          : 100;  // temp

  for (size_t i = 0; i < width; ++i) {
    int normalized =
        (processed_data[i] - scale_min) * height / (scale_max - scale_min);
    result[i] = std::clamp(normalized, 0, height);
  }

  return result;
}

std::vector<int> GraphsTab::GpuGraphData::get_normalized_util(
    int width, int height) const {
  return get_normalized("util", width, height);
}

std::vector<int> GraphsTab::GpuGraphData::get_normalized_mem(int width,
                                                             int height) const {
  return get_normalized("mem", width, height);
}

std::vector<int> GraphsTab::GpuGraphData::get_normalized_gpu_clock(
    int width, int height) const {
  return get_normalized("gpu_clock", width, height);
}

std::vector<int> GraphsTab::GpuGraphData::get_normalized_temp(
    int width, int height) const {
  return get_normalized("temp", width, height);
}

// -----------------------------------------------------------------------------
// GraphsTab
// -----------------------------------------------------------------------------

GraphsTab::GraphsTab(const std::vector<GpuState>& gpu_states)
    : gpu_states_(gpu_states) {
  gpu_history_.resize(gpu_states_.size());
  for (size_t i = 0; i < gpu_states_.size(); ++i) {
    gpu_history_[i].max_supported_gpu_clock = gpu_states_[i].gpu_max_clock_mhz;
  }

  menu_entries_.clear();
  for (size_t i = 0; i < gpu_states_.size(); ++i) {
    menu_entries_.push_back(std::to_string(i));
  }
  menu_component_ = Menu(&menu_entries_, &selected_gpu_);

  subtab_components_.clear();
  for (size_t i = 0; i < gpu_states_.size(); ++i) {
    auto util_func = [this, i](int width, int height) {
      return gpu_history_[i].get_normalized_util(width, height);
    };
    auto mem_func = [this, i](int width, int height) {
      return gpu_history_[i].get_normalized_mem(width, height);
    };
    auto gpu_clock_func = [this, i](int width, int height) {
      return gpu_history_[i].get_normalized_gpu_clock(width, height);
    };
    auto temp_func = [this, i](int width, int height) {
      return gpu_history_[i].get_normalized_temp(width, height);
    };

    auto subtab_component = Renderer([=] {
      std::string util_title =
          fmt::format("Util {}%", gpu_states_[i].gpu_util_percent);
      std::string mem_title =
          fmt::format("Memory {}%", gpu_states_[i].mem_util_percent);
      std::string gpu_clock_title =
          fmt::format("GPU Clock {} MHz", gpu_states_[i].gpu_clock_mhz);
      std::string temp_title =
          fmt::format("Temperature {} C", gpu_states_[i].temperature_c);
      auto util_chart =
          create_chart(fmt::format("Util {}%", gpu_states_[i].gpu_util_percent),
                       "100", "0", util_func, Color::BlueLight);
      auto mem_chart = create_chart(
          fmt::format("Memory {}%", gpu_states_[i].mem_util_percent), "100",
          "0", mem_func, Color::GreenLight);
      auto gpu_clock_chart = create_chart(
          fmt::format("GPU Clock {} MHz", gpu_states_[i].gpu_clock_mhz),
          fmt::format("{}", gpu_states_[i].gpu_max_clock_mhz), "0",
          gpu_clock_func, Color::White);
      auto temp_chart = create_chart(
          fmt::format("Temperature {} C", gpu_states_[i].temperature_c), "100",
          "0", temp_func, Color::Orange1);

      return hbox({
                 vbox({
                     util_chart | flex,
                     separator(),
                     temp_chart | flex,
                 }) | flex,
                 separator(),
                 vbox({
                     gpu_clock_chart | flex,
                     separator(),
                     mem_chart | flex,
                 }) | flex,
             }) |
             flex;
    });

    subtab_components_.push_back(subtab_component);
  }

  auto subtabs_container = Container::Tab(subtab_components_, &selected_gpu_);
  main_component_ = Renderer(menu_component_, [this, subtabs_container] {
    return hbox({
        vbox({
            text(fmt::format("GPU {}", selected_gpu_)),
            separator(),
            menu_component_->Render(),
        }),
        separator(),
        subtabs_container->Render(),
    });
  });
}

void GraphsTab::update() {
  for (size_t i = 0; i < gpu_states_.size() && i < gpu_history_.size(); ++i) {
    gpu_history_[i].add_sample(gpu_states_[i]);
  }
}

Element GraphsTab::create_chart(
    const std::string& title, const std::string& max_label,
    const std::string& min_label,
    std::function<std::vector<int>(int, int)> data_func, Color chart_color) {
  return vbox({
      text(title) | hcenter,
      hbox({
          vbox({
              text(max_label),
              filler(),
              text(min_label),
          }),
          graph(data_func) | color(chart_color),
      }) | flex,
  });
}
