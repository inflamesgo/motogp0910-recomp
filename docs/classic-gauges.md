# Classic analog gauge adaptations

The native **Options → Display Settings → Gauge Style** row selects Original,
MotoGP 17, MotoGP 06, MotoGP URT 2 or MotoGP URT 3. Each custom choice appears only after its
own complete resource has decoded and its GPU textures have been created.
One missing pack does not disable other working styles. Selection persists in
the existing user-data `hud-display.toml`, independently of original saves.

Converted resources are self-contained files under
`resources/hud/<style>/gauge.json`, resolved from the portable application root.
CMake and the shared Windows packager include them automatically. No donor
archive, executable or extra file belongs in `game/`. Raw uploads and analysis
remain in ignored `out/analysis/`; only the explicitly requested converted
gauge artwork and reproducible conversion scripts are distributed.

`gauge_classic.cpp` renders original extracted sprites and bitmap digits. It
uses the same validated, copied gameplay telemetry and native visibility gates
as MotoGP 17. Menus, replay, missing samples and invalid native pointers retain
the established fallback behavior. Speed uses the original 09/10 displayed MPH
and its existing KPH conversion/truncation; gear is 0 through 6. These classic
faces use original numeric 0 for neutral. They do not inherit MotoGP 17's N.

Faces occupy 280 reference pixels vertically in each player's lower-right
viewport, with 120-pixel right and 73-pixel bottom margins at 1920×1080. Donor
transparent padding is accounted for separately. Split-screen and ultrawide
views retain their own clipped placement. The existing CleanSection indicator
translation applies while any usable replacement is selected.

## MotoGP 06

Inputs are the supplied Xbox 360 `overlays.ark` and `frontend.ark`. Extraction
reads the archives' own big-endian record tables and bounded LZSS payloads;
`index.xml` is not needed at runtime or by the converter.

```bash
python scripts/import-motogp06-gauge.py \
  --overlays PRIVATE_DONOR/overlays.ark \
  --frontend PRIVATE_DONOR/frontend.ark \
  --output PRIVATE_EXPORT/gauge.json
```

The converter verifies the exact supported decoded record identities and keeps
the original face, needle pixels and digital glyph pixels. `speedo.tpage`
record `51b4189d` uses linear BC3 with 16-bit endian swapping. The font atlas
`ae08fec6` uses linear A4R4G4B4; font map `1430aeb0` resolves U+0185..U+018E to
glyphs 357..366. Only these ten glyphs are repacked with transparent gutters.
Their bearings, 53-pixel advance and 54-pixel line height are preserved.
The original native SP font scale becomes `.33092687995210784` on the logical
256-pixel face. Speed remains zero-padded to three digits, with faint `888`
and `8` backgrounds at alpha .2, matching the source renderer.

The native rotating helper `827D4738` explicitly selects sprite record 1,
whose UV rectangle `(272,0)..(288,16)` contains a soft 16×16 alpha shape. This
sprite is stretched into the needle quad. The earlier conversion mistakenly
used record 2's solid 14×287 strip, creating an opaque bar through the pivot;
the combined renderer inspection caught this and the resource is corrected.

The original face pivot `(128,128)`, needle rectangle
`(125.44,48.64)..(130.56,163.84)` and speed/gear centers are retained. RPM is
deliberately calibrated to the printed marks, rather than copying the donor's
engine-normalized rotation. Each extracted tick angle anchors its actual RPM;
0..6000 interpolates between the bottom zero and 6000 marks. 19,000 RPM
uses the remaining face arc beyond 18,000, with a shorter final interval to
keep the needle clear of the speed box. All printed tick anchors stay fixed. The artwork
and game physics are unchanged. Pixel-derived tick calibration is approximate
to the source image's resolution, not a claim of sub-pixel physical precision.

The donor executable's constructor defaults use 6000..16000 input bounds,
stroke-dependent travel and an angle cap that do not agree with its printed
scale. Copying those defaults would mislabel actual 09/10 RPM. The supplied
09/10 stock catalog was separately inventoried: all 102 resolved configurations
have `MaxRpm=19000`. This is a configured engine limit, not a measurement of
every transient during live play.

Original optional donor-specific damage and auxiliary indicators are not
fabricated from unrelated values. The face's speed, gear and needle use the
verified 09/10 values.

