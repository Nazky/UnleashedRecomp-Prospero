#include <os/version.h>

os::version::OSVersion os::version::GetOSVersion()
{
#if defined(__PROSPERO__)
    return os::version::OSVersion{ 9, 0, 0 };
#else
    assert(false && "Unimplemented.");
    return os::version::OSVersion();
#endif
}
