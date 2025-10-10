SRC=riscv64
ASM=$SRC/asm
COMM=common
BUILD_DIR=build_dir

# make directory
if [ -d "$BUILD_DIR" ]; then
    rm -rf $BUILD_DIR/*
else
    mkdir $BUILD_DIR
fi

# build common tools
g++ -O3 -c $COMM/table.cpp -o $BUILD_DIR/table.o
g++ -O3 -pthread -c $COMM/thread_pool.cpp -o $BUILD_DIR/thread_pool.o

# gen benchmark macro according to cpuid feature
gcc $SRC/cpuid.c -o $BUILD_DIR/cpuid
SIMD_MACRO=" "
SIMD_OBJ=" "
for SIMD in `$BUILD_DIR/cpuid`;
do
    SIMD_MACRO="$SIMD_MACRO-D$SIMD "
    SIMD_OBJ="$SIMD_OBJ$BUILD_DIR/$SIMD.o "
    as -march=rv64gcv_zfh -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
done

# compile cpufb
g++ -O3 -march=rv64gcv_zfh -I$COMM $SIMD_MACRO -c $SRC/cpufb.cpp -o $BUILD_DIR/cpufb.o
g++ -O3 -z noexecstack -pthread -o cpufb $BUILD_DIR/cpufb.o $BUILD_DIR/thread_pool.o $BUILD_DIR/table.o $SIMD_OBJ
