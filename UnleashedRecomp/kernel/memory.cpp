#include <stdafx.h>
#include "memory.h"
#if defined(__PROSPERO__)
#include <sys/mman.h>
#include <ps5platform/shm.h>
#endif

Memory::Memory()
{
#ifdef _WIN32
    base = (uint8_t*)VirtualAlloc((void*)0x100000000ull, PPC_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (base == nullptr)
        base = (uint8_t*)VirtualAlloc(nullptr, PPC_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (base == nullptr)
        return;

    DWORD oldProtect;
    VirtualProtect(base, 4096, PAGE_NOACCESS, &oldProtect);
#elif defined(__PROSPERO__)
    struct ps5_shm shm{};
    void* view = nullptr;
    if (ps5_shm_create(PPC_MEMORY_SIZE, &shm) == 0)
    {
        if (ps5_shm_map(&shm, 0, PPC_MEMORY_SIZE, (void*)0x1000000000ull,
                        PS5_SHM_READ | PS5_SHM_WRITE, 0, &view) == 0)
        {
            base = (uint8_t*)view;
        }
    }
    if (base == nullptr)
    {
        if (ps5_vrange_reserve(PPC_MEMORY_SIZE, (void*)0x1000000000ull, 0x10000, &view) == 0 && view != nullptr)
        {
            struct ps5_shm shm_user{}, shm_img{}, shm_phys{};
            void* subview = nullptr;
            if (ps5_shm_create(0x7FEF0000ull, &shm_user) == 0)
                ps5_shm_map(&shm_user, 0, 0x7FEF0000ull, (uint8_t*)view + 0x00010000ull, PS5_SHM_READ | PS5_SHM_WRITE, PS5_SHM_FIXED, &subview);
            if (ps5_shm_create(0x06000000ull, &shm_img) == 0)
                ps5_shm_map(&shm_img, 0, 0x06000000ull, (uint8_t*)view + 0x82000000ull, PS5_SHM_READ | PS5_SHM_WRITE, PS5_SHM_FIXED, &subview);
            if (ps5_shm_create(0x60000000ull, &shm_phys) == 0)
                ps5_shm_map(&shm_phys, 0, 0x60000000ull, (uint8_t*)view + 0xA0000000ull, PS5_SHM_READ | PS5_SHM_WRITE, PS5_SHM_FIXED, &subview);
            base = (uint8_t*)view;
        }
    }
    if (base == nullptr)
        return;

    mprotect(base, 0x4000, PROT_NONE);
#else
    base = (uint8_t*)mmap((void*)0x100000000ull, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (base == (uint8_t*)MAP_FAILED)
        base = (uint8_t*)mmap(NULL, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (base == nullptr)
        return;

    mprotect(base, 4096, PROT_NONE);
#endif

    for (size_t i = 0; PPCFuncMappings[i].guest != 0; i++)
    {
        if (PPCFuncMappings[i].host != nullptr)
            InsertFunction(PPCFuncMappings[i].guest, PPCFuncMappings[i].host);
    }
}

void* MmGetHostAddress(uint32_t ptr)
{
    return g_memory.Translate(ptr);
}
