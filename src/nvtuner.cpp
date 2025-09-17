#include "nvtuner.h"

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>

#include "nlohmann/json.hpp"
#include "nvml_compat.h"

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

// --- NvmlManager Implementation ---

NvmlManager::NvmlManager() {
  initialize_nvml_compat();
  check(nvmlInit_v2(), "Failed to initialize NVML");

  // Get system-wide info
  char driver_buf[NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE];
  char nvml_buf[NVML_SYSTEM_NVML_VERSION_BUFFER_SIZE];
  check(nvmlSystemGetDriverVersion(driver_buf, sizeof(driver_buf)),
        "Failed to get driver version");
  check(nvmlSystemGetNVMLVersion(nvml_buf, sizeof(nvml_buf)),
        "Failed to get NVML version");
  check(nvmlSystemGetCudaDriverVersion(&cuda_version_),
        "Failed to get CUDA version");
  driver_version_ = driver_buf;
  nvml_version_ = nvml_buf;

  // Discover GPUs and get their static info
  unsigned int device_count;
  check(nvmlDeviceGetCount_v2(&device_count), "Failed to get device count");

  for (unsigned int i = 0; i < device_count; ++i) {
    GpuState gpu{};
    gpu.index = i;
    check(nvmlDeviceGetHandleByIndex_v2(i, &gpu.handle),
          "Failed to get handle for GPU " + std::to_string(i));

    char uuid_buf[NVML_DEVICE_UUID_BUFFER_SIZE];
    char name_buf[NVML_DEVICE_NAME_BUFFER_SIZE];
    check(nvmlDeviceGetUUID(gpu.handle, uuid_buf, sizeof(uuid_buf)),
          "Failed to get UUID for GPU " + std::to_string(i));
    check(nvmlDeviceGetName(gpu.handle, name_buf, sizeof(name_buf)),
          "Failed to get name for GPU " + std::to_string(i));
    gpu.uuid = uuid_buf;
    gpu.name = name_buf;
    gpu.name_short = "*" +
                     std::regex_replace(
                         gpu.name, std::regex(R"(\b(NVIDIA|GeForce)\s*)"), "") +
                     "*";

    nvmlReturn_t ret;
    unsigned int val, val2;

    ret = nvmlDeviceGetPowerManagementLimitConstraints(gpu.handle, &val, &val2);
    gpu.power_limit_min_w = (ret == NVML_SUCCESS) ? val / 1000 : -1;
    gpu.power_limit_max_w = (ret == NVML_SUCCESS) ? val2 / 1000 : -1;

    ret = nvmlDeviceGetPowerManagementDefaultLimit(gpu.handle, &val);
    gpu.power_limit_default_w = (ret == NVML_SUCCESS) ? val / 1000 : -1;

    ret = nvmlDeviceGetMaxClockInfo(gpu.handle, NVML_CLOCK_GRAPHICS, &val);
    gpu.gpu_max_clock_mhz = (ret == NVML_SUCCESS) ? val : -1;
    const int REASONABLE_MAX_CLOCK = 99999;
    const int CLAMPED_MAX_CLOCK = 5000;
    if (gpu.gpu_max_clock_mhz > REASONABLE_MAX_CLOCK) {
      std::clog << fmt::format(
                       "Warning: GPU {} reported an unrealistic max clock "
                       "({}MHz). Clamping to {}MHz.",
                       gpu.index, gpu.gpu_max_clock_mhz, CLAMPED_MAX_CLOCK)
                << std::endl;
      gpu.gpu_max_clock_mhz = 5000;
    }

    if (nvmlDeviceGetClockOffsets_p) {
      nvmlClockOffset_t clock_info;
      clock_info.version = nvmlClockOffset_v1;
      clock_info.type = NVML_CLOCK_GRAPHICS;
      clock_info.pstate = NVML_PSTATE_0;
      if (nvmlDeviceGetClockOffsets_p(gpu.handle, &clock_info) ==
          NVML_SUCCESS) {
        gpu.clock_offset_min_mhz = clock_info.minClockOffsetMHz;
        gpu.clock_offset_max_mhz = clock_info.maxClockOffsetMHz;
      } else {
        gpu.clock_offset_min_mhz = 0;
        gpu.clock_offset_max_mhz = 0;
      }
    } else {
      if (nvmlDeviceGetGpcClkMinMaxVfOffset(
              gpu.handle, &gpu.clock_offset_min_mhz,
              &gpu.clock_offset_max_mhz) != NVML_SUCCESS) {
        gpu.clock_offset_min_mhz = 0;
        gpu.clock_offset_max_mhz = 0;
      }
    }
    // gpu.clock_offset_min_mhz = (std::max)(gpu.clock_offset_min_mhz, -180);
    // gpu.clock_offset_max_mhz = (std::min)(gpu.clock_offset_max_mhz, 180);

    gpus_.push_back(gpu);
  }
}

NvmlManager::~NvmlManager() { nvmlShutdown(); }

