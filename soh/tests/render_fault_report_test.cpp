#include "../soh/RenderFaultReport.h"
#include <cstdio>
#include <cstring>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)

static bool ReadFixture(uintptr_t address, void* output, size_t size) {
    // A synthetic command byte range; all other addresses are unavailable.
    const uintptr_t words[] = { 0xDE000000u, 0x08000001u };
    if (address != 0x10000000u || size != sizeof(words)) {
        return false;
    }
    std::memcpy(output, words, size);
    return true;
}

static bool RejectRead(uintptr_t, void*, size_t) {
    return false;
}

int main() {
    char buffer[512]{};
    size_t used = 0;
    RenderFaultReport::AppendCommand(buffer, sizeof(buffer), &used, "current", 0x08000001u, ReadFixture);
    CHECK(std::strstr(buffer, "segment=08 offset=000000") != nullptr);
    CHECK(std::strstr(buffer, "not dereferenced") != nullptr);

    RenderFaultReport::AppendCommand(buffer, sizeof(buffer), &used, "caller", 0x10000000u, ReadFixture);
    CHECK(std::strstr(buffer, "opcode=DE") != nullptr);
    CHECK(std::strstr(buffer, "08000001") != nullptr);

    RenderFaultReport::AppendCommand(buffer, sizeof(buffer), &used, "unreadable", 0x20000000u, RejectRead);
    CHECK(std::strstr(buffer, "unreadable") != nullptr);
    CHECK(used == std::strlen(buffer));

    struct { char bytes[20]; char sentinel[4]; } tiny{};
    std::memcpy(tiny.sentinel, "KEEP", 4);
    used = 0;
    RenderFaultReport::AppendCommand(tiny.bytes, sizeof(tiny.bytes), &used, "current", 0x08000001u, ReadFixture);
    CHECK(used < sizeof(tiny.bytes));
    CHECK(tiny.bytes[used] == '\0');
    CHECK(std::memcmp(tiny.sentinel, "KEEP", 4) == 0);
    RenderFaultReport::AppendCommand(tiny.bytes, sizeof(tiny.bytes), &used, "caller", 0x10000000u, ReadFixture);
    CHECK(std::memcmp(tiny.sentinel, "KEEP", 4) == 0);

    used = 99;
    RenderFaultReport::AppendCommand(tiny.bytes, sizeof(tiny.bytes), &used, "full", 0, RejectRead);
    CHECK(used == 99);
    CHECK(std::memcmp(tiny.sentinel, "KEEP", 4) == 0);
    RenderFaultReport::AppendCommand(nullptr, 0, &used, "empty", 0, RejectRead);
    std::puts("PASS render fault report: tagged PC, caller opcode/operand, unreadable memory, bounded buffer");
    return 0;
}
