set -x
SRC=arm64
ASM=$SRC/asm
KERNEL=$SRC/kernel
COMM=common
BUILD_DIR=build_dir


# Get system information
OS=$(uname -s)
CPU=$(uname -m)

CXX=g++
CC=gcc
CFLAG=

echo "Operating System  : $OS"
echo "CPU Architecture  : $CPU"

# Cross Compile 
if [[ "$CPU" == "x86_64" || "$CPU" == "i686" ]]; then
    CXX=aarch64-linux-gnu-g++
    CC=aarch64-linux-gnu-gcc
    CFLAG=-DCROSS_COMPILE
fi



# make directory
if [ -d "$BUILD_DIR" ]; then
    rm -rf $BUILD_DIR/*
else
    mkdir $BUILD_DIR
fi

# build common tools
$CXX  -g -O2 -c $COMM/table.cpp -o $BUILD_DIR/table.o
$CXX  -g -O2 -pthread -c $COMM/thread_pool.cpp -o $BUILD_DIR/thread_pool.o

# gen benchmark macro according to cpuid feature
$CC $SRC/cpuid_android.c -o $BUILD_DIR/cpuid $CFLAG -static
adb push $BUILD_DIR/cpuid  /data/local/tmp
SIMD_MACRO=" "
SIMD_OBJ=" "
#-march=native 交叉编译不用加#
for SIMD in `adb shell  /data/local/tmp/cpuid`;
do
    SIMD_MACRO="$SIMD_MACRO-D$SIMD "
    SIMD_OBJ="$SIMD_OBJ$BUILD_DIR/$SIMD.o "
    # $CC -march=armv8.4-a+bf16+i8mm+sve -g -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    as -g -mcpu=all -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    echo $SIMD
done
# compile cpufp
$CXX -g -O2 -I$COMM -I$KERNEL $SIMD_MACRO -c $SRC/cpufp.cpp -o $BUILD_DIR/cpufp.o 
$CXX -g -O2 -I$KERNEL -I$COMM $SIMD_MACRO -c $KERNEL/frequency.cpp -o $BUILD_DIR/frequency.o 
$CXX -g -O0 -I$KERNEL -I$COMM -c $KERNEL/load.cpp -o $BUILD_DIR/load.o 
$CXX -g -O2 -z noexecstack -pthread -static -o cpufp $BUILD_DIR/cpufp.o $BUILD_DIR/frequency.o $BUILD_DIR/load.o $BUILD_DIR/thread_pool.o $BUILD_DIR/table.o $SIMD_OBJ 
set +x
adb push cpufp /data/local/tmp/
adb shell /data/local/tmp/cpufp --thread_pool=[2]