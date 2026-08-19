#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>

namespace MemoryPatcher {
    inline void Unprotect(uintptr_t address, size_t size) {
        DWORD oldProtect;
        VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect);
    }

    inline void WriteBytes(uintptr_t address, const std::vector<uint8_t>& bytes) {
        Unprotect(address, bytes.size());
        memcpy(reinterpret_cast<void*>(address), bytes.data(), bytes.size());
    }

    inline void Nop(uintptr_t address, size_t size) {
        Unprotect(address, size);
        memset(reinterpret_cast<void*>(address), 0x90, size);
    }

    template <typename T>
    inline void Write(uintptr_t address, T value) {
        Unprotect(address, sizeof(T));
        *reinterpret_cast<T*>(address) = value;
    }
}