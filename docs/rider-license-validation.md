# Rider License local validation — 22 October 2026

Scope: current local changes after `1b8e105`, including Rider Rank, summary
labels, footer and Personal Best alignment, removal of routine diagnostics,
general bio emoji rendering and approved launcher version `1.1.11`.
The existing integration PR remains unchanged; transfer is paused by the user.

## Process failure and prevention

The validation process took unacceptably long. Broad reruns after localized
failures, late review of stale test contracts, repeated optimized probe rebuilds
and manual reuse of a stateful fixture wasted time. Large assertion counts do
not excuse that execution failure. The user's request for thorough checks did
not require those choices.

The repository instructions now require bounded acceptance criteria, a duration
estimate, a five-minute diagnostic checkpoint, isolation after two failed
attempts, preservation of passing evidence and explicit pushback on harmful
scope/cost. Ordinary build requests retain their existing Actions workflow.
The runner now supports `--probe` and `--launcher-phase` so a failed phase can
be rerun alone using its existing fresh-fixture setup. For example, append
`--probe launcher_layout_probe --launcher-phase layout` to the runner command
below; use `--list-targets` to inspect the selection without executing tests.
This process follow-up did not start another Windows build or regression run.

## Environment and commands

Windows Release cross-build uses `scripts/cloud-env.sh`, the existing
`windows-nightly-release` build directory and the SDK/runtime pins tracked by
the repository. Windows probes run under Wine with the packaged Microsoft CRT,
an isolated Wine prefix and Xvfb. The full-disc fixture references local game
assets; original game files, settings, saves and logs are not committed or
packaged.

```sh
python3 -m compileall -q server scripts
python3 -m unittest discover -s server -p 'test_*.py' -v
source scripts/cloud-env.sh
cmake --build out/build/windows-nightly-release --target gamer_card_probe
# With out/wine-env.sh, an isolated WINEPREFIX and a working DISPLAY:
wine64 out/build/windows-nightly-release/gamer_card_probe.exe FONT_PATH EVIDENCE_DIR
python3 scripts/verify-windows.py \
  --build-dir out/build/windows-nightly-release \
  --game-root FULL_DISC_FIXTURE --output-dir EVIDENCE_DIR --wine WINE64_PATH
# After completing checks:
bash scripts/build-windows-cross.sh
```

## Findings and corrections

- At 640×480, the previous 12-pixel minimum profile face exceeded bio/Records
  baseline spacing. Prepared native faces now extend down to 5 pixels; viewport
  scaling selects an appropriate face without changing F1 fonts or card size.
- Source parsing for Rider Rank and record summaries is shared with the probe.
  Tests distinguish 16 unique held circuits, 17 held boards and 19 entered
  boards; verify site ranking tie-breaks and selected-board/2009 scope; and
  reject duplicate or malformed ranking entries.
- Non-BMP emoji cannot be represented by the pinned 16-bit ImGui character
  type. A display-only cluster mapping now uses shared embedded Noto sprites
  without modifying the SDK ABI or stored UTF-8 bio. All 5,709 catalogue aliases
  are checked; representative joker, face, heart, skin tone, flag, family and
  keycap sprites are checked against original RGBA pixels at every profile
  font size. Family artwork can legitimately be monochrome.
- The localization probe still expected an older launcher A binding. It now
  checks the current native Default mapping: A Tuck in, B Gear down, Y Gear up.
- The HUD probe required bit-exact computed float positions and failed on a
  one-ULP difference. Only transformed-coordinate comparisons now allow a
  `1e-6` tolerance; unaffected coordinates and Z/color/UV/source restoration
  remain bit-exact. No production HUD behavior changed.
- The launcher physical-click probe clicked an offscreen VSync control and
  attempted a popup test after another helper switched pages. It now reveals
  the intended controls/pages first. The full bilingual physical-input,
  resizing and ISO-import audit receives 600 seconds; ordinary CPU probes
  retain their 180-second limit.
- The launcher status control reached one pixel outside the window at 75%
  scale because position and height rounded independently. Its transparent
  control now keeps a two-design-pixel bottom inset; text position is unchanged.
  A targeted `--layout-only` probe mode allows viewport checks without repeating
  the unrelated ISO/physical-input/catalog audit.
- The Online sign-in introduction extended four design pixels into the
  username/password caption row. Its two-line text box now ends before those
  captions; input positions and account behavior are unchanged.
- Launcher verification checks predated mandatory DLC inventory and the
  versioned readiness footer. The UI fixture now explicitly checks base-only
  and partial installations stay blocked, then supplies inventory-only sparse
  files/headers before testing ready-state controls and read-only verification.
  They contain no usable game content and are never launched. Existing DLC
  installations are rejected before fixture creation and cannot be overwritten.
  Localized help and footer expectations now follow current approved behavior. The legacy network checks now require
  sign-in before showing the Online hosting field and assert that the hidden
  server field cannot override the fixed community endpoint.
  `--remaining-only` runs the later regression cases without repeating the
  already completed physical-input/file-verification/viewport phases.

## Results

- Python compilation: passed. Server unit tests: **60 passed**.
- Rider License Windows probe: **6,807 checks passed**, including 30 concurrent
  F1/card opening races, focus/neutral-input release, native view forwarding,
  resource decoding, English/Turkish tabs with/without bio at 640×480, 720p,
  1080p, ultrawide and 4K; rendered footer centering, bounded layout and modal
  isolation. Combined font/emoji atlas: **4096×4096**, shared sprites stored once.
- Six CPU-rendered captures were inspected/generated for both languages/tabs
  and emoji preview/modal. The rendered joker is present in both bio displays;
  no replacement diamond is used for the tested catalogue entries.
- Launcher viewport audit: **65,765 checks passed** across all seven pages,
  both languages and 75%/100%/125%/150% scales after the two layout fixes.
- General Windows regression: **31 non-launcher-layout scenarios passed**.
  Launcher checks were completed in phases: earlier physical-input, dropdown,
  tooltip, read-only file-verification and catalog checks were exercised during
  the full audit; the corrected viewport phase passed 65,765 checks; the focused
  `--remaining-only` run passed **10,807 checks** in a fresh fixture, covering
  catalog/save isolation, scrolling/painting, bindings, presets, adaptive-trigger
  controls, connection modes, serialization and actual child-process exit.
  This is phased validation, not a claim that the umbrella runner produced a
  final single-run 32/32 report. Earlier timeout, stale-expectation and reused-
  fixture failures remain in historical evidence; known causes were corrected.
- Production Windows Release build: **passed**, including the game EXE,
  launcher/UI DLL, crash monitor and update helper dependencies, using the
  pinned SDK and verified runtime pair.
- Local package: `MotoGP0910-Windows-x64-v1.1.11-20261022-212459.zip`, 37.7 MiB,
  41 entries. Runtime DLL hashes, embedded emoji PNG and launcher version were
  checked in the ZIP. It contains no original XEX/patch/DLC content, personal
  configuration/logs, README files or separate license directory. Historical
  archives are unchanged; notices remain embedded in the launcher.

Local evidence is under `out/rider-license-validation/` and is ignored by Git.
These checks exercise CPU code, real Win32 controls under Wine and production
ImGui draw lists. They do not establish native Windows GPU output, physical
gamepad behavior, a complete race or live production-service acceptance.
Lobby circuit integration, season/Moto2 metadata and DLC outline publication
remain separate documented integration work.
