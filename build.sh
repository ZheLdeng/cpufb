#!/bin/bash

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored message
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Show usage
show_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

OPTIONS:
    -h, --help              Show this help message
    -a, --android           Build for Android (auto-detect cross-compilation)
    -c, --clean             Clean build directory before building
    -j, --jobs N            Number of parallel jobs (default: auto)
    -d, --debug             Build in Debug mode (default: Release)
    -r, --rebuild           Force rebuild (clean + build)
    --push                  Push to Android device after build (Android only)
    --run-core N            Run on Android device core N after push (Android only)
    --toolchain FILE        Use custom toolchain file

EXAMPLES:
    $0                      # Build for current platform
    $0 -a                   # Build for Android (auto cross-compile if needed)
    $0 -a --push            # Build for Android and push to device
    $0 -c -j8               # Clean build with 8 parallel jobs
    $0 -d                   # Build in Debug mode
    $0 -a --run-core 0      # Build, push and run on Android core 0

EOF
}

# Parse arguments
ANDROID=false
CLEAN=false
REBUILD=false
PUSH=false
RUN_CORE=""
BUILD_TYPE="Release"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
TOOLCHAIN_FILE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -a|--android)
            ANDROID=true
            shift
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--rebuild)
            REBUILD=true
            shift
            ;;
        --push)
            PUSH=true
            shift
            ;;
        --run-core)
            RUN_CORE="$2"
            shift 2
            ;;
        --toolchain)
            TOOLCHAIN_FILE="$2"
            shift 2
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Get system information
HOST_OS=$(uname -s)
HOST_ARCH=$(uname -m)

print_info "Host System: $HOST_OS"
print_info "Host Architecture: $HOST_ARCH"

# Determine build directory
if [ "$ANDROID" = true ]; then
    BUILD_DIR="build-android"
else
    BUILD_DIR="build"
fi

print_info "Build directory: $BUILD_DIR"
print_info "Build type: $BUILD_TYPE"
print_info "Parallel jobs: $JOBS"

# Handle clean/rebuild
if [ "$REBUILD" = true ] || [ "$CLEAN" = true ]; then
    if [ -d "$BUILD_DIR" ]; then
        print_info "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    fi
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Prepare CMake arguments
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

# Android build configuration
if [ "$ANDROID" = true ]; then
    print_info "Configuring for Android build..."
    
    # Check if cross-compilation is needed
    if [[ "$HOST_ARCH" == "x86_64" || "$HOST_ARCH" == "i686" ]]; then
        print_warning "Cross-compilation detected (x86/x64 -> ARM64)"
        
        # Check for cross-compiler
        if ! command -v aarch64-linux-gnu-g++ &> /dev/null; then
            print_error "Cross-compiler not found!"
            print_info "Please install: sudo apt-get install g++-aarch64-linux-gnu gcc-aarch64-linux-gnu"
            exit 1
        fi
        
        # Use toolchain file
        if [ -z "$TOOLCHAIN_FILE" ]; then
            TOOLCHAIN_FILE="../android-toolchain.cmake"
        fi
        
        if [ ! -f "$TOOLCHAIN_FILE" ]; then
            print_error "Toolchain file not found: $TOOLCHAIN_FILE"
            exit 1
        fi
        
        CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE"
        print_info "Using toolchain: $TOOLCHAIN_FILE"
    else
        print_info "Native Android build on ARM64"
        CMAKE_ARGS="$CMAKE_ARGS -DANDROID=ON"
    fi
    
    # Check for adb
    if ! command -v adb &> /dev/null; then
        print_warning "adb not found. Push/Run features will not work."
        print_info "Install Android SDK Platform-Tools to use these features."
    fi
else
    # Native build
    print_info "Configuring for native build ($HOST_ARCH)..."
fi

