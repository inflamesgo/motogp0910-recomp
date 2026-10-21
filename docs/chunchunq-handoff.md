# MotoGP 09/10: online integration handoff

Target integration branch: **ChunchunQ / `nobk-online`**.
Scope: DLC-aware online records, website catalogue, room compatibility,
account recovery and the MotoGP launcher update service. Prepared on 21 October 2026. This document and
[the factual catalogue](online-dlc-catalog.json) are self-contained; no website
or production service was modified during this investigation.

## Integration status at a glance

The comparison baseline is ChunchunQ's `nobk-online` at
`6ca5339e47b1d55ea33486f5da3dead4c29ff290`, fetched on 21 October 2026.
The baseline is already included in the integration source. This document
describes the completed client delivery and its remaining service integration.

| Component | Implemented in our client | Remaining integration |
| --- | --- | --- |
| Launcher updates | **FIFA 09's update-service contract is implemented**, with the existing MotoGP UI and local rollback fallback. | Configure the MotoGP manifest endpoint and publish its ZIP, checksum and notes. See section 5; no new client update backend or UI redesign is needed. |
| Timed boards | Correct native read-column projection and mode labels for Lap Times, All Time Ghost and Monthly Ghost. | Carry the client fix; preserve stored units and metadata. This display correction itself needs no server migration. |
| DLC installation/loading | Required three-package import plus runtime enumeration/mounting from portable `dlc/434307F0`. | Carry the runtime/path support; extend website metadata and verify Moto2/season behavior below. |
| Public room capacity | Private Slots row removed; new PC-hosted rooms use zero private slots. | Carry client menu/bridge changes; retain wire fields for compatibility. No server schema change is needed for this alone. |
| Account UI and recovery | **Forgot password?** and **My account** follow FIFA's shared website URLs. Remember me uses persistent or session-only tokens; country is read from the account API. | Carry the launcher changes. Preserve the optional `country` string on login/token refresh. No new recovery API is needed; Online PLAY still requires sign-in. |

Include the complete client changes listed here in the integration patch. The
remaining actions concern service configuration, website/backend extensions and
native online validation, rather than recreating the completed client features.

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
The existing site labels class slots 0/1/2 as 125cc/250cc/MotoGP. This table does
not assign Moto2 to one of those slots; that separate mapping needs native
submission evidence. Do not append Silverstone at index 17 or compact the hole.

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

## 3. Changes required on the current website

