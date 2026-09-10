#include "CrashHandlerExt.h"
#include "RenderFaultReport.h"
#include <fast/Fast3dWindow.h>
#include <ship/Context.h>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <sys/uio.h>
#include <unistd.h>
#endif
#include "variables.h"
#include "z64.h"
#include "z64actor.h"
#include <stdio.h>
#include <array>
#include "soh/ActorDB.h"
#include <fast/interpreter.h>

#define WRITE_VAR_LINE(buff, len, varName, varValue) \
    append_str(buff, len, varName);                  \
    append_line(buff, len, varValue);
#define WRITE_VAR(buff, len, varName, varValue) \
    append_str(buff, len, varName);             \
    append_str(buff, len, varValue);

extern "C" PlayState* gPlayState;

static std::array<const char*, ACTORCAT_MAX> sCatToStrArray{
    "SWITCH", "BG", "PLAYER", "EXPLOSIVE", "NPC", "ENEMY", "PROP", "ITEMACTION", "MISC", "BOSS", "DOOR", "CHEST",
};

#define DEFINE_SCENE(_1, _2, enumName, _4, _5, _6) #enumName,

static std::array<const char*, SCENE_ID_MAX> sSceneIdToStrArray{
#include "tables/scene_table.h"
};

#undef DEFINE_SCENE

// Matches the callback buffer in the pinned libultraship CrashHandler.
static constexpr size_t kCrashReportCapacity = 32768;

static void append_str(char* buf, size_t* len, const char* str) {
    RenderFaultReport::Append(buf, kCrashReportCapacity, len, "%s", str != nullptr ? str : "<null>");
}

static void append_line(char* buf, size_t* len, const char* str) {
    RenderFaultReport::Append(buf, kCrashReportCapacity, len, "%s\n", str != nullptr ? str : "<null>");
}

static bool CrashHandler_ReadMemory(uintptr_t address, void* output, size_t size) {
#ifdef _WIN32
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &copied) &&
           copied == size;
#elif defined(__linux__)
    struct iovec local = { output, size };
    struct iovec remote = { reinterpret_cast<void*>(address), size };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
#else
    // Keep the raw addresses on unsupported platforms rather than risk a
    // second fault by directly dereferencing an invalid renderer command.
    return false;
#endif
}

static void CrashHandler_WriteGfxExecution(char* buffer, size_t* pos) {
    auto& stack = Fast::g_exec_stack;
    append_line(buffer, pos, "GFX execution (actual display-list chain):");
    if (stack.cmd_stack.empty()) {
        append_line(buffer, pos, "  No active display-list execution");
        return;
    }
    RenderFaultReport::AppendCommand(buffer, kCrashReportCapacity, pos, "current",
                                    reinterpret_cast<uintptr_t>(stack.cmd_stack.top()), CrashHandler_ReadMemory);
    // disp_stack contains source OPEN/CLOSE markers, and may be empty in these
    // builds. gfx_path contains the actual callers retained by the interpreter.
    const size_t first = stack.gfx_path.size() > 16 ? stack.gfx_path.size() - 16 : 0;
    for (size_t i = first; i < stack.gfx_path.size(); ++i) {
        RenderFaultReport::Append(buffer, kCrashReportCapacity, pos, "  frame=%zu\n", i);
        const uintptr_t caller = reinterpret_cast<uintptr_t>(stack.gfx_path[i]);
        RenderFaultReport::AppendCommand(buffer, kCrashReportCapacity, pos, "caller", caller,
                                        CrashHandler_ReadMemory);
        // Nearby words identify multiword hash commands and local segment
        // setup. They are context, not a claim that each command was executed.
        if (i + 2 >= stack.gfx_path.size() && caller >= 2 * sizeof(Fast::F3DGfx) && (caller & 1u) == 0) {
            RenderFaultReport::AppendCommand(buffer, kCrashReportCapacity, pos, "nearby -2",
                                            caller - 2 * sizeof(Fast::F3DGfx), CrashHandler_ReadMemory);
            RenderFaultReport::AppendCommand(buffer, kCrashReportCapacity, pos, "nearby -1",
                                            caller - sizeof(Fast::F3DGfx), CrashHandler_ReadMemory);
            RenderFaultReport::AppendCommand(buffer, kCrashReportCapacity, pos, "nearby +1",
                                            caller + sizeof(Fast::F3DGfx), CrashHandler_ReadMemory);
        }
    }
    auto* context = Ship::Context::GetRawInstance();
    if (context == nullptr) {
        return;
    }
    auto window = std::dynamic_pointer_cast<Fast::Fast3dWindow>(context->GetWindow());
    auto interpreter = window != nullptr ? window->GetInterpreterWeak().lock() : nullptr;
    if (interpreter != nullptr) {
        append_line(buffer, pos, "GFX segment bindings at fault:");
        for (size_t i = 0; i < 16; ++i) {
            RenderFaultReport::Append(buffer, kCrashReportCapacity, pos, "  segment=%02X base=0x%016" PRIXPTR "\n",
                                      static_cast<unsigned>(i), interpreter->mSegmentPointers[i]);
        }
    }
}

static void CrashHandler_WriteActorData(char* buffer, size_t* pos) {
    for (unsigned int i = 0; i < ACTORCAT_MAX; i++) {

        ActorListEntry* entry = &gPlayState->actorCtx.actorLists[i];
        Actor* cur;

        if (entry->length == 0) {
            continue;
        }
        WRITE_VAR_LINE(buffer, pos, "  Category: ", sCatToStrArray[i]);
        cur = entry->head;
        while (cur != nullptr) {
            std::string actorLine = "    ";
            actorLine += ActorDB::Instance->RetrieveEntry(cur->id).entry.valid
                             ? ActorDB::Instance->RetrieveEntry(cur->id).entry.desc
                             : "???";
            actorLine += " (" + std::to_string(cur->params) + ")";
            append_line(buffer, pos, actorLine.c_str());

            cur = cur->next;
        }
    }
}

extern "C" void CrashHandler_PrintSohData(char* buffer, size_t* pos) {
    char intCharBuffer[16];
    append_line(buffer, pos, "Build Information:");
    WRITE_VAR_LINE(buffer, pos, "  Game Version: ", (const char*)gBuildVersion);
    WRITE_VAR_LINE(buffer, pos, "  Git Branch: ", (const char*)gGitBranch);
    WRITE_VAR_LINE(buffer, pos, "  Git Commit: ", (const char*)gGitCommitHash);
    WRITE_VAR_LINE(buffer, pos, "  Build Date: ", (const char*)gBuildDate);

    CrashHandler_WriteGfxExecution(buffer, pos);

    if (gPlayState != nullptr) {
        WRITE_VAR_LINE(buffer, pos, "Scene: ", sSceneIdToStrArray[gPlayState->sceneNum]);

        snprintf(intCharBuffer, sizeof(intCharBuffer), "%i", gPlayState->roomCtx.curRoom.num);
        WRITE_VAR_LINE(buffer, pos, "Room: ", intCharBuffer);

        append_line(buffer, pos, "Actors:");
        CrashHandler_WriteActorData(buffer, pos);

        append_line(buffer, pos, "GFX Stack:");
        for (auto& disp : Fast::g_exec_stack.disp_stack) {
            std::string line = "  ";
            line += disp.file;
            line += ":";
            line += std::to_string(disp.line);
            append_line(buffer, pos, line.c_str());
        }
    }
}
