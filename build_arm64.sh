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
CFLAG="-g -w"

echo "Operating System  : $OS"
echo "CPU Architecture  : $CPU"

# make directory
if [ -d "$BUILD_DIR" ]; then
    rm -rf $BUILD_DIR/*
else
    mkdir $BUILD_DIR
fi

# build common tools
$CXX $CFLAG -O2 -c $COMM/table.cpp -o $BUILD_DIR/table.o
$CXX $CFLAG -O2 -pthread -c $COMM/thread_pool.cpp -o $BUILD_DIR/thread_pool.o

# gen benchmark macro according to cpuid feature
gcc $CFLAG $SRC/cpuid.c -o $BUILD_DIR/cpuid
MARCH_FLAG=" "
SIMD_MACRO=" "
SIMD_OBJ=" "
for SIMD in `$BUILD_DIR/cpuid`;
do
    SIMD_MACRO="$SIMD_MACRO-D$SIMD "
    SIMD_OBJ="$SIMD_OBJ$BUILD_DIR/$SIMD.o "
    
    #as $CFLAG -mcpu=all -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    if [ _BF16_ = $SIMD ]; then
    $CC -march=armv8.2-a+bf16 $CFLAG -I$ASM -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    elif [ _I8MM_ = $SIMD ]; then
    $CC -march=armv8.2-a+i8mm $CFLAG -I$ASM -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    elif [ _SME_ = $SIMD ]; then
    $CC -march=armv9-a+sme $CFLAG -I$ASM -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    elif [ _SME2_ = $SIMD ]; then
    $CC -march=armv9-a+sme2 $CFLAG -I$ASM -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    elif [ _SMEf64_ = $SIMD ]; then
    $CC -march=armv9-a+sme2+sme-f64f64 $CFLAG -I$ASM -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    else
    $CC -march=native $CFLAG -I$ASM -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    fi
    case "$SIMD" in
    *_SME_*) 
        MARCH_FLAG="-march=armv9-a+sme "
        ;;
    *_SVE_FMLA_*) 
        MARCH_FLAG="-march=armv9-a+sve "
        ;;
    esac

done
# compile cpufp
$CXX $CFLAG -O2 -I$COMM -I$KERNEL $MARCH_FLAG $SIMD_MACRO -c $SRC/cpufp.cpp -o $BUILD_DIR/cpufp.o
$CXX $CFLAG -O2 -I$KERNEL -I$COMM $SIMD_MACRO -c $KERNEL/frequency.cpp -o $BUILD_DIR/frequency.o
$CXX $CFLAG -c $ASM/access.S -o $BUILD_DIR/access.o
$CXX $CFLAG -I$KERNEL -I$COMM $SIMD_MACRO -c $KERNEL/load.cpp -o $BUILD_DIR/load.o

$CXX $CFLAG -O2 -pthread -o cpufp $BUILD_DIR/cpufp.o $BUILD_DIR/frequency.o $BUILD_DIR/access.o $BUILD_DIR/load.o $BUILD_DIR/thread_pool.o $BUILD_DIR/table.o $SIMD_OBJ
set +x
