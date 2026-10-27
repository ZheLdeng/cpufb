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

if(WIN32)
    if(NOT CPUFB_ARCH STREQUAL "x64")
        message(FATAL_ERROR "cpufb: Windows support currently requires x64")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR
            "cpufb: Windows currently requires the MinGW-w64 GNU toolchain; "
            "MSVC does not support the x64 GNU assembly sources")
    endif()
    include(CheckIncludeFileCXX)
    check_include_file_cxx("pthread.h" CPUFB_HAS_PTHREAD_H)
    if(NOT CPUFB_HAS_PTHREAD_H)
        message(FATAL_ERROR
            "cpufb: Windows builds require the MinGW-w64 POSIX thread model")
    endif()
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles(
        "__attribute__((sysv_abi)) void kernel() {} int main() { kernel(); }"
        CPUFB_HAS_SYSV_ABI_ATTRIBUTE)
    if(NOT CPUFB_HAS_SYSV_ABI_ATTRIBUTE)
        message(FATAL_ERROR
            "cpufb: this MinGW compiler must support GCC's sysv_abi attribute")
    endif()
    add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601 NOMINMAX
        CLOCK_MONOTONIC_RAW=CLOCK_MONOTONIC)
endif()

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
    "${CPUFB_COMMON_DIR}/cache/cache_bandwidth.cpp"
    "${CPUFB_COMMON_DIR}/cache/cache_curve.cpp"
    "${CPUFB_COMMON_DIR}/cache/cache_report.cpp"
    "${CPUFB_COMMON_DIR}/cache/cache_topology.cpp"
    "${CPUFB_COMMON_DIR}/cache/associativity_probe.cpp"
    "${CPUFB_COMMON_DIR}/cache/cacheline_probe.cpp"
    "${CPUFB_COMMON_DIR}/benchmark_tables.cpp"
    "${CPUFB_COMMON_DIR}/cli.cpp"
    "${CPUFB_COMMON_DIR}/issue/issue_probe.cpp"
    "${CPUFB_COMMON_DIR}/cache/prefetch_probe.cpp"
    "${CPUFB_COMMON_DIR}/sys/system_info.cpp"
    "${CPUFB_COMMON_DIR}/table.cpp"
    "${CPUFB_COMMON_DIR}/sys/thread_pool.cpp"
)
if(WIN32)
    target_sources(cpufb_common PRIVATE
        "${CPUFB_COMMON_DIR}/sys/windows_platform.cpp")
