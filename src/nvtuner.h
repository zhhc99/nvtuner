#pragma once

#include <nvml.h>

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "sys_utils.h"

struct OcProfile {
  int power_limit;       // in Watts
  int gpu_clock_offset;  // in MHz
  int max_gpu_clock;     // in MHz
};

struct GpuState {
  // Static Info
  unsigned int index;
  nvmlDevice_t handle;
  std::string uuid;
  std::string name;
  std::string name_short;
  int power_limit_min_w;
  int power_limit_max_w;
  int power_limit_default_w;
  int clock_offset_min_mhz;
  int clock_offset_max_mhz;
  int gpu_max_clock_mhz;

  // Dynamic Info
  int fan_speed_percent;
  int fan_speed_rpm;
  int temperature_c;
  int power_usage_w;
  int power_limit_w;  // Note: this is the *current* limit, not from profile
  int enforced_power_limit_w; // Real power limit on 40xx+ Laptop GPUs (Onboard dynamic boost)
  int mem_used_mib;
  int mem_total_mib;
  int gpu_util_percent;
  int mem_util_percent;
  int gpu_clock_mhz;

  std::optional<std::chrono::system_clock::time_point>
      last_event_power_cap_time;
  std::optional<std::chrono::system_clock::time_point>
      last_event_swt_slowdown_time;
  std::optional<std::chrono::system_clock::time_point>
      last_event_hwt_slowdown_time;
};

class NvmlManager {
 public:
  NvmlManager();
  ~NvmlManager();

  void update_dynamic_state();

  /**
   * @return true on success
   */
  bool apply_profiles(const std::map<std::string, OcProfile> &profiles);

  const std::string &get_driver_version() const { return driver_version_; }
  const std::string &get_nvml_version() const { return nvml_version_; }
  const int &get_cuda_version() const { return cuda_version_; }
  const std::vector<GpuState> &get_gpus() const { return gpus_; }

 private:
  void check(nvmlReturn_t result, const std::string &error_msg);
  nvmlDevice_t get_handle_by_uuid(const std::string &uuid);

  std::string driver_version_;
  std::string nvml_version_;
  int cuda_version_;  // major is value/1000, minor is (value%1000)/10
  std::vector<GpuState> gpus_;
};

class ProfileManager {
 public:
  ProfileManager(const std::string &file_path,
                 const std::vector<GpuState> &gpus);

  void load();
  void save();

  const std::map<std::string, OcProfile> &get_all_profiles() const {
    return profiles_;
  };
  std::map<std::string, OcProfile> &get_all_profiles() { return profiles_; };
  const OcProfile &get_profile(const std::string &uuid) const {
    return profiles_.at(uuid);
  }
  OcProfile &get_profile(const std::string &uuid) {
    return profiles_.at(uuid);
  };

 private:
  std::string file_path_;
  const std::vector<GpuState> &gpus_;
  std::map<std::string, OcProfile> profiles_;  // Key is UUID
};
