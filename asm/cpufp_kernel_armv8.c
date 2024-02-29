#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include<time.h>
#include<arm_neon.h>
static double get_time(struct timespec *start,
	struct timespec *end)
{
	return end->tv_sec - start->tv_sec +
		(end->tv_nsec - start->tv_nsec) * 1e-9;
}
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
    struct timespec start, end;
    double time_used;
int data_size = 32 * 1024;
    // looptime = 1;
    float* data = (float*)malloc(data_size);
    // float* tail = (float*)malloc(4 * sizeof(float));
    // memset(data, 1, data_size/sizeof(float));
    for(int i=0;i<data_size/sizeof(float);i++){
        data[i]=i;
    }
    int inner_loop = data_size / sizeof(float) / (4 * 32);
    
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    load_ldp_kernel(data, inner_loop, looptime);
    // free(data);
    // data = (float*)malloc(data_size);
    // for(int i=0;i<data_size/sizeof(float);i++){
    //     data[i]=i;
    // }
    // load_ldp_kernel0(data, inner_loop, looptime);
    // float* tmp = &data[0];
    // for (int i = 0; i < 1; i++) {
    //     // *data=*tmp;
    //     // printf("%s\n", data);
    //     //  asm volatile(
    //     //     "mov x4, %1\n\t"
    //     //     "mov x3, %0\n\t"
    //     //     "1:               \n\t"    
    //     //     "ldr q0, [x3], #16\n\t"
    //     //     "ldr q1, [x3], #16\n\t"
    //     //     "ldr q2, [x3], #16\n\t"
    //     //     "ldr q3, [x3], #16\n\t"
    //     //     "ldr q4, [x3], #16\n\t"
    //     //     "ldr q5, [x3], #16\n\t"
    //     //     "ldr q6, [x3], #16\n\t"
    //     //     "ldr q7, [x3], #16\n\t"

    //     //     "ldr q8, [x3], #16\n\t"
    //     //     "ldr q9, [x3], #16\n\t"
    //     //     "ldr q10, [x3], #16\n\t"
    //     //     "ldr q11, [x3], #16\n\t"
    //     //     "ldr q12, [x3], #16\n\t"
    //     //     "ldr q13, [x3], #16\n\t"
    //     //     "ldr q14, [x3], #16\n\t"
    //     //     "ldr q15, [x3], #16\n\t"

    //     //     "ldr q16, [x3], #16\n\t"
    //     //     "ldr q17, [x3], #16\n\t"
    //     //     "ldr q18, [x3], #16\n\t"
    //     //     "ldr q19, [x3], #16\n\t"
    //     //     "ldr q20, [x3], #16\n\t"
    //     //     "ldr q21, [x3], #16\n\t"
    //     //     "ldr q22, [x3], #16\n\t"
    //     //     "ldr q23, [x3], #16\n\t"

    //     //     "ldr q24, [x3], #16\n\t"
    //     //     "ldr q25, [x3], #16\n\t"
    //     //     "ldr q26, [x3], #16\n\t"
    //     //     "ldr q27, [x3], #16\n\t"
    //     //     "ldr q28, [x3], #16\n\t"
    //     //     "ldr q29, [x3], #16\n\t"
    //     //     "ldr q30, [x3], #16\n\t"
    //     //     "ldr q31, [x3], #16\n\t"
            
    //     //     "subs x4, x4, #1\n\t"
    //     //     "bne 1b         \n\t"
    //     //     :
    //     //     :"r"(data),"r"(inner_loop)
    //     //     :
    //     // );
    //     // printf("%s\n", data);
    //     // data=tmp;
    //     for (int j = 0; j <  inner_loop; j++) {     //400
    //         // printf("i=%d,j=%d\n",i,j);
    //         // load_ldp_kernel(&data[32 * 4 * j]);
    //         asm volatile(
    //             "ld1 {v0.4s, v1.4s},[%1]  , #32\n\t"
    //             "ld1 {v2.4s, v3.4s},[%1]  , #32\n\t"
    //             "ld1 {v4.4s, v5.4s},[%1]  , #32\n\t"
    //             "ld1 {v6.4s, v7.4s},[%1]  , #32\n\t"
    //             "ld1 {v8.4s, v9.4s},[%1]  , #32\n\t"
    //             "ld1 {v10.4s, v11.4s},[%1], #32\n\t"
    //             "ld1 {v12.4s, v13.4s},[%1], #32\n\t"
    //             "ld1 {v14.4s, v15.4s},[%1], #32\n\t"
    //             "ld1 {v16.4s, v17.4s},[%1], #32\n\t"
    //             "ld1 {v18.4s, v19.4s},[%1], #32\n\t"
    //             "ld1 {v20.4s, v21.4s},[%1], #32\n\t"
    //             "ld1 {v22.4s, v23.4s},[%1], #32\n\t"
    //             "ld1 {v24.4s, v25.4s},[%1], #32\n\t"
    //             "ld1 {v26.4s, v27.4s},[%1], #32\n\t"
    //             "ld1 {v28.4s, v29.4s},[%1], #32\n\t"
    //             "ld1 {v30.4s, v31.4s},[%1], #32\n\t"

    //             // "ldp q0, q1,[%0], #32\n\t"
    //             // "ldp q2, q3,[%0], #32\n\t"
    //             // "ldp q4, q5,[%0], #32\n\t"
    //             // "ldp q6, q7,[%0], #32\n\t"
    //             // "ldp q8, q9,[%0], #32\n\t"

    //             // "ldp q10, q11,[%0], #32\n\t"
    //             // "ldp q12, q13,[%0], #32\n\t"
    //             // "ldp q14, q15,[%0], #32\n\t"
    //             // "ldp q16, q17,[%0], #32\n\t"
    //             // "ldp q18, q19,[%0], #32\n\t"

    //             // "ldp q20, q21,[%0], #32\n\t"
    //             // "ldp q22, q23,[%0], #32\n\t"
    //             // "ldp q24, q25,[%0], #32\n\t"
    //             // "ldp q26, q27,[%0], #32\n\t"
    //             // "ldp q28, q29,[%0], #32\n\t"

    //             // "ldp q30, q31,[%0], #32\n\t"

                
    //             // "ldp q0, q1, [%0, #0]\n\t"
    //             // "ldp q2, q3, [%0, #32]\n\t"
    //             // "ldp q4, q5, [%0, #64]\n\t"
    //             // "ldp q6, q7, [%0, #96]\n\t"
    //             // "ldp q8, q9, [%0, #128]\n\t"

    //             // "ldp q10, q11, [%0, #160]\n\t"
    //             // "ldp q12, q13, [%0, #192]\n\t"
    //             // "ldp q14, q15, [%0, #224]\n\t"
    //             // "ldp q16, q17, [%0, #256]\n\t"
    //             // "ldp q18, q19, [%0, #288]\n\t"

    //             // "ldp q20, q21, [%0, #320]\n\t"
    //             // "ldp q22, q23, [%0, #352]\n\t"
    //             // "ldp q24, q25, [%0, #384]\n\t"
    //             // "ldp q26, q27, [%0, #416]\n\t"
    //             // "ldp q28, q29, [%0, #448]\n\t"

    //             // "ldp q30, q31, [%0, #480]\n\t"

    //             // "ld1 {v0.4s},[%0]\n\t"
    //             // "ld1 {v1.4s},[%0]\n\t"
    //             // "ld1 {v2.4s},[%0]\n\t"
    //             // "ld1 {v3.4s},[%0]\n\t"
    //             // "ld1 {v4.4s},[%0]\n\t"
    //             // "ld1 {v5.4s},[%0]\n\t"
    //             // "ld1 {v6.4s},[%0]\n\t"
    //             // "ld1 {v7.4s},[%0]\n\t"

    //             // "ld1 {v8.4s},[%0]\n\t"
    //             // "ld1 {v9.4s},[%0]\n\t"
    //             // "ld1 {v10.4s},[%0]\n\t"
    //             // "ld1 {v11.4s},[%0]\n\t"
    //             // "ld1 {v12.4s},[%0]\n\t"
    //             // "ld1 {v13.4s},[%0]\n\t"
    //             // "ld1 {v14.4s},[%0]\n\t"
    //             // "ld1 {v15.4s},[%0]\n\t"

    //             // "ld1 {v16.4s},[%0]\n\t"
    //             // "ld1 {v17.4s},[%0]\n\t"
    //             // "ld1 {v18.4s},[%0]\n\t"
    //             // "ld1 {v19.4s},[%0]\n\t"
    //             // "ld1 {v20.4s},[%0]\n\t"
    //             // "ld1 {v21.4s},[%0]\n\t"
    //             // "ld1 {v22.4s},[%0]\n\t"
    //             // "ld1 {v23.4s},[%0]\n\t"

    //             // "ld1 {v24.4s},[%0]\n\t"
    //             // "ld1 {v25.4s},[%0]\n\t"
    //             // "ld1 {v26.4s},[%0]\n\t"
    //             // "ld1 {v27.4s},[%0]\n\t"
    //             // "ld1 {v28.4s},[%0]\n\t"
    //             // "ld1 {v29.4s},[%0]\n\t"
    //             // "ld1 {v30.4s},[%0]\n\t"
    //             // "ld1 {v31.4s},[%0]\n\t"



    //             // "ld1 {v0.4s}, [%0], #16\n\t"
    //             // "ld1 {v1.4s}, [%0], #16\n\t"
    //             // "ld1 {v2.4s}, [%0], #16\n\t"
    //             // "ld1 {v3.4s}, [%0], #16\n\t"
    //             // "ld1 {v4.4s}, [%0], #16\n\t"
    //             // "ld1 {v5.4s}, [%0], #16\n\t"
    //             // "ld1 {v6.4s}, [%0], #16\n\t"
    //             // "ld1 {v7.4s}, [%0], #16\n\t"
    //             // "ld1 {v8.4s}, [%0], #16\n\t"
    //             // "ld1 {v9.4s}, [%0], #16\n\t"

    //             // "ld1 {v10.4s}, [%0], #16\n\t"
    //             // "ld1 {v11.4s}, [%0], #16\n\t"
    //             // "ld1 {v12.4s}, [%0], #16\n\t"
    //             // "ld1 {v13.4s}, [%0], #16\n\t"
    //             // "ld1 {v14.4s}, [%0], #16\n\t"
    //             // "ld1 {v15.4s}, [%0], #16\n\t"
    //             // "ld1 {v16.4s}, [%0], #16\n\t"
    //             // "ld1 {v17.4s}, [%0], #16\n\t"
    //             // "ld1 {v18.4s}, [%0], #16\n\t"
    //             // "ld1 {v19.4s}, [%0], #16\n\t"

    //             // "ld1 {v20.4s}, [%0], #16\n\t"
    //             // "ld1 {v21.4s}, [%0], #16\n\t"
    //             // "ld1 {v22.4s}, [%0], #16\n\t"
    //             // "ld1 {v23.4s}, [%0], #16\n\t"
    //             // "ld1 {v24.4s}, [%0], #16\n\t"
    //             // "ld1 {v25.4s}, [%0], #16\n\t"
    //             // "ld1 {v26.4s}, [%0], #16\n\t"
    //             // "ld1 {v27.4s}, [%0], #16\n\t"
    //             // "ld1 {v28.4s}, [%0], #16\n\t"
    //             // "ld1 {v29.4s}, [%0], #16\n\t"
                
    //             // "ld1 {v30.4s}, [%0], #16\n\t"
    //             // "ld1 {v31.4s}, [%0], #16\n\t"



    //             "st1 {v1.4s}, [%0]      \n\t" 
    //             :"+r"(tail)
    //             :"r"(&data[j*32*4])
    //             // :
    //             // :
    //             // :"r"(&data[j*32*4])
    //             // :
    //         );
    //         for (int k = 0; k < 4; k++){
    //             printf("%f ", tail[k]);
    //         }
    //         printf("\n");
            
    //     }
    // }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    
    double perf = looptime * data_size /
        (time_used * 2.6 * 1e9);
    double perf_1 = looptime * data_size /
        (time_used * 1e9);
    printf("time = %.10f, perf = %.2f byte/cycle \n", time_used, perf);
    // printf("aaaaaaaa\n");
    free(data);
    // exit(0);
}