### Reviewed high-resolution artwork

The bundled MotoGP 06 resource now uses the reviewed restoration of the face,
digital numerals and needle. The preceding extraction details describe the
original inputs and logical geometry; the restored sprite pixels are not
byte-identical to those inputs. `origin` retains the original extraction
provenance, while `restoration` records the reviewed artwork input hashes and
conversion method. This is a visual reconstruction, not an assertion of an
exact recovery of higher-resolution donor artwork.

The packed face is 1024×1024, the needle is 64×64 and the digit atlas is
1160×512. Logical dimensions, pivot, RPM calibration, glyph bearings, advances,
speed/gear centers and faint digit backgrounds are unchanged. The donor's
translucent background opacity field is reused; printed marks remain opaque.
Black backgrounds in the numeral and needle review sheets are converted to
alpha coverage and do not become opaque rectangles in the HUD.

`scripts/pack-motogp06-restoration.py` accepts an unrestored `--base-pack` plus
reviewed `--face`, `--digits` and `--needle` images, and writes `--output`.
Pillow inspects bounds; ImageMagick performs the texture conversion and packing.
Original uploads and review images remain private analysis inputs. The bundled
JSON is self-contained and does not require those files at runtime. Re-running
the original importer alone produces the original low-resolution pack; run the
restoration packer afterward to reproduce this reviewed variant.

Focused verification checked texture limits, transparent margins, translucent
interior, all ten digit crops and unchanged logical geometry. The existing
production-loader/ImGui fixture also passed under Wine using the restored pack,
including placement and absolute RPM behavior. This is CPU rendering evidence;
no application rebuild or native Windows GPU playthrough was performed for the
artwork change.

## MotoGP URT 2

The supplied PC `overlays.ark`, `fonts.ark`, `frontend.ark` and `motogp2.exe`
were sufficient for extraction and static geometry analysis. An XML index is
not required: these ARK files contain their own little-endian record tables.

```bash
python scripts/import-motogpurt2-gauge.py \
  --overlays PRIVATE_DONOR/overlays.ark \
  --fonts PRIVATE_DONOR/fonts.ark \
  --output PRIVATE_EXPORT/gauge.json
```

The original 101×101 face, red needle, digital font, yellow speed/gear readouts
and five RPM lights are retained. The font stores A8 alpha coverage, which is
preserved while repacking only the ten required digit glyphs. Speed is
zero-padded to three digits, neutral uses 0, and ghost digits retain alpha .2.
The renderer uses the original pivot, readout centers and needle quad geometry.

The PC texture-format enum 2 is BC3/DXT5, established independently in the
URT2 executable's D3D format table at `0x5DC418`. The initial BC2/DXT3 conversion
was incorrect and has been replaced. Comparing two images produced by that
same old decoder was not independent format verification. The corrected pack
uses the native format mapping and preserves the intended alpha coverage.

This face has no RPM numerals to relabel. Its full original needle sweep maps
0..the current 09/10 bike's actual configured maximum, as on the unnumbered
MotoGP 17 face. This intentionally replaces the donor's 6000..14986 input
normalization while preserving its visual start/end angles. The five lights
now follow the same per-bike scale. Their thresholds are approximately
64.58%, 67.81%, 71.04%, 74.27% and 77.5% of the current engine maximum.
The final threshold aligns with the reviewed face's red-zone entry tick
(about -17.36 degrees from screen right). The previous absolute donor
10000/10500/11000/11500/12000 thresholds lit too early against the adapted
needle; their relative spacing is retained. For a 19,000-RPM engine, the final
light now starts at 14,725 RPM instead of 12,000. It still blinks for five of
each ten UI frames. This is a visual shift cue, not a change to engine limits,
the limiter, gear selection or physics.

The focused `gauge_ui_probe ASSET_PACK NEW_FIXTURE --urt2-lights-only` phase
checks the actual emitted light colors across 14,000/19,000/23,000-RPM engine
limits, the reported early 12,000-RPM warning, needle-mesh alignment with the
red-zone entry tick and the original blink phases. These are Wine/CPU render
checks; native Windows gameplay appearance remains a separate acceptance.

