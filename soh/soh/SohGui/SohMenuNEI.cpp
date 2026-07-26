#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "UIWidgets.hpp"
#include "soh/ResourceManagerHelpers.h"
#include "soh/resource/type/PlayerAnimation.h"
#include "soh/resource/type/SohResourceType.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Notification/Notification.h"
#include "soh/FleetShipCombo/FleetShipCombo.h"
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <string>

extern "C" {
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "functions.h"
#include "z64animation.h"
#include "transformation_masks/assets/mm_asset_loader.h"
#include "mods/transformation_masks/transformation_masks.h"
#include "mods/pak_loader/pak_loader.h"
#include "mods/voice_pack/voice_pack.h"
#include "mods/nei_save.h" // Skijer's NEI — ultrashotOwned (Ultrashot toggle)
void TwilightUpgrade_Grant(void);
extern unsigned char TwilightUpgrade_HasClawshot(void);
extern unsigned char TwilightUpgrade_HasBombArrows(void);
extern unsigned char TwilightUpgrade_HasGaleBoomerang(void);
void TwilightUpgrade_SetClawshot(unsigned char on);
void TwilightUpgrade_SetBombArrows(unsigned char on);
void TwilightUpgrade_SetGaleBoomerang(unsigned char on);
// NEI Weapon Upgrades (mods/items/logic/weapon_upgrades.c)
extern unsigned char WeaponUpgrade_HasHammerAxe(void);
extern unsigned char WeaponUpgrade_HasRazor(void);
extern unsigned char WeaponUpgrade_HasGilded(void);
extern unsigned char WeaponUpgrade_HasTrueMaster(void);
extern unsigned char WeaponUpgrade_HasGreatFairy(void);
void WeaponUpgrade_SetHammerAxe(unsigned char on);
void WeaponUpgrade_SetRazor(unsigned char on);
void WeaponUpgrade_SetGilded(unsigned char on);
void WeaponUpgrade_SetTrueMaster(unsigned char on);
void WeaponUpgrade_SetGreatFairy(unsigned char on);
void WeaponUpgrade_GrantAll(void);

// Pictograph Box (mods/items/logic/picto_box.c)
extern unsigned char Picto_IsOwned(void);
void Picto_SetOwned(unsigned char on);
void Picto_TakePhotoNow(void);
// Power Keg (mods/items/logic/power_keg.c)
extern unsigned char PowerKeg_IsOwned(void);
void PowerKeg_SetOwned(unsigned char on);
unsigned char PowerKeg_GetCount(void);
void PowerKeg_SetCount(unsigned char n);
// MM adult trade-quest items (mods/items/logic/trade_items.c)
int TradeAdult_Count(void);
unsigned char TradeAdult_IsOwnedIndex(int index);
void TradeAdult_GiveIndex(int index);
int TradeAdult_OwnedCount(void);
void PikachuControls_OpenWindow(void); // pikachu_hud.cpp — Pikachu mode bindings window
extern PlayState* gPlayState;
u8 GerudoForm_IsActive(void); // gerudo_form.cpp
}

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

