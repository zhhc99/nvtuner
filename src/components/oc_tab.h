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

  static const int POWER_STEP = 1;
  static const int CLOCK_STEP = 15;
  static const int MAX_CLOCK_MIN_MHZ = 210;
  static const int OC_SLIDER_TAG_WIDTH = 30;

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