The original PC UI selects an external `tex_gouraudalpha` pixel shader absent
from the supplied files. Its exact color gain remains unverified. A separate
fixed-function path uses MODULATE2X, but that does not establish the shader's
behavior. Converted sprite pixels remain unchanged; source light RGB values
are retained without speculative doubling. Native in-game appearance can
settle this remaining brightness detail during the combined visual check.

### Reviewed high-resolution resource

`scripts/pack-motogpurt2-restoration.py` prepares a separate review resource from
the unrestored URT2 `--base-pack`, reviewed `--face`, `--needle`, `--light` and
the restored MotoGP 06 `--digit-pack`. It does not replace a runtime resource
unless that path is explicitly selected as `--output`. The user approved the
combined face, shared digits, clean red needle and light mask; the bundled URT2
runtime resource now contains that reviewed pack. Integration copied the exact
bytes that had already passed the loader/render fixture, without re-running
unaffected checks or building the application.

The original ten URT2 and 06 glyphs have identical logical metrics and binary
alpha masks at the 128 threshold. Mean per-pixel alpha differences are below
one byte, consistent with their different A8 and A4 encodings. The reviewed
high-resolution numeral shapes can therefore be shared, while URT2 retains
its own yellow tint, speed/gear sizing, bearings and faint digit backgrounds.

The review face is 808×808, the red needle 64×512, the common light mask 128×128
and the shared digit atlas 1160×512. Texture coordinates change; logical drawing
rectangles, pivot, normalized RPM sweep, all five light positions, original
on/off colors and fifth-light blink remain unchanged. Light thresholds use
the synchronized per-bike scale described above. Repacking an older base pack
migrates its absolute thresholds once; already normalized packs are retained.
The needle
uses the original `(206,4,0)` RGB. Its generated black presentation background
becomes alpha coverage, as does the white light mask; neither introduces an
opaque rectangle behind the sprite. The original translucent face opacity
field is reused between the restored printed marks.

The staged resource passed geometry/metadata/texture-bound checks and the
existing production-loader/ImGui fixture under Wine. Low- and high-RPM draw
lists were rasterized for the combined visual review. These are CPU fixture
images, not a native Windows GPU playthrough. No application build was made.

## MotoGP URT 3

The supplied PC archives include the original numbered face and digital font.
The shared PC ARK/TPAGE decoder is reused from the adjacent URT2 importer:

```bash
python scripts/import-motogpurt3-gauge.py \
  --overlays PRIVATE_DONOR/overlays.ark \
  --fonts PRIVATE_DONOR/fonts.ark \
  --pshader PRIVATE_DONOR/pshader.ark \
  --output PRIVATE_EXPORT/gauge.json
```

The original face is 128×128, with pivot `(62.98880005,60.352)`, rather than
the canvas center. Original needle geometry, digital glyph bearings, yellow
zero-padded speed, numeric gear and faint eights are preserved. Original shift
lights are disabled by this title's defaults, so the pack adds none.

The requested face changes replace **15 with 16** and **16 with 18**. Only the
second-digit cells `(96,39)..(104,48)` and `(100,59)..(108,68)` are modified.
The new digits reuse the original face's 6 and 8 pixels without resampling;
the removed ink is backed with a nearby untouched carbon strip. The leading
1s, other labels, ticks and rims remain unchanged. Source/destination cells
and original/modified face hashes are recorded in the pack's `origin`.

The RPM mapping follows the printed tick positions in absolute RPM, including
the altered 16,000 and 18,000 marks. Thus spacing is intentionally nonuniform:
the last two labeled intervals each represent 2,000 RPM. At 19,000 the needle
uses the remaining original capped sweep beyond 18,000; extending a full extra
interval would run into the gear bubble. Values between anchors interpolate
continuously, including the bottom zero-to-8,000 interval. Engine physics and
RPM limits are unchanged. Tick angles are limited by the source's pixel size.

The native D3D format table at `0x61DE60` establishes BC3/DXT5. The supplied
`tex_gouraudalpha.pso` establishes `saturate(2*texture.rgb*vertex.rgb)` and
`texture.a*vertex.a`. Original sprite RGB128 is consequently effectively
neutral through this shader; the pack retains the raw texture RGB. This
shader evidence applies to URT3, without asserting that URT2's unsupplied
shader has identical bytes. Native source geometry and shader evidence are
retained under `out/analysis/motogpurt3-gauge/native-work`.

### Reviewed high-resolution resource

