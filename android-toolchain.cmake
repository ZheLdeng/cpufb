# DEPRECATED: this file has moved to cmake/toolchains/aarch64-linux-gnu.cmake.
# It is kept here so that older shell scripts referring to it still work.
message(WARNING "android-toolchain.cmake is deprecated; "
                "use cmake/toolchains/aarch64-linux-gnu.cmake "
                "(picked up automatically by the aarch64-cross preset).")

# Android Cross-Compilation Toolchain File
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Set cross-compiler
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_ASM_COMPILER aarch64-linux-gnu-as)

# Set Android flag
set(ANDROID ON)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)