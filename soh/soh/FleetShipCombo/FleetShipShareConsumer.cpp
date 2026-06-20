// Fleet Ship Combo - picture-in-picture CONSUMER (Ship side).
//
// Opens the D3D11 shared texture that 2ship publishes (its rendered game image) and
// draws it in a resizable ImGui window, so MM appears picture-in-picture inside Ship.
// Uses ONLY public libultraship APIs + D3D11 COM, so there are NO submodule changes.
//
// Getting Ship's render device without submodule changes: Window::GetGfxFrameBuffer()
// returns Ship's own game framebuffer SRV (when internal res != 1.0x); srv->GetDevice()
// gives the ID3D11Device that ImGui renders with. We only need it once, then cache it.

#include "FleetShipCombo.h"

#include <libultraship/libultraship.h>
#include <libultraship/bridge.h>
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <memory>
#include <cstdio>

#ifdef _WIN32
#include <d3d11.h>
#include <dxgi.h>

namespace {
ID3D11Device* sDevice = nullptr;
ID3D11DeviceContext* sCtx = nullptr;
ID3D11Texture2D* sSharedTex = nullptr; // opened shared (keyed-mutex) texture from 2ship
IDXGIKeyedMutex* sMutex = nullptr;
ID3D11Texture2D* sPrivateTex = nullptr;            // our own copy that ImGui samples
ID3D11ShaderResourceView* sPrivateSrv = nullptr;
unsigned long long sOpenedHandle = 0;
unsigned int sTexW = 0;
unsigned int sTexH = 0;

// Grab Ship's render device once (the same device ImGui draws with). Returns null until
// the game renders to an offscreen fb (we nudge the internal resolution to make it so).
ID3D11Device* GetShipDevice() {
    if (sDevice) {
        return sDevice;
    }
    auto window = Ship::Context::GetRawInstance()->GetWindow();
    if (!window) {
        return nullptr;
    }
    uintptr_t fb = window->GetGfxFrameBuffer();
    if (fb == 0) {
        // Coax the game to render to an fb so we can read the device next frame.
        window->SetResolutionMultiplier(1.01f);
        return nullptr;
    }
    ID3D11ShaderResourceView* srv = reinterpret_cast<ID3D11ShaderResourceView*>(fb);
    srv->GetDevice(&sDevice); // AddRef; cached for the app's lifetime
    if (sDevice) {
        sDevice->GetImmediateContext(&sCtx);
    }
    return sDevice;
}

void ReleaseOpened() {
    if (sPrivateSrv) {
        sPrivateSrv->Release();
        sPrivateSrv = nullptr;
    }
    if (sPrivateTex) {
        sPrivateTex->Release();
        sPrivateTex = nullptr;
    }
    if (sMutex) {
        sMutex->Release();
        sMutex = nullptr;
    }
    if (sSharedTex) {
        sSharedTex->Release();
        sSharedTex = nullptr;
    }
    sOpenedHandle = 0;
    sTexW = sTexH = 0;
}

// Reopen 2ship's shared texture on handle change and (re)create our private copy + SRV.
void EnsureTexture() {
    unsigned long long handle = 0;
    unsigned int w = 0, h = 0, fmt = 0, frame = 0;
    if (!FleetShipCombo_GetSharedTexture(&handle, &w, &h, &fmt, &frame) || handle == 0) {
        return;
    }
    ID3D11Device* dev = GetShipDevice();
    if (!dev) {
        return;
    }
    if (handle == sOpenedHandle && sPrivateSrv) {
        return; // already opened this handle
    }

    ReleaseOpened();

    ID3D11Resource* res = nullptr;
    if (FAILED(dev->OpenSharedResource(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle)),
                                       __uuidof(ID3D11Resource), reinterpret_cast<void**>(&res))) ||
        !res) {
        return;
    }
    res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&sSharedTex));
    res->Release();
    if (!sSharedTex) {
        return;
    }
    sSharedTex->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&sMutex));

    D3D11_TEXTURE2D_DESC sd;
    sSharedTex->GetDesc(&sd);
    D3D11_TEXTURE2D_DESC pd = {};
    pd.Width = sd.Width;
    pd.Height = sd.Height;
    pd.MipLevels = 1;
    pd.ArraySize = 1;
    pd.Format = sd.Format;
    pd.SampleDesc.Count = 1;
    pd.Usage = D3D11_USAGE_DEFAULT;
    pd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (SUCCEEDED(dev->CreateTexture2D(&pd, nullptr, &sPrivateTex)) && sPrivateTex &&
        SUCCEEDED(dev->CreateShaderResourceView(sPrivateTex, nullptr, &sPrivateSrv)) && sPrivateSrv) {
        sOpenedHandle = handle;
        sTexW = sd.Width;
        sTexH = sd.Height;
        SPDLOG_INFO("[FleetShipCombo] Consumer opened 2ship texture {}x{}", sd.Width, sd.Height);
    } else {
        ReleaseOpened();
    }
}

