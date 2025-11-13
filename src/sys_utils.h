#pragma once
#include <string>
#include <filesystem>

namespace SysUtils {

#ifdef _WIN32
using path_string_t = std::wstring;
#else
using path_string_t = std::string;
#endif

int run_command(const std::string& utf8_cmd);

std::string get_executable_path();
std::string get_executable_dir();
std::filesystem::path get_user_config_path();

#ifdef _WIN32
#else
std::string get_user_name();
#endif

/**
 * @brief Use this to convert utf8 paths to OS specific paths.
 *
 * @see SysUtils::get_executable_path()
 * @see SysUtils::get_executable_dir()
 */
path_string_t make_path_string(const std::string& utf8_path);

bool register_startup_task();
bool unregister_startup_task();
}  // namespace SysUtils
