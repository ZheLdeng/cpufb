SRC=x64
ASM=$SRC/asm
KERNEL=$SRC/kernel
COMM=common
BUILD_DIR=build_dir

# 可以通过环境变量或直接修改此处更改标准（默认使用 c++17）
CXX_STD=-std=c++11

# make directory
if [ -d "$BUILD_DIR" ]; then
    rm -rf $BUILD_DIR/*
else
    mkdir $BUILD_DIR
fi

# build common tools
g++ $CXX_STD -O2 -c $COMM/table.cpp -o $BUILD_DIR/table.o
g++ $CXX_STD -O2 -pthread -c $COMM/thread_pool.cpp -o $BUILD_DIR/thread_pool.o

# gen benchmark macro according to cpuid feature
gcc $SRC/cpuid.c -o $BUILD_DIR/cpuid
SIMD_MACRO=" "
SIMD_OBJ=" "
for SIMD in `$BUILD_DIR/cpuid`;
do
    SIMD_MACRO="$SIMD_MACRO-D$SIMD "
    SIMD_OBJ="$SIMD_OBJ$BUILD_DIR/$SIMD.o "
    g++ $CXX_STD -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
done

# compile cpufb
g++ $CXX_STD -g -O2 -I$COMM -I$KERNEL $SIMD_MACRO -c $SRC/cpufb.cpp -o $BUILD_DIR/cpufb.o
g++ $CXX_STD -g -O0 -I$COMM -I$KERNEL $SIMD_MACRO -c $KERNEL/frequency.cpp -o $BUILD_DIR/frequency.o
g++ $CXX_STD -g -O0 -I$COMM -I$KERNEL $SIMD_MACRO -c $KERNEL/load.cpp -o $BUILD_DIR/load.o

g++ $CXX_STD -g -O2 -z noexecstack -pthread -o cpufb $BUILD_DIR/cpufb.o $BUILD_DIR/frequency.o $BUILD_DIR/load.o $BUILD_DIR/thread_pool.o $BUILD_DIR/table.o $SIMD_OBJ
