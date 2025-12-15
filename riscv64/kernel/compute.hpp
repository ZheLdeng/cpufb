#ifndef _COMPUTE_HPP
#define _COMPUTE_HPP

extern "C"
{
#ifdef _IME_
    void ime_vmadot_s32s8s8(int64_t);
    void ime_vmadotu_u32u8u8(int64_t);
    void ime_vmadotus_s32u8s8(int64_t);
    void ime_vmadotsu_s32s8u8(int64_t);
    void ime_vmadotslide_s32s8s8(int64_t);
#endif

#ifdef _VECTOR_
    void vector_vfmacc_vf_f16f16f16(int64_t);
    void vector_vfmacc_vv_f16f16f16(int64_t);
    void vector_vfmacc_vf_f32f32f32(int64_t);
    void vector_vfmacc_vv_f32f32f32(int64_t);
    void vector_vfmacc_vf_f64f64f64(int64_t);
    void vector_vfmacc_vv_f64f64f64(int64_t);
#endif
}
#endif