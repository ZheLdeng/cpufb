# cpufb arm64 测试扩展建议

针对 HPC 与 AI 两类负载，对当前 arm64 benchmark 覆盖范围的盘点与扩展建议。

---

## 1. 当前已有覆盖

| 扩展 | 已测指令 |
|---|---|
| ASIMD (NEON) | `fmla` fp32/fp64 (vs/vv + latency)，`hybrid_fp32_mla_6x16` |
| ASIMD_HP | `fmla` fp16 (vs/vv) |
| ASIMD_DP | `sdot/udot` (vs/vv) s8/u8 |
| BF16 | `bfmmla`、`bfdot`、`bfmlalb/t` |
| I8MM | `smmla/ummla/usmmla`、`sudot/usdot` |
| SVE | `fmla` f32/f64 (vs/vv + latency) |
| SVE_I8MM | `smmla/ummla/usmmla/sdot/udot/usdot` |
| SVE_BF16 | `bfmmla`、`bfdot` (vs/vv) |
| SME | `bfmopa/fmopa f32f32/f32f16/smopa i32i8` + LD1W/LDR |
| SME2 | `bfmlal/bfdot/fmla/fmlal/fdot/fvdot`（vs/vv/mvv，单/×4） |
| SMEf64 | `fmopa f64`、`fmla f64` |

> ⚠️ `cpuid.cpp` 中 `test_sve2` 已经探测了 SVE2，但**没有注册任何 kernel**，能力被浪费。

---

## 2. 强烈建议补充

### 🥇 第一梯队：缺失的核心扩展（影响峰值 FLOPS/OPS 数据完整性）

| 优先级 | ISA 扩展 | 指令 | 数据类型 | 为什么 |
|---|---|---|---|---|
| ★★★★★ | **FHM (FP16FML)** | `fmlal/fmlal2 v.4s, v.4h, v.4h` | fp32 ← fp16 × fp16 | ARMv8.2 起非常普遍（A75+/M1+），AI 推理常用混合精度，**比纯 fp16 fmla 实际更常见** |
| ★★★★★ | **SVE_F32MM** | `fmmla z.s, z.s, z.s` | fp32 4×4 外积 | HPC 浮点峰值的关键，单条指令 32 FLOPs |
| ★★★★★ | **SVE_F64MM** | `fmmla z.d, z.d, z.d` | fp64 2×2 外积 | HPC 双精度峰值（科学计算 / LINPACK 类） |
| ★★★★ | **SVE FP16 FMLA** | `fmla z.h, p/m, z.h, z.h`、`fmla z.h, z.h, z.h[i]` | fp16 | AI 推理 + SVE 长向量，目前 SVE 只测了 f32/f64 |
| ★★★★ | **SVE2** | `sqrdmlah/sqdmulh/uaba` 等 | int8/16/32 | cpuid 已探测但**没有任何 kernel**，浪费了能力 |
| ★★★★ | **SME F16F16** | `fmopa za.h, p, p, z.h, z.h` | fp16 ← fp16 × fp16（同精度累加） | Armv9.2 SME-F16F16 扩展，AI 训练 / 推理外积峰值 |
| ★★★★ | **SME I16I32** | `smopa/umopa za.s, p, p, z.h, z.h` | i32 ← i16 × i16 | INT16 量化（语音、某些视觉模型）的峰值 |
| ★★★ | **SME UMOPA / USMOPA / SUMOPA** | i8 无符号 / 混合签名外积 | u32←u8×u8、s32←u8×s8 等 | 现在只有 `smopa i32i8i8`，量化模型常用混合签名 |

### 🥈 第二梯队：HPC 关键补强

| ISA | 指令 | 数据类型 | 用途 |
|---|---|---|---|
| ASIMD | `fcmla v.4s, v.4s, v.4s, #0/#90/#180/#270` | 复数 fp32/fp16 | FFT、雷达、通信、QM 模拟 |
| SVE | `fcmla z.s, p/m, z.s, z.s` | 复数 fp32/fp64 | 同上，向量长度无关 |
| ASIMD | `fmls`、`fneg + fmla` 路径 | fp32/fp64 | 减法 FMA，部分实现走不同 issue 端口 |
| ASIMD | `fadd/fmul`（无 FMA） | fp32/fp64 | 区分有 / 无 FMA 的峰值，老核心可能 add+mul 双发 |
| ASIMD | `mla v.4s, v.4s, v.s[i]` 整数 indexed | i32/i16 整数 MAC | 定点 HPC、信号处理 |
| ASIMD | `sqdmlal/sqdmlal2` 饱和 MAC | i32←i16×i16 | DSP / 通信 |
| SVE | `fadda`（顺序归约）、`faddv`（任意顺序归约） | fp32/fp64 | 严格顺序归约 vs 快速归约的延迟差异，HPC 求和精度敏感 |
| ASIMD | `faddv/fmaxv/saddlv` | fp32/fp16/i8 | 归约带宽，AI/HPC 都用 |
| ASIMD | `frecpe + frecps`、`frsqrte + frsqrts` | fp32/fp16 | 倒数 / 平方根倒数迭代，渲染 / 物理仿真 |
| ASIMD | `tbl/tbx` 4-table | u8 | 量化 LUT、shuffle 密集 kernel（GEMM 重排） |

### 🥉 第三梯队：访存与内存子系统

> 当前只有 L1/L2，没有 L3/DRAM，对 HPC/AI 真实瓶颈刻画不足。

