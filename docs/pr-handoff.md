# PR preparation and transfer manifest — 28 October 2026

Source baseline: `inflamesgo:main` at `28ba765`, with the later production
changes listed below. Delivery is `inflamesgo:preview` →
`ChunchunQ:nobk-online`. The target was rechecked on 28 October at
`c432d67ffb95a2c9383493b9a8c7a723b9ed52b0`. Existing preview history through
`d788d0c`, the target ancestry and earlier feature/documentation commits are
preserved. The user authorized the final document check, preview commit/push
and PR creation. No merge, production deployment or new build is included.

## Proposed title

`feat(port): add gauges, invitations, season leaderboards and dual-source updates`

This revision includes the dual-source updater, Multiplayer CHAMPIONSHIP
text/audio, website notes and approved **1.2.0** version. Local 1.1.14/1.2.0
updater-UI test packages were built from the matching primary source tree;
the selective preview branch itself has not received a separate build or broad
native acceptance. Publication uses the reviewed source and the body below.

The copy-ready body is [pr-description.md](pr-description.md). Paste that file
into the PR description; this document is the dependency/review reference.

## Proposed description

MotoGP's native leaderboard now starts with 2009/2010 selection, separating
2009 MotoGP/250cc/125cc from 2010 MotoGP/Moto2/125cc. It retains native menu
transitions and one-level Back navigation, uses class-specific headings and
the game's existing class/season announcements. Client reads and uploads carry
explicit class/season identity rather than merging Moto2 into 250cc.

The port also adds four selectable custom gauges, refines the URT2 shift-light
thresholds, updates the Apex NOBK.FRIENDS presentation and invitation actions,
and includes native/Discord lobby invitation source. Incoming invitations use
the compact pit board; F1 provides explicit accept/decline and per-friend
send/pending/sent/retry feedback. The panel remains F1/held Back, with explicit
Xbox/PlayStation hints and Online-only startup/presence notifications. Existing
keyboard controls, Offline/LAN, account authority and native join flow remain.

Validation: the local Windows x64 build/package passed; the user confirmed both
seasons, all three class selections, forward/Back navigation, text and audio.
The submitted log agrees with that menu flow and records normal shutdown.
The user separately reported Discord invitations working locally. Temporary
menu debug traces were then removed; no rebuild followed that logging-only edit.
These observations do not establish a complete multiplayer/production QA pass.

**Integration required before public release:** current `server/` lacks the
scoped stats capability/read/write routes and isolated storage. The website/API
must carry season identity, separate Moto2 records and correct installation /
controller instructions. Native NOBK invitations require immutable friend IDs
and deployment acceptance. Ghost download remains analysis/API planning in this
source tree, not native recording transfer/save/playback. See the handoff links.

## Required accompanying documents

- [Master handoff](chunchunq-handoff.md): read first.
- [Scoped leaderboard protocol/migration](leaderboard-season-integration.md).
- [Critical website checklist](website-release-checklist.md).
- [Native/Discord invitation and ghost status](chunchunq-friends-invites-ghost-handoff.md).
- [Native invitations](online-friends-invites.md), [Discord invitations](discord-invites.md)
  and [ghost design](ghost-download-plan.md).
- [Menu evidence and package](leaderboard-menu-recovery.md), [panel maintenance map](nobk-panel.md),
  [gauge dependencies](classic-gauges.md) and [DLC catalogue](online-dlc-catalog.json).
- [Dual-source publication/installation contract](launcher-updates.md),
  [native menu scope](pc-menu-cleanup.md) and
  [How to play design comparison](how-to-play-design-review.md).
- [Remaining work and ownership for 1.2.0](release-readiness.md).

## Source dependency manifest

Compare final file contents to the actual target, not commit counts. Older
integration/cherry-pick history can include features already present upstream.
Carry feature dependencies together; review mixed CMake/app/launcher hunks.
Do not copy the entire local `main` branch merely to obtain the latest hooks.

| Area | Source/resources that must stay together |
| --- | --- |
| Season leaderboard | `src/leaderboard_categories.*`, `leaderboard_categories_hooks.cpp`, `leaderboard_scope_capture.*`, `leaderboard_scope_hooks.cpp`; scoped `online/client.*`, `online/stats.*`, `online/leaderboard_display.cpp`; observer/label/init hooks in `local_splitscreen.cpp`, `options_menu.cpp`, `motogp0910_app.h`; CMake category library and hook sources |
| Dual-source updater | `src/launcher/updates.h`, `updates.cpp`, `update_panel.inc`, `main.cpp` button/timer painting, `update_install.cpp`, `update_helper.cpp`, existing `update_protocol.h`; keep the launcher/helper pair and package-owned gauge/font paths together |
| Multiplayer CHAMPIONSHIP | Scoped label in `src/options_menu.cpp`, scoped native audio in `leaderboard_categories_hooks.cpp`; no native bank or asset replacement |
| Native invitations | `src/online/social.*`, `social_guest.*`, `social_codec.h`; relevant `lan/runtime.cpp` / `guest_bridge.cpp` wiring; `server/social.py`, `community_server.py`, `security.py`, `Caddyfile.example`, `test_social.py`; CMake library/Windows links |
| Discord | `src/discord/*`, `discord_presence.*`, `online/discord_invites.*`; launcher activation/configuration handling and app lifecycle; `server/discord_invites.py` / social dispatch and `test_discord_invites.py`; existing presence assets/notices |
| Apex panel and notifications | `src/nobk_panel.*`, `nobk_panel_visuals.h`, `nobk_startup_strip.h`, `nobk_friend_toast.h`; app fonts/input gates, panel worker wiring and room-provider callback; achievement preference handling |
| Background rendering | `cmake/sdk-nobk-backdrop.patch`, relevant `sdk-temporal-fsr.cmake` / `scripts/build-spatial-sdk.py` integration; `src/nobk_backdrop_gpu.*`, `nobk_backdrop_shader.h`; matching custom runtime archive, manifest and notices under `third_party/rexglue-runtime` |
| Four custom gauges | `src/gauge_hud.*`, `gauge_classic.*`, relevant settings/menu/init/frame hooks; all four approved `resources/hud/{motogp17,motogp06,motogpurt2,motogpurt3}/gauge.json`; CMake and Windows packaging; retain supporting gauge/bike capacity code where referenced |
| Control/launcher fixes | Relevant keyboard/button-prompt specs, prompt/input handling, launcher controls/General/achievement layout; retain existing live settings and config defaults without replacing upstream account/update behavior |

