#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

// 测试函数指针类型
typedef void (*test_func_t)(void);

// 各个指令集的测试函数
#ifdef __aarch64__
void test_asimd(void) {
    // FMOV D0, #1.0
    __asm__ volatile(".inst 0x1e601000" ::: "v0", "memory");
}

void test_asimd_hp(void) {
    // FCVT H0, S0 - 半精度浮点转换
    __asm__ volatile(".inst 0x1e23c000" ::: "v0", "memory");
}

void test_asimd_dp(void) {
    // UDOT v0.2s, v1.8b, v2.8b - DotProd
    __asm__ volatile(".inst 0x2e829420" ::: "v0", "memory");
}
// void test_asimd(void) {
//     __asm__ volatile(
//         ".inst 0x1C61C608\n"  // FMOV D0, #1.0
//         ::: "v0"
//     );
// }

// void test_asimd_hp(void) {
//     __asm__ volatile(
//         ".inst 0x6E01E408\n"  // FCVT H0, S0
//         ::: "v0"
//     );
// }

// void test_asimd_dp(void) {
//     __asm__ volatile(
//         ".inst 0x4E81A408\n"  // UDOT v0.2s, v1.8b, v2.8b
//         ::: "v0"
//     );
// }

void test_i8mm(void) {
    __asm__ volatile(
        ".inst 0x4e82a420\n"  // SMMLA v0.4s, v1.16b, v2.16b
        ::: "v0"
    );
}

void test_bf16(void) {
    __asm__ volatile(
        ".inst 0x2e42fc20\n"  // BFDOT v0.2s, v1.4h, v2.4h
        ::: "v0"
    );
}

void test_sve(void) {
    __asm__ volatile(
        ".inst 0x2518e3e0\n"  // PTRUE p0.b
        :::
    );
}

void test_sve2(void) {
    __asm__ volatile(
        ".inst 0x45004000\n"  // SADDLB z0.h, z0.b, z0.b
        :::
    );
}

void test_sme(void) {
    __asm__ volatile(
        ".inst 0xd503467f\n"  // SMSTART
        ".inst 0xd503477f\n"  // SMSTOP
        :::
    );
}

void test_sme2(void) {
    __asm__ volatile(
        ".inst 0xd503477f\n"
        ".inst 0xC1328240\n"  // BFMLAL
        ".inst 0xd503467f\n"
        :::
    );
}

void test_sme_f64(void) {
    __asm__ volatile(
        ".inst 0xd503477f\n"
        ".inst 0x80C82100\n"
        ".inst 0xd503467f\n"
        :::
    );
}

#else
// ARM32位或其他架构
void test_asimd(void) {
    __asm__ volatile(".inst 0xeeb70b00" :::);
}

void test_asimd_hp(void) {
    __asm__ volatile(".inst 0xeeb20ac0" :::);
}

void test_asimd_dp(void) {
    __asm__ volatile(".inst 0xfe000d10" :::);
}

void test_i8mm(void) {
    __asm__ volatile(".inst 0xfc200c40" :::);
}

void test_bf16(void) {
    __asm__ volatile(".inst 0xfe000d00" :::);
}

void test_sve(void) { }
void test_sve2(void) { }
void test_sme(void) { }
void test_sme2(void) { }
void test_sme_f64(void) { }

#endif

// 信号处理函数
static volatile const char* current_test_name = NULL;

void sigill_handler(int sig) {
    (void)sig;
    // 子进程中捕获到 SIGILL，静默退出，状态码为1
    _exit(1);
}

// 在子进程中执行测试
bool test_instruction_in_child(test_func_t test_func, const char* name) {
    pid_t pid = fork();
    
    if (pid < 0) {
        // fork 失败
        fprintf(stderr, "fork failed for %s\n", name);
        return false;
    }
    
    if (pid == 0) {
        // 子进程
        current_test_name = name;
        
        // 设置信号处理
        signal(SIGILL, sigill_handler);
        signal(SIGSEGV, sigill_handler);
        signal(SIGBUS, sigill_handler);
        
        // 执行测试指令
        test_func();
        
        // 如果执行到这里，说明指令支持，正常退出
        _exit(0);
    } else {
        // 父进程
        int status = 0;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            // 正常退出
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 0) {
                // 指令支持
                printf("_%s_\n", name);
                fflush(stdout);
                return true;
            } else {
                // 指令不支持（捕获到信号后退出）
                return false;
            }
        } else if (WIFSIGNALED(status)) {
            // 被信号终止（未捕获的信号）
            return false;
        }
    }
    
    return false;
}

int get_cpuid(void) {
    fflush(stdout);
    
    // 按依赖顺序测试
    test_instruction_in_child(test_asimd, "ASIMD");
    test_instruction_in_child(test_asimd_hp, "ASIMD_HP");
    test_instruction_in_child(test_asimd_dp, "ASIMD_DP");
    test_instruction_in_child(test_i8mm, "I8MM");
    test_instruction_in_child(test_bf16, "BF16");
    
    // SVE 和 SVE2
    bool sve_supported = test_instruction_in_child(test_sve, "SVE");
    if (sve_supported) {
        test_instruction_in_child(test_sve2, "SVE2");
    }
    
    // SME 系列
    bool sme_supported = test_instruction_in_child(test_sme, "SME");
    if (sme_supported) {
        bool sme2_supported = test_instruction_in_child(test_sme2, "SME2");
        if (sme2_supported) {
            test_instruction_in_child(test_sme_f64, "SMEf64");
        }
    }
    
    // 这些总是支持的（ARMv8基础特性）
    printf("_LDP_\n");
    printf("_ISSUE_\n");
    fflush(stdout);
    
    return 0;
}

int main(void) {
    get_cpuid();
    return 0;
}