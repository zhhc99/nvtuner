#pragma once
#include <filesystem>
#include <string>

namespace SysUtils {

#ifdef _WIN32
using path_string_t = std::wstring;
#else
using path_string_t = std::string;
#endif

int exec_command(const std::string& utf8_cmd);
#ifdef _WIN32
/**
 * @brief Execute command as administrator using UAC.
 * @param utf8_cmd UTF-8 encoded command to execute, which will be wrapped in
 * cmd.exe /c. If path of executable contains spaces, MANUALLY quote it with "".
 * @return 0 on success, non-zero on failure (user cancel / error).
 */
int exec_cmd_as_admin_uac(const std::string& utf8_cmd);
#endif

std::string get_user_name();
std::string get_executable_path();
std::string get_executable_dir();
std::filesystem::path get_user_config_path();
#ifdef _WIN32
std::string make_utf8_from_wstring(const std::wstring& wstr);
std::wstring make_wstring_from_utf8(const std::string& utf8);
#endif

/**
 * @brief Use this to convert utf8 paths to OS specific paths.
 *
 * @see SysUtils::get_executable_path()
 * @see SysUtils::get_executable_dir()
 */
path_string_t make_path_string(const std::string& utf8_path);

/**
 * @return true on success.
 */
bool register_startup_task();
bool unregister_startup_task();
bool call_apply_profiles_as_root();
}  // namespace SysUtils
