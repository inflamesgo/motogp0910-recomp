# Active work checkpoint — 28 October 2026

Use this checkpoint to resume the current task. Distinguish confirmed workspace
contents, previous delivery reports, user observations and unresolved questions.
Read `AGENTS.md` before editing. Do not repeat broad searches or run builds/tests
without a new reason and the authorization required there.

## Confirmed Git boundary

- Workspace: `/workspace/motogp0910-recomp`.
- Local branch: `work`; base before this change set: `43ca880`,
  `feat(port): add friend notifications and polish HUD controls`.
- The user explicitly requested committing/pushing ONLY the changes preceding
  their invitation/Discord/ghost review request. Later work intentionally stayed
  uncommitted until the user explicitly requested committing and pushing the
  entire pending change set on 28 October. Its earlier absence from GitHub was
  expected and is not evidence of loss.
- Remote branches `main`, `preview` and `nobk-online-integration` were inspected.
  None contains the later season-selector implementation. Do not search them
  again unless a reference changes.

## Current local UI source changes — preserved

- Approved outgoing F1 invitations use clipped-corner pit tickets: orange
  `INVITE`, pending text, muted `SENT` with check after acknowledgement, `RETRY`
  after failure. Feedback is per peer and invalidates with room identity or
  eligibility changes. The old global `Invitation sent.` heading is removed.
- Incoming invitations use the approved 404 × 108 pit board, matching the
  friend-online toast's position and animation, with inviter and panel-key hint.
- F1 incoming rows have portrait/name/subtitle and separate orange `ACCEPT`
  and muted `DECLINE` tickets. Answer deduplication disables repeated actions.
- Only Xbox A/X or PlayStation Cross/Square hints appear beneath these buttons.
  Keyboard prompt style hides the hints; keyboard actions remain functional.
- Selected-row underline and the vertical rail LEFT of the navigation tabs are
  two reference pixels thick. The curved decoration retains original thickness.
- Startup strip explicitly says `Controller: hold Back` for Xbox or
  `Controller: hold Select` for PlayStation. Keyboard style omits that hint and
  divider. The existing configurable panel key remains.
- Source changes and documentation are uncommitted. Whitespace check passed.
  The user-requested local Windows x64 build and package completed successfully
  on 2026-10-28; no runtime tests, CI dispatch or native visual acceptance were
  performed. The new package is listed below; older binaries retain older UI.
- A source backup of the earlier UI state, BEFORE the incoming accept/decline
  implementation and controller-only hint edits, exists at:
  `/workspace/shared/motogp-local-source-recovery-20261027T212001Z/`.
  The working tree contains the subsequent edits; do not overwrite it with that
  older backup. Preserve current changes before any recovery or branch switch.

See [invitation UI implementation](invitation-ui-concept.md) and the maintenance
map in [NOBK panel](nobk-panel.md).

## Current objective: five leaderboard menu corrections

The user supplied screenshots of the working custom season-first menu and a
video link: https://drive.google.com/file/d/1r_3phPZJGeEsPidJgdNZxSLU1qhfUGRq/view
The video is now downloaded as
`out/analysis/season-menu-recovery/reference-video.mp4` (23.167 seconds).
Sampled visual frames were inspected; no audio playback verification is claimed.

Requested corrections:

1. Restore the native selection/transition animation when choosing a season.
2. Use `250cc` or `Moto2` in the category heading according to the chosen class;
   do not show the shared `250cc / Moto2` label.
3. Back must return exactly one menu level: category/type selector to class
   selector, class selector to season selector. Do not skip back to season.
4. Season entries must read `2009 SEASON` and `2010 SEASON`.
5. Selecting Moto2 must use the existing Moto2 announcer clip, as Time Trial
   does, rather than the 250cc clip. Do not synthesize new speech.

Previously agreed categories remain season first:
2009 A MotoGP / B 250cc / C 125cc;
2010 A MotoGP / B Moto2 / C 125cc.
Do not revert to simultaneous four-class A/B/C/D navigation.

## Correct earlier delivery and unresolved source availability

- The user explicitly identified the leaderboard delivery as
  `MotoGP0910-Windows-x64-online-season-leaderboards.zip`.
- Do NOT confuse it with `MotoGP0910-Windows-x64-local-online-test-3.zip`.
  The latter was the separate single-PC synthetic friends/records fixture.
- In this workspace, the later `leaderboard_categories*`,
  `leaderboard_scope*`, `record_scopes.py`, ghost-transfer and local-test-fixture
  sources have not been located. The screenshots prove the menu exists in the
  user's binary; they do not establish where its C++ source is now stored.
- Already searched: current working tree, available local history/reflog,
  unreachable Git blobs, and 77 available ZIP names/indexes. No matching later
  source was found. The current generic project ZIP also lacks these files.
- Why this source copy differs from the earlier working state is UNKNOWN.
  Do not claim the user deleted files, that Git push was required, or that the
  environment definitely reset. Do not claim recovery has succeeded.
- The user supplied the correct ZIP's Drive link:
  https://drive.google.com/file/d/1jRd0xdBly_wM4O9qGjcG5Jd4m86Z4ABR/view
  It is downloaded to
  `/workspace/shared/MotoGP0910-Windows-x64-online-season-leaderboards.zip`.
  Size 42,647,690 bytes; SHA-256
  `b91b7aa4343791e2f85576abd7af7b5f8620fe507142929f6e25a0236783e3ef`,
  matching the original delivery receipt. Do not request this package again.
