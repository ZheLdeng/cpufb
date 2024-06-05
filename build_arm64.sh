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
gcc $SRC/cpuid.c -o $BUILD_DIR/cpuid $CFLAG
SIMD_MACRO=" "
SIMD_OBJ=" "
for SIMD in `$BUILD_DIR/cpuid`;
do
    SIMD_MACRO="$SIMD_MACRO-D$SIMD "
    SIMD_OBJ="$SIMD_OBJ$BUILD_DIR/$SIMD.o "
    $CC -march=native -g -I$ASM -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    #as -g -mcpu=all -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    
done
# compile cpufp
$CXX -g -O2 -I$COMM -I$KERNEL $SIMD_MACRO -c $SRC/cpufp.cpp -o $BUILD_DIR/cpufp.o 
$CXX -g -O2 -I$KERNEL -I$COMM $SIMD_MACRO -c $KERNEL/frequency.cpp -o $BUILD_DIR/frequency.o 
$CXX -g -O0 -I$KERNEL -I$COMM $SIMD_MACRO -c $KERNEL/load.cpp -o $BUILD_DIR/load.o 
$CXX -g -O2 -z noexecstack -pthread -o cpufp $BUILD_DIR/cpufp.o $BUILD_DIR/frequency.o $BUILD_DIR/load.o $BUILD_DIR/thread_pool.o $BUILD_DIR/table.o $SIMD_OBJ 
set +x
