# SimdFlags.cmake
#
# Single source of truth mapping a SIMD feature token (e.g. "_BF16_", "_SVE_")
# to:
#   * the -march=... flag needed when assembling that feature's .S file, and
#   * the broader -march=... flag the main C++ TU needs when *using* that
#     feature's intrinsics/assembly.
#
# Both used to live in CmakeLists.txt and build_arm64.sh; this file is the
# only place to edit them now.

# Per-feature assembler flag for ${arch}/asm/${feature}.S
function(cpufb_simd_asm_flags arch feature out_var)
    set(flag "")
    if(arch STREQUAL "arm64")
        if(feature STREQUAL "_BF16_")
            set(flag "-march=armv8.2-a+bf16")
        elseif(feature STREQUAL "_I8MM_")
            set(flag "-march=armv8.2-a+i8mm")
        elseif(feature STREQUAL "_ASIMD_DP_")
            set(flag "-march=armv8.2-a+dotprod")
        elseif(feature STREQUAL "_FHM_")
            set(flag "-march=armv8.2-a+fp16+fp16fml")
        elseif(feature STREQUAL "_ASIMD_FCMA_")
            set(flag "-march=armv8.3-a+fp16")
        elseif(feature STREQUAL "_ASIMD_REDUCE_"
                OR feature STREQUAL "_ASIMD_RECIP_")
            # fp16 lanes inside these files are guarded by `#ifdef _ASIMD_HP_`,
            # but the assembler still needs `+fp16` so the (potentially
            # included) fp16 ops parse. Harmless on hosts without HP — those
            # blocks are excluded by the preprocessor.
            set(flag "-march=armv8.2-a+fp16")
        elseif(feature STREQUAL "_ASIMD_INT_MAC_"
                OR feature STREQUAL "_ASIMD_TBL_")
            set(flag "-march=armv8-a")
        elseif(feature STREQUAL "_SME_")
            set(flag "-march=armv9-a+sme")
        elseif(feature STREQUAL "_SME2_")
            set(flag "-march=armv9-a+sme2")
        elseif(feature STREQUAL "_SMEf64_")
            set(flag "-march=armv9-a+sme2+sme-f64f64")
        elseif(feature STREQUAL "_SME_F16F16_")
            set(flag "-march=armv9.2-a+sme2+sme-f16f16")
        elseif(feature STREQUAL "_SME_I16I32_")
            set(flag "-march=armv9.2-a+sme2+sme-i16i32")
        elseif(feature STREQUAL "_SVE_I8MM_")
            set(flag "-march=armv8.6-a+sve")
        elseif(feature STREQUAL "_SVE_BF16_")
            set(flag "-march=armv8.6-a+sve")
        elseif(feature STREQUAL "_SVE_F32MM_")
            set(flag "-march=armv8.6-a+sve+f32mm")
        elseif(feature STREQUAL "_SVE_F64MM_")
            set(flag "-march=armv8.6-a+sve+f64mm")
        elseif(feature STREQUAL "_SVE_FP16_FMLA_")
            set(flag "-march=armv8.2-a+sve+fp16")
        elseif(feature STREQUAL "_SVE2_")
            set(flag "-march=armv9-a+sve2")
        elseif(feature STREQUAL "_SVE_")
            set(flag "-march=armv8-a+sve")
        elseif(feature STREQUAL "_ASIMD_HP_")
            set(flag "-march=armv8.2-a+fp16")
        elseif(feature STREQUAL "_ISSUE_")
            # _ISSUE_.S contains both the baseline and SVE-only variants when
            # the multi-ISA object is built.
            set(flag "-march=armv8-a+sve")
        else()
            # Mandatory AArch64/ASIMD, LDP and other baseline sources use an
            # explicit architecture so an x86-hosted cross compiler works.
            set(flag "-march=armv8-a")
        endif()
    elseif(arch STREQUAL "riscv64")
        # All riscv64 .S files are assembled with the same flag today
        set(flag "-march=rv64gcv_zfh")
    endif()
    # x64: no per-file march flags; assembler picks features from the directives
    # inside the .S file itself.
    set(${out_var} "${flag}" PARENT_SCOPE)
endfunction()

# Highest-priority "umbrella" -march flag that the main cpufb.cpp TU needs to
# compile against the union of detected SIMD features. Mirrors the "case" block
# in build_arm64.sh - later matches override earlier ones in priority order.
function(cpufb_compute_march_flag arch features_list out_var)
    set(march "")
    if(arch STREQUAL "arm64" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # Optional instructions live only in per-ISA assembly objects.  Keep
        # C++ translation units at the mandatory architecture baseline.
        set(march "-march=armv8-a")
    elseif(arch STREQUAL "arm64")
        # Priority order matches build_arm64.sh's case-statement order: later
        # cases (SME family) override earlier (SVE family) override base SVE.
        foreach(feat IN LISTS features_list)
            if(feat STREQUAL "_SVE_")
                if(march STREQUAL "")
                    set(march "-march=armv8-a+sve")
                endif()
            elseif(feat STREQUAL "_SVE2_")
                set(march "-march=armv9-a+sve2")
            elseif(feat STREQUAL "_SVE_BF16_")
                set(march "-march=armv8.6-a+sve")
            elseif(feat STREQUAL "_SVE_I8MM_")
                set(march "-march=armv8.6-a+sve")
            elseif(feat STREQUAL "_SVE_FP16_FMLA_")
                set(march "-march=armv8.2-a+sve+fp16")
            elseif(feat STREQUAL "_SVE_F64MM_")
                set(march "-march=armv8.6-a+sve+f64mm")
            elseif(feat STREQUAL "_SVE_F32MM_")
                set(march "-march=armv8.6-a+sve+f32mm")
            elseif(feat STREQUAL "_SME_")
                set(march "-march=armv9-a+sme")
            elseif(feat STREQUAL "_SME_I16I32_")
                set(march "-march=armv9.2-a+sme2+sme-i16i64")
            elseif(feat STREQUAL "_SME_F16F16_")
                set(march "-march=armv9.2-a+sme2+sme-f16f16")
            endif()
        endforeach()
    elseif(arch STREQUAL "riscv64")
        set(march "-march=rv64gcv_zfh")
    endif()
    set(${out_var} "${march}" PARENT_SCOPE)
endfunction()
