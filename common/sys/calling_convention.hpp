#ifndef CPUFB_CALLING_CONVENTION_HPP
#define CPUFB_CALLING_CONVENTION_HPP

// The x64 assembly kernels use the System V register convention even when
// called from a Windows executable, whose C++ code uses the Microsoft ABI.
#if defined(_WIN32) && defined(__GNUC__) && defined(__x86_64__)
#define CPUFB_SYSV_ABI __attribute__((sysv_abi))
#else
#define CPUFB_SYSV_ABI
#endif

#endif // CPUFB_CALLING_CONVENTION_HPP
