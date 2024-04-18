SRC=arm64
ASM=$SRC/asm
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
    CFLAG=-DCROSS_COMPLIE
fi



# make directory
if [ -d "$BUILD_DIR" ]; then
    rm -rf $BUILD_DIR/*
else
    mkdir $BUILD_DIR
fi

# build common tools
$CXX  -g -O2 -c $COMM/table.cpp -o $BUILD_DIR/table.o
$CXX  -g -O2 -pthread -c $COMM/smtl.cpp -o $BUILD_DIR/smtl.o

# gen benchmark macro according to cpuid feature
$CC $SRC/cpuid.c -o $BUILD_DIR/cpuid $CFLAG
SIMD_MACRO=" "
SIMD_OBJ=" "
for SIMD in `$BUILD_DIR/cpuid`;
do
    SIMD_MACRO="$SIMD_MACRO-D$SIMD "
    SIMD_OBJ="$SIMD_OBJ$BUILD_DIR/$SIMD.o "
    as -g -mcpu=all -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
done
# compile cpufp
$CXX -g -O2 -I$COMM $SIMD_MACRO -c $SRC/cpufp.cpp -o $BUILD_DIR/cpufp.o 
$CXX -g -O2 -z noexecstack -pthread -static -o cpufp $BUILD_DIR/cpufp.o $BUILD_DIR/smtl.o $BUILD_DIR/table.o $SIMD_OBJ 
