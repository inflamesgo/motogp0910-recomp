# Project status — 23 October 2026

This is the current status entry point, reconciled against `main` source
revision `8a33ea9` and the published `preview` head `ab7d3f7`. Older milestone
reports retain their original evidence; they do not replace current build pins,
installation instructions or validation limits.

## Platform and delivery

Windows 10/11 x64 is the product target. Launcher **1.1.12** and the supported
game title update **0.0.1.1** have independent versions. ReXGlue is pinned to
`0.10.0.24-dev.gbd833a2` (`bd833a2ab69d305f99ddf57418fc38ffec8af93d`) in
`sdk-version.json`, with the matching custom DLLs in the runtime manifest.
Do not mix stable/nightly packages or individual runtime DLLs.

The public entry point is `MotoGP09-10Launcher.exe`; game/runtime binaries are
under `bin/`. Launcher and game share root `config.json`. The default Windows
save/cache root is `Documents/MotoGP 09-10`; migration preserves existing data.
Original game, TU and all three supported DLC packages are local prerequisites
for every connection mode. See [layout](game-layout.md), [portable settings](portable-launcher.md)
and [DLC installation](dlc.md).

Ordinary build requests push authorized changes to `main` and dispatch Windows
build plus Quick quality checks in Actions. Local builds/tests require a
separate request. See [workflow instructions](github-actions.md) and
[working/validation safeguards](../AGENTS.md). Source documentation changes do
not increment the launcher version.

## Implemented features and remaining limits

| Area | Current implementation | Evidence or remaining boundary |
| --- | --- | --- |
| Startup and collision | Supported TU, missing native callback repairs, serialized coordinate/endian repair | User reported working gameplay and 4–5 Qatar Time Trial laps without falling through the track; not all-track acceptance |
| Career liveries | Scoped synchronous GPU readback with preserved flags/ABI | CPU/ABI checks; first career-race Windows visual acceptance remains outstanding |
| Launcher | Racing Lines, seven pages, ISO/TU/DLC setup, verification, Steam shortcuts/artwork, native controls/help | Wine Win32 UI and persistence checks; native composition, actual imports and Steam review remain separate |
| Settings and saves | Shared JSON, legacy backups, Windows save-folder migration and custom-root preservation | Isolated format/persistence/UI coverage; preserve original saves/configuration during upgrades |
| Input and local play | Xbox/PS3/keyboard prompts, corrected Default action labels, explicit preference preservation, controller/keyboard splitscreen, non-career rider availability | Native gameplay, reconnects and physical-device combinations remain to be checked |
| Native menus | PC menu cleanup, zero private slots, native Quit/Press Start confirmation, themed system dialogs | Native visual/gameplay review remains separate from CPU/input evidence |
| Live settings and achievements | F2 immediate controls, F7/launcher read-only achievement list, shared notification preferences | CPU/persistence/UI checks; GPU, listening and physical-controller acceptance remain outstanding |
| Turkish localization | Native text/font overlay, launcher/account/update strings and 50 achievements | Native-bank/CPU checks; full Windows career/tutorial/HUD visual sweep remains outstanding |
| LAN | LAN02 discovery, direct-IP fallback, guest session/socket bridge and lobby retry repairs | User confirmed earlier two-PC Radmin racing; revised retry/rejoin, larger grids and poor networks need native checks |
| NOBK Online | Fixed production service, shared-site sign-in/token refresh, PC linking, authenticated relay, 378 native statistics views | Source and local protocol coverage; no new production deployment or Internet race acceptance is claimed |
| Native leaderboard display | Lap Times, All Time Ghost and Monthly Ghost column/mode projection | Included in local regression; replay transfer and calendar-based Monthly rollover are not implemented/validated by this display fix |
| NOBK Online panel | F1 or held Back+Start opens friends, server/rooms, records and achievements; English/Turkish UI and notifications | Integrated from upstream `c432d67`; native Windows and live service acceptance remain required; see [panel](nobk-panel.md) |
| Discord activity | Local Windows Rich Presence with Arcade/Career, circuit/class/season, Online/LAN room counts and launcher toggle | Uses public Application ID 1562961932061904906; real Discord/native gameplay verification remains required; see [details](discord-presence.md) |
| Rider License | View profile, Overview/Records, avatar/flag/bio with general emoji, Rider Rank, selected-board lap/map/date and isolated input | Local CPU/ImGui checks and user screenshots; lobby circuit context, season/Moto2 integration and DLC outlines remain deferred |
| Launcher updates | FIFA-compatible manifest/download contract, helper, restart acknowledgement and rollback | Endpoint remains empty; update-preview checks exist, but real download/install/restart/rollback acceptance remains outstanding |
| DualSense adaptive triggers | Default-off SDL resistance profiles in launcher/F1, focus/shutdown reset | Software/transport checks; physical USB/Bluetooth resistance unverified |
| Rendering and performance | Spatial FSR 1/CAS, experimental FSR 2/3 and horizontal-plus ultrawide; scoped runtime fixes | No full temporal reconstruction, all-camera GPU validation or measured native FPS improvement claimed |
| Diagnostics | Native crash monitor, optional F3 diagnostics, actionable warnings | Nine isolated crash scenarios; reporting does not diagnose every exit or certify race stability |

## Rider License statistics

- **Rider Rank:** website overall rider ordering, prioritizing records held, then
  tracks held (17 base circuits plus DLC), points and source position; separate from a selected board's rank.
- **Tracks Held:** distinct playable circuits led, including DLC, out of **20**.
- **Records Held:** circuit/class boards where the rider is first.
- **Boards Entered:** published circuit/class personal-best entries, not unique
  circuits driven. **Total Track Domination** uses Tracks Held / 20.

The profile summary/Records currently use the agreed temporary 2009 scope;
website Rider Rank uses the site's own scope. See [the full contract](gamer-card.md).

## Latest recorded local validation and package

The Rider License source milestone was built locally as Windows Release and
packaged as `MotoGP0910-Windows-x64-v1.1.11-20261022-212459.zip` (37.7 MiB,
41 entries). SHA-256:
`abf223121d12d9b9269d30184bf742c7da6ee0761575e373cefff10c9e0419c9`.
This ignored local archive is not a permanent GitHub download.

Recorded evidence: 60 server tests, 6,807 Rider License checks, 65,765 launcher
viewport checks, 10,807 focused later launcher checks and 31 non-layout Windows
regression scenarios. Launcher validation completed in phases; there was no
final single-run 32/32 report. See [the validation report](rider-license-validation.md)
for corrections, fixture failures and exact scope. Do not rerun broad suites
merely to reproduce historical assertion totals.

These are Wine/CPU/Win32/ImGui results, not native Windows GPU, physical-gamepad,
complete-race or live-service acceptance. Documentation maintenance adds no new
application validation evidence.

## Upstream integration

Rider License was pushed to `main` as `8a33ea9`, then selectively transferred to
`preview` as `ab7d3f7` for the existing
[PR #1](https://github.com/ChunchunQ/motogp0910-recomp/pull/1), targeting
`ChunchunQ:nobk-online`. The extension preserves the earlier production features
and excludes CI, workspace rules, local validation implementations, unrelated
launcher layout fixes, screenshots and historical/original game archives.
The selective integration worktree was not separately rebuilt. The attempted
PR-description update was denied by GitHub permissions; code delivery does not
prove that the description was subsequently edited.

See the [handoff](chunchunq-handoff.md), [full local changelog](local-release-changelog.md)
and [roadmap](roadmap.md). Physics/handling experiments were cancelled and
reverted; frame generation remains excluded.
