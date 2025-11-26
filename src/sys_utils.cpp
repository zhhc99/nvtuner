#include "sys_utils.h"

#include <cstdlib>
#include <fstream>
#include <string>

#include "fmt/core.h"

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

int SysUtils::exec_command(const std::string& cmd) {
  // return std::system(cmd.c_str());
  std::string silent_cmd;
#ifdef _WIN32
  silent_cmd = cmd + " > nul 2>&1";
#else
  silent_cmd = cmd + " > /dev/null 2>&1";
#endif
  return std::system(silent_cmd.c_str());
}

#ifdef _WIN32
int SysUtils::exec_cmd_as_admin_uac(const std::string& utf8_cmd) {
  std::string full_cmd = "/c " + utf8_cmd;

  SysUtils::path_string_t w_params = SysUtils::make_path_string(full_cmd);

  SHELLEXECUTEINFOW sei = {sizeof(sei)};
  sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
  sei.hwnd = NULL;

  sei.lpVerb = L"runas";
  sei.lpFile = L"cmd.exe";

  sei.lpParameters = w_params.c_str();
  sei.nShow = SW_HIDE;

  if (ShellExecuteExW(&sei)) {
    WaitForSingleObject(sei.hProcess, INFINITE);

    DWORD exit_code = (DWORD)-1;
    GetExitCodeProcess(sei.hProcess, &exit_code);
    CloseHandle(sei.hProcess);

    return (int)exit_code;
  } else {
    return -1;
  }
}
#endif

std::string SysUtils::get_user_name() {
#ifdef _WIN32
  DWORD size = 0;
  if (!GetUserNameW(NULL, &size) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return std::string();
  }
  std::vector<wchar_t> name_buffer(size);
  if (GetUserNameW(name_buffer.data(), &size)) {
    std::wstring wstr(name_buffer.data(), size - 1);
    return make_utf8_from_wstring(wstr);
  }
  return std::string();
#else
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

std::filesystem::path SysUtils::get_user_config_path() {
#ifdef _WIN32
  const wchar_t* userprofile = _wgetenv(L"APPDATA");
  if (userprofile) {
    return std::filesystem::path(userprofile) / L"nvtuner";
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
std::string SysUtils::make_utf8_from_wstring(const std::wstring& wstr) {
  if (wstr.empty()) {
    return std::string();
  }
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(),
                                        (int)wstr.size(), NULL, 0, NULL, NULL);
  std::string str_to(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str_to[0],
                      size_needed, NULL, NULL);
  return str_to;
}
std::wstring SysUtils::make_wstring_from_utf8(const std::string& utf8) {
  if (utf8.empty()) {
    return std::wstring();
  }
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                        (int)utf8.size(), nullptr, 0);
  std::wstring wstr_to(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wstr_to[0],
                      size_needed);
  return wstr_to;
}
#endif

SysUtils::path_string_t SysUtils::make_path_string(
    const std::string& utf8_path) {
#ifdef _WIN32
  return make_wstring_from_utf8(utf8_path);
#else
  return utf8_path;
#endif
}

bool SysUtils::register_startup_task() {
  std::string exe_path = get_executable_path();
  if (exe_path.empty()) {
    return false;
  }

  std::string user_name = get_user_name();
  std::string args = "--apply-profiles";
  std::string command;

#ifdef _WIN32
  std::string user_task_name = std::string(SERVICE_NAME) + "@" + user_name;

  std::string schtasks_template =
      R"(schtasks /create /tn "{}" /tr "\"{}\" {}" /sc ONLOGON /ru %USERNAME% /rl HIGHEST /f /IT)";

  command = fmt::format(schtasks_template, user_task_name, exe_path, args);

  return exec_cmd_as_admin_uac(command) == 0;
#else
  std::filesystem::path service_file_path =
      std::filesystem::path("/etc/systemd/system") /
      fmt::format("{}@.service", SERVICE_NAME);
  std::string user_service_name =
      fmt::format("{}@{}.service", SERVICE_NAME, user_name);

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
  exec_command(command);

  std::string check_cmd = fmt::format(
      "systemctl is-enabled {} > /dev/null 2>&1", user_service_name);
  return std::system(check_cmd.c_str()) == 0;
#endif
}

bool SysUtils::unregister_startup_task() {
  std::string user_name = get_user_name();
  std::string command;
#ifdef _WIN32
  std::string user_task_name = std::string(SERVICE_NAME) + "@" + user_name;

  std::string schtasks_template = R"(schtasks /delete /tn "{}" /f)";

  command = fmt::format(schtasks_template, user_task_name);

  return exec_cmd_as_admin_uac(command) == 0;
#else
  std::filesystem::path service_file_path =
      std::filesystem::path("/etc/systemd/system") /
      fmt::format("{}@.service", SERVICE_NAME);
  std::string user_service_name =
      fmt::format("{}@{}.service", SERVICE_NAME, user_name);

  std::string command_template =
      "pkexec sh -c '"
      "systemctl disable {}\n"
      "systemctl daemon-reload'";

  command = fmt::format(command_template, user_service_name);
  exec_command(command);

  std::string check_cmd = fmt::format(
      "systemctl is-enabled {} > /dev/null 2>&1", user_service_name);
  return std::system(check_cmd.c_str()) != 0;
#endif
}

bool SysUtils::call_apply_profiles_as_root() {
  std::string exe_path = get_executable_path();
  if (exe_path.empty()) {
    return false;
  }

  std::string command = fmt::format("\"{}\" --apply-profiles", exe_path);
  int exit_code;

#ifdef _WIN32
  exit_code = exec_cmd_as_admin_uac(command);
#else
  std::string user_name = get_user_name();
  int status = exec_command(fmt::format(
      "pkexec sh -c 'NVTUNER_TARGET_USER=\"{}\" {}'", user_name, command));
  exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
  return exit_code == 0;
}
