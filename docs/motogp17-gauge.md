# MotoGP 17 radial gauge adaptation

The first optional replacement uses the supplied PC game's original
`HUD_GAUGE.MWF`, `GFX_RACE.LIB`/DDS atlas and Geogrotesque bitmap fonts.
It preserves the authored bitmap artwork, transforms, color transforms and
68-frame RPM/8-frame gear masks. Concave masks are triangulated without
replacing their outlines. This is a resource adaptation, not transplanted
MotoGP 17 executable code or a recreated dial image.

## Native selection and bundled assets

The selector is the last row in **Options → Display Settings**, after
Brightness / Gamma when that native row is present:

`Gauge Style / Gösterge stili: Original / Orijinal | MotoGP 17 | MotoGP 06 | MotoGP URT 2 | MotoGP URT 3`

Classic styles and their conversion/calibration are documented in
[classic-gauges.md](classic-gauges.md). Choices are filtered independently by
successful asset loading.

The selector is available only after its complete asset set has loaded and GPU
textures have been created. Missing/corrupt assets retain the original HUD.
Selection is published atomically by native callbacks; the UI thread persists
it in `hud-display.toml` under the existing user data root. No original profile
field or launcher option is changed. A corrupt preference file is preserved.

The application includes its self-contained converted resource at:

`resources/hud/motogp17/gauge.json`

This JSON embeds the original converted PNG bytes and is self-contained; its
numbered PNG siblings are optional inspection outputs. The loader also accepts
the earlier split-file layout. The user explicitly approved bundling this JSON;
it is tracked in source, copied beside local build outputs by CMake, and included
by the shared local/Actions Windows packager. The runtime resolves it relative
to the portable application root, independently of the selected game directory.
No manual copy into the original game directory is required. Raw donor archives
and extraction intermediates remain under ignored local `assets/`/`out/`.

`scripts/extract-motogp17-gauge.ps1` reads the observed MIX variant without
modifying it. It decodes names using the tool's verified XOR rule, extracts
selected raw-DEFLATE blocks with size checks, and records inventory and hashes.
The default filter selects gauge/GFX_Race resources from `gfxpak.mix`.
For `menu.mix`, additionally export `HUD_GAUGE.MWF` and the `.BML`/`_0.DDS`
pairs for `GEOGROTESQUE_REGULAR61` and `GEOGROTESQUE_LIGHT40`.

After extraction, convert those original resources with Pillow available:

```bash
python scripts/import-motogp17-gauge.py \
  --menu-dir PRIVATE_MENU_EXPORT/MENU \
  --gfxpak-dir PRIVATE_GFXPAK_EXPORT/GFXPAK \
  --output PRIVATE_CONVERTED_EXPORT
```

The output directory must be new. Original inputs and existing exports are
preserved. The JSON records source hashes for the movie, atlas/library and
font assets. The approved converted JSON is then maintained at the bundled
resource path above. No original game executable is required by the converter.

## Native telemetry and rendering boundary

`gauge_capture.cpp` retains the original filled-gauge callback `823191C8`,
including its visibility, viewport and matrix work. A thread-local scope
suppresses only children `82318CB0` and `82318248`, after a complete validated
sample and usable replacement have been established. Other uses of the child
functions retain native behavior.

| Native source | Meaning |
| --- | --- |
| `view + 140` | Game bike pointer |
| `bike + 556` | Current engine RPM mirrored by the native physics bridge |
| `bike + 560` | Current gear; neutral is rendered as N |
| `bike + 564` | Native displayed MPH, including the game's original speedometer adjustment |
| `bike + 1472` → physics `+ 644 / + 648` | This bike's actual minimum/maximum engine RPM |
| Profile `+ 0x9647C` | Original MPH/KPH preference; native KPH conversion/truncation is preserved |
| Profile `+ 0x96480` and native draw invocation | Original HUD visibility authority |
| View `+ 56 / 60 / 64 / 68` | Normalized per-player view rectangle |
| Native input cache `0x833741B8 + 88 × user`, bytes `+2 / +3` | Actual LT (front brake) / RT (throttle) pressure, normalized by 255 |
| Same input cache, word `+72` | Last native input query succeeded; disconnected inputs leave the bars empty |

Pointers and memory protection, finite values, calibrated engine limits, gear
range and view bounds are checked before replacing any child. Invalid data
falls back to native rendering. No RPM is estimated from speed, and there is no
fixed 18,000-RPM assumption. The unnumbered donor sweep maps zero to the actual
engine maximum. Existing game physics are not changed.

At the existing frame observer before guest presentation, up to two samples
are published by value and staging is cleared. A frame without a native draw
publishes no gauge, preventing stale data in menus. The UI never reads guest
pointers. F1/F2/Rider License/quit input ownership disables the custom layer.
The native active screen stack also gates capture and presentation: Pause,
Time Trial restart/configure/quit, continue/results, replay/spectate, settings
and unknown popups suppress the custom gauge even while old race data survives.
Only the verified StartingLights and ArcadeBonusTime HUD screens are allowed;
slot 3 is excluded exactly as in native GetScreen(8). Attract mode is excluded.
The presentation check discards samples staged before a menu opened that frame.
The native crash movement counter at `gameBike+0x278` also gates capture and
presentation: a positive value hides the replacement until remount clears it.
The original viewport path remains intact, and each accepted split-screen view
has its own copied telemetry and scaled bottom-right placement. The face is
restored to its original 280 reference pixels, at (1519,727) on 1920×1080,
with 121-pixel right and 73-pixel bottom insets.

