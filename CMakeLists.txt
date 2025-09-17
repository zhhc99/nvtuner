cmake_minimum_required(VERSION 3.15)
project(nvtuner VERSION 0.1.1 DESCRIPTION "NVIDIA GPU monitoring and tuning tool")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG v6.1.9
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ftxui)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt
  GIT_TAG 10.2.1
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json
  GIT_TAG v3.12.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(NVML REQUIRED)

# --- Executable ---
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/nvtuner.cpp
    src/sys_utils.cpp
    src/nvml_compat.cpp
)

target_compile_definitions(nvtuner PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="${PROJECT_NAME}"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    NVIDIA::nvml
    ftxui::screen
    ftxui::dom
    ftxui::component
    fmt::fmt
    nlohmann_json::nlohmann_json
)

# --- platform specific dependencies ---
if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()

# --- CPack Packaging ---
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "bana")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE "https://github.com/zhhc99/nvtuner")

set(CPACK_GENERATOR "RPM")

set(CPACK_RPM_PACKAGE_GROUP "System/Utilities")
set(CPACK_RPM_PACKAGE_AUTOREQPROV "yes")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_PACKAGE_DESCRIPTION "A lightweight, terminal-based NVIDIA GPU monitoring and tuning tool.")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")

set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/preuninstall.sh")

include(CPack)
