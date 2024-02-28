void cpufp_kernel_armv8_fmla_f32(int looptime)
{
    for (int i = 0; i < looptime; i++) {
        __asm____volatile__(
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