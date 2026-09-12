#include <cstdint>

#include "test_require.h"

#include "../mods/transformation_masks/assets/mm_display_list_patch.h"

struct ResolveFixture {
    uint64_t nestedHash;
    uintptr_t nestedPointer;
    uint64_t vertexHash;
    uintptr_t vertexPointer;
    size_t vertexSize;
};

static uintptr_t ResolveResource(void* context, MmDisplayListReferenceKind kind, uint64_t hash, size_t* resourceSize) {
    ResolveFixture* fixture = static_cast<ResolveFixture*>(context);

    if (kind == MM_DISPLAY_LIST_REFERENCE_CULL) {
        return hash == 0 ? UINT64_C(0x30000000) : hash == 2 ? UINT64_C(0x30000080) : 0;
    }

    if (kind == MM_DISPLAY_LIST_REFERENCE_NESTED && hash == fixture->nestedHash) {
        return fixture->nestedPointer;
    }
    if (kind == MM_DISPLAY_LIST_REFERENCE_VERTEX && hash == fixture->vertexHash) {
        *resourceSize = fixture->vertexSize;
        return fixture->vertexPointer;
    }
    return 0;
}

int main() {
    uint8_t arrayVertices[64] = {};
    MmDisplayListVertexResourceView vertexView = {};

    REQUIRE(MmDisplayList_SelectVertexResource(nullptr, 0, arrayVertices, sizeof(arrayVertices), true, &vertexView));
    REQUIRE(vertexView.pointer == reinterpret_cast<uintptr_t>(arrayVertices));
    REQUIRE(vertexView.size == sizeof(arrayVertices));
    REQUIRE(!MmDisplayList_SelectVertexResource(nullptr, 0, arrayVertices, sizeof(arrayVertices), false,
                                                &vertexView));

    constexpr uint64_t nestedHash = UINT64_C(0x0123456789ABCDEF);
    constexpr uint64_t vertexHash = UINT64_C(0xFEDCBA9876543210);
    ResolveFixture fixture = { nestedHash, UINT64_C(0x12345000), vertexHash, UINT64_C(0x20000000), 0x100 };
    MmDisplayListCommand commands[] = {
        { UINT32_C(0x31010000), 0 },
        { UINT32_C(0x01234567), UINT32_C(0x89ABCDEF) },
        { UINT32_C(0x32001002), UINT32_C(0x20) },
        { UINT32_C(0xFEDCBA98), UINT32_C(0x76543210) },
        { UINT32_C(0xDF000000), 0 },
    };
    MmDisplayListPatchStats stats = {};

    REQUIRE(MmDisplayList_PatchCommands(commands, 5, ResolveResource, &fixture, &stats));
    REQUIRE(commands[0].w0 == UINT32_C(0xDE010000));
    REQUIRE(commands[0].w1 == fixture.nestedPointer);
    REQUIRE(commands[1].w0 == 0);
    REQUIRE(commands[1].w1 == 0);
    /* G_VTX_OTR_HASH's first w1 is vestigial, not a byte offset. Once the
     * resource is resolved, emit an ordinary G_VTX with the resource base and
     * consume its hash payload so Fast3D cannot reinterpret it as commands. */
    REQUIRE(commands[2].w0 == UINT32_C(0x01001002));
    REQUIRE(commands[2].w1 == fixture.vertexPointer);
    REQUIRE(commands[3].w0 == 0);
    REQUIRE(commands[3].w1 == 0);
    REQUIRE(stats.nestedPatched == 1);
    REQUIRE(stats.verticesPatched == 1);
    REQUIRE(stats.unresolved == 0);
    REQUIRE(stats.malformed == 0);

    MmDisplayListCommand unresolved[] = {
        { UINT32_C(0x31000000), 0 },
        { UINT32_C(0x11111111), UINT32_C(0x22222222) },
        { UINT32_C(0xDF000000), 0 },
    };
    stats = {};
    REQUIRE(MmDisplayList_PatchCommands(unresolved, 3, ResolveResource, &fixture, &stats));
    REQUIRE(unresolved[0].w0 == UINT32_C(0x31000000));
    REQUIRE(stats.unresolved == 1);

    MmDisplayListCommand outOfBounds[] = {
        { UINT32_C(0x32011004), UINT32_C(0xF0) },
        { UINT32_C(0xFEDCBA98), UINT32_C(0x76543210) },
        { UINT32_C(0xDF000000), 0 },
    };
    stats = {};
    REQUIRE(!MmDisplayList_PatchCommands(outOfBounds, 3, ResolveResource, &fixture, &stats));
    REQUIRE(stats.malformed == 1);
    REQUIRE(outOfBounds[0].w0 == UINT32_C(0x32011004));

    MmDisplayListCommand malformed[] = { { UINT32_C(0x31000000), 0 } };
    stats = {};
    REQUIRE(!MmDisplayList_PatchCommands(malformed, 1, ResolveResource, &fixture, &stats));
    REQUIRE(stats.malformed == 1);
    // Exact command from mm.o2r gSkullKidTorsoDL: a front-cull call via
    // segment 0x0C, index 2. EnViewer does not own that segment, and separate
    // C arrays are not guaranteed adjacent even when a caller binds index 0.
    MmDisplayListCommand cull[] = {
        { UINT32_C(0x3D000000), UINT32_C(0x0C000002) },
        { UINT32_C(0xDF000000), 0 },
    };
    stats = {};
    REQUIRE(MmDisplayList_PatchCommands(cull, 2, ResolveResource, &fixture, &stats));
    REQUIRE(cull[0].w0 == UINT32_C(0xDE000000));
    REQUIRE(cull[0].w1 == UINT64_C(0x30000080));
    REQUIRE(stats.cullPatched == 1);
    MmDisplayListCommand backCull[] = {
        { UINT32_C(0x3D010000), UINT32_C(0x0C000000) },
        { UINT32_C(0xDF000000), 0 },
    };
    REQUIRE(MmDisplayList_PatchCommands(backCull, 2, ResolveResource, &fixture, &stats));
    REQUIRE(backCull[0].w0 == UINT32_C(0xDE010000));
    REQUIRE(backCull[0].w1 == UINT64_C(0x30000000));
    MmDisplayListCommand invalidCull[] = {
        { UINT32_C(0x3D000000), UINT32_C(0x0C000003) },
        { UINT32_C(0xDF000000), 0 },
    };
    REQUIRE(!MmDisplayList_PatchCommands(invalidCull, 2, ResolveResource, &fixture, &stats));
    REQUIRE(stats.malformed == 1);
    // A hash payload that looks like G_DL_INDEX is data, not a cull call.
    MmDisplayListCommand payload[] = {
        { UINT32_C(0x33000000), 0 },
        { UINT32_C(0x3D000000), UINT32_C(0x0C000002) },
        { UINT32_C(0xDF000000), 0 },
    };
    REQUIRE(MmDisplayList_PatchCommands(payload, 3, ResolveResource, &fixture, &stats));
    REQUIRE(payload[1].w0 == UINT32_C(0x3D000000));
    REQUIRE(stats.cullPatched == 0);
    return 0;
}
