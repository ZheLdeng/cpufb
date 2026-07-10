# DetectSimd.cmake
#
# Populates ${out_features} (CMake list of "_FOO_" tokens) by either:
#   (a) building+running the per-arch cpuid detector via try_run, or
#   (b) honoring a user-supplied -DCPUFB_SIMD_FEATURES="..." when cross-compiling
#       (try_run cannot execute target binaries on the host).
#
# Replaces the old two-pass configure (build cpuid_detector then re-cmake) with
# a single configure step.

include_guard(GLOBAL)

# cpufb_detect_simd(<arch> <out_features>)
#
# arch         : "x64" | "arm64" | "riscv64"
# out_features : variable name to populate, e.g. CPUFB_SIMD_FEATURES
function(cpufb_detect_simd arch out_features)
    # Linux/Android AArch64 binaries are intentionally multi-ISA.  This list
    # says which kernels are compiled into the executable; runtime HWCAP
    # probing decides which of them may actually be registered and executed.
    if(arch STREQUAL "arm64" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        if(DEFINED CPUFB_SIMD_FEATURES_OVERRIDE AND NOT CPUFB_SIMD_FEATURES_OVERRIDE STREQUAL "")
            set(features "${CPUFB_SIMD_FEATURES_OVERRIDE}")
        else()
            set(features
                _ASIMD_ _ASIMD_HP_ _ASIMD_DP_ _BF16_ _I8MM_ _FHM_
                _ASIMD_FCMA_ _ASIMD_REDUCE_ _ASIMD_RECIP_
                _ASIMD_INT_MAC_ _ASIMD_TBL_ _SVE_ _SVE_I8MM_
                _SVE_BF16_ _SVE_F32MM_ _SVE_F64MM_ _SVE_FP16_FMLA_
                _SVE2_ _LDP_ _ISSUE_)
        endif()
        message(STATUS "cpufb: compiled ARM64 kernels: ${features}")
        message(STATUS "cpufb: runnable ARM64 kernels will be selected from HWCAP at startup")
        set(${out_features} "${features}" PARENT_SCOPE)
        return()
    endif()

    # Resolve which source file holds the detector for this arch.
    if(arch STREQUAL "x64")
        set(detector_src "${CMAKE_SOURCE_DIR}/x64/cpuid.c")
    elseif(arch STREQUAL "riscv64")
        set(detector_src "${CMAKE_SOURCE_DIR}/riscv64/cpuid.c")
    elseif(arch STREQUAL "arm64")
        if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
            set(detector_src "${CMAKE_SOURCE_DIR}/arm64/cpuid_android.c")
        else()
            set(detector_src "${CMAKE_SOURCE_DIR}/arm64/cpuid.cpp")
        endif()
    else()
        message(FATAL_ERROR "cpufb_detect_simd: unsupported arch '${arch}'")
    endif()

    # User override: skip detection entirely. Useful for cross-compile or
    # forcing a specific feature subset.
    if(DEFINED CPUFB_SIMD_FEATURES_OVERRIDE AND NOT CPUFB_SIMD_FEATURES_OVERRIDE STREQUAL "")
        message(STATUS "cpufb: using user-supplied SIMD features: ${CPUFB_SIMD_FEATURES_OVERRIDE}")
        set(${out_features} "${CPUFB_SIMD_FEATURES_OVERRIDE}" PARENT_SCOPE)
        return()
    endif()

    # Cross-compiling: try_run cannot execute target binaries on the host.
    # Fall back to a conservative arch-default set, which the user can override
    # via -DCPUFB_SIMD_FEATURES_OVERRIDE.
    if(CMAKE_CROSSCOMPILING)
        if(arch STREQUAL "arm64")
            set(default_features "_ASIMD_;_ASIMD_HP_;_ASIMD_DP_;_BF16_;_I8MM_;_FHM_;_LDP_;_ISSUE_")
        elseif(arch STREQUAL "x64")
            set(default_features "")
        elseif(arch STREQUAL "riscv64")
            set(default_features "")
        endif()
        message(WARNING
            "cpufb: cross-compiling, cannot run cpuid detector. "
            "Using default SIMD set for ${arch}: ${default_features}. "
            "Override with -DCPUFB_SIMD_FEATURES_OVERRIDE=\"_FOO_;_BAR_\".")
        set(${out_features} "${default_features}" PARENT_SCOPE)
        return()
    endif()

    # Native build: compile and run the detector.
    set(run_dir "${CMAKE_BINARY_DIR}/cpufb_cpuid_probe")
    file(MAKE_DIRECTORY "${run_dir}")
    set(stdout_file "${run_dir}/stdout.txt")

    try_run(run_result compile_result
        "${run_dir}"
        SOURCES "${detector_src}"
        RUN_OUTPUT_VARIABLE detector_stdout
        COMPILE_OUTPUT_VARIABLE compile_log
    )

    if(NOT compile_result)
        message(FATAL_ERROR
            "cpufb: failed to compile SIMD detector ${detector_src}\n${compile_log}")
    endif()
    if(NOT run_result EQUAL 0)
        message(WARNING
            "cpufb: SIMD detector exited with status ${run_result}; "
            "output may be partial:\n${detector_stdout}")
    endif()

    # detector_stdout is one feature per line, like "_BF16_\n_I8MM_\n..."
    string(REPLACE "\n" ";" lines "${detector_stdout}")
    set(features "")
    foreach(line IN LISTS lines)
        string(STRIP "${line}" tok)
        if(tok MATCHES "^_[A-Za-z0-9_]+_$")
            list(APPEND features "${tok}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES features)

    message(STATUS "cpufb: detected SIMD features: ${features}")
    set(${out_features} "${features}" PARENT_SCOPE)
endfunction()
