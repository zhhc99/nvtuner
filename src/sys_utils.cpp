#include "sys_utils.h"

#include <cstdlib>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <linux/limits.h>
#include <unistd.h>
#endif

namespace {
const char* SERVICE_NAME = "NVTunerService";
}

int SysUtils::run_command(const std::string& cmd) {
  // return std::system(cmd.c_str());
  std::string silent_cmd = cmd + " > /dev/null 2>&1";
#ifdef _WIN32
  silent_cmd = cmd + " > nul 2>&1";
#endif
  return std::system(silent_cmd.c_str());
}

std::string SysUtils::get_executable_path() {
#ifdef _WIN32
  WCHAR path[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, path, MAX_PATH);
  int size_needed =
      WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
  if (size_needed == 0) {
    return "";
  }
  std::string str(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, path, -1, &str[0], size_needed, NULL, NULL);
  str.pop_back();
  return str;
#else
  char result[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
  if (count < 0) {
    return "";
  }
  return std::string(result, count);
#endif
}

std::string SysUtils::get_executable_dir() {
  std::string path = get_executable_path();
  if (path.empty()) {
    return "";
  }
  auto const pos = path.find_last_of("/\\");
  if (pos == std::string::npos) {
    return "";
  }
  return path.substr(0, pos);
}

SysUtils::path_string_t SysUtils::make_path_string(
    const std::string& utf8_path) {
#ifdef _WIN32
  if (utf8_path.empty()) {
    return std::wstring();
  }
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_path.c_str(),
                                        (int)utf8_path.size(), nullptr, 0);
  std::wstring wstr_to(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, utf8_path.c_str(), (int)utf8_path.size(),
                      &wstr_to[0], size_needed);
  return wstr_to;
#else
  return utf8_path;
#endif
}

bool SysUtils::register_startup_task() {
  std::string exe_path = get_executable_path();
  if (exe_path.empty()) {
    return false;
  }

  std::string args = "--apply-profiles";
  std::string command;

#ifdef _WIN32
  command = "schtasks /create /tn " + std::string(SERVICE_NAME) +
            " /tr \"\\\"" + exe_path + "\\\" " + args +
            "\" /sc ONLOGON /ru SYSTEM /f";
#else
  std::string service_file_path =
      "/etc/systemd/system/" + std::string(SERVICE_NAME) + ".service";

  std::ofstream service_file(service_file_path);
  if (!service_file.is_open()) {
    return false;
  }

  service_file << "[Unit]\n"
               << "Description=Apply NVTuner Profiles at Startup\n\n"
               << "[Service]\n"
               << "Type=oneshot\n"
               << "ExecStart=" << exe_path << " " << args << "\n\n"
               << "[Install]\n"
               << "WantedBy=multi-user.target\n";
  service_file.close();

  command = "systemctl daemon-reload && systemctl enable " +
            std::string(SERVICE_NAME);
#endif

  return run_command(command) == 0;
}

bool SysUtils::unregister_startup_task() {
  std::string command;
#ifdef _WIN32
  command = "schtasks /delete /tn \"" + std::string(SERVICE_NAME) + "\" /f";
#else
  std::string service_file_path =
      "/etc/systemd/system/" + std::string(SERVICE_NAME) + ".service";
  command = "systemctl disable " + std::string(SERVICE_NAME) + " && " +
            "rm -f " + service_file_path + " && " + "systemctl daemon-reload";
#endif

  return run_command(command) == 0;
}
