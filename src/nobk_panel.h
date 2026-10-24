#pragma once
// NOBK Online Apex overlay (F1, or Back + Start on a controller): friends,
// server, records and achievements while playing Online. Public site data
// plus community-authenticated invitations; no launcher token/password is used.
#include <memory>
#include <rex/ui/imgui_dialog.h>

struct ImFont;
struct ImFontAtlas;
namespace rex { class Runtime; }
namespace rex::system { class AchievementManager; }
namespace rex::ui { class Window; class ImmediateDrawer; }
namespace motogp::achievements { struct Preferences; }
namespace motogp::settings { class InputGate; }

namespace motogp::nobk {
void ConfigureFonts(ImFontAtlas* atlas,const char* regular,const char* semibold,const char* bold);
void Initialize(rex::Runtime* runtime, std::shared_ptr<settings::InputGate> gate);
void Shutdown();
bool Active();
#ifdef MOTOGP_NOBK_VISUAL_PROBE
// Isolated presentation fixture; absent from application builds.
void SetVisualFixture(bool enabled,bool reference=false);
#endif
// Called once per game frame (guest thread): Back + Start held opens the panel.
void PollPad();
std::unique_ptr<rex::ui::ImGuiDialog> CreateMenu(rex::ui::ImGuiDrawer* drawer, rex::ui::Window* window,
    rex::system::AchievementManager* achievements, ImFont* font, ImFont* heading, bool turkish,
    rex::ui::ImmediateDrawer* immediate, rex::Runtime* runtime,
    std::shared_ptr<motogp::achievements::Preferences> preferences);
}
