#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
// void load_ldp_kernel(float*);

void cpufp_kernel_armv8_fmla_f32(int64_t looptime)
{
    for (int i = 0; i < looptime; i++) {
        asm volatile(
            "fmla v0.4s, v0.4s, v0.4s\n\t"
            "fmla v1.4s, v1.4s, v1.4s\n\t"
            "fmla v2.4s, v2.4s, v2.4s\n\t"
            "fmla v3.4s, v3.4s, v3.4s\n\t"

            "fmla v4.4s, v4.4s, v4.4s\n\t"
            "fmla v5.4s, v5.4s, v5.4s\n\t"
            "fmla v6.4s, v6.4s, v6.4s\n\t"
            "fmla v7.4s, v7.4s, v7.4s\n\t"

            "fmla v8.4s, v8.4s, v8.4s\n\t"
            "fmla v9.4s, v9.4s, v9.4s\n\t"
            "fmla v10.4s, v10.4s, v10.4s\n\t"
            "fmla v11.4s, v11.4s, v11.4s\n\t"

            "fmla v12.4s, v12.4s, v12.4s\n\t"
            "fmla v13.4s, v13.4s, v13.4s\n\t"
            "fmla v14.4s, v14.4s, v14.4s\n\t"
            "fmla v15.4s, v15.4s, v15.4s\n\t"

            "fmla v16.4s, v16.4s, v16.4s\n\t"
            "fmla v17.4s, v17.4s, v17.4s\n\t"
            "fmla v18.4s, v18.4s, v18.4s\n\t"
            "fmla v19.4s, v19.4s, v19.4s\n\t"

            "fmla v20.4s, v20.4s, v20.4s\n\t"
            "fmla v21.4s, v21.4s, v21.4s\n\t"
            "fmla v22.4s, v22.4s, v22.4s\n\t"
            "fmla v23.4s, v23.4s, v23.4s\n\t"

            "fmla v24.4s, v24.4s, v24.4s\n\t"
            "fmla v25.4s, v25.4s, v25.4s\n\t"
            "fmla v26.4s, v26.4s, v26.4s\n\t"
            "fmla v27.4s, v27.4s, v27.4s\n\t"

            "fmla v28.4s, v28.4s, v28.4s\n\t"
            "fmla v29.4s, v29.4s, v29.4s\n\t"
            "fmla v30.4s, v30.4s, v30.4s\n\t"
            "fmla v31.4s, v31.4s, v31.4s\n\t"
        );
    }   
}

void cpufp_kernel_armv8_fmla_f64(int64_t looptime)
{
    for (int i = 0; i < looptime; i++) {
        asm volatile(
            "fmla v0.2d, v0.2d, v0.2d\n\t"
            "fmla v1.2d, v1.2d, v1.2d\n\t"
            "fmla v2.2d, v2.2d, v2.2d\n\t"
            "fmla v3.2d, v3.2d, v3.2d\n\t"

            "fmla v4.2d, v4.2d, v4.2d\n\t"
            "fmla v5.2d, v5.2d, v5.2d\n\t"
            "fmla v6.2d, v6.2d, v6.2d\n\t"
            "fmla v7.2d, v7.2d, v7.2d\n\t"

            "fmla v8.2d, v8.2d, v8.2d\n\t"
            "fmla v9.2d, v9.2d, v9.2d\n\t"
            "fmla v10.2d, v10.2d, v10.2d\n\t"
            "fmla v11.2d, v11.2d, v11.2d\n\t"

            "fmla v12.2d, v12.2d, v12.2d\n\t"
            "fmla v13.2d, v13.2d, v13.2d\n\t"
            "fmla v14.2d, v14.2d, v14.2d\n\t"
            "fmla v15.2d, v15.2d, v15.2d\n\t"

            "fmla v16.2d, v16.2d, v16.2d\n\t"
            "fmla v17.2d, v17.2d, v17.2d\n\t"
            "fmla v18.2d, v18.2d, v18.2d\n\t"
            "fmla v19.2d, v19.2d, v19.2d\n\t"

            "fmla v20.2d, v20.2d, v20.2d\n\t"
            "fmla v21.2d, v21.2d, v21.2d\n\t"
            "fmla v22.2d, v22.2d, v22.2d\n\t"
            "fmla v23.2d, v23.2d, v23.2d\n\t"

            "fmla v24.2d, v24.2d, v24.2d\n\t"
            "fmla v25.2d, v25.2d, v25.2d\n\t"
            "fmla v26.2d, v26.2d, v26.2d\n\t"
            "fmla v27.2d, v27.2d, v27.2d\n\t"

            "fmla v28.2d, v28.2d, v28.2d\n\t"
            "fmla v29.2d, v29.2d, v29.2d\n\t"
            "fmla v30.2d, v30.2d, v30.2d\n\t"
            "fmla v31.2d, v31.2d, v31.2d\n\t"
        );
    }   
}
extern float* cache_data;

