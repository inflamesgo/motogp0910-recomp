# Leaderboard menu source reconstruction — 28 October 2026

## Current delivery status

Source `28ba765` includes the reconstructed menu/scoping, uppercase season
labels, callback correction and native season/class audio. The user accepted
both seasons, all three class selections, text/audio and repeated Back/forward
on Windows. Temporary `LEADERBOARD_MENU` traces were subsequently removed; the
latest tested ZIP predates only that logging cleanup. Historical source-only
notes below describe intermediate stages, not the final build status.

The matching backend is not implemented in current `server/`; see the
[exact scoped protocol/migration](leaderboard-season-integration.md),
[website release requirements](website-release-checklist.md) and
[PR transfer manifest](pr-handoff.md). No new runtime test was performed for
this documentation pass.

## Artifact and authorization

The user explicitly approved reconstructing the missing menu sources from the
working `MotoGP0910-Windows-x64-online-season-leaderboards.zip`, while preserving
the current invitation/UI changes, and applying five reported menu corrections.
The ZIP is 42,647,690 bytes with SHA-256:

`b91b7aa4343791e2f85576abd7af7b5f8620fe507142929f6e25a0236783e3ef`

This matches the earlier package receipt. The ZIP contains the game executable
and an 11,730,104-byte linker map, but no original C++ source. The sources now
present are a reconstruction, not a byte-for-byte recovery of the original
files. The supplied ZIP and unrelated local UI changes were preserved.

## Evidence and scope

The linker map identifies `leaderboard_categories.cpp`, its native hook unit,
`leaderboard_scope_capture.cpp` and the scope hooks. Focused x64 disassembly was
compared with the already available generated native functions. The user's
23.167-second reference video was downloaded and sampled to inspect the menu
sequence; its audio was not used to claim an announcer playback test.

Ignored evidence is under `out/analysis/season-menu-recovery/`:
`recovery-evidence.json`, `categories.asm`, `hooks.asm`, `scope.asm`,
`setup-scope.asm`, the extracted EXE/map and the reference video/timeline.
These are ordinary ZIP/linker-map/GNU objdump/source observations. No REA native
provider was available or connected; no REA Evidence ID is claimed.

Key static observations:

- The shipped `Complete` helper at `0x14282E430` rewrites rows and returns -1
  on season selection. Its input hook calls that helper before the normal
  native selection-completion callback. This explains the reported missing
  transition animation.
- The native class callback is `822410B0`. Its native queue operation,
  `8235ACB0`, saves a 76-byte history entry when its history argument is set;
  `8235A9C0` copies the previous frame and payload. The normal Back operation
  `8235AF40` restores one history entry. Reuse this engine path.
- Class records remain 40 bytes with four allocated records, including the
  disabled sentinel. The class model vtable is `8203639C`.
- The standalone board embeds its shared board at +32; its enclosing vtable
  is `82036F14`. A generic constructor or active menu alone is not proof that
  a track/race query belongs to the standalone leaderboard.
- `822417F8..8224182C` formats the middle-class heading from separate 250cc
  and Moto2 strings. Replacing a hypothetical combined message-bank entry
  would miss this formatting site.
- Native class-selection paths `821E41F8`, `821F7540` and `821F8E10` use
  announcement IDs 687=MotoGP, 282=125cc, 283=250cc and 285=Moto2. The native
  leaderboard announcement call returns to `8224112C`.

## Five source corrections

| Request | Source behavior |
| --- | --- |
| Play selection animation | The input hook returns the native result unchanged. Stage changes occur in the later native completion callback; season selection queues the native selector as another history frame. |
| Separate 250cc/Moto2 heading | Only the scoped `8224182C` formatting call uses the chosen class and a `%s` format instead of the native combined heading. |
| Back one level | Native history holds season, class and category frames. Constructor re-entry preserves the selected season/stage; Back from category returns to class, then Back from class returns to season. |
| Explicit season labels | English entries are `2009 SEASON` and `2010 SEASON`; Turkish entries are `2009 SEZONU` and `2010 SEZONU`. Labels use separately allocated 24-byte slots. |
| Correct Moto2 announcement | Only the leaderboard announcement site maps the selected category to the native cue, including 285 for Moto2. No voice is synthesized and other game announcement paths stay native. |

The retained categories are 2009 A MotoGP / B 250cc / C 125cc and
2010 A MotoGP / B Moto2 / C 125cc. Native class flags and the 378 view IDs
stay within their existing layouts. The displayed class-row index is not
temporarily rewritten during exit, so the native history saves that same
displayed selection.

## Data identity restored with the menu

`leaderboard_scope_capture.*` restores nested read/setup capture. Standalone
queries use the explicit selected scope only when the enclosing model proves
ownership. Track/race requests use native season global `82926FC0`. Query
specifications copy identity before asynchronous work, rather than reading
the current menu selection later. Uploads group native views by their actual
season property 13. Missing/invalid seasons fail closed.

The restored client uses the original additive protocol:
`/v2/stats/scoped/capabilities`, `/read` and `/write`. The scoped read/write
body prepends `[1, category_id, season_raw, 0]` to the ordinary binary request;
IDs are 1=MotoGP, 2=Moto2, 3=250cc, 4=125cc and raw season is 0/1. A server
without `supported:true, protocol:1` does not fall back to mixed legacy boards.
Existing explicit unscoped client calls remain available separately.

