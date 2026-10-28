# MotoGP 09/10 integration handoff — 28 October 2026

Source anchor: **`inflamesgo/motogp0910-recomp` main at `28ba765`**,
`feat(port): add season leaderboards and polish invitations`. Documentation
review after that commit adds no new application behavior or release version.
Target: **`ChunchunQ/motogp0910-recomp:nobk-online`**. Its branch tip was
read on 28 October as `c432d67ffb95a2c9383493b9a8c7a723b9ed52b0`.
This is a source-content reference, not a claim that a new PR is open or merged.
The earlier PR #1 and `preview` delivery are historical, not instructions to
append to an open PR.

## Read order and release decision

1. [PR draft, transfer dependencies and validation](pr-handoff.md).
2. [Season/class backend contract and migration](leaderboard-season-integration.md).
3. [Critical website and installation corrections](website-release-checklist.md).
4. [Native/Discord invitation and ghost ownership](chunchunq-friends-invites-ghost-handoff.md).
5. [Verified DLC catalogue](online-dlc-catalog.json) and the facts below.

**The source is ready for an integration review, not a claim of complete
production deployment. The highest-priority release work is the scoped
season/class backend and its matching website/API/help update.** Do not advertise
independent 2009/2010 records or working ghost downloads until their respective
end-to-end gates pass. The client deliberately refuses mixed legacy boards.

| Area | Delivered in the source anchor | Remaining integration |
| --- | --- | --- |
| Season/class leaderboard | 2009/2010 selectors, native transitions/history, class-specific headings and native announcements; explicit read/write identity | Scoped service/storage routes are absent from current `server/`; public API/site consumers need matching identity and migration |
| Invitations | Native F1 picker, accept/decline, per-friend feedback, pit-board incoming toast and room validation; Discord warm/cold-launch code | Shared immutable friend IDs, matching service/proxy deployment, real-account and cold-launch acceptance |
| Ghosts | Native analysis and proposed lifecycle/API; older synthetic transport success reported by the user | No tracked ghost transport/service/task/save/playback implementation in this source anchor; native binding is ChunchunQ integration work |
| UI/HUD/input | Apex panel, startup strip, friend-online card, four custom gauges, control/layout fixes and precise URT2 shift-light source changes | Carry resource/runtime dependencies; do not claim a fresh full-game/controller QA pass |
| Updater | Launcher 1.1.12, configured MotoGP manifest, install helper and rollback | Publish actual package/manifest and verify real install/restart/rollback |
| Rider License | Public profile, rank, 20-circuit domination and selected-view personal best | Already includes published rows from both seasons; view-only public identity, DLC outline bindings and lobby context need integration |

Native Windows user acceptance of the latest test package covers both seasons,
all three class selections, forward/Back navigation, text and audio. The user
also reported Discord invitations working locally. This does not validate
production scoped storage, a real two-player Internet race or ghost playback.
See the exact build/log boundary in [the reconstruction report](leaderboard-menu-recovery.md).

## 1. Confirmed DLC contents

The three original packages were inspected, including their payload inventories
and all 24 English metadata scripts. Metadata was decoded and parsed as data,
without executing Lua scripts. This supersedes the earlier incomplete two-track
summary.

| Package | Confirmed additions |
| --- | --- |
| Free GP Class Season 2010 Update | 17 GP rider entries, 2010 teams/bikes, **Silverstone**, 2010 track assets and season schedules. |
| Twin Track Pack | **Istanbul Park (2007)** and **Shanghai International Circuit (2008)**, with 2009/2010 AI spline data. These are historical bonus circuits. |
| Free 2010 Moto2 and 125 Update | **40 Moto2 (`gp2`) and 27 125cc rider entries**, their teams/bikes, Silverstone and 2010 track assets. It also supplies the same 17 GP rider entries and identical GP team/bike fallback tables as the GP package; do not count these twice. |

The base roster has 72 entries: 18 GP, 23 250cc and 31 125cc. The supplied 2010
roster has 84 entries: 17 GP, 40 Moto2 and 27 125cc. These are metadata counts,
not a claim that every rider has passed a native online selection/race test.
The catalogue includes season-specific indices, rider names, numbers and teams.
Indices are reused across seasons, so a 2009-only rider lookup is insufficient.

The supplied 2010 GP schedule has 17 rounds, using Silverstone instead of
Donington. The supplied 125cc schedule has 16 rounds and omits Laguna Seca.
There is no separate `gp_2010_gp2` table in these schedule files: verify its
runtime binding rather than inventing one. Istanbul and Shanghai are not extra
rounds in those calendars. No Aragon or Balaton track definition/geometry was
found in these three packages.

