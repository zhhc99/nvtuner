#pragma once
#include <ftxui/component/component.hpp>

#include "nvtuner.h"

class OCTab {
 private:
  ProfileManager& pm_;
  NvmlManager& nvml_;

  ftxui::Component action_buttons_;
  std::vector<ftxui::Component> oc_panels_;
  ftxui::Component main_component_;

  static inline const int POWER_STEP = 1;
  static inline const int CLOCK_STEP = 15;
  static inline const int MAX_CLOCK_MIN_MHZ = 210;
  static inline const int OC_SLIDER_TAG_WIDTH = 30;
#ifdef _WIN32
  static inline const std::string REGISTER_SUCCESSFUL =
      "Startup scheduled task has been added.";
  static inline const std::string REGISTER_FAILED =
      "Failed to register startup scheduled task. Run as admin?";
  static inline const std::string UNREGISTER_SUCCESSFUL =
      "Startup scheduled task has been removed.";
  static inline const std::string UNREGISTER_FAILED =
      "Failed to remove startup scheduled task. Run as admin?";
#else
  static inline const std::string REGISTER_SUCCESSFUL =
      "Startup systemd service has been added.";
  static inline const std::string REGISTER_FAILED =
      "Failed to register startup systemd service. Run as root?";
  static inline const std::string UNREGISTER_SUCCESSFUL =
      "Startup systemd service has been removed.";
  static inline const std::string UNREGISTER_FAILED =
      "Failed to remove startup systemd service. Run as root?";
#endif

 public:
  explicit OCTab(ProfileManager& profile_manager, NvmlManager& nvml_manager);

  ftxui::Component get_component();

 private:
  void setup_action_buttons();
  void setup_oc_panels();
  ftxui::Component create_gpu_panel(size_t gpu_index);
  ftxui::Element create_slider_row(const std::string& label,
                                   const std::string& value,
                                   ftxui::Component slider_component);
};