void cpufp_kernel_neon_bandwith_l1cache(int64_t looptime)
{
    int data_size = 32 * 1024;
    float* data = (float*)malloc(data_size);
    memset(data, 1, data_size/sizeof(float));
    int inner_loop = data_size / sizeof(float) / (4 * 32);
    for (int i = 0; i < looptime; i++) {
        for (int j = 0; j <  inner_loop; j++) {     
            // printf("i=%d,j=%d\n",i,j);
            load_ldp_kernel(&data[32 * 4 * j]);
            // asm volatile(


            //     "ldp q0, q1,[x0], #32\n\t"
            //     "ldp q2, q3,[x0], #32\n\t"
            //     "ldp q4, q5,[x0], #32\n\t"
            //     "ldp q6, q7,[x0], #32\n\t"
            //     "ldp q8, q9,[x0], #32\n\t"

            //     "ldp q10, q11,[x0], #32\n\t"
            //     "ldp q12, q13,[x0], #32\n\t"
            //     "ldp q14, q15,[x0], #32\n\t"
            //     "ldp q16, q17,[x0], #32\n\t"
            //     "ldp q18, q19,[x0], #32\n\t"

            //     "ldp q20, q21,[x0], #32\n\t"
            //     "ldp q22, q23,[x0], #32\n\t"
            //     "ldp q24, q25,[x0], #32\n\t"
            //     "ldp q26, q27,[x0], #32\n\t"
            //     "ldp q28, q29,[x0], #32\n\t"

            //     "ldp q30, q31,[x0], #32\n\t"

            //     :
            //     :"r"(&data[32 * 4 * j])
            //     :
            // );
        }
    }
}

void cpufp_kernel_neon_bandwith_l2cache(int64_t looptime)
{
    int data_size = 2048 * 1024;
    int inner_loop = data_size / sizeof(float) / (4 * 32);
    for (int i = 0; i < looptime; i++) {
        for (int j = 0; j <  inner_loop; j++) {     
            // asm volatile(
            //     "ldp q0, q1,[x0], #32\n\t"
            //     "ldp q2, q3,[x0], #32\n\t"
            //     "ldp q4, q5,[x0], #32\n\t"
            //     "ldp q6, q7,[x0], #32\n\t"
            //     "ldp q8, q9,[x0], #32\n\t"

            //     "ldp q10, q11,[x0], #32\n\t"
            //     "ldp q12, q13,[x0], #32\n\t"
            //     "ldp q14, q15,[x0], #32\n\t"
            //     "ldp q16, q17,[x0], #32\n\t"
            //     "ldp q18, q19,[x0], #32\n\t"

            //     "ldp q20, q21,[x0], #32\n\t"
            //     "ldp q22, q23,[x0], #32\n\t"
            //     "ldp q24, q25,[x0], #32\n\t"
            //     "ldp q26, q27,[x0], #32\n\t"
            //     "ldp q28, q29,[x0], #32\n\t"

            //     "ldp q30, q31,[x0], #32\n\t"

            //     ::"r"(&cache_data[32 * 4 * j]):
            // );
            load_ldp_kernel(&cache_data[32 * 4 * j]);
            
        }
    }
}        