## 2. Exact track mappings

The native track-key pointer array has 21 slots. The first 17 match the live
website's base-game list; the remaining four are:

| Native index | Native key | Content status | Group-0 views for existing class slots 0 / 1 / 2 |
| --- | --- | --- | --- |
| 17 | `balaton_2009` | Reserved/unpopulated in the supplied content; do not advertise as playable. | 18 / 39 / 60 |
| **18** | `silverstone_2009` | Silverstone; asset basename `silverstone_2010`, present in packages 1 and 3. | **19 / 40 / 61** |
| **19** | `shanghai_2009` | Shanghai (2008); asset basename `shanghai_2010`, package 2. | **20 / 41 / 62** |
| **20** | `istanbul_2009` | Istanbul Park (2007); asset basename `istanbul_2010`, package 2. | **21 / 42 / 63** |

Existing formula: `view = 1 + group*63 + class_slot*21 + native_track_index`.
The existing site labels class slots 0/1/2 as 125cc/250cc/MotoGP. The current client maps Moto2 and 250cc to native slot 1,
separated by season; see the scoped protocol below. Real upload/read
acceptance remains required. Do not append Silverstone at index 17 or compact the hole.

The native keys still end in `_2009` while their asset basenames end in `_2010`.
Neither suffix alone identifies the record's season. Across the base game and
these three packages there are **20 distinct supplied circuit locations**, not
21 playable circuits. A season/class may have fewer eligible circuits.

Track outline/photo sources are `frontend_track_dlc1.mgb`,
`frontend_track_dlc2.mgb` and `frontend_track_dlc3.mgb`. Use the appropriate
historical layouts and associate them with the native indices above.
**Do not publish the DLC scripts' distance/turn fields without checking them:**
Silverstone and Istanbul repeat 4023 m / 11 turns, while Shanghai repeats
4051 m / 14 turns. These duplicate base-track values and may be placeholders.
They are preserved as unverified source facts in the catalogue.

## 3. Website and independent records: mandatory next work

Use the [current website checklist](website-release-checklist.md); it supersedes
the old 17-track/v7 observations. The 28 October read-only check found v9,
20 artwork entries and DLC catalogue/denominator support already present.
Season identity, separate Moto2 boards and the old Back + Start help text still
need attention. Do not ask the operator to redo completed DLC work.

The agreed display classes are:

| Season | A | B | C |
| --- | --- | --- | --- |
| 2009 | MotoGP | 250cc | 125cc |
| 2010 | MotoGP | Moto2 | 125cc |

This replaces the earlier simultaneous A/B/C/D proposal. Badge B in both
seasons is intentional; a shared leaderboard identity is not. Native slot 1
requires a season discriminator. The existing native views remain 1..378;
do not invent a fourth view block or change the 21-track stride.

The exact client protocol and conservative data migration are specified in
[season integration](leaderboard-season-integration.md). The current service
uses one score per `(player, view)` and lacks the new scoped routes. Both reads
and race uploads require the updated service. Menu access does not make the
missing-capability warning harmless or establish that results were stored.
Existing overwritten bests cannot be reconstructed from surviving rows.

## 4. Rider License contracts and remaining integration

The native Online lobby/leaderboard **Gamer Card** action now reads **View
profile** / **Profili incele** and opens **RIDER LICENSE** / **PİLOT LİSANSI**.
Overview and Records share an avatar/flag, inline membership date, optional
three-line bio with a full-bio popup and a compact browser action. Keyboard
bindings still work, but the footer displays controller hints only and centers
the complete hint group on the same right-pane center in both tabs. This is
client implementation; it requires no new friends/social service or SDK patch.

- **Identity and public account data:** resolve the selected MotoGP XUID's
  nonzero 24-bit ID through `/api/motogp/player?id=<id>` on
  `https://www.nobkonline.com`, requiring the response ID to match. Shared
  country, membership, bio and avatar metadata comes from the general site's
  `/api/fifa09/player?name=<resolved-name>`, requiring a matching account name.
  The shared endpoint name is intentional, not a numeric FIFA identity lookup.
  The browser action opens `/profile/?u=<resolved-name>`. No credentials are
  sent. Production Online mode alone uses this mapping; Offline, LAN and other
  community servers must not resolve their local IDs against production NOBK.