The user approved the four converted self-contained gauge JSONs for source and
runtime distribution. Do not include raw donor archives, original game/TU/DLC
containers, personal saves/config/profiles, private credentials or recordings.
Preserve existing source licenses and embedded launcher notices. The two public social/Discord server test files accompany the service modules.
New local `tools/` validation implementations and probe targets are excluded
under the established selective-transfer rule. Existing optional probe targets
retain only link dependencies required by the new production application code.

The selective transfer checks its final diff and imports:
no unresolved category/social symbols, duplicate native hooks, missing runtime
manifest/archive, missing gauge resources or overwritten upstream fixes. A build
of local `main` is not a build of a new selective transfer branch.

## Validation evidence and explicit gaps

| Evidence | What it establishes | What it does not establish |
| --- | --- | --- |
| `season-input-audio-fix` local package | Pinned Windows cross-build, link and packaging integrity | Native GPU/gameplay correctness by itself |
| User's Windows report and supplied menu log | Both seasons, all class menu selections, repeated Back/forward, text and audio; no reported menu issue | Correct storage/ranking of a new result; scope service was unavailable |
| User's Discord `Joined` observation | A working local Discord invitation scenario | Two independent real accounts, cold launch, production proxy/account configuration or an Internet race |
| Existing server tests and historical main-branch probe reports | Available focused protocol checks and dated earlier evidence | A new full test run or coverage of absent ghost/scoped service code |

The tested package is `MotoGP0910-Windows-x64-season-input-audio-fix.zip`,
42,659,815 bytes, SHA-256
`1db08fdcd8571035870e0c374ddf241eddec77893f2a215a4b799f89e48bc55d`.
It predates removal of temporary `LEADERBOARD_MENU` traces and the later local
dual-source updater / Multiplayer CHAMPIONSHIP changes. It contains
no original game files. It is a local test delivery, not a published release.

## Release blockers and owner actions

1. **Scoped service/storage and public API:** ChunchunQ/service operator must
   implement the exact contract, migration, idempotency and acceptance described
   in the scoped integration document. Source client writes fail without it.
2. **Website and How to play:** operator must complete the P0 checklist, including
   six scoped combinations, stable links, correct panel control instructions
   and matching-release download/setup guidance. Existing 20-track work stays.
3. **Native invitations:** shared account API must supply verified immutable
   owner/friend IDs; deploy the matching service/proxy and verify two accounts,
   decline/expiry and native room join. Discord deployment is separate from
   the native friends-ID prerequisite.
4. **Ghost feature claim:** leave unsupported until native fixtures, full format
   validation and task/save/playback integration are delivered. An earlier
   synthetic transport message is not a supplied implementation in this commit.
5. **Launcher release:** build the approved **1.2.0** source with its matching
   launcher/helper, publish coherent **1.2.0** / **v1.2.0** GitHub/NOBK
   metadata and perform the disposable-installation cases in the updater doc.
   Syntax checks do not establish installation or rollback acceptance.

This is suitable for a client/integration review with explicit dependent work.
It is not a fully working Online release until these owners complete the
applicable gates. Do not mark unfinished routes or playback as implemented to
make the PR checklist look complete.


## Prepared preview boundary

The initial production transfer matched main anchor `28ba765`. This revision
adds later changes in `src/launcher/{main.cpp,updates.h,updates.cpp,update_panel.inc,
update_install.cpp,update_helper.cpp}`, `src/options_menu.cpp` and
`src/leaderboard_categories_hooks.cpp`. These eight files match the latest
primary checkout and the 1.2.0 local-build receipt. CMake retains the production libraries, hooks, resources and links while
excluding newly added local probe targets. Existing preview `tools/`, CI,
workspace rules, root README, historical archives and screenshots are preserved.
The resource regeneration scripts, approved gauge JSONs and matching runtime
archive/manifest are included; original donor/game/player data are not.
The shared account/ranking/updater target commits are retained as ancestors.
No selective-branch build or native acceptance was run for this transfer.