// Copy 2ship's shared texture into our private copy under the keyed mutex (cross-process
// GPU sync). If the producer holds the lock, skip this frame (keep the last copy) instead
// of reading a half-written / stale surface -> no "stuck"/torn image.
void CopyFrameLocked() {
    if (!sMutex || !sSharedTex || !sPrivateTex || !sCtx) {
        return;
    }
    if (sMutex->AcquireSync(0, 8) == S_OK) {
        sCtx->CopyResource(sPrivateTex, sSharedTex);
        sMutex->ReleaseSync(0);
    }
}

class FleetShip2ShipWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {
    }
    void UpdateElement() override {
    }
    void DrawElement() override {
    }

    // Full-window overlay: when MM (2ship) is the ACTIVE game, draw its image covering
    // the whole Ship window (OoT is frozen behind it). When OoT is active, draw nothing
    // so Ship's own game shows. Layout = "active game big, inactive not shown".
    // Draw on the FOREGROUND draw list: SoH renders its own game inside a "Main Game"
    // ImGui window, which sits ABOVE the background draw list — so background-list MM was
    // hidden behind OoT. Foreground is above the game. To keep the menu usable we SKIP
    // the MM overlay while the menu is open (you then see frozen OoT + the menu to switch).
    void Draw() override {
        int active = FleetShipCombo_GetActiveGame();

        auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
        auto menu = gui ? gui->GetMenu() : nullptr;
        bool menuOpen = menu && menu->IsVisible();
        bool guestUiFront = (FleetShipCombo_GetUiFocus() == 1); // 2ship window is on top for config

        if (active == 1 && !menuOpen && !guestUiFront) {
            EnsureTexture();
            CopyFrameLocked(); // pull a fresh frame under the keyed mutex
            if (sPrivateSrv && sTexW != 0 && sTexH != 0) {
                ImGuiViewport* vp = ImGui::GetMainViewport();
                ImDrawList* fg = ImGui::GetForegroundDrawList();
                ImVec2 p0 = vp->Pos;
                ImVec2 p1 = ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);

                // Black fill so the frozen OoT frame never peeks through the letterbox bars.
                fg->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));

                // Fit MM to the window, preserving aspect ratio (letterboxed).
                float winW = vp->Size.x, winH = vp->Size.y;
                float scale = winW / (float)sTexW;
                float scaleY = winH / (float)sTexH;
                if (scaleY < scale) {
                    scale = scaleY;
                }
                float w = (float)sTexW * scale, h = (float)sTexH * scale;
                float x = vp->Pos.x + (winW - w) * 0.5f, y = vp->Pos.y + (winH - h) * 0.5f;
                fg->AddImage(reinterpret_cast<ImTextureID>(sPrivateSrv), ImVec2(x, y), ImVec2(x + w, y + h));
            }
        }

        // Diagnostic, drawn LAST so it stays on top of the MM overlay. Remove once stable.
        {
            unsigned long long h = 0;
            unsigned int tw = 0, th = 0, fmt = 0, frame = 0;
            int hasTex = FleetShipCombo_GetSharedTexture(&h, &tw, &th, &fmt, &frame);
            char buf[256];
            snprintf(buf, sizeof(buf), "FSC active=%d shm=%d handle=%llu %ux%u texFrame=%u dev=%d srv=%d", active,
                     hasTex, (unsigned long long)h, tw, th, frame, sDevice ? 1 : 0, sPrivateSrv ? 1 : 0);
            ImGui::GetForegroundDrawList()->AddText(ImVec2(8.0f, 34.0f), IM_COL32(255, 255, 0, 255), buf);
        }
    }
};

std::shared_ptr<FleetShip2ShipWindow> sWindow;
} // namespace
#endif // _WIN32

void FleetShipCombo_RegisterConsumerWindow(void) {
#ifdef _WIN32
    if (!CVarGetInteger("isFleetShipCombo.Enabled", 0)) {
        return; // only when the combo is on
    }
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
    if (gui == nullptr) {
        return;
    }
    static const char* kName = "2ship (MM)";
    if (gui->GetGuiWindow(kName) != nullptr) {
        return;
    }
    sWindow = std::make_shared<FleetShip2ShipWindow>("gOpenWindows.FleetShip2Ship", kName);
    gui->AddGuiWindow(sWindow);
    sWindow->Show();
    SPDLOG_INFO("[FleetShipCombo] Consumer PiP window registered");
#endif
}
