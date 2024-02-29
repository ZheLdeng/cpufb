#!/bin/bash
set -e

clean(){
    if [ -d "output" ]; then 
        rm -rf output
    fi
    if [ -e "cpufp" ]; then 
        rm -rf cpufp
    fi
}

if [ "$1" == "clean" ]; then
    clean
    exit
elif [ "$1" == "sve" ]; then
    g++ -O3 -c arm/cpuid_arm.cpp -DSVE
    g++ -O3 -o cpuid_gen_arm cpuid_arm.o
    ./cpuid_gen_arm $2
    sh build_kernel_arm.sh
    g++ -O3 -c arm/cpubm_arm.cpp
    g++ -O3 -c arm/cpufp_arm.cpp -DSVE
elif [ "$1" == "x86" ]; then
    g++ -O3 -c x86/cpuid_x86.cpp
    g++ -O3 -o cpuid_gen_x86 cpuid_x86.o
    ./cpuid_gen
    sh build_kernel.sh
    g++ -O3 -c x86/cpubm_x86.cpp
    g++ -O3 -c x86/cpufp_x86.cpp
else
    # clean
    if [ ! -d "output" ]; then 
        mkdir output
    fi
    cd output 
    gcc -c ../asm/ldp_asm.s 
    g++ -g -O3 -c ../arm/cpuid_arm.cpp -DNEON
    g++ -g -O3 -o cpuid_gen_arm cpuid_arm.o
    ./cpuid_gen_arm
    # sh build_kernel_arm.sh
    g++ -g -O3 -c -I ../common ../arm/cpubm_arm.cpp
    g++ -g -O3 -c -I../common -I./ -I../ -I../arm  ../arm/cpufp_arm.cpp -DNEON
fi

    g++ -g -O3 -c -I ../common ../common/table.cpp
    g++ -g -O3 -c -I ../common ../common/smtl.cpp

if [ "$1" == "x86" ]; then
    sh link.sh
else 
    sh link_arm.sh
fi
mv cpufp ../
