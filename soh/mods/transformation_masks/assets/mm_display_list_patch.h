#ifndef MM_DISPLAY_LIST_PATCH_H
#define MM_DISPLAY_LIST_PATCH_H

#include <cstddef>
#include <cstdint>

struct MmDisplayListCommand {
    uint32_t w0;
    uintptr_t w1;
};

enum MmDisplayListReferenceKind {
    MM_DISPLAY_LIST_REFERENCE_NESTED,
    MM_DISPLAY_LIST_REFERENCE_VERTEX,
};

struct MmDisplayListPatchStats {
    size_t nestedPatched;
    size_t verticesPatched;
    size_t unresolved;
    size_t malformed;
};

using MmDisplayListResolveResource = uintptr_t (*)(void* context, MmDisplayListReferenceKind kind, uint64_t hash,
                                                   size_t* resourceSize);

bool MmDisplayList_PatchCommands(MmDisplayListCommand* commands, size_t commandCount,
                                 MmDisplayListResolveResource resolveResource, void* context,
                                 MmDisplayListPatchStats* stats);

#endif