- The package has no original C++ sources, but it DOES include
  `bin/motogp0910.map` alongside the EXE. Both are extracted under
  `out/analysis/season-menu-recovery/`. The map identifies the missing category
  and scope modules and their function addresses. The recovery receipt is
  `recovery-evidence.json`; focused disassembly is `categories.asm` and
  `hooks.asm`. These are ordinary shell-tool observations; no REA provider
  was connected, and no binary execution was performed during recovery analysis.
- The shipped input hook is `sub_82240CD8`; its category `Complete` helper
  rewrites rows and returns -1 on season selection. Together with the user's
  report, this explains why normal transition animation is bypassed.
  `sub_822410B0` is the later native selection-completion callback. Native
  generated functions remain available under `generated/default/` for comparison.
- The user explicitly approved reconstruction. The missing category/header,
  hook and scope-capture files have now been recreated, with the five requested
  corrections implemented in source. Native read/write scope capture and the
  matching client's scoped protocol were restored too, to avoid mixing data.
  See [the reconstruction report](leaderboard-menu-recovery.md) for evidence,
  integration details and limits. The current invitation/UI changes remain.
- Source review, whitespace checking and the user-requested local Windows x64
  build/package completed. No runtime tests, CI dispatch, commit, push or native
  Windows acceptance was performed. Gameplay behavior remains unverified.
- The original scoped Python server/storage/API, ghost infrastructure and local
  fixture sources are still not recovered. This Windows application ZIP does
  not contain them. The restored client requires the same matching scoped
  server and fails closed instead of querying legacy mixed boards. Resolve this
  separate server/source handoff gap before claiming the whole handoff is ready.

## Existing user evidence and handoff boundaries

- The user reports actual Discord invitations worked in their local setup and
  supplied a Discord card displaying `Joined`. Do not ask them to repeat that
  same check without a reason; it is not full production acceptance.
- The ghost probe's `checks passed` message indicates synthetic transport
  success. Native recording validation, save/task integration and playback were
  still pending for ChunchunQ. Do not claim complete ghost gameplay support.
- Main handoff documents still contain older implementation descriptions and
  need reconciliation with recovered sources before final delivery.
- Production NOBK native invitations need the shared account API's immutable
  identities and matching service/proxy configuration. Discord capability
  invitations are a separate authenticated path.

## Communication and next step

The user is frustrated by repeated questions, inconsistent package names and
token-heavy investigations. Acknowledge mistakes plainly; do not substitute a
new broad audit for missing evidence. Keep updates short and do not make them
re-explain decisions listed above. Ask if a genuine implementation ambiguity
arises, as the user explicitly requested. The current task remains the five
leaderboard corrections, followed by accurate handoff preparation.

## Local build delivery — 2026-10-28

- Pinned Linux/Windows SDK packages downloaded and SHA-256 verified against
  `sdk-version.json`; verified custom runtime staged by the build helper.
- `bash scripts/build-windows-cross.sh` completed successfully (203 steps).
- Package: `out/dist/MotoGP0910-Windows-x64-season-menu-invite-polish.zip`
  (42,656,174 bytes), SHA-256
  `1d649a857d12ab44612b92182246597f3068085db97e516f58e3c76b5f617fdb`.
- Packaging integrity checks passed. Original game data, saves and personal
  configuration are excluded. Logs: `out/local-season-build.log` and
  `out/local-season-package.log`. No game/probe execution was performed.
- Matching scoped server implementation remains outstanding as described above.


## Latest feedback and source correction — 2026-10-28

- The user tested the preceding build: season labels need uppercase, and the
  class menu appears to ignore navigation, selection and Back while the game
  keeps running. The attached `motogp0910_001.log` has no menu action traces;
  normal runtime activity continues until a window close.
- Follow-up source fixes use `2009 SEASON` / `2010 SEASON` (Turkish uppercase
  too) and pass `822410B0` as r4 to the class-frame queue. The earlier r4=0
  installed a native no-op, losing selection/Back callbacks.
- Added bounded debug `LEADERBOARD_MENU` entry/input/result traces. No input
  routing or other menus were changed. See the recovery report for details.
- These fixes are source-only: no new build/test/commit/push. The delivered
  `season-menu-invite-polish` ZIP predates them and still has the reported bug.
- Directional navigation still needs runtime verification separately from the
  proven missing selection/Back callback.

- Additional requested source change: successful season confirmation now reuses
  Time Trial callback `821F73F8` audio: 2009 cue 374; 2010 native random cues
  375/376. Uses audio helpers only, with an isolated PPC context and temporary
  guest cue array; no Time Trial mode/profile changes. No sound on focus/Back.
  Still source-only, without a new build or runtime audio test.


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


## Publication request — 2026-10-28

The user authorized committing and pushing all pending source/documentation
changes, including invitation UI, reconstructed season/class leaderboards and
removal of temporary menu traces. Native Windows menu/text/audio acceptance is
user-confirmed as recorded above. The last local build predates only the
logging-only cleanup. Original game data, personal logs/configuration, ignored
analysis files and development ZIPs are excluded. The scoped service and native
ghost-playback handoff gaps remain explicit; this commit does not deploy them.
