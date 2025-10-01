#pragma once
#include <ftxui/component/component.hpp>

#include "nvtuner.h"

ftxui::Component Dashboard(const std::vector<GpuState>& gpu_states);
