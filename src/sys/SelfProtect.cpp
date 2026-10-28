#include "SelfProtect.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sddl.h>

namespace bcg::selfprotect {

namespace {

bool apply(const wchar_t* sddl)
{
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, nullptr))
        return false;
    const BOOL ok = SetKernelObjectSecurity(GetCurrentProcess(), DACL_SECURITY_INFORMATION, sd);
    LocalFree(sd);
    return ok != FALSE;
}

} // namespace

// Rights denied to everyone: TERMINATE 0x1, CREATE_THREAD 0x2, VM_OPERATION 0x8,
// VM_WRITE 0x20, SUSPEND_RESUME 0x800. Everything else stays allowed, so Task
// Manager can still list the process and this program keeps working.
bool enable()
{
    return apply(L"D:(D;;0x082B;;;WD)(A;;0x1FFFFF;;;WD)");
}

bool disable()
{
    return apply(L"D:(A;;0x1FFFFF;;;WD)");
}

} // namespace bcg::selfprotect
