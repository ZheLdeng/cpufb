#pragma once

#include <string>
#include <vector>

struct Arm64RuntimeFeatures {
    bool asimd = false;
    bool fp16 = false;
    bool dotprod = false;
    bool fcma = false;
    bool fhm = false;
    bool sve = false;
    bool i8mm = false;
    bool bf16 = false;
    bool sve2 = false;
    bool sve_i8mm = false;
    bool sve_bf16 = false;
    bool sve_f32mm = false;
    bool sve_f64mm = false;
    bool sme = false;
    bool sme2 = false;
    bool sme_i8i32 = false;
    bool sme_f16f32 = false;
    bool sme_b16f32 = false;
    bool sme_f32f32 = false;
    bool sme_f64f64 = false;
    bool sme_i16i32 = false;
    bool sme_f16f16 = false;

    bool supports(const std::string &compiled_token) const;
    std::vector<std::string> runnable_tokens() const;
};

const Arm64RuntimeFeatures &arm64_runtime_features();
