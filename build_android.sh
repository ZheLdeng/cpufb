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
AS=as
CFLAG=

echo "Operating System  : $OS"
echo "CPU Architecture  : $CPU"

# Cross Compile
if [[ "$CPU" == "x86_64" || "$CPU" == "i686" ]]; then
    CXX=aarch64-linux-gnu-g++
    CC=aarch64-linux-gnu-gcc
    AS=aarch64-linux-gnu-as
    CFLAG=-DCROSS_COMPILE
fi


echo $CC
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
MARCH_FLAG=" "
#-march=native 交叉编译不用加#
for SIMD in `adb shell  /data/local/tmp/cpuid`;
do
    SIMD_MACRO="$SIMD_MACRO-D$SIMD "
    SIMD_OBJ="$SIMD_OBJ$BUILD_DIR/$SIMD.o "
    #$AS  -g -mcpu=all -I$ASM/ -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    #$CC -march=armv8.6-a+sve -I$ASM/ -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
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
    elif [ _SVE_ = $SIMD ]; then
    $CC -march=armv8-a+sve $CFLAG -I$ASM -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    else
    $CC -march=native $CFLAG -I$ASM -c $ASM/$SIMD.S -o $BUILD_DIR/$SIMD.o
    fi
    case "$SIMD" in
    *_SME_*) 
        MARCH_FLAG="-march=armv9-a+sme "
        ;;
    *_SVE_*) 
        MARCH_FLAG="-march=armv8-a+sve "
        ;;
    esac

done
# compile cpufp
$CXX $CFLAG -O2 -I$COMM -I$KERNEL $MARCH_FLAG $SIMD_MACRO -c $SRC/cpufp.cpp -o $BUILD_DIR/cpufp.o
$CXX $CFLAG -O2 -I$KERNEL -I$COMM $SIMD_MACRO -c $KERNEL/frequency.cpp -o $BUILD_DIR/frequency.o
$CXX $CFLAG -c $ASM/access.S -o $BUILD_DIR/access.o
$CXX $CFLAG -I$KERNEL -I$COMM $SIMD_MACRO -c $KERNEL/load.cpp -o $BUILD_DIR/load.o

$CXX $CFLAG -static -O2 -pthread -o cpufp $BUILD_DIR/cpufp.o $BUILD_DIR/frequency.o $BUILD_DIR/access.o $BUILD_DIR/load.o $BUILD_DIR/thread_pool.o $BUILD_DIR/table.o $SIMD_OBJ
set +x
adb push cpufp /data/local/tmp/
adb shell taskset 1 /data/local/tmp/cpufp --thread_pool=[0]
adb shell taskset 2 /data/local/tmp/cpufp --thread_pool=[1]
adb shell taskset 8 /data/local/tmp/cpufp --thread_pool=[7]

