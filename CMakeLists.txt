cmake_minimum_required(VERSION 3.19)
project(cpufb LANGUAGES CXX C ASM)

# ---------------------------------------------------------------------------
# Project options
# ---------------------------------------------------------------------------
option(CPUFB_ENABLE_ANDROID_DEPLOY
    "Add adb push/run helper targets (only useful when cross-compiling for ARM64)"
    OFF)

set(CPUFB_SIMD_FEATURES_OVERRIDE "" CACHE STRING
    "Semicolon-separated list of SIMD kernels to compile (e.g. \"_BF16_;_I8MM_\"). \
On ARM64 Linux/Android the executable still filters this list using runtime HWCAP.")

# ---------------------------------------------------------------------------
# C++ standard
# ---------------------------------------------------------------------------
# x64 paths are written against C++11; arm64 amx_kernel uses C++20 features.
# We pick the higher one as the project default and downgrade the x64 main TU
# below per-target.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Architecture detection
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(CPUFB_ARCH "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(CPUFB_ARCH "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    set(CPUFB_ARCH "riscv64")
else()
    message(FATAL_ERROR "cpufb: unsupported architecture '${CMAKE_SYSTEM_PROCESSOR}'")
endif()
message(STATUS "cpufb: arch=${CPUFB_ARCH} system=${CMAKE_SYSTEM_NAME}")

set(CPUFB_ARCH_DIR "${CMAKE_SOURCE_DIR}/${CPUFB_ARCH}")
set(CPUFB_ASM_DIR  "${CPUFB_ARCH_DIR}/asm")
set(CPUFB_KERNEL_DIR "${CPUFB_ARCH_DIR}/kernel")
set(CPUFB_COMMON_DIR "${CMAKE_SOURCE_DIR}/common")

# ---------------------------------------------------------------------------
# SIMD feature detection (configure-time, single pass)
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(DetectSimd)
include(SimdFlags)

cpufb_detect_simd("${CPUFB_ARCH}" CPUFB_SIMD_FEATURES)
cpufb_compute_march_flag("${CPUFB_ARCH}" "${CPUFB_SIMD_FEATURES}" CPUFB_MARCH_FLAG)
message(STATUS "cpufb: cpufb.cpp march flag: ${CPUFB_MARCH_FLAG}")

# ---------------------------------------------------------------------------
# Threads (replaces bare `pthread` link)
# ---------------------------------------------------------------------------
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# common/ as an OBJECT library
# ---------------------------------------------------------------------------
add_library(cpufb_common OBJECT
    "${CPUFB_COMMON_DIR}/table.cpp"
    "${CPUFB_COMMON_DIR}/thread_pool.cpp"
)
target_include_directories(cpufb_common PUBLIC "${CPUFB_COMMON_DIR}")
target_compile_options(cpufb_common PRIVATE -O2)
if(NOT CPUFB_ARCH STREQUAL "x64")
    # arm64 / riscv64 originally compiled common with -g -w
    target_compile_options(cpufb_common PRIVATE -g -w)
endif()
target_link_libraries(cpufb_common PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# Per-feature SIMD assembly objects (arm64 / riscv64). x64 .S files are still
# detected at runtime, but the project keeps them assembled by the C++ driver.
# ---------------------------------------------------------------------------
set(_simd_sources "")
foreach(feat IN LISTS CPUFB_SIMD_FEATURES)
    set(asm_file "${CPUFB_ASM_DIR}/${feat}.S")
    if(NOT EXISTS "${asm_file}")
        message(STATUS "cpufb: skipping ${feat} (no ${asm_file})")
        continue()
    endif()
    list(APPEND _simd_sources "${asm_file}")

    cpufb_simd_asm_flags("${CPUFB_ARCH}" "${feat}" _flags)
    if(_flags)
        set_source_files_properties("${asm_file}" PROPERTIES
            COMPILE_OPTIONS "${_flags}")
    endif()
endforeach()

# arm64 has an extra access.S that is always built (not feature-gated)
if(CPUFB_ARCH STREQUAL "arm64" AND EXISTS "${CPUFB_ASM_DIR}/access.S")
    list(APPEND _simd_sources "${CPUFB_ASM_DIR}/access.S")
endif()

if(_simd_sources)
    add_library(cpufb_simd OBJECT ${_simd_sources})
    target_include_directories(cpufb_simd PRIVATE "${CPUFB_ASM_DIR}")
    target_compile_options(cpufb_simd PRIVATE -g -w)
    # Pass detected SIMD feature tokens through to the .S preprocessor so
    # asm files can do conditional compilation (e.g. fp16 lanes inside an
    # otherwise-base-asimd file guarded by `#ifdef _ASIMD_HP_`).
    target_compile_definitions(cpufb_simd PRIVATE ${CPUFB_SIMD_FEATURES})
    if(ANDROID AND CMAKE_CROSSCOMPILING)
        target_compile_definitions(cpufb_simd PRIVATE CROSS_COMPILE)
    endif()
endif()

# ---------------------------------------------------------------------------
# Kernel sources (frequency.cpp / load.cpp / optional amx_kernel.cpp)
# ---------------------------------------------------------------------------
set(_kernel_sources
    "${CPUFB_KERNEL_DIR}/frequency.cpp"
    "${CPUFB_KERNEL_DIR}/load.cpp"
)
if(CPUFB_ARCH STREQUAL "arm64" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND _kernel_sources "${CPUFB_ARCH_DIR}/runtime_features.cpp")
endif()

# Apple Silicon has an extra AMX kernel
set(_amx_target "")
if(CPUFB_ARCH STREQUAL "arm64" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin"
        AND EXISTS "${CPUFB_KERNEL_DIR}/amx_kernel.cpp")
    add_library(cpufb_amx OBJECT "${CPUFB_KERNEL_DIR}/amx_kernel.cpp")
    target_include_directories(cpufb_amx PRIVATE
        "${CPUFB_COMMON_DIR}" "${CPUFB_KERNEL_DIR}")
    target_compile_options(cpufb_amx PRIVATE -O3 -g -w)
    set(_amx_target cpufb_amx)
endif()

# Per-source overrides matching the legacy scripts' optimization choices.
if(CPUFB_ARCH STREQUAL "x64")
    set_source_files_properties("${CPUFB_KERNEL_DIR}/frequency.cpp"
        PROPERTIES COMPILE_FLAGS "-g -O0")
    set_source_files_properties("${CPUFB_KERNEL_DIR}/load.cpp"
        PROPERTIES COMPILE_FLAGS "-g -O0")
elseif(CPUFB_ARCH STREQUAL "arm64")
    set_source_files_properties("${CPUFB_KERNEL_DIR}/frequency.cpp"
        PROPERTIES COMPILE_FLAGS "-O2 -g -w")
    # Match legacy build_arm64.sh: load.cpp is compiled with no explicit -O
    # (i.e. -O0). The benchmarked work is in the asm load_ptr() loop, but the
    # Fisher-Yates init and other setup code in this TU is sensitive to the
    # optimization level and we want runtime to match the legacy script.
    # The trailing -O0 here overrides the target-level -O2 because GCC/Clang
    # honor the last -O flag on the command line.
    set_source_files_properties("${CPUFB_KERNEL_DIR}/load.cpp"
        PROPERTIES COMPILE_FLAGS "-O0 -g -w")
endif()

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(cpufb
    "${CPUFB_ARCH_DIR}/cpufb.cpp"
    ${_kernel_sources}
)
target_include_directories(cpufb PRIVATE
    "${CPUFB_COMMON_DIR}"
    "${CPUFB_KERNEL_DIR}"
)
target_compile_definitions(cpufb PRIVATE ${CPUFB_SIMD_FEATURES})

if(CPUFB_ARCH STREQUAL "x64")
    target_compile_features(cpufb PRIVATE cxx_std_11)
    target_compile_options(cpufb PRIVATE -g -O2)
    target_link_options(cpufb PRIVATE -z noexecstack)
elseif(CPUFB_ARCH STREQUAL "riscv64")
    target_compile_options(cpufb PRIVATE -O3 -march=rv64gcv_zfh)
    target_link_options(cpufb PRIVATE -z noexecstack)
elseif(CPUFB_ARCH STREQUAL "arm64")
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # macOS Apple Silicon: legacy build_arm64.sh used -O0 here on purpose.
        target_compile_options(cpufb PRIVATE -O0 -g -w)
    else()
        target_compile_options(cpufb PRIVATE -O2 -g -w)
    endif()
    if(CPUFB_MARCH_FLAG)
        target_compile_options(cpufb PRIVATE ${CPUFB_MARCH_FLAG})
    endif()
    if(ANDROID AND CMAKE_CROSSCOMPILING)
        target_compile_definitions(cpufb PRIVATE CROSS_COMPILE)
        target_link_options(cpufb PRIVATE -static)
    endif()
endif()

target_link_libraries(cpufb PRIVATE
    cpufb_common
    Threads::Threads
)
if(TARGET cpufb_simd)
    target_link_libraries(cpufb PRIVATE cpufb_simd)
endif()
if(_amx_target)
    target_link_libraries(cpufb PRIVATE ${_amx_target})
endif()

# ---------------------------------------------------------------------------
# Optional: install + Android deploy helpers
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS cpufb RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

include(AndroidDeploy)
