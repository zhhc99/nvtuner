#pragma once
#include <nvml.h>

#include <cstddef>

#if NVML_API_VERSION >= 13
extern decltype(&nvmlDeviceGetClockOffsets) nvmlDeviceGetClockOffsets_p;
extern decltype(&nvmlDeviceSetClockOffsets) nvmlDeviceSetClockOffsets_p;
extern decltype(&nvmlDeviceGetFanSpeedRPM) nvmlDeviceGetFanSpeedRPM_p;
extern decltype(&nvmlDeviceGetTemperatureV) nvmlDeviceGetTemperatureV_p;

void initialize_nvml_compat();
#else
inline constexpr std::nullptr_t nvmlDeviceGetClockOffsets_p = nullptr;
inline constexpr std::nullptr_t nvmlDeviceSetClockOffsets_p = nullptr;
inline constexpr std::nullptr_t nvmlDeviceGetFanSpeedRPM_p = nullptr;
inline constexpr std::nullptr_t nvmlDeviceGetTemperatureV_p = nullptr;

inline void initialize_nvml_compat() {}
#endif
