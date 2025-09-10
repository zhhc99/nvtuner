find_path(NVML_INCLUDE_DIR nvml.h
    HINTS
        "$ENV{CUDA_PATH}/include"
        "/usr/local/cuda/include"
        "/usr/include/nvidia"
)
message(STATUS "NVML_INCLUDE_DIR found: ${NVML_INCLUDE_DIR}/nvml.h")

if(WIN32)
    find_library(NVML_LIBRARY
        NAMES nvml
        HINTS
            "$ENV{CUDA_PATH}/lib/x64"
            "$ENV{ProgramW6432}/NVIDIA Corporation/NVSMI"
            "C:/Windows/System32"
    )
else()
    find_library(NVML_LIBRARY
        NAMES nvidia-ml
        HINTS
            "/usr/lib"
            "/usr/lib64"
            "/usr/lib/x86_64-linux-gnu"
            "/usr/lib/aarch64-linux-gnu"
            "$ENV{CUDA_PATH}/targets/x86_64-linux/lib/stubs"
            "/usr/lib64-nvidia/"
            "/usr/local/cuda/targets/x86_64-linux/lib/stubs"
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NVML REQUIRED_VARS NVML_LIBRARY NVML_INCLUDE_DIR)

if(NVML_FOUND)
    set(NVML_INCLUDE_DIRS ${NVML_INCLUDE_DIR})
    set(NVML_LIBRARIES ${NVML_LIBRARY})
    if(NOT TARGET NVIDIA::nvml)
        add_library(NVIDIA::nvml UNKNOWN IMPORTED)
        set_target_properties(NVIDIA::nvml PROPERTIES
            IMPORTED_LOCATION "${NVML_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${NVML_INCLUDE_DIRS}"
        )
    endif()
endif()

mark_as_advanced(NVML_INCLUDE_DIR NVML_LIBRARY)
