# NOBK Online panel

Integrated from ChunchunQ's `nobk-online` commit `c432d67` after PR #1 was merged.
Press **F1**, or hold **Back + Start** for about 450 ms on a controller, to open
the English/Turkish panel in production Online mode. F1 and the controller
combo do not open this panel in Offline/LAN mode. Live settings use **F2**.
The old F4/F1 settings binding migrates to F2; other custom settings bindings remain unchanged.

The four tabs show:

- Friends: NOBK friends and whether they are offline, in MotoGP menus, in a room
  or racing. The shared friends endpoint can also report FIFA 09 activity.
  Selecting a friend opens the public account profile in the browser.
  With the optional matching server update, Invite sends a community lobby
  invitation; incoming invitations can be accepted or declined. The native
  lobby Invite entry opens this tab as a friend picker. See the
  [friends/invitations implementation and deployment](online-friends-invites.md).
- Server: status, rider/record totals, open rooms, hosts, membership/capacity and
  which rooms contain friends. This is a view of existing rooms, not a join API.
- Records: the current rider's summary, recent public records and an activity
  feed. Notifications include friends becoming active, room activity and record
  changes detected by polling.
- Achievements: the complete local catalog, original icons, locked/unlocked
  descriptions and gamerscore, overall progress, All / Unlocked / Locked
  filters, search and a scrollable list. Pop-up, unlock-sound and volume
  preferences are shared with the launcher and saved immediately.
  This tab does not wait for website data. F7 is disabled in Online mode;
  Offline/LAN retains the dedicated F7 achievement overlay with the same
  content and preferences.

The panel is available for the configured production Online service. It reads
public website/API data on a background worker and does not send launcher tokens
or passwords. Community invitations use a separate worker and the existing
authenticated MotoGP profile; public website requests receive no profile secret.
Refresh intervals depend on whether the panel is open; the live
snapshot is polled about every five seconds while open and twenty while closed.
Other public datasets use their own longer refresh intervals. The panel shares
the existing input gates so settings, Rider License, quit handling and the panel
do not acquire gameplay input simultaneously. Its pad polling joins the existing
frame observer; Discord retains its separate guest-frame capture path.

Upstream also changed Rider Rank to records held first, then distinct tracks
held (including DLC indices 18–20, excluding slot 17), points and API source
position. Launcher version is 1.1.12 and the configured update manifest is
`https://nobkonline.com/downloads/motogp/version.json`.

Integration validation is limited to changed-source compilation and the focused
ranking/settings regression expectations. Native Windows controller/UI behavior,
production public API responses and a live launcher update transaction require
separate acceptance; merging the feature does not establish those results.

## Maintenance map and boundaries

`nobk_panel.cpp::State` owns one background worker, public HTTP access, the
snapshot and the session event/toast queues. `State::Apply` combines friends and
MotoGP presence by ASCII-case-insensitive player names. The panel does not
introduce a new durable identity or friends database. `Dialog` renders snapshots
and reads the existing local AchievementManager; it does not publish lap results
or unlock achievements. `FrameTick` samples open-menu input before polling the
controller combo. Preserve that observer composition when changing frame work.

| Source | Consumer | Meaning |
| --- | --- | --- |
| `/api/account/friends_of?name=...` | Friends list | Existing shared NOBK friendship data; its `online` flag denotes the shared FIFA lobby presence |
| `/api/fifa09/player?name=...` | Local profile header | Existing shared account metadata used by Rider License; country and `member_since` only, with the returned account name required to match |
| `/api/motogp/live` | Friends and room state | MotoGP connection, room members, hosts and room race state; not precise solo Career/Arcade activity |
| `/api/motogp/status` | Server counters | `in_race` counts racing rooms, not individual racing riders |
| `/api/motogp/player?name=...` | Own record/podium/board counters | Website summaries across published player-best entries |
| `/api/motogp/recent?limit=12` | Latest records | Recently updated published player bests, including positions below first; not every completed lap |
| Local AchievementManager | Achievements tab | Complete local list, filters, search and notification/sound preferences; F7 provides the same content only in Offline/LAN |

Live/status are requested about every 5 seconds while open; closed polling uses
20 seconds and omits status/recent. Successful friends reads schedule their next
refresh for 20/60 seconds (open/closed); own records use 15/45 seconds; recent
entries use 15 seconds while open. Failed fetches use the ordinary polling loop,
not those successful-refresh deadlines. There is no push/WebSocket subscription.

The first successful snapshot seeds state without announcing existing activity.
Later polls enqueue a toast when a friend enters MotoGP or hosts a new room,
or when the set of first-place boards owned by the current rider gains/loses a
view. A faster lap on an already-owned board does not change that set. Keep this
ownership-based notification separate from personal-best improvement messages.
The event feed holds at most 30 entries and the toast queue at most six; these
are session memory, not persistent history. Toasts last about six seconds.

Friend selection opens a browser profile outside the native invite picker;
room rows remain informational. Accepting a community invitation triggers the
game's native join flow. Adding friends, downloading ghosts and changing records
are not panel operations. Offline/LAN/custom service origins must not be sent to the production
public APIs. The keyboard shortcut is configurable through `bind_nobk_panel`;
its F1 default and the F2 live-settings migration must remain coordinated.
Keep exclusive input gates, focus/neutral rearming, guest keystroke blocking and
shutdown lifetime intact when adding panel actions. The Achievements tab owns
keyboard/controller navigation only while visible and restores the previous
ImGui navigation flags on exit. Q/E shortcuts must not run while typing in
the achievement search field. W has no browser shortcut in the panel. HTTP and JSON processing
belong on the worker; game input and local achievement access keep their current
thread ownership.

