#pragma once

#include <memory>
#include <rex/ui/imgui_dialog.h>

struct ImFont;
namespace rex { class Runtime; }
namespace rex::ui { class Window; class ImmediateDrawer; }
namespace motogp::settings { class InputGate; }

namespace motogp::gamer_card {
// The native Gamer Card import supplies a selected XUID, not a user index or
// a row number. No guest pointer is retained by the asynchronous lookup.
void Initialize(rex::Runtime* runtime, std::shared_ptr<settings::InputGate> gate);
void Shutdown();
bool Active();
std::unique_ptr<rex::ui::ImGuiDialog> CreateMenu(
    rex::ui::ImGuiDrawer* drawer, rex::ui::Window* window, rex::ui::ImmediateDrawer* immediate,
    ImFont* font, ImFont* heading, bool turkish);
}
