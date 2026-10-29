#include "amx.hpp"

#include <csetjmp>
#include <csignal>
#include <cstddef>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace {

sigjmp_buf amx_jump;

void on_illegal_instruction(int)
{
    siglongjmp(amx_jump, 1);
}

// Execute AMX_SET/AMX_CLR once with SIGILL (and SIGBUS, which some kernels
// raise for a disabled coprocessor) caught: the only reliable test for an
// undocumented unit.  Decided once per process.
bool probe_amx()
{
    struct sigaction guard, old_ill, old_bus;
    guard.sa_handler = on_illegal_instruction;
    sigemptyset(&guard.sa_mask);
    guard.sa_flags = 0;
    sigaction(SIGILL, &guard, &old_ill);
    sigaction(SIGBUS, &guard, &old_bus);
    bool available = false;
    if (sigsetjmp(amx_jump, 1) == 0) {
        amx_probe();
        available = true;
    }
    sigaction(SIGILL, &old_ill, nullptr);
    sigaction(SIGBUS, &old_bus, nullptr);
    return available;
}

} // namespace

bool amx_available()
{
    static const bool available = probe_amx();
    return available;
}

int amx_version()
{
#ifdef __APPLE__
    int version = 0;
    size_t size = sizeof(version);
    if (sysctlbyname("hw.optional.amx_version", &version, &size, nullptr, 0) ==
        0)
        return version;
#endif
    return 0;
}
