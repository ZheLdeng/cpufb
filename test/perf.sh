gcc -O0 -o  load load.c ldp_asm.s
perf stat     -e armv8_pmuv3_0/l1d_cache/  -e  armv8_pmuv3_0/l1d_cache_refill/ -e armv8_pmuv3_0/l2d_cache/ -e  armv8_pmuv3_0/l2d_cache_refill/  ./load 