The adjacent native vertical indicator is `CleanSection` (vtable `8206FC68`,
draw `82317380`), which displays clean-section progression/feedback used by
Arcade time extensions and career reputation scoring. While a custom gauge
is selected with usable telemetry, its native UI transform is translated upward
by 160 reference pixels. The scope is thread-local to this draw callback;
other HUD elements and menus do not move. Per-player viewport height scales
the offset. At native matrix upload `8236E858` (c4), `y_clip += lift * w_clip`
preserves size and X/Z/W while moving the complete indicator together. The
input matrix bits are restored after upload; the original GPU transform is
republished when the CleanSection draw returns so following HUD sprites can
reuse it. No gameplay/scoring or shared tweak
property is modified. Original HUD selection retains the original placement.

## Display Settings capacity

The original list builder `822390B0` emits IDs 0–3, at most one of 4/5, ID6
and optional ID7 (Gamma): at most seven settings. Custom ID8 and a terminator
therefore fit the existing nine ID/row slots. Rows start at `+68`; metadata at
`+428` and the 760-byte object are not enlarged or overwritten.

`options_menu.cpp` uses the original native type-4 choice widget and
keyboard/controller navigation. Its available direct-string options have both
leading and trailing sentinel records, because native decrement reads the
preceding record before rejecting a negative index. The exact custom label
handle is intercepted; other message banks and original actions are preserved.
The original native commits skip ID8. Game Settings retains its existing
Storage cleanup and receives no new gauge row.

## Scope and verification

This style supplies current speed, original units, RPM sweep, current gear,
adjacent gear labels and gear progress from verified native data. Original
17-frame green throttle and red front-brake animations use the native per-user
input cache populated by `8236D7C0` from `XamInputGetState` via `8239DE10`.
These show the input pressure delivered to the game, including keyboard mapping;
they do not represent physics force or a post-assistance control output. Rear
brake is a separate action and is not combined with LT. No extra device polling
or input mutation is introduced. Older packs without these optional animation
arrays retain their original empty outline. Temperature/damage states and the
limiter light remain inactive without corresponding verified telemetry.

The optional original tyre panel now binds independently validated front/rear
09/10 tyre-friction health. Native tyre visibility and wear-enabled gates are
retained; only a loaded panel with a native per-view replacement handshake
draws in the host overlay. Otherwise the original small motorcycle/tyre HUD
remains available and follows the same upward translation as the distinct
tall CleanSection widget, preserving their original relative position.
See [classic-gauges.md](classic-gauges.md#crash-visibility-and-tyre-hud)
for the native state/curve evidence and original donor layer order.

The original body rectangle now supplies a fixed RGB 128 neutral gray through
the bitmap's motorcycle cutout. Two opaque RGB 60 covers match the panel backing
and hide the baked thermometer cutouts, including their bright edges. These
decorative layers do not claim damage or temperature telemetry.

The donor `HUD_GAUGE.MWF` also contains root sprite 166, depth 11 `MOV_Led`,
sprite 81 / shape 80 using `GaugeRedBkg`. Its single authored frame supplies
the red center artwork, but contains no activation condition. The supplied
resource files do not establish the donor RPM threshold, limiter signal or
blink/hysteresis behavior. The layer remains inactive pending verified trigger
evidence; a redline rule is not inferred from the screenshot alone.

Focused Windows/Wine checks exercise the actual generated native selector,
forward/reverse navigation, bounded choices, conditional Gamma/Ghost variants,
object canaries, copied telemetry and fallback paths. A separate fixture uses
the production image loader, bitmap fonts, ImGui meshes and preference save/
reload at neutral, changing gears, RPM limits, imperial/metric, split-screen,
ultrawide and absent samples. Captures are CPU-rasterized from actual draw data;
they are not native Windows GPU race evidence. Original extraction outputs were
independently compared byte-for-byte by SHA-256.

No application build, commit, push or CI dispatch is implied by those probes.
Live race transitions, physical controllers and native Windows GPU appearance/
cost still require in-game acceptance.

REA provenance is retained under ignored `out/analysis/motogp17-gauge`:

- Uploaded archive inventory: `ev_4baf2713582e5a7226f920cfc700ca976c51a2a29b1e872f330dce4518bd0eef`.
- ReMixer managed identity: `ev_f712e572f1ab8693fe5e715246c83d771f0e557ab9f1fab1cd53b78ab9046f65`.
- ReMixer member/CIL inventory: `ev_ece94bb49ac9cc27cf2d984e36149fa8fe310903402d1c725737278427221b66`.
- TweakerBase name-decoder CIL: `ev_16bb42e7fb0262aa5569ffc95cd04b8b231e6541b92f3be875e6cb8711b9076b`.

MIX/MWF/BML parsing, conversion and generated PPC inspection are separately
recorded ordinary tooling observations. Static analysis is not runtime
observation; no native REA analysis session was opened and left running.