void cpufp_kernel_neon_bandwith_l2cache(int64_t looptime)
{
    int data_size = 2048 * 1024;
    int inner_loop = data_size / sizeof(float) / (4 * 32);
    // float* data = (float*)malloc(data_size);
    // memset(data, 1, data_size/sizeof(float));
    for (int i = 0; i < looptime; i++) {
        for (int j = 0; j <  inner_loop; j++) {     
            asm volatile(

                // "ldp q0, q1,[x0], #32\n\t"
                // "ldp q2, q3,[x0], #32\n\t"
                // "ldp q4, q5,[x0], #32\n\t"
                // "ldp q6, q7,[x0], #32\n\t"
                // "ldp q8, q9,[x0], #32\n\t"

                // "ldp q10, q11,[x0], #32\n\t"
                // "ldp q12, q13,[x0], #32\n\t"
                // "ldp q14, q15,[x0], #32\n\t"
                // "ldp q16, q17,[x0], #32\n\t"
                // "ldp q18, q19,[x0], #32\n\t"

                // "ldp q20, q21,[x0], #32\n\t"
                // "ldp q22, q23,[x0], #32\n\t"
                // "ldp q24, q25,[x0], #32\n\t"
                // "ldp q26, q27,[x0], #32\n\t"
                // "ldp q28, q29,[x0], #32\n\t"

                // "ldp q30, q31,[x0], #32\n\t"

                

                "ldp q0, q1, [x0, #0]\n\t"
                "ldp q2, q3, [x0, #32]\n\t"
                "ldp q4, q5, [x0, #64]\n\t"
                "ldp q6, q7, [x0, #96]\n\t"
                "ldp q8, q9, [x0, #128]\n\t"

                "ldp q10, q11, [x0, #160]\n\t"
                "ldp q12, q13, [x0, #192]\n\t"
                "ldp q14, q15, [x0, #224]\n\t"
                "ldp q16, q17, [x0, #256]\n\t"
                "ldp q18, q19, [x0, #288]\n\t"

                "ldp q20, q21, [x0, #320]\n\t"
                "ldp q22, q23, [x0, #352]\n\t"
                "ldp q24, q25, [x0, #384]\n\t"
                "ldp q26, q27, [x0, #416]\n\t"
                "ldp q28, q29, [x0, #448]\n\t"

                "ldp q30, q31, [x0, #480]\n\t"

                :
                :"r"(&cache_data[32 * 4 * j])
                :
            );
            
        }
    }
    //  free(data);
}        