- **Metric definitions:** **Tracks Held** counts distinct playable circuits
  with at least one first-place board; **Records Held** counts first-place
  boards; **Boards Entered** counts separate published circuit/class boards,
  not distinct circuits ridden. **Total Track Domination** uses Tracks Held
  divided by 20, including DLC and excluding reserved slot 17. For example,
  17 first-place boards across 16 circuits means 16/20 = 80%, with 19 board
  entries possible. The current site's v9 catalogue also counts 20 supplied circuits, excluding
  reserved slot 17; the older 17-track difference is historical.
- **Rider Rank:** a separate compact header value reproduces the website's
  ordering. Join `/api/motogp/ranking` players with first-place owners from
  `/api/motogp/views`, then sort by `firsts` (records held) descending, distinct tracks held
  descending, `points` descending and original API `pos` ascending. Renumber
  the result and select by MotoGP player ID. Using raw API `pos` alone can give
  the wrong displayed rank. The current API exposes at most 100 riders; an
  absent or unavailable rank is a hyphen, never a guessed placement. This rank
  follows the site's full ranking scope, and is independent of a selected native board. The old 2009-only card filter
  has been removed. When the site's track catalogue/ranking contract changes, update
  `ReadRanking` in `src/gamer_card.cpp` too: its current track membership
  check includes indices 0..16 and 18..20 and excludes reserved slot 17. Do not leave the two policies drifting.
- **Season and class limits:** Overview/Records include all published 2009/2010
  rows, including DLC. Property 13 supplies the displayed year; unknown legacy
  years are not invented. This fixes the earlier 2009-only filter, not storage
  separation: `ReadResults` and `ReadRanking` still deduplicate boards by view
  alone. A scoped public API must update both identity validators and selected
  personal-best context together, not just class labels.
- **Selected personal best and lobby context:** verified leaderboard calls
  carry the exact selected view. Personal Best matches that view for lap,
  class/mode, rank/population and date, never the fastest lap on another board.
  Lobby openings currently have no board context and show **No track selected**;
  they do not inherit the last leaderboard view. Later lobby integration must
  provide verified track/class/season/mode values before this panel can use
  the lobby's circuit. The personal-best context currently supplies a view, not an independent scoped
  year/category. Carry the selected scope when extending the public contract.
- **Set time:** both Records and Personal Best use the accepted result's
  existing `updated` timestamp, relative below seven days and then a date,
  with full local date/time on hover. The score write path preserves this time
  for equal/worse submissions; no extra date endpoint is needed.
- **Art and native index mapping:** base outlines use explicit filenames under
  `/assets/motogp/tracks/`, retaining native indices. Losail's transparent
  `omap-qatar.png` was retrieved and visually inspected. The 28 October public
  catalogue now names `omap-silverstone.png`, `omap-shanghai.png` and
  `omap-istanbul.png` for indices 18–20. The client table still has empty entries
  there and shows **Map unavailable**. Bind and visually verify those published
  assets in `kTrackMaps`; do not re-extract or substitute another
  circuit. The transparent default helmet is embedded RCDATA; flag artwork
  uses an embedded atlas with its flag-icons MIT attribution.
- **Loading and diagnostics:** identity/records publish before optional
  metadata/photo work; ranking and outlines follow on the cancellable worker.
  Preserve the five-second HTTP deadline, initial transient identity retry,
  per-player cache, shared 60-second ranking cache, per-circuit artwork cache
  and close/switch cancellation. The renderer uses preloaded font sizes for
  sharp text. Routine debug traces are removed; actionable transport, data,
  decode, font, texture and browser failures remain warnings in the game log.

Transfer dependencies include `src/gamer_card.cpp/.h`, `gamer_card_flags.h`,
the embedded helmet resources, native menu label replacements, the shared input
gate and application lifecycle/font wiring, Windows WinHTTP/WIC/COM libraries,
and flag-icons attribution in both source and embedded notices. Do not copy the
dialog alone or omit its input isolation/cleanup. Personal Best now centers its
circuit name, lap and a single class/known-season/mode line in the middle column, with
equal insets between the map and rank dividers. The outline and rank/date retain
their positions. This spacing adjustment is included along with the other
Rider License refinements. Emoji PNG/metadata resources, their regeneration
script, Noto OFL notice and the flag-icons MIT attribution accompany the feature.

Native DLC online writes/reads and mixed-content races were not executed in this
investigation. The reconstructed client now separates the menu and query identity, but
production scoped storage, public API consumers and real result acceptance
remain incomplete.

