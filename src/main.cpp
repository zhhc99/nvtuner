#include <fmt/chrono.h>
#include <fmt/core.h>

#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <iostream>
#include <mutex>
#include <vector>

#include "nvtuner.h"
#include "sys_utils.h"

#ifndef _WIN32
#include <sys/types.h>  // For uid_t
#include <unistd.h>     // For geteuid()
#endif

using namespace ftxui;

#ifndef APP_VERSION  // defined in CMake
#define APP_VERSION "unknown"
#endif

std::vector<std::string> log_messages;
std::mutex log_mutex;

class LogStreamBuffer : public std::stringbuf {
 public:
  explicit LogStreamBuffer(std::string prefix, std::ofstream& log_file)
      : prefix_(std::move(prefix)), log_file_(log_file) {}
  ~LogStreamBuffer() override { pubsync(); }

 protected:
  int sync() override {
    std::string content = str();
    if (content.empty()) {
      return 0;
    }

    str("");

    size_t start = 0;
    while (true) {
      size_t end = content.find('\n', start);
      if (end == std::string::npos) {
        sputn(content.c_str() + start, content.length() - start);
        break;
      }
      std::string prefixed_line = prefix_ + content.substr(start, end - start);

      {
        std::lock_guard<std::mutex> lock(log_mutex);
        log_messages.push_back(prefixed_line);
        if (log_messages.size() > 100) {
          log_messages.erase(log_messages.begin(),
                             log_messages.begin() + log_messages.size() - 90);
        }
      }

      if (log_file_.is_open()) {
        log_file_ << prefixed_line << std::endl;
      }
      start = end + 1;
    }
    return 0;
  }

 private:
  std::string prefix_;
  std::ofstream& log_file_;
};

class StreamRedirector {
 public:
  StreamRedirector(std::ostream& stream, std::streambuf* new_buf)
      : stream_(stream), old_buf_(stream.rdbuf(new_buf)) {}

  ~StreamRedirector() { stream_.rdbuf(old_buf_); }

  StreamRedirector(const StreamRedirector&) = delete;
  StreamRedirector& operator=(const StreamRedirector&) = delete;

 private:
  std::ostream& stream_;
  std::streambuf* old_buf_;
};