The bundled URT3 pack now contains the user-approved final face restoration,
with the reviewed long/intermediate/fine tick hierarchy and existing 16/18
labels. `scripts/pack-motogpurt3-restoration.py` packs that face without another
generated redraw. It aligns the extracted foreground silhouette to the
original visible alpha bounds and restores the original background opacity
field. The gray preview exterior is not an opaque in-game rectangle.

The original URT3 and URT2 needle RGBA pixels are byte-identical. Their digit
metrics and thresholded shape masks also match; the reviewed restored needle
and high-resolution digit atlas are therefore reused from URT2. URT3 retains
its own original needle rectangle, pivot, absolute RPM calibration, yellow
readout centers/scales, ghost digits and disabled shift lights. The 1024×1024
face, 64×512 needle and 1160×512 numeral atlas remain embedded in one JSON.
The approved face input and source/shared pack hashes are recorded separately
in `restoration`; `origin` continues to describe the original extraction.

## Crash visibility and tyre HUD

The native crash movement uses signed `gameBike+0x278 > 0` in `8231E028`.
`82354090` increments that counter while the physics off-bike state at `+0xC94`
is active and clears it when the rider returns. Gauge children still execute
during the original movement, which previously left the independent host
overlay visible. Capture now follows the proven state and rechecks it at
presentation, including a fall occurring after capture in the same frame.

The small native motorcycle/tyre overlay is `TyreWearIndicator` (`82322620`),
separate from the tall CleanSection element. With a classic custom gauge both
native indicators move up by 160 reference pixels, preserving their original
horizontal spacing and vertical alignment. Matrix inputs and the GPU UI constant are
restored after each scoped draw. Original gauge style retains native layout.

MotoGP17 uses the extracted donor motorcycle bitmap and original front/rear
vector fills only when that optional panel is loaded, native tyre visibility
allows it and validated native values exist. The original small tyre HUD is
then suppressed for that view; a per-frame handshake prevents duplicate donor
rendering when suppression did not occur. Invalid/unavailable values retain
the moved native fallback. Front and rear are sampled independently from the
two wheel structures at `physics+0x5A4+index*0x264`.

The read-only host curve evaluator translates `82290978`/`823511C0` with bounded
guest reads. Displayed remaining health is the original native formula:
`max(0,(friction-min_friction)/(1-min_friction))`, using the live
`overlays/tyre_wear/min_friction_level` property rather than a guessed wear
percentage. Native display and wear-enabled gates are retained. The donor
white vector silhouettes use that health for fill clipping and the verified
bad/yellow/good color stops; the donor bitmap and wheel outlines draw afterward.
This is an adaptation of actual 09/10 tyre state, not a claim that the absent
MotoGP17 simulation-to-artwork code was recovered. Damage and temperature
telemetry remains inactive. The original motorcycle cutout has a fixed neutral
gray underlay; panel-gray covers hide the unused baked thermometer cutouts.

## Evidence and verification boundaries

Archive identities, source offsets, decoded hashes and conversion decisions
are retained in each pack's `origin` metadata. MotoGP 06's face, needle and all
ten digit crops were compared by RGBA against the extracted originals; repeated
conversion produced identical JSON. These are asset conversion checks, not a
native Windows GPU playthrough.

Ignored evidence for MotoGP 06 is under `out/analysis/motogp06-gauge`, including
`native-work/calibration-report.md` and `font-work/report.md`. REA archive
Evidence: initial donor `ev_a0c57b493356214db55734fbaea779786fa797b27ae273ab637df987e46f14fb`;
frontend upload `ev_6704ce2a2707b934444e75e1ad65b8661991f2bd9ba1ccdb3a2c2cab0ebe4046`.
Donor binaries were inspected statically and not executed. The supplied XEX's
MD5 differs from its old index entry; its actual identity is retained separately.

The local collection build passed the five-choice generated-native menu and
telemetry fixture, plus the production renderer/load/persistence fixture under
Wine/CPU. CPU draw inspection caught and corrected the MotoGP 06 needle sprite;
the 19,000-RPM endpoint also stays clear of the speed box. The accepted ZIP
contains all four resources and the matching verified runtime pair. This is
not a native Windows GPU playthrough or physical controller acceptance.
No commit, push or CI dispatch was performed for the local build request.
