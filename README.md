# NVTuner

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: Windows | Linux](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-blue)](https://github.com/zhhc99/nvtuner)

[README](./README.md) | [中文文档](./README_zh.md)

A lightweight, terminal-based NVIDIA GPU monitoring and tuning tool.

![NVTuner Gallery 1](./docs/images/nvtuner-win-1.png)

![NVTuner Gallery 2](./docs/images/nvtuner-win-2.png)

## Why Use NVTuner?

NVTuner is designed to be your assistant for the following needs:

- Monitor performance metrics to identify performance bottlenecks.
- Undervolt, overclock, or adjust power limits with simple configurations.
- Save your configurations and have them automatically applied at each startup/login.
- Avoid installing complex software packages.

NVTuner is a lightweight tool with no background processes. It utilizes NVIDIA's **native APIs** and is **cross-platform**, supporting both Windows and Linux. Its terminal-based GUI makes it convenient for use in remote sessions.

For detailed instructions on GPU overclocking with NVTuner, please see the [Instant Overclocking Guide](./docs/easy-oc-guide.md) (Chinese version only).

> ⚠️ **Disclaimer**: Overclocking and modifying GPU settings carry inherent risks. These actions may cause system instability, or, in rare cases, damage your (or a remote machine's) hardware. You are solely responsible for any actions taken using this software.

## System Requirements

- **Hardware**: An NVIDIA GPU, Pascal architecture (e.g., GTX 1060, released in 2016) or newer is recommended.
- **Driver**: NVIDIA Driver version 565 or newer is recommended for full functionality. Older drivers may result in limited features or unexpected errors.

## Quick Start

1. **Download** the latest release from the [Releases page](https://github.com/zhhc99/nvtuner/releases)
2. **Windows**: Extract the executable to an empty folder (e.g., `C:/Software/nvtuner`), then run `nvtuner.exe` as administrator.
3. **Linux**: Install using your package manager, then run with `sudo nvtuner`.

## Building from Source

- **Prerequisites**:
  - **Windows**: Visual Studio 2019 (MSVC) or newer.
  - **Linux**: A C++17 compatible compiler (e.g., GCC/Clang) and `make`.
  - CMake 3.15+
- **Dependencies**:
  - NVIDIA Management Library (NVML), which is installed with the CUDA Toolkit.
  - [FTXUI](https://github.com/ArthurSonzogni/FTXUI/)
  - [nlohmann-json](https://github.com/nlohmann/json)

> **A Note on NVML**: For full functionality, it is recommended to compile against NVML v13+ APIs. Compiling with older versions might succeed, but the software's features will be limited.

### Windows

```bash
git clone https://github.com/zhhc99/nvtuner.git
cd nvtuner

mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux

```bash
git clone https://github.com/zhhc99/nvtuner.git
cd nvtuner

mkdir build
cd build
cmake ..
make -j$(nproc)

cpack # for packaging
```

## Important Notes

### Before You Start

When using NVTuner, please note the following:

- **Windows Users**:
  - Monitoring does not require administrator privileges, but adjusting GPU settings does require administrator privileges to run.
  - Configuration files are located in the same directory as `nvtuner.exe`.
  - If you **move the application**, you need to re-execute `Register Service` within the app for the service to work correctly.
  - **To Uninstall**: Start `nvtuner.exe` as administrator, execute `Remove Service` under `OC Profiles`, then delete all application files.
- **Linux Users**:
  - Please run with `root` privileges: `sudo nvtuner`.
  - Configuration files are located in `/etc/nvtuner/`.
  - Use your package manager to install and uninstall the application. The service will be cleaned automatically once uninstalled.
  - For most users, it is recommended to enable the `nvidia-persistenced` service: `sudo systemctl enable --now nvidia-persistenced`. This service should be installed with your NVIDIA driver. Simply put, if this service is not enabled, your GPU configurations might automatically reset when the GPU is idle.

### Known Issues and Limitations

- Adjusting sliders by dragging with the mouse does not adhere to the 15MHz step value. Use the arrow keys for precise adjustments. (This is a limitation of the upstream `FTXUI` library).
- Linux support is tested on Fedora 42. Other distros are unsupported yet.

Found a bug? [Feel free to open an issue!](https://github.com/zhhc99/nvtuner/issues)

## Related Tools

- **MSI Afterburner**: A comprehensive GPU overclocking utility for Windows, supporting in-depth configurations.
- **HWiNFO64**: A graphical tool for monitoring the status of various hardware components.
- **OCBASE/OCCT**: A professional stress-testing tool for verifying overclock stability.
- **nvtop / nvitop**: Terminal-based GPU monitoring tools.
- **nvidia-settings**: A GPU configuration tool for Linux, by NVIDIA official.
