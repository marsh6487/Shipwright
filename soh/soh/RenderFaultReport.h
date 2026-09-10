#pragma once

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace RenderFaultReport {
using ReadMemory = bool (*)(uintptr_t address, void* output, size_t size);

// The crash callback must not overrun its caller's fixed buffer or dereference
// the bad command pointer while trying to describe the original exception.
template <typename... Args>
inline void Append(char* buffer, size_t capacity, size_t* used, const char* format, Args... args) {
    if (buffer == nullptr || used == nullptr || *used >= capacity) {
        return;
    }
    const size_t remaining = capacity - *used;
    const int written = std::snprintf(buffer + *used, remaining, format, args...);
    if (written > 0) {
        *used += static_cast<size_t>(written) < remaining ? static_cast<size_t>(written) : remaining - 1;
    }
}

inline void AppendCommand(char* buffer, size_t capacity, size_t* used, const char* label,
                          uintptr_t address, ReadMemory readMemory) {
    // Fast3D marks segmented references with bit zero. They are not host Gfx
    // pointers. Report the segment/offset without probing the tagged address.
    if ((address & 1u) != 0 && address <= UINT32_MAX) {
        Append(buffer, capacity, used,
               "  %s pc=0x%016" PRIXPTR " tagged segment=%02X offset=%06X (not dereferenced)\n",
               label, address, static_cast<unsigned>(address >> 24),
               static_cast<unsigned>(address & 0x00FFFFFEu));
        return;
    }
    uintptr_t words[2]{};
    if (address == 0 || readMemory == nullptr || !readMemory(address, words, sizeof(words))) {
        Append(buffer, capacity, used, "  %s pc=0x%016" PRIXPTR " unreadable\n", label, address);
        return;
    }
    Append(buffer, capacity, used,
           "  %s pc=0x%016" PRIXPTR " opcode=%02X w0=0x%016" PRIXPTR " w1=0x%016" PRIXPTR "\n",
           label, address, static_cast<unsigned>((words[0] >> 24) & 0xFFu), words[0], words[1]);
}
} // namespace RenderFaultReport
