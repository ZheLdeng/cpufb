# AndroidDeploy.cmake
#
# Optional adb-based deploy/run targets for Android (or any cross-built ARM64
# binary that wants to be pushed to /data/local/tmp on a connected device).
# Enabled with -DCPUFB_ENABLE_ANDROID_DEPLOY=ON.

include_guard(GLOBAL)

if(NOT CPUFB_ENABLE_ANDROID_DEPLOY)
    return()
endif()

find_program(ADB_EXECUTABLE adb)
if(NOT ADB_EXECUTABLE)
    message(WARNING "cpufb: CPUFB_ENABLE_ANDROID_DEPLOY=ON but `adb` was not found in PATH; "
                    "push_android / run_android_* targets will not work.")
    return()
endif()

set(_remote_path "/data/local/tmp/cpufb")

add_custom_target(push_android
    COMMAND "${ADB_EXECUTABLE}" push "$<TARGET_FILE:cpufb>" "${_remote_path}"
    DEPENDS cpufb
    COMMENT "adb push cpufb -> ${_remote_path}"
    USES_TERMINAL
)

# Convenience targets for individual cores. taskset mask = 1 << core.
foreach(core IN ITEMS 0 1 7)
    math(EXPR mask "1 << ${core}")
    add_custom_target(run_android_core${core}
        COMMAND "${ADB_EXECUTABLE}" shell taskset ${mask} ${_remote_path} --thread_pool=[${core}]
        DEPENDS push_android
        COMMENT "adb shell taskset ${mask} cpufb --thread_pool=[${core}]"
        USES_TERMINAL
    )
endforeach()