void NvmlManager::update_dynamic_state() {
  for (auto &gpu : gpus_) {
    unsigned int val;
    nvmlReturn_t ret;

    ret = nvmlDeviceGetFanSpeed(gpu.handle, &val);
    gpu.fan_speed_percent = (ret == NVML_SUCCESS) ? val : -1;

    if (nvmlDeviceGetFanSpeedRPM_p) {
      nvmlFanSpeedInfo_t fan_info;
      fan_info.version = nvmlFanSpeedInfo_v1;
      fan_info.fan = 0;
      ret = nvmlDeviceGetFanSpeedRPM_p(gpu.handle, &fan_info);
      gpu.fan_speed_rpm = (ret == NVML_SUCCESS) ? fan_info.speed : -1;
    } else {
      gpu.fan_speed_rpm = -1;
    }

    ret = nvmlDeviceGetTemperature(gpu.handle, NVML_TEMPERATURE_GPU, &val);
    gpu.temperature_c = (ret == NVML_SUCCESS) ? val : -1;

    ret = nvmlDeviceGetPowerUsage(gpu.handle, &val);
    gpu.power_usage_w = (ret == NVML_SUCCESS) ? (val / 1000) : -1;
    if (ret != NVML_SUCCESS) {
      nvmlSample_t sample;
      nvmlValueType_t sample_type;
      unsigned int sample_count = 1;
      ret = nvmlDeviceGetSamples(gpu.handle, NVML_TOTAL_POWER_SAMPLES, 0,
                                 &sample_type, &sample_count, &sample);
      val = static_cast<int>(sample.sampleValue.uiVal / 1000.0);
      gpu.power_usage_w = (ret == NVML_SUCCESS && sample_count > 0) ? val : -1;
    }

    ret = nvmlDeviceGetPowerManagementLimit(gpu.handle, &val);
    gpu.power_limit_w = (ret == NVML_SUCCESS) ? (val / 1000) : -1;

    nvmlMemory_t mem_info;
    ret = nvmlDeviceGetMemoryInfo(gpu.handle, &mem_info);
    gpu.mem_used_mib = (ret == NVML_SUCCESS) ? (mem_info.used / 1048576) : -1;
    gpu.mem_total_mib = (ret == NVML_SUCCESS) ? (mem_info.total / 1048576) : -1;

    nvmlUtilization_t util;
    ret = nvmlDeviceGetUtilizationRates(gpu.handle, &util);
    gpu.gpu_util_percent = (ret == NVML_SUCCESS) ? util.gpu : -1;
    gpu.mem_util_percent = (ret == NVML_SUCCESS) ? util.memory : -1;

    ret = nvmlDeviceGetClockInfo(gpu.handle, NVML_CLOCK_GRAPHICS, &val);
    gpu.gpu_clock_mhz = (ret == NVML_SUCCESS) ? val : -1;

    unsigned long long reasons;
    if (nvmlDeviceGetCurrentClocksEventReasons(gpu.handle, &reasons) ==
        NVML_SUCCESS) {
      auto now = std::chrono::system_clock::now();
      if (reasons & nvmlClocksEventReasonSwPowerCap) {
        gpu.last_event_power_cap_time = now;
      }
      if (reasons & nvmlClocksEventReasonSwThermalSlowdown) {
        gpu.last_event_swt_slowdown_time = now;
      }
      if (reasons & nvmlClocksThrottleReasonHwThermalSlowdown) {
        gpu.last_event_hwt_slowdown_time = now;
      }
    }
  }
}

