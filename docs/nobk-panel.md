# NOBK Online panel

Integrated from ChunchunQ's `nobk-online` commit `c432d67` after PR #1 was merged.
Press **F1**, or hold **Back** for about 450 ms on a controller, to open
the English/Turkish panel in production Online mode. F1 and the controller
hold gesture do not open this panel in Offline/LAN mode. Live settings use **F2**.
The old F4/F1 settings binding migrates to F2; other custom settings bindings remain unchanged.

The four tabs show:

- Friends: NOBK friends and whether they are offline, in MotoGP menus, in a room
  or racing. The shared friends endpoint can also report FIFA 09 activity.
  Selecting a friend opens the public account profile in the browser.
  With the optional matching server update, Invite sends a community lobby
  invitation; incoming invitations can be accepted or declined. The native
  lobby Invite entry opens this tab as a friend picker. See the
  [friends/invitations implementation and deployment](online-friends-invites.md).
  [Discord invitations](discord-invites.md) use the same native accepted-session
  path and report their progress through the panel's existing toast renderer.
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

Current integration note (28 October 2026): this public-data panel and Rider
License are separate from the new native scoped stats path. Both must adopt the
[scoped public identity contract](leaderboard-season-integration.md) when the
service/site is upgraded; a view-only public record is not six independently
stored season/class combinations. See [current invitation presentation](invitation-ui-concept.md)
and the [operator handoff](chunchunq-friends-invites-ghost-handoff.md). Temporary
`LEADERBOARD_MENU` traces were removed after user acceptance; ordinary failure
warnings remain. The current manifest is configured, not empty.

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
| `/api/fifa09/player?name=...` | Local profile header and friend photos | Existing shared account metadata used by Rider License; country, `member_since` and avatar path, with the returned account name required to match |
| `/api/avatar/...` | Friend photos | Existing public avatar route; HTTPS on NOBK only, without redirects or credentials |
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

Trusted presence seeds each friend without announcing existing activity,
including friends whose first list response arrives after the initial live
snapshot. Failed or stale live reads do not replace this baseline. Later polls
enqueue a toast when a known offline friend enters MotoGP or hosts a new room,
or when the set of first-place boards owned by the current rider gains/loses a
view. A faster lap on an already-owned board does not change that set. Keep this
ownership-based notification separate from personal-best improvement messages.
The event feed holds at most 30 entries and the toast queue at most six; these
are session memory, not persistent history. Ordinary notices last about six
seconds. Friend-online notifications use the MW reference's nine-second base
duration, extending for longer messages up to eighteen seconds.

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

### Startup NOBK.FRIENDS strip