# Custom toolchain
if [ -n "$TOOLCHAIN_FILE" ] && [ "$ANDROID" = false ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE"
    print_info "Using custom toolchain: $TOOLCHAIN_FILE"
fi

print_info "Running CMake configuration (1st pass)..."
cmake .. $CMAKE_ARGS

if [ $? -ne 0 ]; then
    print_error "CMake configuration failed!"
    exit 1
fi

print_success "CMake configuration (1st pass) completed"

# First build to generate SIMD detection
print_info "Running initial build to detect CPU features..."
make -j1

if [ $? -ne 0 ]; then
    print_error "Initial build failed!"
    exit 1
fi

print_success "CPU feature detection completed"

# Check if simd_config.cmake was generated
if [ -f "simd_config.cmake" ]; then
    print_success "SIMD configuration generated"
    
    # Show detected features
    if grep -q "SIMD_FEATURES" simd_config.cmake; then
        FEATURES=$(grep "set(SIMD_FEATURES" simd_config.cmake | sed 's/set(SIMD_FEATURES//' | sed 's/)//' | tr -d '\n')
        if [ -n "$FEATURES" ]; then
            print_info "Detected SIMD features:$FEATURES"
        else
            print_warning "No SIMD features detected"
        fi
    fi
else
    print_warning "SIMD configuration not generated, continuing anyway..."
fi

# Reconfigure with detected features
print_info "Running CMake configuration (2nd pass)..."
cmake .. $CMAKE_ARGS

if [ $? -ne 0 ]; then
    print_error "CMake reconfiguration failed!"
    exit 1
fi

print_success "CMake configuration (2nd pass) completed"

# Final build
print_info "Building cpufb with $JOBS parallel jobs..."
make -j$JOBS

if [ $? -ne 0 ]; then
    print_error "Build failed!"
    exit 1
fi

print_success "Build completed successfully!"

# Show binary location
if [ -f "cpufb" ]; then
    BINARY_PATH=$(pwd)/cpufb
    print_success "Binary location: $BINARY_PATH"
    
    # Show binary info
    if command -v file &> /dev/null; then
        FILE_INFO=$(file cpufb)
        print_info "Binary info: $FILE_INFO"
    fi
    
    if command -v ls &> /dev/null; then
        SIZE=$(ls -lh cpufb | awk '{print $5}')
        print_info "Binary size: $SIZE"
    fi
else
    print_error "Binary 'cpufb' not found!"
    exit 1
fi

# Android-specific operations
if [ "$ANDROID" = true ]; then
    # Check adb connection
    if command -v adb &> /dev/null; then
        ADB_DEVICES=$(adb devices | grep -v "List" | grep "device$" | wc -l)
        
        if [ $ADB_DEVICES -eq 0 ]; then
            print_warning "No Android device connected"
            print_info "Connect device and run: make push_android"
        else
            print_success "Android device(s) connected: $ADB_DEVICES"
            
            # Push to device if requested
            if [ "$PUSH" = true ]; then
                print_info "Pushing binary to Android device..."
                make push_android
                
                if [ $? -eq 0 ]; then
                    print_success "Binary pushed to /data/local/tmp/cpufb"
                    
                    # Run on specific core if requested
                    if [ -n "$RUN_CORE" ]; then
                        print_info "Running on core $RUN_CORE..."
                        case $RUN_CORE in
                            0)
                                make run_android_core0
                                ;;
                            1)
                                make run_android_core1
                                ;;
                            7)
                                make run_android_core7
                                ;;
                            *)
                                print_warning "No predefined target for core $RUN_CORE"
                                TASKSET=$((2 ** RUN_CORE))
                                print_info "Running: adb shell taskset $TASKSET /data/local/tmp/cpufb"
                                adb shell taskset $TASKSET /data/local/tmp/cpufb
                                ;;
                        esac
                    fi
                else
                    print_error "Failed to push binary to device"
                fi
            else
                print_info "To push to device: cd $BUILD_DIR && make push_android"
                print_info "Or rebuild with: $0 -a --push"
            fi
        fi
    fi
fi

# Show next steps
echo ""
print_success "=========================================="
print_success "Build completed successfully!"
print_success "=========================================="
echo ""

if [ "$ANDROID" = true ]; then
    echo "Next steps for Android:"
    echo "  1. Push to device:  cd $BUILD_DIR && make push_android"
    echo "  2. Run on core 0:   cd $BUILD_DIR && make run_android_core0"
    echo "  3. Run on core 1:   cd $BUILD_DIR && make run_android_core1"
    echo "  4. Run on core 7:   cd $BUILD_DIR && make run_android_core7"
    echo ""
    echo "Or use shortcuts:"
    echo "  $0 -a --push            # Build and push"
    echo "  $0 -a --run-core 0      # Build, push and run on core 0"
else
    echo "Run the binary:"
    echo "  ./$BUILD_DIR/cpufb --thread_pool=[]"
    echo ""
    echo "Build for Android:"
    echo "  $0 -a                   # Build for Android"
    echo "  $0 -a --push            # Build and push to device"
fi

echo ""