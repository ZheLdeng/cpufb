# Toolchain for cross-compiling cpufb to aarch64-linux-gnu (used for Android
# /data/local/tmp deployment via adb, and for generic Linux ARM64 targets).
#
# Replaces the legacy android-toolchain.cmake at the repo root.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)

# When pushing to Android via adb, the binary needs to be self-contained.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")

# Don't search the host's /usr for libraries/headers.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