The matching server implementation is NOT recovered by this Windows ZIP.
The current `server/` tree still lacks that earlier scoped storage/API work;
recover its source from a server/test-source backup before preparing the final
handoff or claiming the repository's service can support this client. No
production service, website, database or migration was changed here. Likewise,
this menu reconstruction does not recover ghost-transfer or fixture sources.

## Integration and validation limits

The reconstructed library is linked through `motogp_lan_guest`; hook units are
part of the game target. Initialization allocates labels. Existing hooks in
`local_splitscreen.cpp`, `options_menu.cpp` and `leaderboard_display.cpp` compose
the new observer, label and scoped-read behavior instead of registering duplicate
hooks. The current invitation/UI work remains in the working tree.

Source review and `git diff --check` were performed. On 2026-10-28 the explicit
local-build request completed successfully using the pinned SDK and custom
runtime. The package is
`out/dist/MotoGP0910-Windows-x64-season-menu-invite-polish.zip`; its integrity
checks passed. No runtime tests, CI dispatch, commit, push or native Windows
acceptance was performed.
Static correspondence to the old EXE does not prove the reconstructed code's
runtime behavior. Before accepting the build in gameplay, verify all five requests in
both seasons, all three classes, repeated Back/forward navigation, retained row
selection, the actual Moto2 voice and independent scoped data. Confirm that
Offline/LAN, Time Trial and race uploads retain their existing behavior.


## First Windows feedback: class-menu actions ignored

The user tested `season-menu-invite-polish` and reported that the class screen
rendered while navigation/selection/Back appeared unresponsive. The supplied
`motogp0910_001.log` continues recording normal runtime activity until a window
close and title termination; it has no menu-level action trace and does not
establish whether directional input reached the selector.

Source inspection found a definite transition defect: `8235ACB0` takes the
result callback in **r4**, not a title. Passing zero installs `823996A0`, a
no-op. Consequently the class frame's selection and Back results never reach
`822410B0`. The new frame now receives `822410B0` explicitly. The native queue,
history and input mapping otherwise remain unchanged. Debug-only
`LEADERBOARD_MENU` traces record entry, changed/nonzero input, selected rows
and results without logging every idle frame. English/Turkish season labels
are uppercase.

These follow-up edits have only received source review and whitespace checks.
They are not present in the preceding ZIP. No subsequent build or runtime test
has been performed. Native Windows acceptance must still check directional
navigation, class selection and Back in both seasons; the callback correction
proves an action-routing defect, not that every observed input symptom has
already been verified resolved.


## Native season announcements

The leaderboard's successful season-confirm transition now uses the same audio
calls as Time Trial's season callback `821F73F8`:

- 2009: cue 374 via `821F3518`, with r4=1, r5=0, r6=1 (return site
  `821F7440` in the original callback).
- 2010: cues 375 and 376 via `821F2FE0`, retaining the native random variant
  selection, with count=2, r5=1, r6=0, r7=0, r8=1 (return site `821F7470`).

Only the audio calls are reused. The Time Trial callback itself is not invoked,
so its mode/profile/bike and menu changes do not execute in the leaderboard.
A copied PPC context preserves pending menu arguments. The 2010 cue array uses
a temporary aligned guest stack frame; the helper resolves a cue synchronously
before returning. The announcement runs once on accepted season confirmation,
before queuing classes, and not on focus changes or Back. Existing class
announcements keep their own native path. A debug event records the chosen year.

This addition and the preceding label/callback fixes remain source-only. Source
review and `git diff --check` passed; no new build or audio/gameplay test was run.


## Latest local test package — 2026-10-28

At the user's explicit request, the local Windows x64 build completed after the
class callback correction, uppercase season labels and native season audio
addition. Packaging integrity checks passed with the pinned SDK/custom runtime.

- Package: `out/dist/MotoGP0910-Windows-x64-season-input-audio-fix.zip`
- Size: 42,659,815 bytes.
- SHA-256: `1db08fdcd8571035870e0c374ddf241eddec77893f2a215a4b799f89e48bc55d`.
- Logs: `out/local-season-input-audio-build.log` and
  `out/local-season-input-audio-package.log`.
- This delivery supersedes the earlier source-only status for these edits.
  No runtime gameplay/audio tests, commit, push or CI dispatch were performed.
- User acceptance: both season labels and announcements; directional input,
  selection and Back in each class menu. Debug `LEADERBOARD_MENU` traces are
  available if navigation still fails. Scoped server recovery remains pending.


## Menu acceptance and temporary trace removal — 2026-10-28

The user confirmed the latest local build's text, audio and forward/Back menu
navigation work correctly. The submitted test log shows both seasons, all three
class selections, repeated parent returns and season announcements, with no
crash. Existing server capability warnings remain a separate handoff dependency.
At the user's request, all temporary `LEADERBOARD_MENU` debug calls and their
input-tracking variables/logging includes were removed from the category files.
Error/warning logging is retained. Whitespace checks passed; no new build,
runtime tests, commit or push were performed after this logging-only cleanup.
The already delivered ZIP still contains the temporary traces.
