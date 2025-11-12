#include "oc_tab.h"

#include <fmt/core.h>

#include <iostream>

using namespace ftxui;

OCTab::OCTab(ProfileManager& profile_manager, NvmlManager& nvml_manager)
    : pm_(profile_manager), nvml_(nvml_manager) {
  setup_action_buttons();
  setup_oc_panels();

  auto oc_tab_component =
      Container::Vertical({action_buttons_, Container::Vertical(oc_panels_)});

  main_component_ = Renderer(oc_tab_component, [this, oc_tab_component] {
    Elements elements;

    elements.push_back(
        window(text("Actions"),
               hbox({vbox(paragraph("Note: Register nvtuner service "
                                    "to automatically apply all saved "
                                    "profiles on startup.") |
                          size(HEIGHT, LESS_THAN, 3)),
                     text("  "), filler(), action_buttons_->Render()})));
    elements.push_back(separator());

    for (size_t i = 0; i < oc_panels_.size(); ++i) {
      elements.push_back(oc_panels_[i]->Render());
      elements.push_back(separator());
    }

    return vbox(elements) | yframe;
  });
}

void OCTab::setup_action_buttons() {
  action_buttons_ = Container::Horizontal({
      Button("Save and Apply All",
             [this] {
               pm_.save();
               nvml_.apply_profiles(pm_.get_all_profiles());
             }),
      Button("Register Service",
             [this] {
               if (SysUtils::register_startup_task()) {
                 std::clog << REGISTER_SUCCESSFUL << std::endl;
               } else {
                 std::cerr << REGISTER_FAILED << std::endl;
               }
             }),
      Button("Remove Service",
             [this] {
               if (SysUtils::unregister_startup_task()) {
                 std::clog << UNREGISTER_SUCCESSFUL << std::endl;
               } else {
                 std::cerr << UNREGISTER_FAILED << std::endl;
               }
             }),
  });
}

void OCTab::setup_oc_panels() {
  oc_panels_.clear();
  for (size_t i = 0; i < nvml_.get_gpus().size(); ++i) {
    oc_panels_.push_back(create_gpu_panel(i));
  }
}

Component OCTab::create_gpu_panel(size_t gpu_index) {
  const auto& gs = nvml_.get_gpus()[gpu_index];
  auto& profile = pm_.get_profile(gs.uuid);

  auto power_limit_slider =
      Slider("", &profile.power_limit, &gs.power_limit_min_w,
             &gs.power_limit_max_w, &POWER_STEP);
  auto gpu_clock_offset_slider =
      Slider("", &profile.gpu_clock_offset, &gs.clock_offset_min_mhz,
             &gs.clock_offset_max_mhz, &CLOCK_STEP);
  auto gpu_max_clock_slider =
      Slider("", &profile.max_gpu_clock, &MAX_CLOCK_MIN_MHZ,
             &gs.gpu_max_clock_mhz, &CLOCK_STEP);

  auto oc_panel_component = Container::Vertical({
      power_limit_slider,
      gpu_clock_offset_slider,
      gpu_max_clock_slider,
  });

  return Renderer(oc_panel_component, [this, gpu_index, &gs, &profile,
                                       power_limit_slider,
                                       gpu_clock_offset_slider,
                                       gpu_max_clock_slider] {
    return vbox({
        hbox({
            text(fmt::format("GPU {}: {} ", gs.index, gs.name)) | bold,
            separatorCharacter("|"),
            text(fmt::format(" OC ({}W, {:+}MHz, <={}MHz)", profile.power_limit,
                             profile.gpu_clock_offset, profile.max_gpu_clock)) |
                dim,
        }),
        create_slider_row("Power Limit",
                          fmt::format("{}W", profile.power_limit),
                          power_limit_slider),
        create_slider_row("GPU Clock Offset",
                          fmt::format("{:+}MHz", profile.gpu_clock_offset),
                          gpu_clock_offset_slider),
        create_slider_row("GPU Max Clock",
                          fmt::format("{}MHz", profile.max_gpu_clock),
                          gpu_max_clock_slider),
    });
  });
}

Element OCTab::create_slider_row(const std::string& label,
                                 const std::string& value,
                                 Component slider_component) {
  int width = OC_SLIDER_TAG_WIDTH - label.length();
  return hbox({
      text(fmt::format("{0}{1:>{2}}", label, value, width)) |
          size(WIDTH, EQUAL, OC_SLIDER_TAG_WIDTH + 1),
      slider_component->Render() | flex,
  });
}

Component OCTab::get_component() { return main_component_; }
