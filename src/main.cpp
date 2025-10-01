#ifndef _WIN32
#include <sys/types.h>  // For uid_t
#include <unistd.h>     // For geteuid()
#endif

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
#include <mutex>

#include "components/dashboard.h"
#include "components/graphs_tab.h"
#include "components/log_console.h"
#include "components/oc_tab.h"
#include "components/sparklines.h"
#include "nvtuner.h"
#include "stream_redirect.h"
#include "sys_utils.h"

using namespace ftxui;

#ifndef APP_VERSION  // defined in CMake
#define APP_VERSION "unknown"
#endif

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
  // FTXUI
  // ---------------------------------------------------------------------------

  Component log_console = LogConsole();

  Component dashboard = Dashboard(nvml->get_gpus());
  Component dashboard_tab = Renderer([&dashboard, &log_console]() {
    return vbox({
        dashboard->Render() | flex,
        separator(),
        log_console->Render(),
    });
  });

  GraphsTab graphs_tab(nvml->get_gpus());

  Sparklines sparklines(nvml->get_gpus());

  OCTab oc(pm, *nvml);
  auto oc_tab = Renderer(oc.get_component(), [&oc, &log_console]() {
    return vbox({
        oc.get_component()->Render() | flex,
        separator(),
        log_console->Render(),
    });
  });

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

  std::vector<std::string> tab_values{"Dashboard", "Graphs", "Sparklines",
                                      "OC Profiles", "About"};
  int tab_selected = 0;
  auto tab_toggle = Toggle(&tab_values, &tab_selected);
  auto tab_container =
      Container::Tab({dashboard_tab, graphs_tab.get_component(),
                      sparklines.get_component(), oc_tab, about_tab},
                     &tab_selected);

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
  graphs_tab.update();
  sparklines.update();
  while (!loop.HasQuitted()) {
    frame_count++;
    if (frame_count % 30 == 0) {
      nvml->update_dynamic_state();
      graphs_tab.update();
    }
    if (frame_count % 60 == 0) {
      sparklines.update();
    }
    screen.RequestAnimationFrame();
    loop.RunOnce();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 60));
  }

  return 0;
}