## 5. Launcher update and setup contract

The current launcher is **1.1.12**, with manifest
`https://nobkonline.com/downloads/motogp/version.json`, configured in
`src/launcher/updates.h`. It is not empty or disabled. Keep the existing
FIFA-compatible lowercase `version`, HTTPS ZIP `url`, actual ZIP `sha256` and
localized `notes` fields, `?v=<installed-version>` and
`User-Agent: NOBK-Launcher/<installed-version>`.

**Before publishing this update, the operator must increase the launcher's
source version above the currently published release (baseline 1.1.12)** in
`src/launcher/updates.h::kConfiguration.current_version`, then rebuild. The
built launcher, public `version.json`, release notes and website download label
must use the same new version. Publish the actual final ZIP URL/size/SHA-256;
changing the manifest alone does not update the binary's reported version.
The original game's TU remains 0.0.1.1. This handoff does not select or publish
a new release number on the operator's behalf.

Publish the complete matching application ZIP before its manifest, using the
application archive itself. Current Actions stages application files directly;
do not instruct users to unpack a ZIP inside another Actions ZIP. The package
opens to `MotoGP09-10Launcher.exe`, `bin/`, `resources/`, `game/` and `dlc/`.
Keep `game/` and `dlc/` empty in distributions. Preserve the helper, embedded
notices, archive-reader source and configured rollback. No live updater
transaction was performed for this documentation review.

The one General-page action follows **Select ISO → Select title update →
Install DLC → Verify game files**, depending on files already present. All
three supported DLC packages and TU 0.0.1.1 are local requirements in every
mode, including direct game starts. Older Documents DLC installations need
reimport or migration of `434307F0` including `Headers` into portable `dlc/`.
Do not add a server-side DLC entitlement check or promise automatic import.

Account sign-in, PC linking, DPAPI token/profile storage, optional Remember me,
website recovery/My account actions and no silent Online-to-Offline fallback
remain unchanged. See [community/account operations](community-online.md).
New local rooms retain zero private slots with existing serialized capacity
fields, keyed admission and older-host compatibility. Non-career rider unlocks
do not alter Career progression or supply missing game content.

## 6. Transfer boundaries and reproducibility

Use [the PR dependency manifest](pr-handoff.md). Compare final file contents
against the actual target, because historical integration/cherry-pick commits
are not a reliable missing-feature list. Do not merge all `main` or copy mixed
CMake/application files blindly. Preserve upstream account, QoS, lap-time and
rank fixes. Include the required runtime and approved converted gauge resources;
raw donor/game data must never be transferred.

Exclude root README/workspace rules, CI configuration unless separately agreed,
private setup/credentials, generated code, player data, logs, old development
archives and screenshot deletions. Public social/Discord server tests accompany their modules. New local
validation `tools/` implementations and probe targets remain excluded under
the established transfer rule; historical main-branch probe results retain
that provenance. Do not describe missing ghost tests as supplied files.
There is no new `source.patch`/`verification.json` handoff bundle from this pass.
The committed repository and the files named in the PR manifest are the source.

The Windows SDK pin is `0.10.0.24-dev.gbd833a2`; the custom F1-backdrop runtime
manifest must match. Copying only an EXE or mixing runtime DLLs breaks that
contract. Build/package checks and older Wine/CPU observations retain their
specific scope. The latest user-tested ZIP predates only removal of temporary
menu debug calls; it is not a new upstream integration build.

## 7. Acceptance still owned by integration

- Deploy scoped service/proxy/storage and verify two seasons and middle classes
  independently for the same rider/circuit, with faster/slower repeats.
- Verify raw score units, timestamps, paging, around-user and Friends/global
  reads, all native board groups and actual race uploads; preserve the old data.
- Complete the critical website/help checklist in English and Turkish and check
  direct board/rider links, ranking scopes and DLC identities.
- Verify native NOBK invitations with two real linked accounts, decline/expiry,
  closed/full/replaced rooms, gameplay leave confirmation and controller input.
  Test Discord warm acceptance and cold launcher activation separately.
- Keep ghost downloads unavailable until native fixtures, complete format
  validation, task completion/cancellation, save integration and two-client
  on-track playback pass. Synthetic transport alone is not this evidence.
- Retain broader native Windows gameplay/GPU/controller and actual updater
  transaction gates from the feature documents. No broad tests, service changes or build are part of this documentation review.
  The user subsequently authorized a selective commit/push to `preview`; PR
  opening and deployment remain with the user/operator.