void NvmlManager::apply_profiles(
    const std::map<std::string, OcProfile> &profiles) {
  std::clog << fmt::format("Applying profiles for {} GPUs.", gpus_.size())
            << std::endl;

  for (const GpuState &gs : gpus_) {
    const OcProfile &profile = profiles.at(gs.uuid);

    if (profile.power_limit == gs.power_limit_default_w &&
        profile.gpu_clock_offset == 0 &&
        profile.max_gpu_clock == gs.gpu_max_clock_mhz) {
      std::clog << fmt::format("Skip for GPU {} because profile is default.",
                               gs.index)
                << std::endl;
      continue;
    }

    std::string oc_text =
        fmt::format("OC ({}W, {:+}MHz, <={}MHz)", profile.power_limit,
                    profile.gpu_clock_offset, profile.max_gpu_clock);
    std::clog << fmt::format("Applying profile {} for GPU {}.", oc_text,
                             gs.index)
              << std::endl;

    // 1. Set Power Limit
    nvmlReturn_t ret_pl = nvmlDeviceSetPowerManagementLimit(
        gs.handle, profile.power_limit * 1000);

    // 2. Set Clock Offset
    nvmlReturn_t ret_co;
    if (nvmlDeviceSetClockOffsets_p) {
      nvmlClockOffset_t clock_offset_info;
      clock_offset_info.version = nvmlClockOffset_v1;
      clock_offset_info.type = NVML_CLOCK_GRAPHICS;
      clock_offset_info.pstate = NVML_PSTATE_0;
      clock_offset_info.clockOffsetMHz = profile.gpu_clock_offset;
      ret_co = nvmlDeviceSetClockOffsets_p(gs.handle, &clock_offset_info);
    } else {
      ret_co = nvmlDeviceSetGpcClkVfOffset(gs.handle, profile.gpu_clock_offset);
      std::clog << "Using deprecated API to set clock offset. Please consider "
                   "updating the driver for better compatibility."
                << std::endl;
    }

    // 3. Set Max Locked Clock
    nvmlReturn_t ret_lc;
    if (profile.max_gpu_clock < gs.gpu_max_clock_mhz) {
      ret_lc =
          nvmlDeviceSetGpuLockedClocks(gs.handle, 0, profile.max_gpu_clock);
    } else {
      ret_lc = nvmlDeviceResetGpuLockedClocks(gs.handle);
    }

    if (ret_pl == NVML_SUCCESS && ret_co == NVML_SUCCESS &&
        ret_lc == NVML_SUCCESS) {
      std::clog << fmt::format("Profile Successfully Applied for GPU {}.",
                               gs.index)
                << std::endl;
    } else {
      std::cerr << fmt::format(
                       "Failed to apply profile for GPU {}. States: PL({}), "
                       "CO({}), LC({}).",
                       gs.index, nvmlErrorString(ret_pl),
                       nvmlErrorString(ret_co), nvmlErrorString(ret_lc))
                << std::endl;
    }
  }
}

void NvmlManager::check(nvmlReturn_t result, const std::string &error_msg) {
  if (result != NVML_SUCCESS) {
    throw std::runtime_error(error_msg +
                             " | Reason: " + nvmlErrorString(result));
  }
}

nvmlDevice_t NvmlManager::get_handle_by_uuid(const std::string &uuid) {
  for (const auto &gpu : gpus_) {
    if (gpu.uuid == uuid) {
      return gpu.handle;
    }
  }
  throw std::runtime_error("Could not find GPU with UUID: " + uuid);
}

// --- ProfileManager Implementation ---

ProfileManager::ProfileManager(const std::string &file_path,
                               const std::vector<GpuState> &gpus)
    : file_path_(file_path), gpus_(gpus) {
  for (const auto &gpu : gpus_) {
    OcProfile &profile = profiles_[gpu.uuid];
    profile.power_limit = gpu.power_limit_default_w;
    profile.gpu_clock_offset = 0;
    profile.max_gpu_clock = gpu.gpu_max_clock_mhz;
  }
  load();
}

void ProfileManager::load() {
  std::clog << fmt::format("Loading profiles from {}.", file_path_)
            << std::endl;

  std::ifstream file(SysUtils::make_path_string(file_path_));
  if (!file.is_open()) {
    std::clog << "Profile file not found. Using defaults." << std::endl;
    return;
  }

  json data;
  try {
    data = json::parse(file);
  } catch (const json::parse_error &e) {
    std::cerr << fmt::format(
                     "Error parsing profiles file. Using defaults. "
                     "Details: {}",
                     file_path_, e.what())
              << std::endl;
    return;
  }

  for (const auto &gpu : gpus_) {
    if (!data.contains(gpu.uuid)) {
      continue;
    }

    const json &profile_json = data[gpu.uuid];
    OcProfile &profile = profiles_.at(gpu.uuid);

    int loaded_power = profile_json.value("power_limit", profile.power_limit);
    profile.power_limit =
        std::clamp(loaded_power, gpu.power_limit_min_w, gpu.power_limit_max_w);

    int loaded_offset =
        profile_json.value("gpu_clock_offset", profile.gpu_clock_offset);
    profile.gpu_clock_offset = std::clamp(
        loaded_offset, gpu.clock_offset_min_mhz, gpu.clock_offset_max_mhz);

    int loaded_max_clock =
        profile_json.value("max_gpu_clock", profile.max_gpu_clock);
    profile.max_gpu_clock =
        std::clamp(loaded_max_clock, 0, gpu.gpu_max_clock_mhz);
  }

  std::clog << fmt::format(
                   "Profiles loaded and validated against hardware limits.")
            << std::endl;
}

void ProfileManager::save() {
  json data;
  for (const auto &[uuid, profile] : profiles_) {
    json profile_json;
    profile_json["power_limit"] = profile.power_limit;
    profile_json["gpu_clock_offset"] = profile.gpu_clock_offset;
    profile_json["max_gpu_clock"] = profile.max_gpu_clock;
    data[uuid] = profile_json;
  }

  std::ofstream outfile(SysUtils::make_path_string(file_path_));
  if (!outfile) {
    std::cerr << fmt::format("Failed to open {}.", file_path_) << std::endl;
  } else {
    outfile << data.dump(4);
    std::clog << fmt::format("Profiles saved to {}.", file_path_) << std::endl;
  }
}