int main(int argc, char* argv[]) {
#ifndef _WIN32
  if (geteuid() != 0) {
    std::cerr << "Error: nvtuner must be run with root privileges "
                 "(e.g., using sudo)."
              << std::endl;
    return 1;
  }
#endif

  std::unique_ptr<NvmlManager> nvml;
  try {
    nvml = std::make_unique<NvmlManager>();
  } catch (const std::exception& e) {
    std::cerr << "Fatal: Cannot initialize NVML: " << e.what() << std::endl;
    return 1;
  }

#ifdef _WIN32
  std::string profile_path =
      (std::filesystem::path(SysUtils::get_executable_dir()) / "nvtuner.json")
          .string();
  std::string log_path =
      (std::filesystem::path(SysUtils::get_executable_dir()) / "nvtuner.log")
          .string();
#else
  std::filesystem::create_directories("/etc/nvtuner");
  std::string profile_path = "/etc/nvtuner/nvtuner.json";
  std::string log_path = "/etc/nvtuner/nvtuner.log";
#endif

  if (argc == 2 && std::string(argv[1]) == "--apply-profiles") {
    ProfileManager pm(profile_path, nvml->get_gpus());
    nvml->apply_profiles(pm.get_all_profiles());
    std::clog.flush();
    std::cout << "Execution finished. For results, see message above."
              << std::endl;
    return 0;
  }

  std::ofstream log_file(SysUtils::make_path_string(log_path), std::ios::app);
  if (log_file.is_open()) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    log_file << "\n--- Session Started on "
             << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X")
             << " ---\n"
             << std::endl;
  }
  LogStreamBuffer clog_log_buffer("[Msg] ", log_file);
  LogStreamBuffer cerr_log_buffer("[Err] ", log_file);
  StreamRedirector redirect_clog(std::clog, &clog_log_buffer);
  StreamRedirector redirect_cerr(std::cerr, &cerr_log_buffer);

  if (!log_file.is_open()) {
    std::cerr << "Failed to open log. File logging will be disabled."
              << std::endl;
  }

  ProfileManager pm(profile_path, nvml->get_gpus());

  // ---------------------------------------------------------------------------
  // OC Panels
  // ---------------------------------------------------------------------------
  std::vector<Component> oc_panels;
  std::vector<bool> oc_profile_modified;
  for (size_t i = 0; i < nvml->get_gpus().size(); ++i) {
    oc_profile_modified.push_back(false);

    auto& gs = nvml->get_gpus()[i];
    auto& profile = pm.get_profile(gs.uuid);

    int power_step = 1;
    int clock_step = 15;
    int max_clock_min_mhz = 210;
    const int total_width = 30;

    auto power_limit_slider =
        Slider("", &profile.power_limit, &gs.power_limit_min_w,
               &gs.power_limit_max_w, &power_step);
    auto gpu_clock_offset_slider =
        Slider("", &profile.gpu_clock_offset, &gs.clock_offset_min_mhz,
               &gs.clock_offset_max_mhz, &clock_step);
    auto gpu_max_clock_slider =
        Slider("", &profile.max_gpu_clock, &max_clock_min_mhz,
               &gs.gpu_max_clock_mhz, &clock_step);

    auto oc_panel_component = Container::Vertical({
        power_limit_slider,
        gpu_clock_offset_slider,
        gpu_max_clock_slider,
    });

    auto oc_panel = Renderer(oc_panel_component, [&] {
      std::string pl_label = "Power Limit";
      std::string pl_value = fmt::format("{}W", profile.power_limit);
      int pl_width = total_width - pl_label.length();
      std::string gco_label = "GPU Clock Offset";
      std::string gco_value = fmt::format("{:+}MHz", profile.gpu_clock_offset);
      int gco_width = total_width - gco_label.length();
      std::string gmc_label = "GPU Max Clock";
      std::string gmc_value = fmt::format("{}MHz", profile.max_gpu_clock);
      int gmc_width = total_width - gmc_label.length();
      return vbox({
          hbox({
              text(fmt::format("GPU {}: {} ", gs.index, gs.name)) | bold,
              separatorCharacter("|"),
              text(fmt::format(" OC ({}W, {:+}MHz, <={}MHz)",
                               profile.power_limit, profile.gpu_clock_offset,
                               profile.max_gpu_clock)) |
                  dim,
          }),
          hbox({
              text(fmt::format("{0}{1:>{2}}", pl_label, pl_value, pl_width)) |
                  size(WIDTH, EQUAL, total_width + 1),
              power_limit_slider->Render() | flex,
          }),
          hbox({
              text(
                  fmt::format("{0}{1:>{2}}", gco_label, gco_value, gco_width)) |
                  size(WIDTH, EQUAL, total_width + 1),
              gpu_clock_offset_slider->Render() | flex,
          }),
          hbox({
              text(
                  fmt::format("{0}{1:>{2}}", gmc_label, gmc_value, gmc_width)) |
                  size(WIDTH, EQUAL, total_width + 1),
              gpu_max_clock_slider->Render() | flex,
          }),
      });
    });

    oc_panels.push_back(oc_panel);
  }

  // ---------------------------------------------------------------------------
  // OC Action Buttons
  // ---------------------------------------------------------------------------
  auto oc_buttons = Container::Horizontal({
      Button("Save and Apply All",
             [&nvml, &pm] {
               pm.save();
               nvml->apply_profiles(pm.get_all_profiles());
             }),
      Button("Register Service",
             [&] {
               if (SysUtils::register_startup_task()) {
                 std::clog << "Startup service registered successfully."
                           << std::endl;
               } else {
                 std::cerr << "Failed to register startup service. Run as root?"
                           << std::endl;
               }
             }),
      Button("Remove Service",
             [&] {
               if (SysUtils::unregister_startup_task()) {
                 std::clog << "Startup service removed successfully."
                           << std::endl;
               } else {
                 std::cerr << "Failed to remove startup service. Run as root?"
                           << std::endl;
               }
             }),
  });

  // ---------------------------------------------------------------------------
  // Log Console
  // ---------------------------------------------------------------------------
  const int log_console_msg_count = 6;
  auto log_console = Renderer([&] {
    Elements lines;
    {
      std::lock_guard<std::mutex> lock(log_mutex);
      int start_index = std::max(
          0, static_cast<int>(log_messages.size()) - log_console_msg_count);
      for (int i = start_index; i < log_messages.size(); ++i) {
        lines.push_back(text(log_messages[i]));
      }
    }
    return vbox(lines) | size(HEIGHT, GREATER_THAN, 4) |
           size(HEIGHT, LESS_THAN, log_console_msg_count + 2);
  });

  // ---------------------------------------------------------------------------
  // Dashboard Tab
  // ---------------------------------------------------------------------------
  auto clock_event_text =
      [](const std::string& code,
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
      };

  auto dashboard_tab = Renderer([&] {
    Elements columns;

    Elements gpu_column;
    gpu_column.push_back(text("GPU") | bold);
    gpu_column.push_back(separator());
    for (const auto& gs : nvml->get_gpus()) {
      gpu_column.push_back(text(fmt::format("{}", gs.index)));
    }
    columns.push_back(vbox(gpu_column) | size(WIDTH, EQUAL, 3));
    columns.push_back(separator());

    Elements name_column;
    int terminal_width = Terminal::Size().dimx;
    bool use_short_name = terminal_width < 112;
    name_column.push_back(text("Name") | bold);
    name_column.push_back(separator());
    for (const auto& gs : nvml->get_gpus()) {
      name_column.push_back(text(use_short_name ? gs.name_short : gs.name));
    }
    columns.push_back(vbox(name_column) | flex);
    columns.push_back(separator());

    Elements util_column;
    util_column.push_back(text("Util") | bold);
    util_column.push_back(separator());
    for (const auto& gs : nvml->get_gpus()) {
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
    for (const auto& gs : nvml->get_gpus()) {
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
    for (const auto& gs : nvml->get_gpus()) {
      clock_column.push_back(text(fmt::format("{}MHz", gs.gpu_clock_mhz)));
    }
    columns.push_back(vbox(clock_column) | size(WIDTH, EQUAL, 7));
    columns.push_back(separator());

    Elements power_column;
    power_column.push_back(text("Power") | bold);
    power_column.push_back(separator());
    for (const auto& gs : nvml->get_gpus()) {
      power_column.push_back(
          text(fmt::format("{}/{}W", gs.power_usage_w, gs.power_limit_w)));
    }
    columns.push_back(vbox(power_column) | size(WIDTH, EQUAL, 11));
    columns.push_back(separator());

    Elements temp_column;
    temp_column.push_back(text("Temp") | bold);
    temp_column.push_back(separator());
    for (const auto& gs : nvml->get_gpus()) {
      temp_column.push_back(text(fmt::format("{}C", gs.temperature_c)));
    }
    columns.push_back(vbox(temp_column) | size(WIDTH, EQUAL, 4));
    columns.push_back(separator());

    Elements fan_column;
    fan_column.push_back(text("Fan") | bold);
    fan_column.push_back(separator());
    for (const auto& gs : nvml->get_gpus()) {
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
    for (const auto& gs : nvml->get_gpus()) {
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

    return vbox({
        vbox(hbox(columns) | border) | flex,
        separator(),
        window(text("Log Console"), log_console->Render()),
    });
  });

  // ---------------------------------------------------------------------------
  // OC Tab
  // ---------------------------------------------------------------------------
  auto oc_tab_component =
      Container::Vertical({oc_buttons, Container::Vertical({oc_panels})});
  auto oc_tab = Renderer(oc_tab_component, [&] {
    Elements elements;
    elements.push_back(
        window(text("Actions"),
               hbox({vbox(paragraph("Note: Register NVTuner service "
                                    "to automatically apply all saved "
                                    "profiles on startup.") |
                          size(HEIGHT, LESS_THAN, 3)),
                     text("  "), filler(), oc_buttons->Render()})));
    elements.push_back(separator());
    for (size_t i = 0; i < nvml->get_gpus().size(); ++i) {
      elements.push_back(oc_panels[i]->Render());
      elements.push_back(separator());
    }

    return vbox({
        vbox(elements) | yframe | flex,
        separator(),
        window(text("Log Console"), log_console->Render()),
    });
  });

  // ---------------------------------------------------------------------------
  // About Tab
  // ---------------------------------------------------------------------------
  auto about_tab = Renderer([] {
    return vbox({
        text(fmt::format("NVTuner {}", APP_VERSION)) | bold | hcenter,
        text("https://github.com/zhhc99/nvtuner") | dim | hcenter,
        separator(),
        window(
            text("About NVTuner"),
            paragraph(
                "An open-source NVIDIA GPU tuning utility.\n"
                "- Live GPU stats\n"
                "- Throttle reasons\n"
                "- Overclock / Undervolt your GPUs\n"
                "- Automatically apply your settings at each startup/login\n")),
        window(text("Clock Event Codes"),
               paragraph("- PC: Power Cap. The clocks have been optimized to "
                         "ensure not to exceed currently set power limits.\n- "
                         "ST: SW Thermal Slowdown. The current clocks have "
                         "been optimized to ensure that GPU is not too hot.\n- "
                         "HT: HW Thermal Slowdown. Temperature being too high, "
                         "clocks are forced to be reduced.")),
    });
  });

  // ---------------------------------------------------------------------------
  // Tab Toggle & Tab Container
  // ---------------------------------------------------------------------------
  std::vector<std::string> tab_values{"Dashboard", "OC Profiles", "About"};
  int tab_selected = 0;
  auto tab_toggle = Toggle(&tab_values, &tab_selected);
  auto tab_container =
      Container::Tab({dashboard_tab, oc_tab, about_tab}, &tab_selected);

  // ---------------------------------------------------------------------------
  // Main Window
  // ---------------------------------------------------------------------------
  auto main_container = Container::Vertical({tab_toggle, tab_container});

  auto main_renderer = Renderer(main_container, [&] {
    auto term_size = Terminal::Size();
    std::string version_text = fmt::format(
        "NVTuner {} | Driver {} | CUDA {}.{} | NVML {}", APP_VERSION,
        nvml->get_driver_version(), nvml->get_cuda_version() / 1000,
        (nvml->get_cuda_version() % 1000) / 10, nvml->get_nvml_version());

    return vbox({
               text(version_text) | bold | hcenter,
               separator(),
               hbox({
                   tab_toggle->Render(),
                   separator(),
               }),
               separator(),
               tab_container->Render() | flex,
           }) |
           border;
  });

  auto screen = ScreenInteractive::Fullscreen();
  // auto screen = ScreenInteractive::TerminalOutput();

  Loop loop(&screen, main_renderer);
  unsigned long long frame_count = 0;
  nvml->update_dynamic_state();
  while (!loop.HasQuitted()) {
    frame_count++;
    if (frame_count % 30 == 0) {
      nvml->update_dynamic_state();
    }
    screen.RequestAnimationFrame();
    loop.RunOnce();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 60));
  }

  return 0;
}