Friends, room and record rows highlight the mouse selection without requesting
scrolling. Only explicit keyboard/D-pad row navigation reveals the selected row;
mouse-wheel and scrollbar scrolling must remain stable instead of repeatedly
centering the selection. Keyboard/D-pad navigation takes priority over hover for
that frame.

Rider Rank uses MotoGP player IDs and current first-place board ownership,
not panel nickname matching. Its order is `firsts`, unique playable circuits,
points, then the original API position. Count DLC indices 18–20, exclude 17,
and include all published seasons in the Rider License summary and records
list as well. Known record years come from property 13; unknown years must not
be presented as 2009.
The update manifest activates the already implemented launcher protocol; it does
not change its HTTPS/checksum/rollback contract or establish that a package is
published. Preserve both the launcher version and the configured MotoGP route.

The integration adds one concurrency correction to the upstream panel: its
language flag is atomic because the UI sets it after worker construction and
worker-side record/event formatting reads it. Other panel behavior is retained.
Do not move mutable UI settings into the worker snapshot without defining their
publication/locking contract.

Integration checks on 23 October 2026: the merged application passed a Windows
syntax check; the settings library, new panel object (including the language
publication correction) and Rider License probe cross-compiled successfully.
The updated Rider License data/ImGui regression passed under Wine in an isolated
fixture, including records-first ordering, DLC inclusion and reserved-slot
exclusion. Settings keyboard regression expectations were updated to F2; the
interactive settings/controller suite was not rerun. No full game package or
live API/update transaction was exercised for this merge.

The later full Achievements integration shares its renderer and preferences with
the Offline/LAN F7 overlay. Its focused Windows/Wine validation is documented in
[achievements.md](achievements.md#f1-integration-validation--23-october-2026).

## Apex presentation

The current F1 renderer follows the user-approved left-side Apex concept. The
approved image is a visual reference, not a shipped game background. The game
scene remains live behind a feathered left-to-right dark scrim. Darkening extends
across the full content width, including the right-aligned times and scores,
before fading out over the remaining game scene. There is no outer
panel border, card stack, diagonal row strip or footer shortcut text.

The Friends reference positions use a 1920 x 1080 canvas: wordmark at (94,76),
profile avatar at (398,70), vertical sections at x=96, and list origin at
(356,214). Friend avatars, names, national flags and activity occupy fixed
columns; seven 100-pixel rows with 2-pixel spacing reproduce the reference.
The layout scales with viewport height and constrains its width on narrow
screens. The right side stays available for the game scene.

The cover-inspired racing decoration uses seven thin strands in orange,
silver-violet, champagne, coral, cream, muted violet and amber. Four secondary
strands are shorter and dimmer. Their sampled geometry is cached once; small
light highlights travel at staggered 8.5–17 second periods using elapsed time,
without moving the paths or content. Drawing stops while F1 is closed and uses
ordinary batched lines, without textures, blur passes or background work.
The selected row
has a small orange pointer and underline, without a filled background. Friend
avatars are charcoal initials with separate presence dots; the local profile
avatar is orange. National flags use the existing embedded atlas also used by
Rider License, with the uppercase two-letter country code beside each flag.
The local profile header adds its small country flag, country code, localized
membership month/year and friend count on one secondary line. It reads optional
shared account metadata on the existing public-data worker only while the panel
is open, refreshing every five minutes or retrying after one minute on failure.
Live panel data is published before this optional request. The response must
match the current account name; absent data is hidden, and request failure keeps
the last valid metadata without changing the panel's live-service error state.
Friend, lobby, record and session-feed rows do not repeat their contents in
hover tooltips. Achievement descriptions appear directly in their rows.
Invalid or missing country data does not
invent a flag. No HTTP request or new asset download is required for flags.

The four sections retain their existing actions, shortcuts and input gates.
Their vertical navigation shows Q/E, Xbox LB/RB or PlayStation L1/R1 according
to the same active prompt style and manual override used by the native game.
Scrollbar styling is thin and rounded. Achievement filters, search, original
icons, trophy fallback and notification/audio preferences are preserved, with
the volume control occupying the remaining F1 width. F7 retains its separate
layout. The other sections follow their separately approved Apex references:

- Server uses three large live counters, rider/record totals and spacious lobby
  rows with host, membership/capacity, racing/lobby status and friend-room hints.
- Records keeps all previous fields: rider, rank, first-place record action,
  circuit/class, relative update time and lap time. Its three personal totals
  remain above the list. The record list and complete session feed scroll
  independently; keyboard reveal does not interfere with manual scrolling.
  Record initials have no online-status dot because recent entries do not
  provide presence. Unavailable personal summaries display a dash.
- Achievements uses larger original icons, wrapped full titles/descriptions,
  right-aligned gamerscore and unlock state, progress, inline filters and search.
  The preferences remain shared. The thin volume track uses the native slider
  for mouse, controller and Ctrl-click numeric input; only its drawing changes.

These sections use a wider content area than Friends while keeping the same
header, vertical section navigation, cover-inspired animated lines and scrim.

`nobk_panel_visuals.h` owns presentation measurements and drawing primitives.
The existing `Http` and `State` class bodies remain unchanged. The three font
roles reuse Open Sans Regular, SemiBold and Bold; CMake and the Windows packager
stage these under `resources/ui`. The fonts are already covered by the bundled
Open Sans notice. Missing optional faces fall back to the existing UI font.

`MOTOGP_NOBK_VISUAL_PROBE` is defined only for the isolated UI probe, not the game
target. Its reference fixture reproduces the seven sample users and flags; its
larger fixture covers scrolling, long names and non-ASCII text. Draw captures
include the actual decoded flag texture. Test profiles and synthetic data are
never added to application packages.