endif()
target_include_directories(cpufb_common PUBLIC "${CPUFB_COMMON_DIR}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # system_info.cpp reads the nominal CPU frequency from the IORegistry.
    # INTERFACE so that every target linking the object library gets them.
    target_link_libraries(cpufb_common INTERFACE
        "-framework IOKit" "-framework CoreFoundation")
endif()
target_compile_options(cpufb_common PRIVATE -O2)
if(NOT CPUFB_ARCH STREQUAL "x64")
    # arm64 / riscv64 originally compiled common with -g -w
    target_compile_options(cpufb_common PRIVATE -g -w)
endif()
target_link_libraries(cpufb_common PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(cpufb_common PUBLIC powrprof)
endif()

# ---------------------------------------------------------------------------
# Per-feature SIMD assembly objects (arm64 / riscv64). x64 .S files are still
# detected at runtime, but the project keeps them assembled by the C++ driver.
# ---------------------------------------------------------------------------
include(CheckCCompilerFlag)
set(_simd_sources "")
set(_dropped_features "")
foreach(feat IN LISTS CPUFB_SIMD_FEATURES)
    set(asm_file "${CPUFB_ASM_DIR}/${feat}.S")
    if(NOT EXISTS "${asm_file}")
        message(STATUS "cpufb: skipping ${feat} (no ${asm_file})")
        continue()
    endif()
    list(APPEND _simd_sources "${asm_file}")

    cpufb_simd_asm_flags("${CPUFB_ARCH}" "${feat}" _flags)
    if(_flags)
        # An older toolchain (GCC 9 / binutils 2.34 rejects +bf16, +i8mm and
        # every SME modifier) must drop the feature, not fail the build: the
        # rows are selected at run time and simply stay absent.
        string(MAKE_C_IDENTIFIER "CPUFB_ASM_ACCEPTS_${feat}" _accepts_var)
        check_c_compiler_flag("${_flags}" ${_accepts_var})
        if(NOT ${_accepts_var})
            message(WARNING
                "cpufb: dropping ${feat}: this toolchain rejects ${_flags}")
            list(REMOVE_ITEM _simd_sources "${asm_file}")
            list(APPEND _dropped_features "${feat}")
            continue()
        endif()
        set_source_files_properties("${asm_file}" PROPERTIES
            COMPILE_OPTIONS "${_flags}")
    endif()
endforeach()
if(_dropped_features)
    list(REMOVE_ITEM CPUFB_SIMD_FEATURES ${_dropped_features})
    message(STATUS "cpufb: kernels after toolchain filtering: ${CPUFB_SIMD_FEATURES}")
endif()

# arm64 has an extra access.S that is always built (not feature-gated).
if(CPUFB_ARCH STREQUAL "arm64" AND EXISTS "${CPUFB_ASM_DIR}/access.S")
    list(APPEND _simd_sources "${CPUFB_ASM_DIR}/access.S")
endif()

# x64 load and baseline multi-issue benchmarks are infrastructure rather than
# optional compute ISA rows.  Keep them available even when a user supplies a
# reduced CPUFB_SIMD_FEATURES_OVERRIDE; native CPUID detection also lists these
# files, so remove any duplicate paths afterwards.
if(CPUFB_ARCH STREQUAL "x64")
    list(APPEND _simd_sources
        "${CPUFB_ASM_DIR}/_LDP_.S"
        "${CPUFB_ASM_DIR}/_ISSUE_.S"
    )
    list(REMOVE_DUPLICATES _simd_sources)
endif()

if(_simd_sources)
    add_library(cpufb_simd OBJECT ${_simd_sources})
    # The issue-probe generator (_ISSUE_.S) includes the kernel directory's
    # class list and the shared layout header.
    target_include_directories(cpufb_simd PRIVATE
        "${CPUFB_ASM_DIR}" "${CPUFB_KERNEL_DIR}" "${CPUFB_COMMON_DIR}")
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
if(CPUFB_ARCH STREQUAL "arm64" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND _kernel_sources "${CPUFB_KERNEL_DIR}/macos_counters.cpp")
endif()
if(CPUFB_ARCH STREQUAL "arm64" OR CPUFB_ARCH STREQUAL "x64")
    # The stream benchmark is shared; only the kernel selection is per arch.
    # issue_kernels.cpp holds the tables of the generated issue-probe kernels.
    list(APPEND _kernel_sources
        "${CPUFB_COMMON_DIR}/cache/memory_bandwidth.cpp"
        "${CPUFB_KERNEL_DIR}/stream_kernel.cpp"
        "${CPUFB_KERNEL_DIR}/issue_kernels.cpp")
endif()
if(CPUFB_ARCH STREQUAL "x64")
    # The frequency probe is an x86-64 SSE/SSE2 baseline and must not depend
    # on whichever optional compute kernels were selected by CPUID/override.
    list(APPEND _kernel_sources "${CPUFB_ASM_DIR}/frequency.S")
endif()
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

# No per-source -O0 overrides: every timed region is either an assembly kernel
# or a dependent-load loop that needs optimization to stay register-resident
# (at -O0 each pointer-chase step round-trips through the stack and adds
# store-forwarding latency to the measured access).  The legacy scripts built
# load.cpp and frequency.cpp unoptimized only by omission; that also masked
# assembly kernels which clobbered callee-saved d8-d15, since fixed.

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
    if(NOT WIN32)
        target_link_options(cpufb PRIVATE -z noexecstack)
    endif()
elseif(CPUFB_ARCH STREQUAL "riscv64")
    target_compile_options(cpufb PRIVATE -O3 -march=rv64gcv_zfh_zicbom)
    target_link_options(cpufb PRIVATE -z noexecstack)
elseif(CPUFB_ARCH STREQUAL "arm64")
    target_compile_options(cpufb PRIVATE -O2 -g -w)
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
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(cpufb PRIVATE "-ldl")
endif()

# ---------------------------------------------------------------------------
# Optional: install + Android deploy helpers
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS cpufb RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

include(AndroidDeploy)

# ---------------------------------------------------------------------------
# CLI regression tests
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    # Unit tests of the common library: one executable per source file.
    function(cpufb_add_unit_test name source)
        add_executable(${name}_test "${CMAKE_SOURCE_DIR}/tests/${source}")
        target_link_libraries(${name}_test PRIVATE cpufb_common)
        add_test(NAME ${name} COMMAND ${name}_test)
        set_tests_properties(${name} PROPERTIES LABELS "unit" TIMEOUT 30)
    endfunction()
    cpufb_add_unit_test(cpufb_latency_pairing test_latency_pairing.cpp)
    cpufb_add_unit_test(cpufb_cache_topology test_cache_topology.cpp)
    cpufb_add_unit_test(cpufb_cache_curve test_cache_curve.cpp)
    cpufb_add_unit_test(cpufb_prefetch_probe test_prefetch_probe.cpp)
    cpufb_add_unit_test(cpufb_issue_probe test_issue_probe.cpp)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME cpufb_pair_issue_unit
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tests/test_pair_issue.py"
        )
        set_tests_properties(cpufb_pair_issue_unit PROPERTIES
            LABELS "unit"
            TIMEOUT 30
        )
    else()
        message(WARNING
            "cpufb: Python3 not found; pair-issue unit tests disabled")
    endif()
endif()

# The shell CLI harness relies on Linux /proc and POSIX paths; Windows has
# native CTest smoke cases below instead.
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND NOT WIN32 AND
        (CPUFB_ARCH STREQUAL "x64" OR CPUFB_ARCH STREQUAL "arm64"
            OR CPUFB_ARCH STREQUAL "riscv64"))
    find_program(CPUFB_BASH_EXECUTABLE bash)
    if(CPUFB_BASH_EXECUTABLE)
        set(_cli_test_script "${CMAKE_SOURCE_DIR}/tests/test_cli.sh")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Testing/tmp")
        set(_cli_test_names
            list
            invalid_filters
            filters_txt
            mode_all_override
            csv_output
            # Asserts that the line-size and associativity probes report a
            # value; riscv64 had no CLI test at all, which is how it kept two
            # columns that were copies of the OS values.
            cache_probes
        )
        if(NOT CPUFB_ARCH STREQUAL "riscv64")
            # The riscv64 backend rejects the instruction sweep and the
            # memory-bandwidth options.
            list(APPEND _cli_test_names
                instruction_matching
                sweep_output
                memory_bandwidth
                cache_bandwidth
                issue_probe
            )
        endif()
        foreach(test_name IN LISTS _cli_test_names)
            add_test(
                NAME "cpufb_cli_${test_name}"
                COMMAND "${CPUFB_BASH_EXECUTABLE}" "${_cli_test_script}"
                    "${test_name}" "$<TARGET_FILE:cpufb>" "${CPUFB_ARCH}"
            )
            # Scratch files stay inside the build tree: on a shared cluster the
            # project directory is the only place a job may write to.
            set_tests_properties("cpufb_cli_${test_name}" PROPERTIES
                LABELS "cli"
                RUN_SERIAL TRUE
                TIMEOUT 240
                ENVIRONMENT "TMPDIR=${CMAKE_BINARY_DIR}/Testing/tmp"
            )
        endforeach()
    else()
        message(WARNING "cpufb: bash not found; CLI regression tests disabled")
    endif()
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND WIN32)
    add_executable(cpufb_windows_platform_test
        "${CMAKE_SOURCE_DIR}/tests/test_windows_platform.cpp")
    target_link_libraries(cpufb_windows_platform_test PRIVATE cpufb_common)
    add_test(NAME cpufb_windows_platform COMMAND cpufb_windows_platform_test)
    set_tests_properties(cpufb_windows_platform PROPERTIES
        LABELS "unit" TIMEOUT 30)
    add_test(
        NAME cpufb_cli_list_categories
        COMMAND cpufb --list-categories
    )
    add_test(
        NAME cpufb_cli_list_instructions
        COMMAND cpufb --list-instructions
    )
    add_test(
        NAME cpufb_cli_invalid_thread_pool
        COMMAND cpufb "--thread_pool=[1,]" --include-test=freq
    )
    set_tests_properties(cpufb_cli_invalid_thread_pool PROPERTIES
        WILL_FAIL TRUE
        LABELS "cli"
        TIMEOUT 10
    )
    set_tests_properties(cpufb_cli_list_categories
            cpufb_cli_list_instructions PROPERTIES
        LABELS "cli"
        TIMEOUT 10
    )
endif()
