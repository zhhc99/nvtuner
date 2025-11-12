#include "sys_utils.h"

#include <cstdlib>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <linux/limits.h>
#include <pwd.h>
#include <unistd.h>

#include <cstring>
#endif
#include <iostream>

namespace {
const char* SERVICE_NAME = "nvtuner";
}

int SysUtils::run_command(const std::string& cmd) {
  // return std::system(cmd.c_str());
  std::string silent_cmd = cmd + " > /dev/null 2>&1";
#ifdef _WIN32
  silent_cmd = cmd + " > nul 2>&1";
#endif
  return std::system(silent_cmd.c_str());
}

std::filesystem::path SysUtils::get_user_config_path() {
#ifdef _WIN32
  const char* userprofile = std::getenv("USERPROFILE");
  if (userprofile) {
    return std::filesystem::path(userprofile) / ".config" / "nvtuner";
  }
  return std::filesystem::path();
#else
  struct passwd* pw = nullptr;

  // $USER (systemd service)
  const char* service_user = std::getenv("USER");
  if (service_user && strcmp(service_user, "root") != 0) {
    pw = getpwnam(service_user);
  }

  // $SUDO_UID (sudo)
  const char* sudo_uid_str = std::getenv("SUDO_UID");
  if (sudo_uid_str) {
    uid_t sudo_uid = std::stoul(sudo_uid_str);
    pw = getpwuid(sudo_uid);
  }

  // $PKEXEC_UID (pkexec)
  if (pw == nullptr) {
    const char* pkexec_uid_str = std::getenv("PKEXEC_UID");
    if (pkexec_uid_str) {
      uid_t pkexec_uid = std::stoul(pkexec_uid_str);
      pw = getpwuid(pkexec_uid);
    }
  }

  // $HOME (running as non-root user)
  if (pw == nullptr) {
    const char* home = std::getenv("HOME");
    if (home && strncmp(home, "/home/", 6) == 0) {
      return std::filesystem::path(home) / ".config" / "nvtuner";
    }
  }

  // fallback to current user. doesn't work with pkexec tho.
  if (pw == nullptr) {
    pw = getpwuid(getuid());
  }

  if (pw == nullptr || pw->pw_dir == nullptr) {
    return std::filesystem::path();
  }

  return std::filesystem::path(pw->pw_dir) / ".config" / "nvtuner";
#endif
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
  return run_command(command) == 0;
#else
  std::filesystem::path config_path = get_user_config_path();
  if (config_path.empty()) {
    return false;
  }

  std::filesystem::path service_file_path =
      std::filesystem::path("/etc/systemd/system") /
      (std::string(SERVICE_NAME) + ".service");

  command =
      "pkexec sh -c '"
      "cat > " +
      service_file_path.string() +
      " << 'EOF'\n"
      "[Unit]\n"
      "Description=Apply NVTuner Profiles at Startup\n\n"
      "[Service]\n"
      "Type=oneshot\n"
      "ExecStart=/bin/sh -c \"HOME=%h USER=%u " +
      exe_path + " " + args +
      "\"\n\n"
      "[Install]\n"
      "WantedBy=multi-user.target\n"
      "EOF\n"
      "systemctl daemon-reload && "
      "systemctl enable " +
      std::string(SERVICE_NAME) + "'";
  run_command(command);
  std::string check_cmd =
      "systemctl is-enabled " + std::string(SERVICE_NAME) + " > /dev/null 2>&1";
  return std::system(check_cmd.c_str()) == 0;
#endif
}

bool SysUtils::unregister_startup_task() {
  std::string command;
#ifdef _WIN32
  command = "schtasks /delete /tn \"" + std::string(SERVICE_NAME) + "\" /f";
  return run_command(command) == 0;
#else
  std::filesystem::path service_file_path =
      std::filesystem::path("/etc/systemd/system") /
      (std::string(SERVICE_NAME) + ".service");

  command =
      "pkexec sh -c '"
      "systemctl disable " +
      std::string(SERVICE_NAME) +
      " 2>/dev/null || true && "
      "rm -f " +
      service_file_path.string() +
      " && "
      "systemctl daemon-reload'";
  run_command(command);
  std::string check_cmd =
      "systemctl is-enabled " + std::string(SERVICE_NAME) + " > /dev/null 2>&1";
  return std::system(check_cmd.c_str()) != 0;
#endif
}
