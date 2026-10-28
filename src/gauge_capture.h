#pragma once

#include <array>
#include <optional>
#include <cstdint>

struct PPCContext;
namespace motogp::gauge {
struct Telemetry {
  // The original HUD's displayed MPH value, including its native speedometer
  // adjustment. This is not an unmodified physics velocity.
  float mph=0,rpm=0,min_rpm=0,max_rpm=0;
  int gear=0;
  bool metric=false;
  // Normalized native view rectangle, before the native HUD aspect adjustment.
  float view_x=0,view_y=0,view_width=1,view_height=1;
  unsigned view_index=0;
  // Actual normalized LT/RT input delivered to this native local user.
  // These are input pressure, not estimated engine/braking force.
  float throttle=0,front_brake=0;
  bool inputs_valid=false;
  // Actual condition normalized by the native tyre HUD's friction threshold.
  // Optional: invalid tyre data must not disable an otherwise valid gauge.
  float front_tyre=1,rear_tyre=1;
  bool tyres_valid=false,tyres_colored=false,tyre_hud_visible=false;
  // True only if this frame's native tyre draw was replaced by the donor panel.
  bool tyre_replaced=false;
};
using TelemetryFrame=std::array<std::optional<Telemetry>,2>;

// UI thread: enable only while the selected replacement's GPU assets are
// usable. Disabling immediately clears both pending and published samples.
void SetCaptureEnabled(bool enabled);
// Guest presentation boundary: publish this frame and clear the next staging
// frame. A native frame with no gauge draws or with a native menu publishes
// an empty frame, including a menu opened after capture in the same frame.
void PublishFrame();
// UI thread: an owning value copy. No guest memory is accessed here.
TelemetryFrame ReadFrame();
void ResetCapture();
#ifdef MOTOGP_GAUGE_PROBE
// Fixture access to the actual read/staging boundaries; absent from the game.
std::optional<Telemetry> CaptureForProbe(uint8_t* base,uint32_t view);
bool StageForProbe(uint8_t* base,uint32_t view);
using NativeDraw=void(*)(PPCContext&,uint8_t*);
void DrawCleanSectionForProbe(PPCContext& context,uint8_t* base,uint32_t view,NativeDraw draw,NativeDraw restore_upload);
bool DrawTyreHudForProbe(PPCContext& context,uint8_t* base,uint32_t view,NativeDraw draw,NativeDraw restore_upload);
void UploadHudTransformForProbe(PPCContext& context,uint8_t* base,NativeDraw upload);
#endif
} // namespace motogp::gauge
