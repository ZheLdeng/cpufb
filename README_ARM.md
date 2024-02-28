# cpufp

This is a cpu tool for benchmarking the floating-points peak performance. Now it supports linux and x86-64 platform. It can automatically sense the local SIMD ISAs while compiling.

## How to use

build:

`./build.sh`

benchmark:

`./cpufp --thread_pool=[xxx]`

clean:

`./build.sh clean`

xxx indicates that all the cores defined by xxx will be used for benchmarking(by affinity setting). For example, [0,3,5-8,13-15].

## Support ARM SIMD ISA

|ISA|Data Type|Description|
| ------------ | ------------ | ------------ |
|FMLA|fp32/fp64||
|L1 cache|fp32|Bandwidth test only test for single core|
|L2 cache|fp32|Bandwidth test only test for single core|

## Some benchmark results

### Kunpeng 920 8cores

For single big core:

<pre>
$ ./cpufp --thread_pool=[0]
Number Threads: 1
Thread Pool Binding: 0
---------------------------------------------------
| Instruction Set | Data Type | Peak Performance  |
| ARMV8_FMLA      | FP32      | 41.571 GFLOPS     |
| ARMV8_FMLA      | FP64      | 10.392 GFLOPS     |
| LOAD_L1         | FP32      | 30.405 Byte/Cycle |
| LOAD_L2         | FP32      | 24.296 Byte/Cycle |
---------------------------------------------------
</pre>

For multiple big cores:

<pre>
$ ./cpufp --thread_pool=[0-4]
Number Threads: 5
Thread Pool Binding: 0 1 2 3 4
---------------------------------------------------
| Instruction Set | Data Type | Peak Performance  |
| ARMV8_FMLA      | FP32      | 207.86 GFLOPS     |
| ARMV8_FMLA      | FP64      | 51.967 GFLOPS     |
| LOAD_L1         | FP32      | 28.841 Byte/Cycle |
| LOAD_L2         | FP32      | 23.498 Byte/Cycle |
---------------------------------------------------
</pre>

## TODO

The next version may support ARMv7 and ARMv8 architectures.

Welcome for bug reporting.