| 类别 | 建议补 | 用途 |
|---|---|---|
| **L3/DRAM 带宽** | 扩大 buffer 到 L3 大小 / 远超 LLC，给 `cpubm_arm_load` 加第 3、4 档 | HPC/AI 的瓶颈往往在 L3 和 DRAM |
| **流式访问 (NT)** | `stnp/ldnp` (NEON non-temporal pair)，SVE `ldnt1w/stnt1w` | 大数据流式拷贝峰值（GEMM packing、AI 张量搬运） |
| **Gather/Scatter** | SVE `ld1w {z}, p/z, [x, z.s, uxtw]` / `st1w` 散列 | 稀疏矩阵、Embedding 查表、MoE，AI 极重要 |
| **Strided LD/ST** | `ld2/ld3/ld4 {v.4s}, [x]` | AoS↔SoA、复数实虚分离、HPC interleaved 数据 |
| **Replicate / broadcast load** | NEON `ld1r`、SVE `ld1rw/ld1rqw` | GEMM 中 broadcast B 列时常用 |
| **First-faulting / non-faulting** | SVE `ldff1w/ldnf1w` | 尾循环消除，HPC kernel 标准技巧 |
| **Atomics (LSE)** | `ldadd/ldset/cas` | 多线程归约 / 原子计数器吞吐（HPC reduce） |
| **预取** | `prfm pldl1keep/pldl2strm`、SVE `prfw` | 测预取吞吐与遮蔽效果 |
| **Store 带宽** | 现在只测 load，加 store-only 和 1:1 mix | 写回带宽常常是 GEMM 瓶颈 |
| **Copy 带宽** | memcpy 风格 ldp+stp 循环 | 张量搬运、weight 加载实测 |
| **跨 NUMA / 跨 cluster** | 绑定不同 socket/cluster 测 L3↔L3、DRAM 远端 | 现代 ARM 服务器（Grace、Ampere、Graviton）必测 |

### 🏅 第四梯队：补丁性的小项

1. **SVE indexed 全套**：现在 `sve_fmla.vs` 用了 `z.s[0]`，但缺 `vs(f64)` indexed、缺 `sve_fmla.vv f16` 整组。
2. **Latency 指标补全**：`bf16 / i8mm / asimd_dp / sve_i8mm / sve_bf16` 都没有 `_latency` 版本，无法看出依赖链延迟。SME 部分有，NEON DP/I8MM 没有，对调度分析价值很大。
3. **NEON dotprod indexed-by-element 全套**：`udot v.4s, v.16b, v.4b[idx]` 已有，但缺 `sdot indexed`（你只有 `vs_s32s8s8` 标量广播）。
4. **`hybrid_*` 风格的真实 GEMM micro-kernel 多精度版**：`hybrid_bf16_mmla_8x12`、`hybrid_int8_mmla_8x12`、`hybrid_fp16_mla_8x24` —— 比单指令循环更接近 ACL/oneDNN 实际峰值。
5. **频率台的 `IPC(SME)` 列**：`init_table` 第 354–368 行，SVE 列出来了，SME 没列。
6. **多核 scaling 指标**：现在 thread_pool 把所有线程跑同一个 kernel，但没报告"多核 vs 单核"的扩展比。HPC 强相关。

---

## 3. 推荐落地顺序（最高 ROI 的 6 个）

| 序号 | 项目 | 主要受益 | 估算工作量 |
|---|---|---|---|
| 1 | **FHM (FMLAL/FMLAL2)** | AI 混合精度峰值 | 1 个新 `.S`、cpuid 探测，~80 行 |
| 2 | **SVE_F32MM / SVE_F64MM** | HPC 浮点 / 双精度峰值 | 共用 1 个 `.S`，~120 行 |
| 3 | **SVE FP16 FMLA**（vs/vv/latency） | AI 推理 + 长向量 | clone `_SVE_.S` 改 `.s→.h`，~100 行 |
| 4 | **SME F16F16 + SME I16I32 + UMOPA/USMOPA** | AI 量化 + fp16 训练峰值 | 扩展 `_SME_.S` |
| 5 | **SVE Gather + LD1R + LDNT1** | AI 稀疏 / Embedding，HPC 散列 | 扩 `_SVE_LD1W_.S` 与 load.cpp |
| 6 | **L3/DRAM 带宽档 + Store/Copy 带宽** | 内存子系统真实瓶颈 | `cpubm_arm_load` 多加几行注册 |

---

## 4. 文件改动概览（参考）

实施时大致需要触及：

- `arm64/cpuid.cpp`：增加 FHM、SVE2、SME-F16F16、SME-I16I32 等的探测分支
- `arm64/asm/_FHM_.S`（新）、`_SVE_F32MM_.S`（新）、`_SVE_F16_.S`（新）等
- `arm64/asm/_SVE_.S`、`_SME_.S`、`_SME2_.S`：补 indexed / latency / UMOPA 等
- `arm64/cpufb.cpp`：`cpufb_register_isa()` 中 `reg_new_isa(...)` 注册新条目；`init_table` 给 SME 频率列
- `arm64/kernel/load.cpp`：扩 L3/DRAM size buckets、store-only / copy / gather kernel
- `build_arm64.sh`：为新增 SIMD 宏添加 `-march` 分支（如 `+fp16fml`、`+sve+f32mm`、`+sve+f64mm`、`+sme-f16f16`、`+sme-i16i64` 等）