Inspected pages: [Live](https://www.nobkonline.com/motogp/),
[Riders](https://www.nobkonline.com/motogp/ranking/),
[Records](https://www.nobkonline.com/motogp/records/), the individual board/rider
renderers and [How to play](https://www.nobkonline.com/motogp/how-to-play/).
The English and Turkish routes share `/assets/motogp.js?v=7`.

### Required catalogue and rendering work

- `/assets/motogp.js`: `TRACKS` and `SHORT` currently contain only indices 0..16.
  Add indices 18, 19 and 20, retaining index 17 as unavailable. Update `trackOf`,
  naming/facts helpers and all consumers accordingly.
- `/assets/motogp/tracks.json` also has only 17 entries. Add each DLC track's
  correct outline and photo mapping. Wire Turkey/China flags as appropriate;
  the existing general SVG flag fallback can be retained.
- Update the public API's deployed view metadata as well. In repository source,
  `server/motogp_api.py` loads optional `views.json` for track/class/mode/format;
  that deployment file is not checked into this repository. Match the confirmed
  native view IDs above so direct API consumers receive correct circuit names,
  rather than relying on JavaScript to repair numeric placeholder labels.
- Apply the catalogue everywhere: Records cards, individual board headers,
  Live/latest-record thumbnails, Riders ranking, held-track flags and rider
  profile results. Update search/filter labels in both languages.
- Rider ranking uses the set of tracks led, and the UI divides by
  `TRACKS.length`. Do not make that denominator 21 by counting the reserved
  slot. Use the eligible playable set; retain the existing ranking policy unless
  a season/content split is deliberately agreed.
- Add a proper **Moto2** label/badge path. `clsBadge` currently recognizes only
  MotoGP, 250cc and 125cc. Both class and season must remain correct across
  records, profiles and search, not only in a menu heading.
- Keep 2009 250cc and Donington available. Add 2010 content without globally
  relabelling old records or replacing Donington artwork with Silverstone.
- If showing the selected real rider/number/team/bike, use season-aware metadata
  from the catalogue. The current board row shows the account rider, gap, date
  and result; a mode/season field would also make mixed historical records clear.

Public API observations on 21 October: 37 populated group-0 boards were returned;
all inspected best rows had property 13 equal to 0. Queries for the nine DLC
views listed above returned zero rows and numeric placeholder track labels
instead of circuit names, confirming catalogue gaps. This does not prove that no
other 2010 record exists. The public live response had no open room during the
inspection; DLC room behavior was not observed.

### Installation/help content

Update How to play for the next matching release: describe Select ISO → Select
title update → Install DLC → Verify game files, using one action button. All
three supported packages are mandatory in Offline, LAN and Online. Missing
packages are listed inline; no additional missing-DLC dialog is introduced.
Successful installation also closes without an information dialog: the launcher
refreshes its missing-package list and launch readiness directly. Base game,
TU and all three packages are local prerequisites, including direct EXE starts;
older releases are outside this enforcement and no server check is requested.
Clarify the base game's 17 circuits/72 riders separately from the supplied 2010
roster, Silverstone and the two historical bonus circuits. Keep ownership of
original game/TU/DLC files explicit. Download size/instructions should match the
actual published package at release time, rather than a local preview build.

## 4. Backend/client integration work that labels alone cannot solve

1. **Capture a Moto2 submission and read.** The source script type is `gp2`,
   distinct from base-game `250`. Establish how the native game normalizes it
   into statistics views, rider properties and its class selector. The existing
   client validates views 1..378 and the server schema configures that range;
   do not invent a fourth view block, change strides or relabel every 250cc row
   without this evidence. Check gameplay, write, read, native UI and website.
2. **Agree on season-separated records.** Stored property 13 is the season
   selector. Repository storage currently keeps one best row per `(player,view)`;
   a `views.json` label belongs to a view, not to each result. If seasons share a
   view, independent 2009/2010 records need storage/query design, preserving the
   existing native read contract. A label/filter cannot restore an older result
   already discarded. The deployed database/schema was not accessed.
3. **Preserve the native time/mode contract.** Property 2 is the lap in 60 Hz
   ticks; race rating property 3 is `-(ticks*100)`, greatest is best. Preserve
   full mode property 6, including Championship bit `0x10000000`, track property
   7 and season property 13. The client display projection
   fixes time/mode columns for Lap Times (`1..63`), All Time Ghost (`127..189`)
   and Monthly Ghost (`316..378`), with the full native UI column specification.
   Website formatting must not duplicate that projection or change stored units.
   Other reads/categories retain their ABI. This does not add replay file
   transfer; ghost upload/download is deliberately deferred. Monthly period
   rollover is a separate service concern, not guaranteed by this display fix.
4. **Required local content policy.** This client requires all three supported
   packages before launching in every connection mode, including direct EXE
   starts. Keep this requirement in release/help instructions. No server-side
   package check, installed-package mask or content negotiation is requested.
   Older client releases are not covered by this local prerequisite; do not
   claim that the server rejects those clients for missing packages.
5. **Live room detail, if desired.** Current room cards show host/members,
   capacity and lobby/racing state. To display track, class, season or required
   DLC, first expose verified values through the live snapshot/API, then render
   them. Do not guess these from the room name or account name.

Native DLC online writes/reads and mixed-content races were not executed in this
investigation. There is no claim that all client-side Moto2 normalization is
already complete or that only the server needs changing.

## 5. MotoGP launcher update service

**The client already uses the same update backend protocol as FIFA 09. The
remaining service work is to connect the MotoGP-specific URLs and release
metadata, not to implement another updater.** Source reference:
[FIFA 09 `launcher/Updater.cs`](https://github.com/ChunchunQ/fifa09-recomp/blob/711da3941d81879db06ea4f2c36c3a519ac826d9/launcher/Updater.cs).
The MotoGP update UI is retained.

1. Provide a **MotoGP-specific HTTPS manifest endpoint** and set
   `src/launcher/updates.h::kConfiguration.manifest_url`. It is currently empty,
   so normal update checks are disabled. Do not reuse FIFA's endpoint unchanged:
   its observed response points to a FIFA package.
2. Serve GET with `?v=<installed-version>` (`&v=` if the URL has a query) and
   `User-Agent: NOBK-Launcher/<installed-version>`. No account token or request
   body is needed. Return HTTP 200 with FIFA's lowercase fields:

   ```json
   {
     "version": "1.1.11",
     "url": "https://<MotoGP-download-host>/<versioned-MotoGP-package>.zip",
     "sha256": "<64 hexadecimal characters: SHA-256 of the actual ZIP bytes>",
     "notes": {"en": "Release notes.", "tr": "Localized release notes."}
   }
   ```

   This is a contract example, not a live endpoint or an approved next version.
   The client constant remains `1.1.10`, the earlier updater milestone. Later
   feature groups are not yet assigned release versions; agree on the actual
   delivery version before publishing the manifest. No new version is approved
   by this document.
3. Upload the complete MotoGP Windows package before publishing its manifest.
   The URL must use HTTPS and end in `.zip`; use the package ZIP itself rather
   than an outer Actions artifact. Keep a versioned URL stable and compute the
   checksum after packaging. Include the update helper in `bin/`.

Numeric version comparison, localized notes, download and SHA-256 verification
are implemented. MotoGP additionally keeps its existing **local fallback
mechanism**: on a handled installation or failed-startup error, restore the
previous application files. Short comments in `update_helper.cpp` and
`update_install.cpp` identify this fallback for handover. It does not require
extra server fields and does not replace game files, DLC, saves or settings.

Client implementation: `updates.h/.cpp`, `update_panel.inc`,
`update_install.cpp`, `update_helper.cpp`, `update_protocol.h`, plus their
launcher/CMake/package wiring. Download/install/restart/rollback still require
an end-to-end check against the real MotoGP service before enabling releases.

## 6. Other online client changes to preserve

- **Portable DLC content:** import writes package payloads under
  `<launcher>/dlc/434307F0/00000002/<package-id>` and SDK metadata under
  `Headers/00000002`. There is no `0000000000000000` parent. The runtime adapter
  in `src/dlc_content.cpp/.h` must accompany `src/dlc_paths.h`, the shared local
  checker `src/dlc_installation.h`, `src/launcher/dlc_packages.h` and launcher
  import/readiness changes. Otherwise moving installation alone will not make
  the game find or require the content. It routes marketplace-content enumeration, open, close and delete
  while preserving the SDK's save-data handling and package license masks.
  All three packages are required locally and user-supplied; the launcher does
  not download them or request a server-side package check.
  Existing DLC in the old Documents content store is not automatically imported
  into this portable location. Users must import their packages again or move
  their existing `434307F0` directory, including `Headers`, into `dlc`.
- **Private Slots:** `src/options_menu.cpp` removes the row from native match
  settings and sets its value to zero. `src/lan/guest_bridge.cpp` combines
  public/private capacity into public capacity for new local hosts and capacity
  changes. Keep the serialized field and existing join behavior for older hosts;
  this does not remove authentication, admission or capacity checks.
- **Account recovery:** the signed-out Online page adds **Forgot password?**
  beside Sign in/Create an account. Like FIFA's `Forgot_Click`, it opens
  `https://nobkonline.com/sifremi-unuttum/` in the default browser. The launcher
  neither requests a reset itself nor submits a password to that URL. See
  `network_page.inc`, `nobk_account.h`, `ui_ids.h` and the action in `main.cpp`.
- **Account session and display:** preserve default-on Remember me and its
  `launcher_remember_me` preference. Remembered tokens and optional country are
  DPAPI-protected; unchecked sessions/refreshes stay in memory. Country comes
  only from the optional two-letter account API field, on login/token refresh.
  **My account** opens `https://nobkonline.com/hesabim/`, as in FIFA. The unframed
  signed-in page uses the orange account name, optional country and host-room
  field below account actions. Username/password remain side by side. The
  signed-out notice correctly requires sign-in for Online PLAY, with the
  existing error and no automatic Offline fallback. Recovery has no hover tip.
  Room-name help reads "Used when you host a room. Up to 31 characters."
- **Rider selection:** the client makes installed riders available in
  non-career selection, including non-career multiplayer; Career progress is
  preserved. It supplies neither missing DLC nor DLC compatibility negotiation.
  This is client behavior to consider for release consistency, not a new
  website entitlement or a mandatory server-side unlock operation.

NOBK sign-in, account-to-PC linking, the 60 Hz website time correction and the
latest QoS/relay diagnostics are already present in the baseline branch.
Preserve those upstream changes; do not count them as new work from this handoff.

## 7. Transfer boundaries and dependencies

Start a clean integration branch from **ChunchunQ's `nobk-online`** and transfer
reviewed feature hunks. This source delivery to our own fork is not that PR.
Do not merge our whole `main` or cherry-pick mixed feature commits blindly.

- Carry the client features in the status table, their headers/implementation
  files and the necessary CMake/application lifecycle wiring. The portable DLC
  adapter, shared prerequisite checker and launcher importer belong together.
- Keep updater protocol/install/helper files and their build/package wiring
  together. `third_party/fifa09-launcher-LICENSE.txt` and the matching notice
  must accompany adapted FIFA code; the existing archive-reader license/source
  requirements still apply. Do not transfer original game/DLC containers.
- Split mixed `motogp0910_app.h`, CMake and launcher changes by feature. Save-data
  migration, local splitscreen/native Quit, Steam artwork and unrelated Turkish
  layout polish are outside this online-only scope. If full files are copied
  instead, their helper sources/libraries must also be carried; omitting their
  dependencies would break the build. Prefer the smaller reviewed hunks.
- Exclude `.github/`, root README edits, `AGENTS.md`, private environment/setup
  instructions, screenshots, historical ZIPs and the full local changelog.
  This English handoff and `online-dlc-catalog.json` are the service reference.
- Publish the real application ZIP for launcher updates, never a GitHub Actions
  artifact wrapper. An empty `dlc/` beside `game/` is part of the package layout;
  neither directory should contain original content in a distributed build.

## 8. Acceptance evidence for integration

- One 2009 250cc and one 2010 Moto2 result with raw view ID/properties, native
  class/season selection, in-game time and matching website output.
- One result on each of Silverstone, Shanghai and Istanbul, with correct name,
  historical layout, flag, map/photo, board link and rider-profile entry.
- A faster/slower repeat proving the best result and associated metadata remain
  consistent; a season change that does not mislabel historical records.
- Base-only and partial installations must remain blocked locally in Offline,
  LAN and Online; completing all three packages must enable launch. Check
  direct EXE starts, archive/folder installation and matching-DLC races.
  No server-side package negotiation is part of this policy.
- English/Turkish Records, Live, Riders and How to play consistency. No reserved
  Balaton entry in playable track totals; no duplicated 2010 GP roster.
- If updates are activated, verify the real MotoGP manifest, successful restart
  and a failed installation/startup rollback.
- Confirm zero private slots/full public capacity when creating or updating a
  room, and confirm the signed-out recovery action opens the shared NOBK page.

The final source review performed no new application build, gameplay/UI/server
or probe tests, service deployment or upstream PR. A previous local Windows
build predates the final DLC-popup removal and Turkish footer polish; it does
not validate this complete delivery. Native online/Moto2/season behavior and
real-service updater/rollback acceptance remain pending.
