#include "dashboard.h"

#include <fmt/core.h>

using namespace ftxui;

static Element clock_event_text(
    const std::string& code,
    const std::optional<std::chrono::system_clock::time_point>& event_time,
    Color active_color) {
  std::string time_suffix;
  if (!event_time) {
    time_suffix = "-";
  } else {
    auto t_sec = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now() - *event_time)
                     .count();
    if (t_sec < 0) t_sec = 0;
    if (t_sec >= 86400) {
      time_suffix = fmt::format("{}d", t_sec / 86400);
    } else if (t_sec >= 3600) {
      time_suffix = fmt::format("{}h", t_sec / 3600);
    } else if (t_sec >= 60) {
      time_suffix = fmt::format("{}m", t_sec / 60);
    } else {
      time_suffix = fmt::format("{}s", t_sec);
    }
  }
  std::string fmt_text = fmt::format("{}:{}", code, time_suffix);

  if (!event_time) return text(fmt_text) | dim;
  return text(fmt_text) | color(active_color);
}

Component Dashboard(const std::vector<GpuState>& gpu_states) {
  return Renderer([&gpu_states] {
    Elements columns;

    Elements gpu_column;
    gpu_column.push_back(text("GPU") | bold);
    gpu_column.push_back(separator());
    for (const auto& gs : gpu_states) {
      gpu_column.push_back(text(fmt::format("{}", gs.index)));
    }
    columns.push_back(vbox(gpu_column) | size(WIDTH, EQUAL, 3));
    columns.push_back(separator());

    Elements name_column;
    int terminal_width = Terminal::Size().dimx;
    bool use_short_name = terminal_width < 112;
    name_column.push_back(text("Name") | bold);
    name_column.push_back(separator());
    for (const auto& gs : gpu_states) {
      name_column.push_back(text(use_short_name ? gs.name_short : gs.name));
    }
    columns.push_back(vbox(name_column) | flex);
    columns.push_back(separator());

    Elements util_column;
    util_column.push_back(text("Util") | bold);
    util_column.push_back(separator());
    for (const auto& gs : gpu_states) {
      Color util_color = Color::Green;
      if (gs.gpu_util_percent >= 80)
        util_color = Color::Red;
      else if (gs.gpu_util_percent >= 50)
        util_color = Color::Yellow;
      util_column.push_back(text(fmt::format("{}%", gs.gpu_util_percent)) |
                            color(util_color));
    }
    columns.push_back(vbox(util_column) | size(WIDTH, EQUAL, 4));
    columns.push_back(separator());

    Elements memory_column;
    memory_column.push_back(text("Memory") | bold);
    memory_column.push_back(separator());
    for (const auto& gs : gpu_states) {
      int mem_percent =
          gs.mem_total_mib ? (gs.mem_used_mib * 100 / gs.mem_total_mib) : 0;
      Color mem_color = Color::Green;
      if (mem_percent >= 80)
        mem_color = Color::Red;
      else if (mem_percent >= 50)
        mem_color = Color::Yellow;
      memory_column.push_back(
          text(fmt::format("{}/{}MiB {}%", gs.mem_used_mib, gs.mem_total_mib,
                           gs.mem_total_mib
                               ? gs.mem_used_mib * 100 / gs.mem_total_mib
                               : 0)) |
          color(mem_color));
    }
    columns.push_back(vbox(memory_column) | size(WIDTH, GREATER_THAN, 18));
    columns.push_back(separator());

    Elements clock_column;
    clock_column.push_back(text("Clock") | bold);
    clock_column.push_back(separator());
    for (const auto& gs : gpu_states) {
      clock_column.push_back(text(fmt::format("{}MHz", gs.gpu_clock_mhz)));
    }
    columns.push_back(vbox(clock_column) | size(WIDTH, EQUAL, 7));
    columns.push_back(separator());

    Elements power_column;
    power_column.push_back(text("Power") | bold);
    power_column.push_back(separator());
    for (const auto& gs : gpu_states) {
      power_column.push_back(
          text(fmt::format("{}/{}W", gs.power_usage_w, gs.power_limit_w)));
    }
    columns.push_back(vbox(power_column) | size(WIDTH, EQUAL, 11));
    columns.push_back(separator());

    Elements temp_column;
    temp_column.push_back(text("Temp") | bold);
    temp_column.push_back(separator());
    for (const auto& gs : gpu_states) {
      temp_column.push_back(text(fmt::format("{}C", gs.temperature_c)));
    }
    columns.push_back(vbox(temp_column) | size(WIDTH, EQUAL, 4));
    columns.push_back(separator());

    Elements fan_column;
    fan_column.push_back(text("Fan") | bold);
    fan_column.push_back(separator());
    for (const auto& gs : gpu_states) {
      std::string fs = gs.fan_speed_percent < 0
                           ? "N/A"
                           : fmt::format("{}%", gs.fan_speed_percent);
      std::string fs_rpm =
          gs.fan_speed_rpm < 0 ? "N/A" : fmt::format("{}R", gs.fan_speed_rpm);
      if (gs.fan_speed_rpm < 0) {
        fan_column.push_back(text(fs));
      } else {
        fan_column.push_back(text(fmt::format("{} {}", fs, fs_rpm)));
      }
    }
    columns.push_back(vbox(fan_column) | size(WIDTH, GREATER_THAN, 4) |
                      size(WIDTH, LESS_THAN, 13));
    columns.push_back(separator());

    Elements clock_event_column;
    clock_event_column.push_back(text("Clock Event") | bold);
    clock_event_column.push_back(separator());
    for (const auto& gs : gpu_states) {
      clock_event_column.push_back(hbox({
          clock_event_text("PC", gs.last_event_power_cap_time, Color::Blue),
          text(" "),
          clock_event_text("ST", gs.last_event_swt_slowdown_time,
                           Color::Yellow),
          text(" "),
          clock_event_text("HT", gs.last_event_hwt_slowdown_time, Color::Red),
      }));
    }
    columns.push_back(vbox(clock_event_column) | size(WIDTH, GREATER_THAN, 18));

    return vbox(hbox(columns) | border);
  });
}
