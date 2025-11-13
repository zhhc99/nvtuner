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

#include "fmt/core.h"
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
  std::string user_name = get_user_name();
  struct passwd* pw = getpwnam(user_name.c_str());
  if (user_name == "root") {
    return std::filesystem::path();
  }
  if (pw == nullptr) {
    return std::filesystem::path();
  }
  return std::filesystem::path(pw->pw_dir) / ".config" / "nvtuner";
#endif
}

#ifdef _WIN32
#else
std::string SysUtils::get_user_name() {
  // environment variables are the only reliable way to get real user.
  // however, system-wide systemd service is running as root, where we use
  // $NVTUNER_TARGET_USER
  char* env_user = std::getenv("USER");
  char* env_sudo_uid = std::getenv("SUDO_UID");
  char* env_pkexec_uid = std::getenv("PKEXEC_UID");
  char* env_nvtuner_user = std::getenv("NVTUNER_TARGET_USER");

  std::string ret = "root";
  struct passwd* pw = nullptr;
  if (env_nvtuner_user != nullptr) {
    ret = env_nvtuner_user;
  } else if (env_sudo_uid != nullptr) {
    pw = getpwuid(std::stoul(env_sudo_uid));
  } else if (env_pkexec_uid != nullptr) {
    pw = getpwuid(std::stoul(env_pkexec_uid));
  } else if (env_user != nullptr) {
    ret = env_user;
  }

  if (pw != nullptr) {
    ret = pw->pw_name;
  }
  return ret;
}
#endif

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
  std::filesystem::path service_file_path =
      std::filesystem::path("/etc/systemd/system") /
      fmt::format("{}@.service", SERVICE_NAME);
  std::string user_name = get_user_name();
  std::string user_service_name =
      fmt::format("{}@{}.service", SERVICE_NAME, user_name);

  if (config_path.empty()) {
    return false;
  }

  std::string service_template = R"DELIM([Unit]
Description=Apply nvtuner profiles on startup for %i
Requires=user@%i.service
After=user@%i.service

[Service]
Type=oneshot
ExecStart=/bin/sh -c 'NVTUNER_TARGET_USER=%i {} {}'

[Install]
WantedBy=multi-user.target
)DELIM";
  std::string command_template =
      "pkexec sh -c \""
      "cat > {} << 'EOF'\n"
      "{}"
      "EOF\n"
      "systemctl daemon-reload\n"
      "systemctl enable {}\"";
  std::string service_content = fmt::format(service_template, exe_path, args);

  command = fmt::format(command_template, service_file_path.string(),
                        service_content, user_service_name);
  run_command(command);

  std::string check_cmd = fmt::format(
      "systemctl is-enabled {} > /dev/null 2>&1", user_service_name);
  return std::system(check_cmd.c_str()) == 0;
#endif
}

bool SysUtils::unregister_startup_task() {
  std::string command;
#ifdef _WIN32
  command = "schtasks /delete /tn \"" + std::string(SERVICE_NAME) + "\" /f";
  return run_command(command) == 0;
#else
  std::filesystem::path config_path = get_user_config_path();
  std::filesystem::path service_file_path =
      std::filesystem::path("/etc/systemd/system") /
      fmt::format("{}@.service", SERVICE_NAME);
  std::string user_name = get_user_name();
  std::string user_service_name =
      fmt::format("{}@{}.service", SERVICE_NAME, user_name);

  std::string command_template =
      "pkexec sh -c '"
      "systemctl disable {}\n"
      "systemctl daemon-reload'";

  command = fmt::format(command_template, user_service_name);
  run_command(command);

  std::string check_cmd = fmt::format(
      "systemctl is-enabled {} > /dev/null 2>&1", user_service_name);
  return std::system(check_cmd.c_str()) != 0;
#endif
}