The startup-only strip adapts the released
[FIFA 09 v1.4.0](https://github.com/ChunchunQ/fifa09-recomp/releases/tag/v1.4.0)
behavior to the approved MotoGP mockup. It is passive and top-centered: at
1080p its height is 52 pixels and top inset is 22 pixels. The charcoal surface
has a fine gray outline, an outlined dark keycap and an orange-dot wordmark.
The friend presence dot/count and optional orange invitation badge precede
a divider and an explicit `Controller: hold Back` hint for Xbox prompts or
`Controller: hold Select` for PlayStation prompts. Keyboard prompts omit the
controller hint and its divider entirely; the configured keyboard key remains.
The hint follows the existing active prompt style and manual override, without
changing the input binding. Width follows actual text
measurements; narrow viewports fit proportionally without cropping text.
This change only affects the startup strip, not the F1 panel or friend-online
notification design.

It starts once a local account name is available on the existing production
Online service, has a three-second presentation window, and uses the donor's
300 ms smoothstep ramp, with 16-pixel movement from above for entrance and exit.
Its local clock advances by at most 1/30 second per draw, so a long startup
frame cannot skip the entrance or expire the entire strip. At 30 FPS and above
the normal timing is unchanged; slower frames prolong presentation instead
of losing animation steps. The game's simulation and other UI clocks are not
modified.
It neither opens F1 nor blocks guest input. Opening another overlay fades it
away without resetting its presentation window. It does not reappear for subsequent
friend changes in this first-stage implementation. Offline, LAN and custom
service origins do not show a production NOBK strip.

The count uses the existing friends snapshot, including shared NOBK presence;
it is not the total connected player count. Incoming-invitation count uses
the existing authenticated invitation snapshot. No new request, worker,
credential use or account database is added. The displayed key follows
`bind_nobk_panel`; the controller hint now correctly says to hold Back.
Short presses and Start retain their native handling; holding Back opens the
same panel gate. Release is required before another activation, including
after focus changes, and F2 remains live settings.

Only the strip uses the donor's Windows font chain: Segoe UI Semibold, Segoe
UI, then Arial, loaded at 48 pixels with 2x oversampling. System fonts are
not redistributed; the existing bundled semibold face is the fallback when
none is present. Both strip faces and the existing fallback lock their baked
size and scale the high-resolution glyphs: the runtime uploads the atlas once,
so notification sizes must not invalidate or resize it during a frame.
F1's current Apex fonts and layout stay unchanged.

Reference evidence is the actual v1.4.0 `rexruntimerd.dll` (`FriendsStrip::OnDraw`
at `0x1805de770`, wordmark helper `0x1805b4480`), with the font selection from
the app source pinned at `4dba1063d742911a81c81aa53347e463c2dc75d5`.
The older public SDK patch is not treated as the complete 1.4.0 renderer.

The focused `achievement_ui_probe ... --startup-strip-only` phase checks the
production lifecycle/ImGui drawing, one-time expiry, overlay interaction,
service gating, four-second first/mid-presentation stalls, narrow/localized
label layout and real input-system Back
hold/release/focus behavior. These Wine/CPU checks do not establish native
Windows GPU or physical-controller acceptance.

### Friend-online notification

`nobk_friend_toast.h` adapts notification kind 3 in the supplied MW
`SpeedBreaker.exe`, SHA-256
`85552cb336f1c12e842841f0ce644c173e0a08c1275214a8b600cfe0384f5f46`.
The `FRIEND ONLINE` branch is at `0x14018f16e` in the native draw function
`0x14018c170..0x14019108a`; the launcher is not its implementation source.

The approved compact pit-board design is 404 by 108 pixels at 1080p, inset
28 pixels from the top and right. Its charcoal surface has a fine gray outline,
chamfered right corners and a thin orange left stripe. A 64-pixel circular
portrait retains a green online dot. The orange heading sits above a larger
white friend name and the muted `Playing MotoGP 09/10` subtitle. Long names
truncate on UTF-8 boundaries rather than overlapping the right edge. The
orange countdown line shrinks over a faint track along the bottom.

It retains the reference's 300 ms smoothstep entrance from the right and
350 ms stationary exit fade. Duration follows the native
`min(18,9+max(0,text_bytes-60)/20)` formula, applied consistently to the full
name and localized activity text. The subtitle wraps at 270 reference pixels,
with a bounded five-line fallback for oversized text; narrow viewports fit
proportionally. The production subtitle normally occupies one line.

The native MW branch uses the current ImGui font. Its effective runtime font
family was not established by the bounded binary/video inspection. This port
uses its existing bundled semibold family with the locked high-resolution
bake, matching the static-atlas renderer's upload contract. Font family
identity across the two applications is not claimed by the geometry recovery.

This notification is backed by an actual transition in `/api/motogp/live`
for an existing friend, not the aggregate player count, a fabricated timer or
the shared FIFA lobby flag. The activity names MotoGP 09/10. Case-only changes,
first sight of a friend, repeated snapshots and recovery from failed/stale
polls do not generate a new notification. A genuine disconnect/reconnect
can generate another one. Other game activity is not inferred from this feed.
Like the startup strip, friend-online notifications only render in production
Online mode. Offline, LAN and custom service origins suppress them even if a
friend event is already queued; existing Discord invitation errors retain
their separate behavior.

Visible notifications reuse the existing bounded, validated account-photo
worker/cache while F1 is closed. No HTTP or image decoding occurs on the UI
thread. Photos fall back to the reference's deterministic colored initial;
the image is center-cropped before circular drawing. Photo work pauses when
neither F1 nor the notification needs it, and notification-only GPU textures
are released afterward. No new endpoint, credential flow or polling worker
is introduced. Incoming NOBK lobby invitations now share this same card,
portrait cache, location and animation with a `LOBBY INVITE` heading, inviter
name and configured-key `Respond` prompt. Their subtitle stays on one line so
the reference size remains 404 by 108. Stale invitations stop their toast.
Discord join-status/error notices retain their separate rendering.

F1's eligible Friends rows use clipped-corner charcoal invitation tickets with
an orange outline and arrow. The same slot shows pending progress, gray `SENT`
with a check after server acknowledgement, or `RETRY` after failure. The social
worker retains this feedback per peer across polls and invalidates it when the
room host/session/key or eligibility changes. The old global `Invitation sent.`
line is removed. Incoming invitations appear above friends with a circular
portrait, invitation heading and separate `ACCEPT`/`DECLINE` tickets. Only Xbox
A/X or PlayStation Cross/Square hints sit below the buttons; keyboard prompt
style hides this line. Clicking the row only selects it; A/Enter accepts
and Delete/X declines. PlayStation uses Cross/Square. Answer controls are
disabled until the queued answer completes or fails, without changing the
native acceptance path. See [the approved invitation UI](invitation-ui-concept.md) for
the source and acceptance boundary. The 28 October local Windows builds include
this change; dedicated native Windows invitation visual acceptance remains
outstanding.

The focused `achievement_ui_probe ... --friend-toast-only` phase exercises
the actual snapshot merger and notification renderer: initial/delayed friends,
duplicates, stale/error recovery, case changes, reconnect, lifetime, passive
input behavior, wrapping, Unicode boundaries, portrait geometry and viewport
limits. Together with the startup-only phase these are Windows/Wine and CPU
draw-data checks, not physical-controller or native Windows GPU acceptance.

### F1 panel

The current F1 renderer follows the user-approved left-side Apex concept. The
approved image is a visual reference, not a shipped game background. The game
scene remains live behind a gentle charcoal scrim and a background-only blur.
The scrim is 42% at the far left and 30% behind the list; a smoothstep feather
extends from just before the last content column to 520 reference pixels past
the content. Blur follows the same feather, including right-aligned times and
scores, without softening any F1 text, flags, photos or Apex strands. There is no outer
panel border, card stack, diagonal row strip or footer shortcut text.

The Windows D3D12 presenter applies a two-pass separable Gaussian before any
host UI. It copies the already-presented game image entirely on the GPU, filters
horizontal samples into a half-resolution texture, then composites vertical
samples through the smooth mask. Existing game menus/HUD remain present; no
native menu layer is hidden, no screenshot is read back to the CPU, and the
user's upscale/sharpening settings are unchanged. F1 publishes versioned,
normalized parameters through `Motogp_SetPanelBackdrop` in the matching custom
runtime. The pass does no GPU work while closed. Pipelines are initialized on
first use; image allocations are reused and resized resources are retired only
after their paint fence completes. Missing system shader compiler or unavailable
blur resources retain the soft scrim and emit one diagnostic instead of blocking
the menu. The synthetic UI probe and Linux/Vulkan retain the scrim only.

`cmake/sdk-nobk-backdrop.patch` inserts the D3D12 pass, while
`src/nobk_backdrop_gpu.cpp` and `src/nobk_backdrop_shader.h` own its resources and
shader. `visual::BackdropFor` and `visual::SoftScrim` control the shared fade
geometry and contrast. Appearance and native Windows GPU cost need in-game
acceptance; shader compilation and source checks alone do not establish them.

The Friends reference positions use a 1920 x 1080 canvas: wordmark at (94,76),
profile avatar at (398,70), vertical sections at x=96, and list origin at
(356,214). Friend avatars, names, national flags and activity occupy fixed
columns; seven 100-pixel rows with 2-pixel spacing reproduce the reference.
The layout scales with viewport height and constrains its width on narrow
screens. The right side stays available for the game scene.
The NOBK.FRIENDS wordmark uses natural font proportions, with no horizontal
stretch or simulated italic transform. The MotoGP subtitle is omitted.

The cover-inspired racing decoration uses seven thin strands in orange,
silver-violet, champagne, coral, cream, muted violet and amber. Four secondary
strands are shorter and dimmer. Their sampled geometry is cached once; small
light highlights travel at staggered 8.5–17 second periods using elapsed time,
without moving the paths or content. Drawing stops while F1 is closed and uses
ordinary batched lines, without textures, blur passes or background work.
The selected row
has a small orange pointer and underline, without a filled background.
The underline's visible core and the vertical rail to the left of the section
tabs are now two reference pixels thick. The curved decoration retains its
original stroke widths, geometry, colors and timing.
Friend avatars show circular account photos with separate presence dots; charcoal
initials remain while loading or when no photo is available. The local profile
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
Earned achievements show the saved unlock day and localized abbreviated month,
without a year, in the secondary right-hand label. Legacy unlocks without a
timestamp retain the unlocked label; locked entries do not invent a date.
Invalid or missing country data does not
invent a flag. No HTTP request or new asset download is required for flags.

Friend photos use a separate serial worker so downloads do not delay live data
polling. Visible rows request photos; closing F1 pauses queued requests and
destroying the dialog aborts HTTP and joins the worker. Successful/absent photos
are cached for five minutes; transient failures retry after one minute. The
name-matched account response supplies the avatar path, with the legacy public
name route as fallback when the field is absent. Explicit empty avatars keep
initials. Only PNG/JPEG up to 512 KiB and 1024 x 1024 are admitted before decoding;
Windows WIC reduces them to at most 128 x 128. The memory cache holds at most
128 entries, while UI textures are retained only for current friends and upload
at most once per frame. No photo is written to disk, and no account credential
or arbitrary external URL is used. Synthetic visual fixtures disable requests.

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