// =============================================================================
// Item Editor - per-item live tuning (Skijer's NEI)
// =============================================================================
// One section per custom item. Every control writes a `gItemEditor.<Item>.*`
// CVar that the item's behavior TU reads once per frame, so edits land live and
// the CVar names are shared with 2ship (a preset carries over between games).
// More items get their own section below as they are made tunable.
namespace {

struct CapeFloatParam {
    const char* label;
    const char* cvar;
    float min;
    float max;
    float def;
    const char* tooltip;
};

// Shape ------------------------------------------------------------------
const CapeFloatParam kCapeShapeParams[] = {
    { "Scale", "gItemEditor.Cape.Scale", 0.1f, 5.0f, 1.0f,
      "Master multiplier over both the cape's length and its width." },
    { "Segment Length", "gItemEditor.Cape.Length", 0.5f, 30.0f, 4.5f,
      "Length of one cloth segment. The cape is 12 joints deep, so the total\n"
      "hanging length is about 11x this value. Vanilla: 4.5." },
    { "Width", "gItemEditor.Cape.Width", 0.1f, 5.0f, 1.0f,
      "Multiplier over the shoulder-to-shoulder span the cloth is pinned across." },
    { "Arc Span (deg)", "gItemEditor.Cape.ArcSpan", 10.0f, 360.0f, 180.0f,
      "Angle the 12 strand roots are spread over. 180 is the vanilla half-circle\n"
      "around the shoulders; lower values bunch the cape into a narrower cloak,\n"
      "higher values wrap it further around the body." },
    { "Arc Bulge", "gItemEditor.Cape.ArcBulge", 0.0f, 4.0f, 1.0f,
      "Depth of the parabolic arc the roots trace - how far the middle of the cape\n"
      "bows out behind Link. 0 flattens the arc into a straight line across the\n"
      "shoulders." },
    { "Arc Spread", "gItemEditor.Cape.ArcSpread", 0.0f, 4.0f, 1.0f,
      "Width of that same arc along the shoulder line." },
};

// Placement & rotation ---------------------------------------------------
const CapeFloatParam kCapePlacementParams[] = {
    { "Offset Back/Forward", "gItemEditor.Cape.OffsetX", -50.0f, 50.0f, 0.0f,
      "Shifts the whole attachment arc backwards (+) or into Link's back (-)." },
    { "Offset Up/Down", "gItemEditor.Cape.OffsetY", -50.0f, 50.0f, 0.0f,
      "Raises (+) or lowers (-) the attachment arc along Link's spine." },
    { "Offset Left/Right", "gItemEditor.Cape.OffsetZ", -50.0f, 50.0f, 0.0f,
      "Slides the attachment arc sideways along the shoulder line." },
    { "Yaw (deg)", "gItemEditor.Cape.Yaw", -180.0f, 180.0f, 0.0f,
      "Turns the cape around Link's vertical axis, on top of the angle derived\n"
      "from his shoulders." },
    { "Pitch (deg)", "gItemEditor.Cape.Pitch", -180.0f, 180.0f, 0.0f,
      "Tips the cape forwards/backwards." },
    { "Roll (deg)", "gItemEditor.Cape.Roll", -180.0f, 180.0f, 0.0f, "Banks the cape sideways." },
};

// Physics ----------------------------------------------------------------
const CapeFloatParam kCapePhysicsParams[] = {
    { "Gravity", "gItemEditor.Cape.Gravity", -20.0f, 5.0f, -3.0f,
      "Downward pull applied to every joint each tick. Vanilla: -3.0.\n"
      "Positive values make the cape float upwards." },
    { "Back Push", "gItemEditor.Cape.BackPush", -20.0f, 20.0f, -4.0f,
      "Constant push away from Link's back that keeps the cloth from clipping\n"
      "into him while standing still. Vanilla: -4.0." },
    { "Body Radius", "gItemEditor.Cape.MinDist", 0.0f, 60.0f, 8.0f,
      "How far from Link's center the cloth is held off. Raise it if the cape\n"
      "clips through a wider custom model. Vanilla: 8.0." },
    { "Floor Limit", "gItemEditor.Cape.FloorOffset", -400.0f, 0.0f, -200.0f,
      "Lowest the cloth may hang, relative to Link's feet. Vanilla: -200." },
    { "Back Sway", "gItemEditor.Cape.BackSway", 0.0f, 3.0f, 0.3f,
      "Backwards billow per unit of running speed. Vanilla: 0.3." },
    { "Side Sway", "gItemEditor.Cape.SideSway", 0.0f, 3.0f, 0.15f,
      "Sideways flutter per unit of running speed. Vanilla: 0.15." },
    { "Damping", "gItemEditor.Cape.Damping", 0.0f, 1.0f, 0.8f,
      "Fraction of a joint's velocity carried into the next tick. Lower = stiffer,\n"
      "higher = floatier and slower to settle. Vanilla: 0.8." },
    { "Velocity Clamp", "gItemEditor.Cape.VelClamp", 0.5f, 30.0f, 5.0f,
      "Per-axis speed limit for a joint. Keeps the cloth from exploding on hard\n"
      "camera cuts. Vanilla: 5.0." },
    { "Settle Rate", "gItemEditor.Cape.Decel", 0.0f, 1.0f, 0.1f,
      "How quickly leftover motion bleeds away when Link stops. Vanilla: 0.1." },
};

void ItemEditorCapeColorWidget(WidgetInfo& info) {
    float col[4] = {
        CVarGetInteger("gItemEditor.Cape.ColorR", 255) / 255.0f,
        CVarGetInteger("gItemEditor.Cape.ColorG", 255) / 255.0f,
        CVarGetInteger("gItemEditor.Cape.ColorB", 255) / 255.0f,
        CVarGetInteger("gItemEditor.Cape.ColorA", 255) / 255.0f,
    };

    if (ImGui::ColorEdit4("Cape Color##ItemEditorCape", col, ImGuiColorEditFlags_AlphaBar)) {
        CVarSetInteger("gItemEditor.Cape.ColorR", (int32_t)(col[0] * 255.0f + 0.5f));
        CVarSetInteger("gItemEditor.Cape.ColorG", (int32_t)(col[1] * 255.0f + 0.5f));
        CVarSetInteger("gItemEditor.Cape.ColorB", (int32_t)(col[2] * 255.0f + 0.5f));
        CVarSetInteger("gItemEditor.Cape.ColorA", (int32_t)(col[3] * 255.0f + 0.5f));
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    UIWidgets::Tooltip("Tints the Ganondorf cape texture. White = untouched.\n"
                       "Dropping the alpha below 255 also switches the cloth to a translucent\n"
                       "render mode so it actually blends instead of being drawn opaque.");
}

void ItemEditorResetCape() {
    static const char* kIntCVars[] = { "gItemEditor.Cape.ColorR", "gItemEditor.Cape.ColorG",
                                       "gItemEditor.Cape.ColorB", "gItemEditor.Cape.ColorA" };

    for (const auto& p : kCapeShapeParams) {
        CVarSetFloat(p.cvar, p.def);
    }
    for (const auto& p : kCapePlacementParams) {
        CVarSetFloat(p.cvar, p.def);
    }
    for (const auto& p : kCapePhysicsParams) {
        CVarSetFloat(p.cvar, p.def);
    }
    for (const char* cvar : kIntCVars) {
        CVarSetInteger(cvar, 255);
    }
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

// Shared PreFunc for every cape slider: greyed out until the section's master
// switch is on, so the vanilla-vs-tuned A/B is a single click.
void ItemEditorCapeGate(WidgetInfo& info) {
    if (!CVarGetInteger("gItemEditor.Cape.Custom", 0)) {
        info.options->disabled = true;
        info.options->disabledTooltip = "Enable 'Enable Custom Cape Settings' first.";
    }
}

} // namespace

// =============================================================================
// "Skijer's NEI" - dedicated top-level menu gathering every NEI feature into
// its own tabs (Custom Items, Item Editor, Masks, Spells, Modes, Pak Loader,
// Randomizer, Controls). The tab list and its contents are kept 1:1 with 2ship's
// mm/2s2h/BenGui/BenMenu.cpp AddNEI() so a feature is always in the same place in
// both games; anything a game genuinely lacks (2ship has no .pak player models,
// Ship has no boss remains) is simply absent from that game's tab rather than
// shown as a dead toggle.
// Built as its own translation unit so the rest of Settings stays clean.
// Widgets are grouped by SIDEBAR (not file order), so each block below just
// (re)sets path.sidebarName to land in the right tab.
// =============================================================================
void RegisterNEIMenu() {
    WidgetPath path = { "Skijer's NEI", "Masks", SECTION_COLUMN_1 };

    mSohMenu->AddMenuEntry("Skijer's NEI", CVAR_SETTING("Menu.SkijerNEISidebarSection"));
    // Sidebar order is shared with 2ship's Skijer's NEI menu.
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Custom Items", 1);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Item Editor", 2);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Masks", 1);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Spells", 1);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Modes", 1);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Pak Loader", 3);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Randomizer", 1);
    mSohMenu->AddSidebarEntry("Skijer's NEI", "Controls", 1);
    path.sectionName = "Skijer's NEI";

    // ===================== Tab: Item Editor =====================
    path.sidebarName = "Item Editor";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Item Editor", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path,
                        "Live tuning for the custom items. Everything here writes gItemEditor.* CVars that "
                        "the item's behavior reads every frame, so changes apply instantly and are shared "
                        "with 2ship (the same preset works in both games). One section per item - more items "
                        "land here as they are made tunable.",
                        WIDGET_TEXT);

    mSohMenu->AddWidget(path, "Magic Cape", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path, "Enable Custom Cape Settings", WIDGET_CVAR_CHECKBOX)
        .CVar("gItemEditor.Cape.Custom")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Master switch for this section. OFF (default) makes the cape use its built-in\n"
            "values and ignore every control below, so you can A/B a tune against vanilla\n"
            "without resetting anything."));
    mSohMenu->AddWidget(path, "Reset Cape to Defaults", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) { ItemEditorResetCape(); })
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip(
            "Puts every cape control below back to the value the cloth ships with."));

    mSohMenu->AddWidget(path, "Cape: Shape & Size", WIDGET_SEPARATOR_TEXT);
    for (const auto& p : kCapeShapeParams) {
        mSohMenu->AddWidget(path, p.label, WIDGET_CVAR_SLIDER_FLOAT)
            .CVar(p.cvar)
            .RaceDisable(false)
            .PreFunc(ItemEditorCapeGate)
            .Options(FloatSliderOptions()
                         .Min(p.min)
                         .Max(p.max)
                         .DefaultValue(p.def)
                         .Step(0.05f)
                         .Format("%.2f")
                         .Tooltip(p.tooltip));
    }

    mSohMenu->AddWidget(path, "Cape: Placement & Rotation", WIDGET_SEPARATOR_TEXT);
    for (const auto& p : kCapePlacementParams) {
        mSohMenu->AddWidget(path, p.label, WIDGET_CVAR_SLIDER_FLOAT)
            .CVar(p.cvar)
            .RaceDisable(false)
            .PreFunc(ItemEditorCapeGate)
            .Options(FloatSliderOptions()
                         .Min(p.min)
                         .Max(p.max)
                         .DefaultValue(p.def)
                         .Step(0.5f)
                         .Format("%.1f")
                         .Tooltip(p.tooltip));
    }

    path.column = SECTION_COLUMN_2;
    mSohMenu->AddWidget(path, "Cape: Physics", WIDGET_SEPARATOR_TEXT);
    for (const auto& p : kCapePhysicsParams) {
        mSohMenu->AddWidget(path, p.label, WIDGET_CVAR_SLIDER_FLOAT)
            .CVar(p.cvar)
            .RaceDisable(false)
            .PreFunc(ItemEditorCapeGate)
            .Options(FloatSliderOptions()
                         .Min(p.min)
                         .Max(p.max)
                         .DefaultValue(p.def)
                         .Step(0.05f)
                         .Format("%.2f")
                         .Tooltip(p.tooltip));
    }

    mSohMenu->AddWidget(path, "Cape: Color", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path, "Cape Color", WIDGET_CUSTOM)
        .CustomFunction(ItemEditorCapeColorWidget)
        .HideInSearch(true);

    // ===================== Tab: Modes =====================
    path.sidebarName = "Modes";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Broken Modes", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path, "Enable Broken Modes", WIDGET_CVAR_CHECKBOX)
        .CVar("gBrokenItems.Enabled")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Form selector (Link / Mario / Pikachu) on the equipment page's transform sub-page (L cycles)."));

    // ===================== Tab: Custom Items =====================
    path.sidebarName = "Custom Items";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Custom Items", WIDGET_SEPARATOR_TEXT);

    // --- MM Quest Page (mirror of the 2ship-side OoT quest page). Skijer's NEI ---
    mSohMenu->AddWidget(path, "MM Quest Page", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path, "Songs: Pause Play (skip minigame)", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.SkijerNEI.PausePlay")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "On the MM Quest Status page (press L on the quest page to flip), if you hold an "
            "ocarina, pressing A on a learned song closes the menu and instantly plays it in-world "
            "(native success flow) INSTEAD of the learn-it minigame. OFF = the minigame."));
    mSohMenu->AddWidget(path, "Grant MM Songs + Remains", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            // FC_MMQ_* bits: remains 0-3; songs Sonata 6, GoronLullaby 7, NewWave 8, Elegy 9,
            // Oath 10, Time-row(Command) 12, Healing 13, Epona-row(Fugue) 14, Soaring 15,
            // Storms-row(Ballad) 16. (Saria/Sun rows come from OoT's own quest items.)
            Nei_Save()->mmQuestItems |= 0x0001FFCF;
        })
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip(
            "Grants the 4 boss remains + all MM-page songs (the 7 MM songs and the 3 NEI custom "
            "songs). Saria's/Sun's rows follow your normal OoT songs. Idempotent."));

    // NEI Weapon Upgrade appearance — pick which model/icon the progressive weapons display
    // for the levels that have two looks (DL + icon load from mm.o2r, with a vanilla fallback).
    mSohMenu->AddWidget(path, "Weapon Upgrade Appearance", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Gilded Sword: use Gilded look", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.SkijerNEI.GildedUsesGildedLook")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "When the Kokiri Sword is at its top upgrade (Gilded), choose the look:\n"
            "ON  = Gilded Sword model + icon.\n"
            "OFF = keep the Razor Sword look.\n"
            "(Razor level always shows the Razor look. DL/icon load from mm.o2r,\n"
            "falling back to the vanilla Kokiri Sword if unavailable.)"));

    mSohMenu->AddWidget(path, "Biggoron Upgrade: use Great Fairy's Sword look", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.SkijerNEI.BgsUsesGfsLook")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "When the Biggoron's Sword is upgraded (Great Fairy's Sword), choose the look:\n"
            "ON  = Great Fairy's Sword model + icon.\n"
            "OFF = keep the Biggoron's Sword look.\n"
            "(DL/icon load from mm.o2r, falling back to the vanilla Biggoron Sword.)"));

    mSohMenu->AddWidget(path, "Enable Extra Equipment", WIDGET_CVAR_CHECKBOX)
        .CVar("gCheats.ExtEquip.Enabled")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Adds 12 new equipment pieces (3 swords, 3 shields, 3 tunics, 3 boots).\n"
            "Press L on the equipment page to toggle between vanilla and extended equipment.\n"
            "(The 'Add Extended Equipment to Rando' toggle in the Randomizer tab controls\n"
            "whether they are shuffled into the seed.)"));

    // Net Model tuning sliders REMOVED: the in-hand placement is final and baked as constants in
    // object_net.c (Scale 0.49, Rot -5/-169/64, Offset 3.2/4.3/-1.0; catch radius 30). Skijer's NEI

    // Roc's Items MM Animations - requires mm.o2r
    mSohMenu->AddWidget(path, "Roc's Items Use MM Animations", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.RocsItemsUseMmAnims")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                CVarSetInteger("gEnhancements.RocsItemsUseMmAnims", 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Use MM animations for Roc's Feather and Roc's Cape jumps.\n"
                                           "Roc's Feather: Backflip on ground jump.\n"
                                           "Roc's Cape: Backflip on ground jump, roll jump on double jump.\n\n"
                                           "REQUIRES: mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0"));

    mSohMenu->AddWidget(path, "Invert Roc's Items Animations", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.RocsItems.InvertAnims")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!CVarGetInteger("gEnhancements.RocsItemsUseMmAnims", 0)) {
                CVarSetInteger("gMods.RocsItems.InvertAnims", 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Enable 'Roc's Items Use MM Animations' first.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Swaps the animation order for Roc's items.\n"
                                           "OFF: Ground = Backflip, Double = Roll Jump\n"
                                           "ON:  Ground = Roll Jump, Double = Backflip\n\n"
                                           "REQUIRES: Roc's Items Use MM Animations"));

    // NEI Aim Cycle — extends the vanilla BowArrowCycle cheat with L button
    // (previous direction), SW97 elemental arrow cycling, slingshot support,
    // and in-game Gust Jar element switching while in first-person aim.
    mSohMenu->AddWidget(path, "NEI Aim Cycle (R/L while aiming)", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.NeiAimCycle")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Cycle between elements while aiming. R = next, L = previous.\n"
            "Applies to:\n"
            "  - Bow & Slingshot: cycles vanilla arrows OR SW97 elemental arrows\n"
            "    (whichever type is currently equipped on the C-button).\n"
            "  - Gust Jar (in first-person, IDLE): cycles unlocked elements\n"
            "    based on owned medallions. Hold C + press L+R together to\n"
            "    cycle the element AND switch to a stored-blow that fires on\n"
            "    C release (no auto-discharge).\n"
            "  - Arrow wheel: now also includes Bombchus when you own any.\n\n"
            "Extends the vanilla 'Bow Arrow Cycle' cheat — both can be enabled\n"
            "at the same time without conflict."));

    // Twilight Upgrade — per-bit save flags. Each checkbox sets/clears one bit
    // of gSaveContext.ship.twilightUpgrade so the player can mix-and-match
    // which sub-upgrades are unlocked on this save (eventually shuffled by
    // rando). State is read live from the save each frame via PreFunc → the
    // shadow bool, and writes go through TwilightUpgrade_Set* on Callback.
    mSohMenu->AddWidget(path, "Twilight Upgrade Bits", WIDGET_SEPARATOR_TEXT);

    static bool sTwilightClawshotShadow = false;
    mSohMenu->AddWidget(path, "Clawshot", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sTwilightClawshotShadow = TwilightUpgrade_HasClawshot() != 0;
            info.valuePointer = &sTwilightClawshotShadow;
        })
        .Callback([](WidgetInfo& info) {
            TwilightUpgrade_SetClawshot(sTwilightClawshotShadow ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Unlocks the Clawshot mode on hookshot/longshot. Tap L during\n"
            "gameplay (with hookshot or longshot equipped) to toggle the\n"
            "reverse-pull behaviour (enemy → Link, pin to grappling point)."));

    static bool sTwilightBombArrowsShadow = false;
    mSohMenu->AddWidget(path, "Bomb Arrows", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sTwilightBombArrowsShadow = TwilightUpgrade_HasBombArrows() != 0;
            info.valuePointer = &sTwilightBombArrowsShadow;
        })
        .Callback([](WidgetInfo& info) {
            TwilightUpgrade_SetBombArrows(sTwilightBombArrowsShadow ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Auto-grants ITEM_BOMB_ARROWS into the inventory once a bomb bag\n"
            "is owned, and adds bomb arrows as a position in the SW97 R/L\n"
            "arrow cycle during bow aim."));

    static bool sTwilightGaleBoomerangShadow = false;
    mSohMenu->AddWidget(path, "Gale Boomerang", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sTwilightGaleBoomerangShadow = TwilightUpgrade_HasGaleBoomerang() != 0;
            info.valuePointer = &sTwilightGaleBoomerangShadow;
        })
        .Callback([](WidgetInfo& info) {
            TwilightUpgrade_SetGaleBoomerang(sTwilightGaleBoomerangShadow ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Unlocks the Gale Boomerang multi-target. During boomerang aim,\n"
            "L tap adds the current Z-target to the route (up to 4 targets,\n"
            "500 units max between consecutive targets)."));

    // Convenience: still expose the grant-all button as a one-click that
    // flips all three checkboxes on at once.
    mSohMenu->AddWidget(path, "Grant All Twilight Bits", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            TwilightUpgrade_Grant();
        })
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip(
            "Sets all three Twilight Upgrade bits at once. Equivalent to\n"
            "checking the three checkboxes above. Idempotent."));

    // Skijer's NEI hookshot overhaul — Ultrashot ownership (its own unlock, persisted in the
    // "nei" save section, NOT one of the Twilight bits above). Same live-shadow pattern.
    mSohMenu->AddWidget(path, "Hookshot Overhaul", WIDGET_SEPARATOR_TEXT);

    static bool sUltrashotShadow = false;
    mSohMenu->AddWidget(path, "Ultrashot", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sUltrashotShadow = Nei_Save()->ultrashotOwned != 0;
            info.valuePointer = &sUltrashotShadow;
        })
        .Callback([](WidgetInfo& info) {
            Nei_Save()->ultrashotOwned = sUltrashotShadow ? 1 : 0;
        })
        .Options(CheckboxOptions().Tooltip(
            "Unlocks the Ultrashot: the LONGSHOT fires 4x the hookshot's\n"
            "distance at double chain speed (Link's reel-in included).\n"
            "The item keeps the Longshot icon — a small Light Medallion\n"
            "corner marker and the 'Ultrashot' name tell it apart."));

    // NEI Weapon Upgrades — per-bit save flags (gSaveContext.ship.weaponUpgrades).
    // Each upgrade requires the BASE weapon to be owned. State is read live from the
    // save each frame via PreFunc → the shadow bool, and writes go through
    // WeaponUpgrade_Set* on Callback (same pattern as the Twilight Upgrade bits).
    // Only the Hammer upgrade has gameplay behavior for now; the sword upgrades are
    // reachable plumbing (rando give/logic) with behavior TBD.
    mSohMenu->AddWidget(path, "Weapon Upgrade Bits", WIDGET_SEPARATOR_TEXT);

    static bool sWuHammerShadow = false;
    mSohMenu->AddWidget(path, "Hammer: Iron Knuckle's Axe", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sWuHammerShadow = WeaponUpgrade_HasHammerAxe() != 0;
            info.valuePointer = &sWuHammerShadow;
        })
        .Callback([](WidgetInfo& info) {
            WeaponUpgrade_SetHammerAxe(sWuHammerShadow ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Upgrades the Megaton Hammer into the Iron Knuckle's Axe:\n"
            "double damage, double reach, chunky heavy swings, and a\n"
            "tomahawk throw (hold R + B). Only takes effect while the\n"
            "hammer is actually equipped and wielded."));

    static bool sWuRazorShadow = false;
    mSohMenu->AddWidget(path, "Kokiri: Razor Sword", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sWuRazorShadow = WeaponUpgrade_HasRazor() != 0;
            info.valuePointer = &sWuRazorShadow;
        })
        .Callback([](WidgetInfo& info) {
            WeaponUpgrade_SetRazor(sWuRazorShadow ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Kokiri Sword upgrade, progressive level 1. (Behavior TBD —\n"
            "reachable rando/save plumbing for now.)"));

    static bool sWuGildedShadow = false;
    mSohMenu->AddWidget(path, "Kokiri: Gilded Sword", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sWuGildedShadow = WeaponUpgrade_HasGilded() != 0;
            info.valuePointer = &sWuGildedShadow;
        })
        .Callback([](WidgetInfo& info) {
            WeaponUpgrade_SetGilded(sWuGildedShadow ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Kokiri Sword upgrade, progressive level 2. (Behavior TBD —\n"
            "reachable rando/save plumbing for now.)"));

    static bool sWuTrueMasterShadow = false;
    mSohMenu->AddWidget(path, "Master: True Master Sword", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sWuTrueMasterShadow = WeaponUpgrade_HasTrueMaster() != 0;
            info.valuePointer = &sWuTrueMasterShadow;
        })
        .Callback([](WidgetInfo& info) {
            WeaponUpgrade_SetTrueMaster(sWuTrueMasterShadow ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Master Sword upgrade: True Master Sword. (Behavior TBD —\n"
            "reachable rando/save plumbing for now.)"));

    static bool sWuGreatFairyShadow = false;
    mSohMenu->AddWidget(path, "Biggoron: Great Fairy's Sword", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sWuGreatFairyShadow = WeaponUpgrade_HasGreatFairy() != 0;
            info.valuePointer = &sWuGreatFairyShadow;
        })
        .Callback([](WidgetInfo& info) {
            WeaponUpgrade_SetGreatFairy(sWuGreatFairyShadow ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Biggoron Sword upgrade: Great Fairy's Sword. (Behavior TBD —\n"
            "reachable rando/save plumbing for now.)"));

    mSohMenu->AddWidget(path, "Grant All Weapon Upgrades", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            WeaponUpgrade_GrantAll();
        })
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip(
            "Sets all weapon-upgrade bits at once. Idempotent."));

    // NEI Pictograph Box (MM port). Photographing a mapped OoT actor writes MM's pictoFlags0/1 +
    // the I5 photo into the NEI save in MM's exact layout, for a 2Ship bridge. No in-OoT reward.
    mSohMenu->AddWidget(path, "Pictograph Box", WIDGET_SEPARATOR_TEXT);

    static bool sPictoOwnedShadow = false;
    mSohMenu->AddWidget(path, "Pictobox: Owned", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sPictoOwnedShadow = Picto_IsOwned() != 0;
            info.valuePointer = &sPictoOwnedShadow;
        })
        .Callback([](WidgetInfo& info) {
            Picto_SetOwned(sPictoOwnedShadow ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Grants the Pictograph Box. Photos are stored in Majora's Mask's\n"
            "exact save layout (pictoFlags0/1 + pictoPhotoI5) for a 2Ship bridge;\n"
            "they have no use in OoT itself."));

    mSohMenu->AddWidget(path, "Take Pictograph Now (debug)", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            Picto_TakePhotoNow();
        })
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip(
            "Fires the shutter immediately (no aim/wheel) to test capture + subject\n"
            "validation: sweeps loaded actors, sets the matching PICTO_VALID_* flags,\n"
            "and captures the I5 photo of the current frame."));

    mSohMenu->AddWidget(path, "Power Keg", WIDGET_SEPARATOR_TEXT);

    static bool sPowerKegOwnedShadow = false;
    mSohMenu->AddWidget(path, "Power Keg: Owned", WIDGET_CHECKBOX)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            sPowerKegOwnedShadow = PowerKeg_IsOwned() != 0;
            info.valuePointer = &sPowerKegOwnedShadow;
        })
        .Callback([](WidgetInfo& info) {
            PowerKeg_SetOwned(sPowerKegOwnedShadow ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Grants the Power Keg (MM Goron's big bomb). Shares the Bomb slot:\n"
            "press A on Bombs in the kaleido to flip Bomb <-> Power Keg, then equip\n"
            "to a C-button. Usable only as Fierce Deity / Goron, or Human/Gerudo\n"
            "with Silver Gauntlets or better. Behavior WIP."));

    mSohMenu->AddWidget(path, "Give 20 Power Kegs", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            PowerKeg_SetOwned(1);
            PowerKeg_SetCount(20);
        })
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip(
            "Grants the Power Keg + 20 kegs of ammo (its own counter, separate from bombs)."));

    mSohMenu->AddWidget(path, "MM Trade Items", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Grant All Trade Items", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            int n = TradeAdult_Count();
            for (int i = 0; i < n; i++) {
                TradeAdult_GiveIndex(i);
            }
        })
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip(
            "Grants every adult trade-quest item (OoT's plus the 9 Majora's Mask ones)\n"
            "into the adult-trade slot wheel. The Pendant of Memories also unlocks its\n"
            "combat moveset (Ext Boots 2) — equip it on the equipment page to a C-button."));

    // ===================== Tab: Spells =====================
    path.sidebarName = "Spells";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Spells & Spiritual Stones", WIDGET_SEPARATOR_TEXT);

    // SW97 Medallion Spells — merged with the old "Enable Sage Spells" rando
    // toggle. This one checkbox now drives BOTH the C-button medallion casting
    // (gEnhancements.SkijerNEI.SW97Medallions) and the seed-locked elemental-
    // damage rando setting (RSK_SW97_SPELLS via CVAR_RANDOMIZER_SETTING).
    mSohMenu->AddWidget(path, "SW97 Medallion Spells", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.SkijerNEI.SW97Medallions")
        .RaceDisable(false)
        .PostFunc([](WidgetInfo& info) {
            CVarSetInteger(CVAR_RANDOMIZER_SETTING("SW97Spells"),
                           CVarGetInteger("gEnhancements.SkijerNEI.SW97Medallions", 0));
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Equip quest medallions to C-buttons from Quest Status.\n"
            "C = cast elemental spell, L+C = set elemental arrow/slingshot.\n"
            "Adult: elemental arrows. Child: elemental slingshot seeds.\n"
            "Also enables the seed-locked elemental-damage setting (Spell + Projectile\n"
            "paths), synced with 'Sage Spells' in the Randomizer menu.\n\n"
            "Credit: z64proto/sw97 team (spell/arrow actors)"));

    mSohMenu->AddWidget(path, "Enable Spiritual Stones", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.SpiritualStones.Enabled")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Master toggle for the custom Spiritual Stones behavior.\n"
            "ON: each owned stone toggles (A on the Quest page) a passive buff\n"
            "(Kokiri = faster walk, Goron = climb, Zora = swim), and held on a C/D-pad slot\n"
            "sets/warps to a saved waypoint statue.\n"
            "OFF: the stones behave like vanilla (no custom behavior)."));

    // ===================== Tab: Masks =====================
    path.sidebarName = "Masks";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Mask Transformations", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Kafei Mask Transform", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.KafeiMaskTransform")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!std::filesystem::exists("nei/N64_Kafei.pak")) {
                CVarSetInteger("gMods.KafeiMaskTransform", 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires N64_Kafei.pak in nei/ folder.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Wearing the Kafei Mask transforms Link into Kafei.\n"
                                           "Adult Link becomes Adult Kafei, Child Link becomes Child Kafei.\n"
                                           "Remove the mask to revert.\n\n"
                                           "REQUIRES: nei/N64_Kafei.pak"));

    mSohMenu->AddWidget(path, "Gerudo Mask Transform", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.GerudoMaskTransform")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Wearing the vanilla Gerudo Mask transforms Link into a Gerudo — uses a\n"
            "Link-rigged gerudo mesh, so all of Link's animations and equipment work\n"
            "exactly as normal. The body just looks gerudo.\n"
            "Effects while the mask is worn:\n"
            "  - Haunted Wasteland sandstorm is suppressed (no more getting lost).\n"
            "  - All Gerudos treat you as a fellow Gerudo (Ge1/Ge2/Ge3 are friendly,\n"
            "    GTG guard lets you in). Access is TEMPORARY — no Gerudo Card is granted.\n"
            "Remove the mask to revert.\n\n"
            "REQUIRES: nei/gerudo.o2r — built by tools/repack_gerudo_player.py from\n"
            "an artist-authored \"00 - Gerudo Player.o2r\" (Link-21-bone-rigged gerudo skin).\n"
            "Also ships 11 baked gerudo anims (visible in the anim viewer)."));

    // Garo Mask: gated by gMods.GaroMaskTransform (default ON). When OFF, the
    // Garo Mask stays a cosmetic mask (no transformation), matching the Gerudo
    // opt-out. Enforced in mm_player_form.cpp (MmForm_GetMaskType / HandleMaskUse).
    mSohMenu->AddWidget(path, "Garo Mask Transform", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.GaroMaskTransform")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Wearing the Garo Mask transforms Link into a Garo (custom garo.o2r skin\n"
            "with a slash-combo + shuriken finisher). Link's normal gameplay still runs\n"
            "1:1 — only the look and the slash combo change.\n"
            "OFF: the Garo Mask draws as a plain cosmetic mask (no transformation).\n\n"
            "REQUIRES: garo.o2r in the nei/ folder."));

    mSohMenu->AddWidget(path, "MM Masks", WIDGET_SEPARATOR_TEXT);

    // Merged option: "Include MM Masks Inventory" + "Extra Mask Effects" are now
    // a single toggle. Enabling the MM masks page also enables the per-mask
    // visual effects and the transformation system. (ExtraEffects had no runtime
    // reader other than this menu, so the merge loses nothing.)
    mSohMenu->AddWidget(path, "Include MM Masks (Inventory + Effects)", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.MmMasks.InventoryEnabled")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                CVarSetInteger("gMods.MmMasks.InventoryEnabled", 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.";
            }
        })
        .PostFunc([](WidgetInfo& info) {
            s32 on = CVarGetInteger("gMods.MmMasks.InventoryEnabled", 0);
            // One toggle drives the whole MM-mask feature set.
            CVarSetInteger("gMods.TransformMasks.ExtraEffects", on);
            if (on) {
                CVarSetInteger("gMods.TransformMasks.Enabled", 1);
            }
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Adds a 3rd inventory page with all 24 MM masks AND enables each mask's\n"
            "custom visual effects. Transformation masks (Deku, Goron, Zora, Fierce\n"
            "Deity) trigger transformations. Removes OOT Goron/Zora masks from the\n"
            "randomizer pool.\n\n"
            "REQUIRES: mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0"));

    mSohMenu->AddWidget(path, "Enable Transformation Masks", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.TransformMasks.Enabled")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                CVarSetInteger("gMods.TransformMasks.Enabled", 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.\n"
                                                "Download 2Ship, extract your MM ROM, then copy mm.o2r here.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Allows you to transform with certain masks like in Majora's Mask.\n"
                                           "Equip transformation masks from the MM Masks inventory page.\n\n"
                                           "REQUIRES: mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0"));

    mSohMenu->AddWidget(path, "Instant Transform", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.TransformMasks.InstantTransform")
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                CVarSetInteger("gMods.TransformMasks.InstantTransform", 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.";
            } else if (!CVarGetInteger("gMods.MmMasks.InventoryEnabled", 1)) {
                CVarSetInteger("gMods.TransformMasks.InstantTransform", 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Enable 'Include MM Masks' first.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Skip the transformation cutscene animation.\n"
                                           "Transform instantly when equipping a transformation mask.\n\n"
                                           "REQUIRES: Include MM Masks + mm.o2r"));

    mSohMenu->AddWidget(path, "Instant Blast Mask", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.BlastMask.Instant")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Removes the cooldown on Blast Mask.\n"
                                           "Normally there is a 310-frame (~5 second) cooldown between uses."));

    mSohMenu->AddWidget(path, "Invisible Non-Transformation Masks", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("HideNonTransformationMasks"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Turns all MM non-transformation masks invisible while still maintaining their effects.\n"
            "Transformation masks (Deku, Goron, Zora, Fierce Deity) remain visible.\n"
            "Only affects MM masks; vanilla OOT child masks are unaffected (use Invisible Bunny Hood for OOT bunny hood)."));

    // Mute MM Audio (moved here from the Spells tab — it's a mask/MM-assets option).
    mSohMenu->AddWidget(path, "Mute MM Audio", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.SkijerNEI.MuteMmAudio")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip("Mute all sounds from MM (mm.o2r).\n"
                                                              "Transformation mask SFX, voices, and instruments\n"
                                                              "will be silenced. OOT sounds play instead."));

    // ===================== Tab: Pak Loader =====================
    path.sidebarName = "Pak Loader";
    path.column = SECTION_COLUMN_1;
    mSohMenu->AddWidget(path, "Custom Models (.pak)", WIDGET_SEPARATOR_TEXT);

    // Build model combobox maps per age (triggers lazy init of PakLoader)
    {
        s32 pakCount = PakLoader_GetModelCount();

        // Adult models
        std::map<int32_t, const char*> adultModelMap;
        adultModelMap[-1] = "Default Link";
        for (s32 i = 0; i < pakCount; i++) {
            if (PakLoader_ModelHasAdult(i)) {
                adultModelMap[i] = PakLoader_GetModelLabel(i);
            }
        }

        // Child models
        std::map<int32_t, const char*> childModelMap;
        childModelMap[-1] = "Default Link";
        for (s32 i = 0; i < pakCount; i++) {
            if (PakLoader_ModelHasChild(i)) {
                childModelMap[i] = PakLoader_GetModelLabel(i);
            }
        }

        mSohMenu->AddWidget(path, "Enable Custom Player Model", WIDGET_CVAR_CHECKBOX)
            .CVar("gMods.PakLoader.Enabled")
            .RaceDisable(false)
            .PreFunc([](WidgetInfo& info) {
                if (PakLoader_GetModelCount() == 0) {
                    CVarSetInteger("gMods.PakLoader.Enabled", 0);
                    info.options->disabled = true;
                    info.options->disabledTooltip = "No .pak files found.\n"
                                                    "Place ModLoader64 .pak model files in the mods/ folder.";
                }
            })
            .PostFunc([](WidgetInfo& info) {
                if (CVarGetInteger("gMods.PakLoader.Enabled", 0)) {
                    s32 adultIdx = CVarGetInteger("gMods.PakLoader.AdultModel", -1);
                    s32 childIdx = CVarGetInteger("gMods.PakLoader.ChildModel", -1);
                    PakLoader_SelectAdultModel(adultIdx);
                    PakLoader_SelectChildModel(childIdx);
                } else {
                    PakLoader_SelectAdultModel(-1);
                    PakLoader_SelectChildModel(-1);
                }
            })
            .Options(CheckboxOptions().Tooltip("Replaces Link's model with a custom model from a .pak file.\n"
                                               "Place ModLoader64 zzplayas .pak files in the mods/ folder.\n"
                                               "You can choose different models for Adult and Child Link."));

        mSohMenu->AddWidget(path, "Adult Link Model", WIDGET_CVAR_COMBOBOX)
            .CVar("gMods.PakLoader.AdultModel")
            .RaceDisable(false)
            .PreFunc([](WidgetInfo& info) {
                if (PakLoader_GetModelCount() == 0 || !CVarGetInteger("gMods.PakLoader.Enabled", 0)) {
                    info.options->disabled = true;
                }
                // Rebuild the comboMap from current sModels every frame so the
                // dropdown stays in sync if paks load after RegisterNEIMenu ran
                // (lazy init), or if Force* added entries at runtime. Also
                // clamp the CVar to -1 if it points outside the rebuilt map —
                // otherwise Combobox<int>::at() throws std::out_of_range and
                // crashes the renderer.
                auto opt = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
                opt->comboMap.clear();
                opt->comboMap[-1] = "Default Link";
                s32 n = PakLoader_GetModelCount();
                for (s32 i = 0; i < n; i++) {
                    if (PakLoader_ModelHasAdult(i)) {
                        opt->comboMap[i] = PakLoader_GetModelLabel(i);
                    }
                }
                s32 v = CVarGetInteger("gMods.PakLoader.AdultModel", -1);
                if (v >= 0 && !opt->comboMap.count(v)) {
                    CVarSetInteger("gMods.PakLoader.AdultModel", -1);
                }
            })
            .PostFunc([](WidgetInfo& info) {
                if (CVarGetInteger("gMods.PakLoader.Enabled", 0)) {
                    PakLoader_SelectAdultModel(CVarGetInteger("gMods.PakLoader.AdultModel", -1));
                }
            })
            .Options(ComboboxOptions()
                         .ComboMap(adultModelMap)
                         .DefaultIndex(-1)
                         .Tooltip("Choose a custom model for Adult Link."));

        mSohMenu->AddWidget(path, "Child Link Model", WIDGET_CVAR_COMBOBOX)
            .CVar("gMods.PakLoader.ChildModel")
            .RaceDisable(false)
            .PreFunc([](WidgetInfo& info) {
                if (PakLoader_GetModelCount() == 0 || !CVarGetInteger("gMods.PakLoader.Enabled", 0)) {
                    info.options->disabled = true;
                }
                auto opt = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
                opt->comboMap.clear();
                opt->comboMap[-1] = "Default Link";
                s32 n = PakLoader_GetModelCount();
                for (s32 i = 0; i < n; i++) {
                    if (PakLoader_ModelHasChild(i)) {
                        opt->comboMap[i] = PakLoader_GetModelLabel(i);
                    }
                }
                s32 v = CVarGetInteger("gMods.PakLoader.ChildModel", -1);
                if (v >= 0 && !opt->comboMap.count(v)) {
                    CVarSetInteger("gMods.PakLoader.ChildModel", -1);
                }
            })
            .PostFunc([](WidgetInfo& info) {
                if (CVarGetInteger("gMods.PakLoader.Enabled", 0)) {
                    PakLoader_SelectChildModel(CVarGetInteger("gMods.PakLoader.ChildModel", -1));
                }
            })
            .Options(ComboboxOptions()
                         .ComboMap(childModelMap)
                         .DefaultIndex(-1)
                         .Tooltip("Choose a custom model for Child Link."));

        // Equipment Pack lists ANY pak that has at least one equipment DL —
        // dedicated zzequipment paks AND Combined paks (body + equipment) both
        // qualify. Selecting a Combined pak here uses its equipment alias
        // table; the body model side is controlled separately by the Adult /
        // Child dropdowns above.
        std::map<int32_t, const char*> equipModelMap;
        equipModelMap[-1] = "Default Equipment";
        for (s32 i = 0; i < pakCount; i++) {
            if (PakLoader_ModelHasAnyEquipment(i)) {
                equipModelMap[i] = PakLoader_GetModelLabel(i);
            }
        }

        if (equipModelMap.size() > 1) { // More than just "Default"
            mSohMenu->AddWidget(path, "Equipment Pack", WIDGET_CVAR_COMBOBOX)
                .CVar("gMods.PakLoader.Equipment")
                .RaceDisable(false)
                .PreFunc([](WidgetInfo& info) {
                    auto opt = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
                    opt->comboMap.clear();
                    opt->comboMap[-1] = "Default Equipment";
                    s32 n = PakLoader_GetModelCount();
                    for (s32 i = 0; i < n; i++) {
                        if (PakLoader_ModelHasAnyEquipment(i)) {
                            opt->comboMap[i] = PakLoader_GetModelLabel(i);
                        }
                    }
                    s32 v = CVarGetInteger("gMods.PakLoader.Equipment", -1);
                    if (v >= 0 && !opt->comboMap.count(v)) {
                        CVarSetInteger("gMods.PakLoader.Equipment", -1);
                    }
                })
                .PostFunc([](WidgetInfo& info) {
                    // Equipment works independently of the body-model toggle —
                    // pak_loader resolves vanilla fists/hands at draw time so an
                    // equipment-only selection (e.g. just a custom sword) is
                    // valid even when Enable Custom Player Model is off.
                    PakLoader_SelectEquipment(CVarGetInteger("gMods.PakLoader.Equipment", -1));
                })
                .Options(ComboboxOptions()
                             .ComboMap(equipModelMap)
                             .DefaultIndex(-1)
                             .Tooltip("Choose a custom equipment pack.\n"
                                      "Replaces swords, shields, and other items.\n"
                                      "Works on its own — you do NOT have to enable Custom Player Model."));
        }

        // ----- Voice Packs (Z64Online .pak with sounds/<HEX>/*.ogg) -----
        mSohMenu->AddWidget(path, "Custom Link Voice", WIDGET_SEPARATOR_TEXT);

        std::map<int32_t, const char*> voicePackMap;
        voicePackMap[-1] = "None";
        for (s32 i = 0; i < VoicePack_GetCount(); i++) {
            voicePackMap[i] = VoicePack_GetName(i);
        }

        mSohMenu->AddWidget(path, "Enable Custom Voice", WIDGET_CVAR_CHECKBOX)
            .CVar("gMods.VoicePack.Enabled")
            .RaceDisable(false)
            .PreFunc([](WidgetInfo& info) {
                if (VoicePack_GetCount() == 0) {
                    CVarSetInteger("gMods.VoicePack.Enabled", 0);
                    info.options->disabled = true;
                    info.options->disabledTooltip =
                        "No voice packs found.\nPlace Z64Online voice .pak files in the mods/ folder.";
                }
            })
            .PostFunc([](WidgetInfo& info) {
                if (CVarGetInteger("gMods.VoicePack.Enabled", 0)) {
                    VoicePack_Select(CVarGetInteger("gMods.VoicePack.Selection", -1));
                } else {
                    VoicePack_Select(-1);
                }
            })
            .Options(CheckboxOptions().Tooltip(
                "Replaces Link's voice grunts (sword swings, falls, damage, etc.)\n"
                "with samples from a Z64Online-format voice pak.\n"
                "Voice samples play as 2D audio (no positional attenuation)."));

        if (voicePackMap.size() > 1) {
            mSohMenu->AddWidget(path, "Voice Pack", WIDGET_CVAR_COMBOBOX)
                .CVar("gMods.VoicePack.Selection")
                .RaceDisable(false)
                .PreFunc([](WidgetInfo& info) {
                    if (!CVarGetInteger("gMods.VoicePack.Enabled", 0)) {
                        info.options->disabled = true;
                    }
                    auto opt = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
                    opt->comboMap.clear();
                    opt->comboMap[-1] = "None";
                    s32 n = VoicePack_GetCount();
                    for (s32 i = 0; i < n; i++) {
                        const char* nm = VoicePack_GetName(i);
                        opt->comboMap[i] = nm ? nm : "(unnamed)";
                    }
                    s32 v = CVarGetInteger("gMods.VoicePack.Selection", -1);
                    if (v >= 0 && !opt->comboMap.count(v)) {
                        CVarSetInteger("gMods.VoicePack.Selection", -1);
                    }
                })
                .PostFunc([](WidgetInfo& info) {
                    if (CVarGetInteger("gMods.VoicePack.Enabled", 0)) {
                        VoicePack_Select(CVarGetInteger("gMods.VoicePack.Selection", -1));
                    }
                })
                .Options(ComboboxOptions()
                             .ComboMap(voicePackMap)
                             .DefaultIndex(-1)
                             .Tooltip("Choose a voice pack.\n"
                                      "Selecting a pack decodes its OGG samples (lazy, ~one-time cost)."));

            mSohMenu->AddWidget(path, "Voice Pack Volume", WIDGET_CVAR_SLIDER_FLOAT)
                .CVar("gMods.VoicePack.Volume")
                .RaceDisable(false)
                .PreFunc([](WidgetInfo& info) {
                    if (!CVarGetInteger("gMods.VoicePack.Enabled", 0)) {
                        info.options->disabled = true;
                    }
                })
                .Options(FloatSliderOptions()
                             .Tooltip("Mix gain for voice pack samples.")
                             .Min(0.0f)
                             .Max(2.0f)
                             .DefaultValue(1.0f)
                             .IsPercentage());
        }
    }

    // ===================== Tab: Randomizer =====================
    path.sidebarName = "Randomizer";
    path.column = SECTION_COLUMN_1;

    // ---- Fleet Ship Combo: live MM <-> OoT switch ----
    // CVar namespace (matches user's "isFleetShipCombo[X]"):
    //   isFleetShipCombo.Enabled - master toggle for the combo / launcher bootstrap
    //   isPlayerIn2Ship          - persistent "where is the player": 1 = Majora's Mask
    //                              (2ship), 0 = Ocarina of Time (Ship). Single source of
    //                              truth for which game is active; also drives auto-start
    //                              (the host resumes the game the player was last in).
    // For now this button just flips and persists isPlayerIn2Ship. Frente B wires the
    // actual seamless hand-off: pause the inactive game's SESSION (no logic/audio/render,
    // process stays alive) + cross-process shared-texture compositing.
    mSohMenu->AddWidget(path, "Fleet Ship Combo (MM <-> OoT)", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Enable Fleet Ship Combo", WIDGET_CVAR_CHECKBOX)
        .CVar("isFleetShipCombo.Enabled")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Master switch for running OoT (Ship) and MM (2ship) together.\n\n"
            "RESTART REQUIRED after toggling: the second game is launched at boot.\n"
            "Place 2ship.exe in a '2ship' folder next to soh.exe (Ship/2ship/2ship.exe)."));

    // Live status so you always know which game you're in (and whether the combo is up).
    mSohMenu->AddWidget(path, "Combo Status", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        int32_t active = FleetShipCombo_GetActiveGame();
        if (active < 0) {
            ImGui::TextColored(ImVec4(0.95f, 0.7f, 0.3f, 1.0f), "Combo NOT running (single game).");
            ImGui::TextWrapped("Enable above + restart, and put 2ship.exe under Ship/2ship/.");
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "Active game: %s",
                               active == 1 ? "Majora's Mask (2ship)" : "Ocarina of Time (Ship)");
        }
    });

    mSohMenu->AddWidget(path, "Switch Active Game", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            // Use shared memory as the source of truth for the CURRENT active game so
            // the toggle stays correct no matter which process last switched.
            int32_t cur = FleetShipCombo_GetActiveGame();
            if (cur < 0) {
                // No shared region -> the combo isn't actually running two games.
                Notification::Emit({
                    .message = "Combo not running - enable it and restart (need 2ship.exe under Ship/2ship/).",
                });
                return;
            }
            int32_t next = cur ? 0 : 1; // 1 = MM (2ship), 0 = OoT (Ship)
            CVarSetInteger("isPlayerIn2Ship", next);
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            // Publish to shared memory so BOTH processes freeze/unfreeze + show/hide in sync.
            FleetShipCombo_SetActiveGame(next);
            FleetShipCombo_SetUiFocus(next); // front window follows the newly-active game
            SPDLOG_INFO("[FleetShipCombo] isPlayerIn2Ship -> {}", next ? "Majora's Mask (2ship)" : "Ocarina of Time (Ship)");
            Notification::Emit({
                .message = next ? "Switching to Majora's Mask..." : "Switching to Ocarina of Time...",
            });
        })
        .Options(ButtonOptions().Tooltip(
            "Toggle the active game between Ocarina of Time and Majora's Mask at any time.\n"
            "Both games stay loaded and running; the inactive one's session is paused (seamless).\n\n"
            "Persists 'isPlayerIn2Ship' so the combo remembers which game you're in and can\n"
            "auto-start there next launch.\n\n"
            "NOTE: the runtime hand-off (session pause + cross-process compositing) is still\n"
            "being wired up. For now this just flips and remembers the active game."));

    mSohMenu->AddWidget(path, "Show 2ship UI overlay (trackers)", WIDGET_CVAR_CHECKBOX)
        .CVar("gFleetCombo.ShowMmUiOverlay")
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Draws 2ship's floating UI windows (Check Tracker, etc.) on top of the game,\n"
            "no matter which game is active — so both games' trackers can be open at once.\n\n"
            "The overlay is view-only (an image): to CLICK 2ship's UI, use the\n"
            "'2 Ship 2 Harkinian' tab at the top of this menu.\n"
            "Open/close the windows themselves from 2ship's own menu.\n\n"
            "2ship-side publishing can be disabled with gFleetShipCombo.UiOverlay = 0\n"
            "(also restores the full render-skip for the inactive game)."));

    // The "View" game switcher (Ship / 2Ship / Shared) moved to the menu's top tab row
    // (DrawFleetShipComboTabs in Menu.cpp) so it's the same in both apps.

    mSohMenu->AddWidget(path, "Randomizer (seed-locked)", WIDGET_SEPARATOR_TEXT);

    mSohMenu->AddWidget(path, "Enable Custom Items", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("SkijerCustomItems"))
        .PostFunc([](WidgetInfo& info) {
            // Mirror to the runtime CVar that gates page-2 visibility in the pause menu.
            CVarSetInteger("gMods.CustomItems.Enabled",
                           CVarGetInteger(CVAR_RANDOMIZER_SETTING("SkijerCustomItems"), 0));
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(CheckboxOptions().Tooltip("Enables the 24 custom items on the second inventory page (seed-locked rando setting).\n"
                                           "When enabled, these items are also added to the randomizer pool and gated logic paths.\n"
                                           "When disabled, page 2 is inaccessible and items are not in rando.\n"
                                           "Synced with the same setting in the Randomizer menu."));

    mSohMenu->AddWidget(path, "Add All MM Masks to Rando", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("MmMasksAll"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                CVarSetInteger(CVAR_RANDOMIZER_SETTING("MmMasksAll"), 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.";
            } else if (!CVarGetInteger("gMods.MmMasks.InventoryEnabled", 0)) {
                CVarSetInteger(CVAR_RANDOMIZER_SETTING("MmMasksAll"), 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Enable 'Include MM Masks' first.";
            }
        })
        .PostFunc([](WidgetInfo& info) {
            if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("MmMasksAll"), 0)) {
                CVarSetInteger(CVAR_RANDOMIZER_SETTING("MmMasksTransform"), 0);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
        })
        .Options(CheckboxOptions().Tooltip("Adds all 24 MM masks to the randomizer item pool.\n"
                                           "Masks can be found at random locations like custom items.\n"
                                           "Removes OOT Goron/Zora masks from pool.\n\n"
                                           "Seed-locked rando setting.\n"
                                           "REQUIRES: 'Include MM Masks' enabled"));

    mSohMenu->AddWidget(path, "Add Transformation Masks to Rando", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("MmMasksTransform"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!MmAssets_IsAvailable()) {
                CVarSetInteger(CVAR_RANDOMIZER_SETTING("MmMasksTransform"), 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Requires mm.o2r from 2Ship2Harkinian Keiichi Alfa 4.0.0.";
            } else if (!CVarGetInteger("gMods.MmMasks.InventoryEnabled", 0)) {
                CVarSetInteger(CVAR_RANDOMIZER_SETTING("MmMasksTransform"), 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "Enable 'Include MM Masks' first.";
            } else if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("MmMasksAll"), 0)) {
                CVarSetInteger(CVAR_RANDOMIZER_SETTING("MmMasksTransform"), 0);
                info.options->disabled = true;
                info.options->disabledTooltip = "'Add All MM Masks to Rando' already includes transformation masks.";
            }
        })
        .Options(CheckboxOptions().Tooltip("Adds only the 4 transformation masks (Deku, Goron, Zora, Fierce Deity)\n"
                                           "to the randomizer item pool.\n"
                                           "Removes OOT Goron/Zora masks from pool.\n\n"
                                           "Seed-locked rando setting.\n"
                                           "REQUIRES: 'Include MM Masks' enabled"));

    mSohMenu->AddWidget(path, "Add Extended Equipment to Rando", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("ExtEquipment"))
        .RaceDisable(false)
        .PostFunc([](WidgetInfo& info) {
            CVarSetInteger("gCheats.ExtEquip.Enabled",
                           CVarGetInteger(CVAR_RANDOMIZER_SETTING("ExtEquipment"), 0));
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(CheckboxOptions().Tooltip(
            "Adds the 12 extended equipment pieces (3 swords, 3 shields, 3 tunics, 3 boots) to the randomizer pool.\n"
            "Press L on the equipment page to toggle between vanilla and extended equipment.\n\n"
            "Seed-locked rando setting (also enables the in-game equipment system)."));

    mSohMenu->AddWidget(path, "Add NEI Weapon Upgrades to Rando", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("NeiWeaponUpgrades"))
        .RaceDisable(false)
        .PostFunc([](WidgetInfo& info) {
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(CheckboxOptions().Tooltip(
            "Adds NEI weapon upgrades to the randomizer pool. Each requires its base weapon:\n"
            "  - Hammer Upgrade (Iron Knuckle's Axe): double damage/reach + tomahawk throw\n"
            "  - Kokiri Sword Upgrade x2: Razor Sword, then Gilded Sword\n"
            "  - True Master Sword (Master Sword)\n"
            "  - Great Fairy's Sword (Biggoron Sword)\n\n"
            "Only the Hammer upgrade has gameplay behavior for now.\n"
            "Seed-locked rando setting."));

    mSohMenu->AddWidget(path, "Bomb Arrows: Auto-grant with Bomb Bag", WIDGET_CVAR_CHECKBOX)
        .CVar("gMods.BombArrows.AutoGrantOnBag")
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Cheat: automatically gives ITEM_BOMB_ARROWS the moment you obtain any bomb bag.\n"
            "When on, the new bow/slingshot arrow wheel will show a Bomb entry as soon as the\n"
            "bag is yours. Has no effect on the randomizer item pool."));

    // ----- Equipment Mix (same Pak Loader tab; uses columns 1-3) -----
    // Per-slot equipment override:
    //   1. Swords + Shields (6 dropdowns)
    //   2. Ranged + tools + ocarinas + boots + gauntlets + bracelet (13)
    //   3. Child masks (8)
    path.sidebarName = "Pak Loader";
    path.column = SECTION_COLUMN_1;

    mSohMenu->AddWidget(path, "Per-slot equipment override", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path,
              "Each slot can pull from a different pak. 'Default' inherits from the main "
              "Equipment Pack dropdown (or vanilla if no pack selected). Sheathed and "
              "unsheathed pieces always come from the same source pak so the look stays "
              "consistent.",
              WIDGET_TEXT);

    mSohMenu->AddWidget(path, "Reset all slots to Default", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) {
            s32 n = PakLoader_GetSlotCount();
            char cvarName[96];
            for (s32 i = 0; i < n; i++) {
                snprintf(cvarName, sizeof(cvarName),
                         "gMods.PakLoader.SlotMix.%s", PakLoader_GetSlotKey(i));
                CVarSetInteger(cvarName, -1);
                PakLoader_SetSlotMix(i, -1);
            }
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(ButtonOptions().Size(Sizes::Inline)
                                .Tooltip("Clear every per-slot override at once."));

    // Cached CVar-name strings per slot so the widgets get stable c_str() pointers.
    // RegisterNEIMenu is called once at boot, so static storage is fine.
    static std::vector<std::string> slotCVarNames;
    {
        s32 n = PakLoader_GetSlotCount();
        slotCVarNames.clear();
        slotCVarNames.reserve(n);
        for (s32 i = 0; i < n; i++) {
            slotCVarNames.emplace_back(std::string("gMods.PakLoader.SlotMix.") +
                                       PakLoader_GetSlotKey(i));
        }
    }

    // Slot-grouping for placement across the three columns. Indices must match
    // sSlotGroups[] order in pak_loader.cpp.
    //  0: Sword0 (Kokiri)        1: Sword1 (Master)      2: Sword2 (Biggoron)
    //  3: Shield0 (Deku)         4: Shield1 (Hylian)     5: Shield2 (Mirror)
    //  6: Bow                    7: Hookshot             8: Slingshot
    //  9: Boomerang             10: Hammer              11: DekuStick
    // 12: Bottle                13: OcarinaFairy        14: OcarinaTime
    // 15: IronBoots             16: HoverBoots          17: Gauntlets
    // 18: Bracelet              19: MaskSkull           20: MaskSpooky
    // 21: MaskKeaton            22: MaskTruth           23: MaskGoron
    // 24: MaskZora              25: MaskGerudo          26: MaskBunny

    auto addSlotWidget = [&path](s32 slotIdx) {
        const char* label = PakLoader_GetSlotLabel(slotIdx);
        const char* cvarName = slotCVarNames[slotIdx].c_str();

        mSohMenu->AddWidget(path, label, WIDGET_CVAR_COMBOBOX)
            .CVar(cvarName)
            .RaceDisable(false)
            .PreFunc([slotIdx](WidgetInfo& info) {
                // Rebuild every frame so newly-loaded paks appear immediately
                // and removed paks disappear without a restart. Clamp the CVar
                // to -1 if it ends up pointing at a slot the chosen pak no
                // longer satisfies — defends against std::map::at out_of_range
                // crashes in Combobox<int>.
                auto opt = std::static_pointer_cast<UIWidgets::ComboboxOptions>(info.options);
                opt->comboMap.clear();
                opt->comboMap[-1] = "Default (inherit)";
                s32 n = PakLoader_GetModelCount();
                for (s32 i = 0; i < n; i++) {
                    if (PakLoader_PakProvidesSlot(i, slotIdx)) {
                        opt->comboMap[i] = PakLoader_GetModelLabel(i);
                    }
                }
                char cvarName[96];
                snprintf(cvarName, sizeof(cvarName),
                         "gMods.PakLoader.SlotMix.%s", PakLoader_GetSlotKey(slotIdx));
                s32 v = CVarGetInteger(cvarName, -1);
                if (v >= 0 && !opt->comboMap.count(v)) {
                    CVarSetInteger(cvarName, -1);
                }
            })
            .PostFunc([slotIdx](WidgetInfo& info) {
                char cvarName[96];
                snprintf(cvarName, sizeof(cvarName),
                         "gMods.PakLoader.SlotMix.%s", PakLoader_GetSlotKey(slotIdx));
                PakLoader_SetSlotMix(slotIdx, CVarGetInteger(cvarName, -1));
            })
            .Options(ComboboxOptions()
                         .DefaultIndex(-1)
                         .Tooltip("Pak that provides this piece. 'Default' = inherit "
                                  "from the Equipment Pack dropdown."));
    };

    // Column 1 — Swords (0..2) + Shields (3..5)
    mSohMenu->AddWidget(path, "Swords", WIDGET_SEPARATOR_TEXT);
    for (s32 i = 0; i <= 2; i++) addSlotWidget(i);
    mSohMenu->AddWidget(path, "Shields", WIDGET_SEPARATOR_TEXT);
    for (s32 i = 3; i <= 5; i++) addSlotWidget(i);

    // Column 2 — Ranged + Tools + Boots + Gauntlets + Bracelet (6..18)
    path.column = SECTION_COLUMN_2;
    mSohMenu->AddWidget(path, "Ranged & Tools", WIDGET_SEPARATOR_TEXT);
    for (s32 i = 6; i <= 11; i++) addSlotWidget(i);   // Bow..DekuStick
    mSohMenu->AddWidget(path, "Items", WIDGET_SEPARATOR_TEXT);
    addSlotWidget(12); // Bottle
    addSlotWidget(13); // OcarinaFairy
    addSlotWidget(14); // OcarinaTime
    mSohMenu->AddWidget(path, "Boots & Gauntlets", WIDGET_SEPARATOR_TEXT);
    addSlotWidget(15); // IronBoots
    addSlotWidget(16); // HoverBoots
    addSlotWidget(17); // Gauntlets
    addSlotWidget(18); // Bracelet

    // Column 3 — Child masks (19..26)
    path.column = SECTION_COLUMN_3;
    mSohMenu->AddWidget(path, "Child Masks", WIDGET_SEPARATOR_TEXT);
    for (s32 i = 19; i < PakLoader_GetSlotCount(); i++) addSlotWidget(i);

    // ===================== Tab: Controls =====================
    path.sidebarName = "Controls";
    path.column = SECTION_COLUMN_1;

    mSohMenu->AddWidget(path, "Pause Menu", WIDGET_SEPARATOR_TEXT);

    // This dropdown picks the button used to change page WITHIN a kaleido page
    // (inventory sub-page, extended equipment, SW97 arrow mode, Broken Modes).
    // The OTHER shoulder button (+ R) changes BETWEEN kaleido pages.
    // Backed by NGCKaleidoSwitcher: 0 = in-page L (pages on Z), 1 = in-page Z (pages on L).
    // NOTE: each label MUST be >1 char or UIWidgets::Combobox hides it (it skips
    // single-character entries), which is why these aren't just "L"/"Z".
    static std::map<int32_t, const char*> inPageBtnMap = {
        { 0, "L button (default)" },
        { 1, "Z button" },
    };
    mSohMenu->AddWidget(path, "In-Page Change Button (L / Z)", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("NGCKaleidoSwitcher"))
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .ComboMap(inPageBtnMap)
                     .DefaultIndex(0)
                     .Tooltip("Which shoulder button changes page WITHIN the current kaleido page:\n"
                              "inventory sub-page (Items), extended equipment (Equipment), SW97 arrow mode\n"
                              "(Quest), and Broken Modes (Map). The OTHER shoulder button, together with R,\n"
                              "changes BETWEEN the kaleido pages.\n"
                              "L button (default): L changes within the page, Z + R change kaleido pages.\n"
                              "Z button: Z changes within the page, L + R change kaleido pages."));

    mSohMenu->AddWidget(path, "Equip Items on D-Pad", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("DpadEquips"))
        .Options(CheckboxOptions().Tooltip("Allow equipping items to the D-Pad directions."));
    mSohMenu->AddWidget(path, "D-Pad on Pause", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("DPadOnPause"))
        .Options(CheckboxOptions().Tooltip("Use the D-Pad to navigate the pause menu."));

    mSohMenu->AddWidget(path, "Camera", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path, "Free Camera", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("FreeLook.Enabled"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Enables free camera control (right stick / mouse).\n"
            "Same setting as Settings > Controls > Free Look — surfaced here for convenience."));

    mSohMenu->AddWidget(path, "Transformation Controls", WIDGET_SEPARATOR_TEXT);
    mSohMenu->AddWidget(path, "Mario Controls", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) { /* placeholder — SM64 Mario rebinding UI coming soon */ })
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip(
            "Rebind SM64 Mario mode controls. (Placeholder — per-move bindings coming in a future update.)"));
    // Pikachu Controls — opens a dedicated assignment window (pikachu_hud.cpp):
    // per-move N64 button binds (gPikaBind.*) + the mode UI style. Applies to
    // the SECRET Broken-Modes Pikachu mode only; the pokeball transformation
    // keeps items on C and the vanilla UI.
    mSohMenu->AddWidget(path, "Pikachu Controls", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) { PikachuControls_OpenWindow(); })
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip(
            "Open the Pikachu mode controls window: assign the N64 button for each move\n"
            "(Jump / Quick Attack / Grass / Gigantamax / Iron Tail / Dark / Sleep) and pick\n"
            "the mode UI style (icons over OOT buttons, or the corner HUD).\n"
            "Secret Broken-Modes Pikachu mode only — the pokeball transformation is untouched."));
}

// Self-register the NEI menu via the same RegisterMenuInitFunc path every other
// module uses (Anchor, Harpoon, Resolution...). SohMenu::AddMenuElements() drains
// MenuInit::GetInitFuncs() at boot, so this replaces the old upstream edits to
// SohMenu.h (member decl) and SohMenu.cpp (explicit AddMenuNEI() call).
static RegisterMenuInitFunc neiMenuInitFunc(RegisterNEIMenu);

} // namespace SohGui
