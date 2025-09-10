#include "nvml_compat.h"

#if NVML_API_VERSION >= 13

decltype(&nvmlDeviceGetClockOffsets) nvmlDeviceGetClockOffsets_p = nullptr;
decltype(&nvmlDeviceSetClockOffsets) nvmlDeviceSetClockOffsets_p = nullptr;
decltype(&nvmlDeviceGetFanSpeedRPM) nvmlDeviceGetFanSpeedRPM_p = nullptr;

#if defined(_WIN32) && defined(_MSC_VER)
#include <windows.h>

void initialize_nvml_compat() {
  HMODULE hNvml = GetModuleHandleA("nvml.dll");
  if (!hNvml) {
    return;
  }
  nvmlDeviceGetClockOffsets_p =
      (decltype(nvmlDeviceGetClockOffsets_p))GetProcAddress(
          hNvml, "nvmlDeviceGetClockOffsets");
  nvmlDeviceSetClockOffsets_p =
      (decltype(nvmlDeviceSetClockOffsets_p))GetProcAddress(
          hNvml, "nvmlDeviceSetClockOffsets");
  nvmlDeviceGetFanSpeedRPM_p =
      (decltype(nvmlDeviceGetFanSpeedRPM_p))GetProcAddress(
          hNvml, "nvmlDeviceGetFanSpeedRPM");
}

#else

__attribute__((weak)) nvmlReturn_t
nvmlDeviceGetClockOffsets(nvmlDevice_t device, nvmlClockOffset_t *clockOffset);
__attribute__((weak)) nvmlReturn_t
nvmlDeviceSetClockOffsets(nvmlDevice_t device, nvmlClockOffset_t *clockOffset);
__attribute__((weak)) nvmlReturn_t
nvmlDeviceGetFanSpeedRPM(nvmlDevice_t device, nvmlFanSpeedInfo_t *fanSpeedInfo);

void initialize_nvml_compat() {
  nvmlDeviceGetClockOffsets_p = &nvmlDeviceGetClockOffsets;
  nvmlDeviceSetClockOffsets_p = &nvmlDeviceSetClockOffsets;
  nvmlDeviceGetFanSpeedRPM_p = &nvmlDeviceGetFanSpeedRPM;
}

#endif

#endif
