# Discord Rich Presence

The Windows game publishes local activity to the Discord desktop app using
Application ID `1562961932061904906`. The application's name in Discord Developer
Portal determines the displayed game title; set it to **MotoGP 09/10**.
Activity display needs no bot token, client secret, online account or game server.
Optional [Discord lobby invitations](discord-invites.md) use the existing Online
account/server and native accepted-invitation path. Closed-game acceptance opens
the public launcher first, preserving sign-in and game-file checks.

**General → Discord → Show Discord activity** enables this feature by default.
The English/Turkish launcher persists `discord_presence_enabled` in the shared
configuration and forwards it to the game. Changes apply on the next launch.
Discord must allow activity sharing in its own Activity Privacy settings.

## Displayed information

- Main menu, or the selected mode's menus, without stale race details.
- Automatic idle/attract playback remains Main menu, with only the large game
  logo; the AI demonstration is not advertised as a player race.
- Native Settings, Leaderboards and DLC browser screens, plus the F2 settings
  overlay. Split Screen setup retains its own status when choosing Career or
  Arcade; the Multiplayer entry screen is displayed as Online with its Online
  badge, and network setup remains Online/LAN rather than a single-player mode.
- Arcade, Career, Championship (including its native Quick Race path), Time Trial, training/tutorial,
  split screen, replay and Online/LAN event context.
- Circuit photograph during an active event, with a small game-mode badge.
  Hovering the large photograph shows circuit and class/year, for example
  `Losail International Circuit · Moto2 | 2010`. The small badge hover remains
  the mode name. Session is on the visible upper row, not repeated on the image.
- Beneath Discord's application name, active events use two compact text rows:
  `Mode · Session`, then `Rider name | Bike manufacturer/model`.
  For example: `Championship · Practice`, followed by
  `Raffaele De Rosa | Tech 3 Mistral 610`. Championship qualifying and race use
  `Qualifying` and `Race` in the same position. Other event modes use the same
  layout, omitting an unavailable session without empty separators.
- The complete native bike manufacturer/model is retained; it is not shortened
  to just the model. Missing rider or bike data leaves the available one alone.
  If both are missing, the lower row is empty. Within the existing RPC byte
  limit, unusually long custom rider text is shortened first so the bounded
  native bike name remains complete; UTF-8 characters are never split.
  This includes Online, LAN, Split Screen, training, tutorial and replay events.
  The native Quick Race enum (1) is displayed as Championship with Race session
  and the trophy badge.
- Active mode classification prefers the live global RaceMode property at
  `8292710C`. Time Trial setup (`821F6944`–`821F6960`) stores 10 there even when
  the per-event config still reports Quick Race (1). Using that config alone
  incorrectly selected the Quick Race label, circuit heading and flag badge.
  A valid event-config mode is only a fallback when the global value is absent
  or outside 1–16. The Championship owner is identified separately through
  `FEMODE_VANILLACHAMP` (4), or the single-race frontend (1), for ordinary event
  modes 1–5; a surviving Arcade value (2) must not relabel it. For shared
  session IDs 3–5, the current frontend identifies Career/Arcade before stale
  single-player menu history, while an Online/LAN context remains networked.
  Explicit Time Trial, network, demo and other special event modes retain their
  own identity. Ordinary-event session reads prefer live 3/4/5, then event-config 3/4/5; the
  direct single-race mode (1) is Race. Unknown phases are omitted.
- Main-menu/lobby activity returns to the large game logo and clears rider/bike
  and track artwork. Mode menus and Online/LAN lobbies retain their mode badge;
  the main menu has no small image or small-image hover label.
- Online/LAN lobby member count and capacity through Discord's native party
  counter when the existing network bridge reports a current room. The custom
  status is `In lobby`, for example `In lobby (1 of 20)`; Discord controls the
  final counter formatting. The old `1 / 20 riders` text row is removed.
  Room names and player names are not included in party metadata. Joinable
  sessions can carry the non-secret host/session party ID described below.
- Elapsed time uses a continuous clock through Main menu, In menus and In lobby.
  Choosing another setup menu, entering/leaving a lobby or updating its player
  count does not restart it. When a circuit photograph appears, the race clock
  starts at zero. A new race session or circuit starts a fresh race clock;
  returning to menus keeps the current elapsed time. This is host wall-clock
  activity time, including loading and pause, rather than a lap timer.

Unknown metadata is omitted. The reserved Balaton track slot is not presented
as a playable circuit. DLC circuit slots use the existing title-update mapping.
The primary local user's selected class, rider and bike are used; other
split-screen players are not listed. Discord join invitations are documented
separately in [the invitation integration](discord-invites.md).

## Implementation and verification

A throttled guest-frame reader copies validated native values into a small
activity snapshot. It never calls Discord or performs pipe I/O on the guest
thread. The active native screen supplies menu context; the guarded race object
supplies event type and season when a frontend menu is not active. The stored
frontend mode is only a fallback for an actual event, because it can retain the
previous selection or be reused by network setup. Leaving the event clears
circuit/class information. Lobby snapshots come from the existing LAN/Online bridge.
Lobby activity sends `state: "In lobby"` and `party.size: [members, capacity]`.
Only positive, consistent counts within signed 32-bit limits are sent; unknown
or invalid counts omit the counter. A display-only lobby needs no join secret.
An updated Online service can also publish an opaque, short-lived join capability
and shared party ID; native keys and account credentials stay out of Discord.
Count changes participate in activity comparison, so joins and
leaves use the same responsive capture and publish policy as menu changes
without resetting the activity timer. Entering a race or leaving a lobby removes
the party metadata; race rider/bike text is independent of lobby membership.
Native names and model strings are copied with bounded, validated UTF-8 reads.
No console function is invoked and no guest memory is written by this reader.

A background worker connects to the standard Windows `discord-ipc-0` through
`discord-ipc-9` named pipes. It sends the version 1 handshake, waits for READY,
responds to PING and frames bounded JSON messages. Capture runs every 100 ms;
publishing wakes the worker immediately, with a 200 ms settling interval and
50 ms pipe polling. Normal settled transitions should reach the pipe in roughly
0.3–0.5 seconds; actual Discord UI propagation is outside the client's control.
The worker conservatively applies Discord's documented activity budget of
[five updates per twenty seconds](https://discord.com/developers/docs/game-sdk/activities#updateactivity).
It allows responsive bursts and retains only the latest pending activity when
that rolling budget is exhausted, including across reconnects. Obsolete menu
states are not queued. An unavailable or closed Discord client causes
silent retries every fifteen seconds. Pipe transfers have a 250 ms timeout;
shutdown wakes the worker, clears activity and closes the connection. The window
close path also stops the worker because the pinned SDK can bypass OnShutdown.
There are no new third-party SDKs, runtime DLLs or game-data modifications.

`StartsNewRaceTimer` gates resets on the next activity using a `track_*` image.
Main menu, setup, settings, leaderboards, DLC and Online/LAN lobby updates retain
one timestamp even when their text, badge or lobby count changes. Leaving a race
for these menus also retains that timestamp. Entering a track-photo activity
starts a fresh timestamp, including returning to the same circuit after menus.
While a track photo remains active, a different actual mode, phase, circuit,
season or class identifies a new race session. Late rider/bike metadata does
not restart it. The internal key omits the raw native RaceMode ID because its
transition from a shared mode to Practice can describe the same Championship
session. This key is never included in the RPC payload.

`discord_presence_probe` checks native-state fixtures, mode transitions, missing
metadata, RPC framing and serialization. On Windows it also checks launcher
persistence/argument forwarding and a mock named-pipe peer, including fragmented
READY, PING/PONG, activity changes and clearing. It runs without a real Discord
account. These checks do not establish acceptance by the actual Discord desktop
client or live-game state transitions; verify those on native Windows with the
configured application before release.

Timing fixtures use a virtual clock to check settling and rolling-budget
boundaries without a twenty-second test delay. The Windows mock pipe checks
prompt delivery, latest-state coalescing, unchanged-state suppression and timer
continuity when rider metadata arrives later. Timer-policy fixtures cover menu
chains, Online/LAN lobbies, count updates, return from a race, re-entering the
same circuit and an equivalent native mode update. The Windows mock pipe checks
that returning to menus after wall time advances preserves the published start
and that the next track-photo entry publishes a newer start. Event fixtures also
verify that an explicit native race type takes priority over stale frontend mode
context.

The native lobby-counter fixtures cover Online and LAN serialization, joins and
leaves, invalid counts, clearing on race/menu transitions and unchanged custom
status. These are state/protocol checks; the live Discord UI remains a native
Windows acceptance check after a build containing the change.
The heading fixtures cover every active event mode, native built-in/custom
name accessors, missing rider/bike metadata, mode/session dot separators,
rider/bike pipe separation and circuit/class/year image-hover formatting. They also
cover the native Quick Race path's Championship label, Race phase and trophy.
The Moto2 regression keeps the legacy rider ID at -1, uses bike ID 101 and a
stale Arcade context/global mode, then checks all three Championship session
headings, phase timer changes, other active modes and a matched local grid
entry after an unrelated rider.

## Menu transitions

`navigation.h` maps explicit native screen vtables verified against the patched
title's RTTI. `8235ACB0` installs an active screen record in the eight-slot array
at `8336F4B0`; each record points to the screen object, whose first word is its
vtable. The reader follows the descending slot order of native `8235A150` and
excludes slot 3. It ignores the adjacent fading-out record array and lets unknown
popups inherit the recognized screen below them. Known in-race screens keep
actual event capture available even if a previous menu is still underneath.

Examples of verified screen identities:

| Native screen | Vtable | Presence context |
| --- | --- | --- |
| MainMenu | `8200CF7C` | Main menu; clears the prior menu context |
| ArcadeMainMenu | `8201789C` | Explicit Arcade entry, clearing previous setup context |
| ArcadeClassMenu (also reused by Championship) | `82017374` | Current setup owner, including Championship |
| TimetrialChooseClass | `82030184` | Time Trial immediately on entry |
| MultiplayerMultiplayer | `8203A0C4` | Online entry, sharing the Online badge and setup context |
| MultiplayerOnline | `8203AAEC` | Online, or LAN for community LAN configuration |
| MP_Splitscreen_GameMode / RaceMode | `8203CF6C` / `8203CFB4` | Split Screen |
| OptionsOptions | `8203411C` | Settings |
| MultiplayerLeaderboards | `82036F14` | Leaderboards |
| DLCScreen | `8200C574` | DLC browser; no race, badge or lobby count |
| MultiplayerLobby | `8203799C` | Online/LAN lobby with real room counts |

Class, rider, transmission, season and track screens with Arcade-derived native
names are shared setup components. Vtables `82017374`, `82017284`, `820186B4`,
`8201841C` and `82017B2C` must not force Arcade just because of their C++ names.
For these components, frontend 1/4 selects Championship, 3 selects Career and
9/10 selects Split Screen. Explicit Online/LAN, Split Screen and Time Trial
setup contexts survive reuse of these components. The actual Arcade entry
`8201789C` remains Arcade even before the old frontend value updates, so leaving
another mode for Arcade resets its selection history.

Common base selection screens inherit the setup owner. Frontend 1/4 repairs a
stale single-player context to Championship, while explicit network, Split
Screen and Time Trial contexts are preserved. Without usable history, known
single-player frontend values recover the owner instead of reporting generic
Race setup. Returning to MainMenu clears history. An unrecognized screen with
no valid event still falls back to Main menu.
An explicitly displayed menu overrides even a surviving race object/active flag,
preventing old track photos, rider data and badges from surviving a menu return.

Native network menus and the community `network_mode` setting distinguish Online
and LAN setup. Room counts are queried only for native lobby screens or the
unknown-screen fallback, so a retained room cannot replace Settings, Leaderboards
or Main menu. F2 uses a separate read-only provider for its own atomic open gate;
other overlay input blockers do not incorrectly report Settings. The underlying
navigation context continues to be captured while F2 is open and is restored
when it closes. No extra native screen or input hook is installed.

The navigation fixtures keep stale Time Trial frontend/race data alive while
entering each real menu, exercise the first Arcade/Time Trial screens, Online
Career/Arcade setup, shared selections, LAN, Settings, Leaderboards, Split Screen,
popup/closing-record handling, in-race screen priority and repeated main-menu
returns. Additional fixtures traverse both Championship frontend variants from
class through track selection, retain stale event data throughout every setup
chain, check all other setup owners, recover skipped preceding screens and
verify a return to real Arcade. The DLC browser clears any old event metadata. The portable model/protocol checks and Windows syntax-only checks cover
the changed reader/provider integration; they do not replace native Windows
menu navigation with a real Discord client. No application build is produced
by these checks.

### Idle attract playback

Demonstration playback can create an ordinary practice/qualifying/race event
config (3–5). Its frontend selection and remembered menu context are not reliable
demo indicators. The native title renderer (`822AE928`–`822AE948`, inside
`822AE390`) dereferences the global RaceMode property pointer at `8292710C` and
draws PRESS START when that value is 15. The guest update at `822AFDD8` also uses
this global property for demo handling. This differs from the event config's
mode, which may still be 3–5.

When no actual setup menu is visible, the reader checks the same global property
before requiring a race object or reading event metadata. A value of 15 returns
a clean Main-menu snapshot, regardless of stale frontend/context values. An
explicit event-config mode of 15 is also excluded. A real setup menu remains
authoritative when leaving the demo. Missing/unreadable global mode data does
not fabricate a demo from frontend/context values. F2 can still display Settings.

The focused fixtures cover all three demo event types with stale frontend and
menu contexts, startup during a demo, loading without a race object, leaving for
a setup menu, and real Arcade/Career/Championship practice, qualifying and race
sessions. These are synthetic memory-reader regressions; a live Discord demo
cycle on native Windows still requires user verification. Only the Rich
Presence snapshot changes; the game's attract playback is preserved.

The protocol follows the public Discord local RPC handshake and SET_ACTIVITY
schema documented in the [Discord RPC reference](https://github.com/discord/discord-rpc).
Menus and unknown/reserved circuits use `motogp_logo`. Valid active circuits
use the explicit `track_*` keys returned by `TrackAsset`. `ModeAsset` supplies
`small_image` for the seven configured modes; `small_text` is the mode's name.
`large_text` contains the full circuit name plus class/year, separated by a
spaced middle dot. The visible upper text row contains mode/session.
Discord hosts the artwork. No game
texture bank, extracted PNG or decompressor is bundled with the application.

## Mode badge setup

Upload the active mode PNGs in `resources/discord/` to **Rich Presence > Art Assets**,
using each filename without `.png` as the exact key:

| Mode | Asset key | Artwork |
| --- | --- | --- |
| Time Trial | `mode_time_trial` | Stopwatch |
| Championship (including native Quick Race) | `mode_championship` | Trophy |
| Career | `mode_career` | Racing helmet with an upward arrow |
| Arcade | `mode_arcade` | Lightning bolt |
| Online | `mode_online` | Globe with connected nodes |
| LAN | `mode_lan` | Two connected monitors |

The previously exported `mode_quick_race` checkered-flag asset is retained on
disk for compatibility but is no longer selected for the Championship path.
These are the approved white/orange designs on a dark background, exported as
1024-square PNGs by slicing the generated atlas and uniformly resizing/padding;
they were not redrawn during export. They are Portal upload sources and are not
bundled as runtime dependencies. Keep `motogp_logo` for the large main-menu image
and existing `track_*` assets for race photos. Unspecified modes have no small
badge. Lobby Online/LAN state takes precedence over a lingering menu choice;
active events use the same resolved mode as their displayed activity text.

The focused model probe checks the active mappings, hover labels, actual-event
overrides, lobby priority and removal of both small asset fields on returning
to the main menu. Portal upload and a new application build are still required
before these keys can appear in the live Discord client.

## Track artwork setup

Upload the twenty prepared PNGs to this application's **Rich Presence > Art
Assets**, preserving their `track_*.png` names as asset keys. Keep the existing
`motogp_logo` asset. Keys must match `TrackAsset`; uploading images does not
change an older game executable. There is no local-file-path image loading in
Discord, and the client cannot detect an unpublished/missing asset key.

`scripts/export-discord-track-assets.py` exports the owner's already decoded
`frontend_track.mgb`, `frontend_track_dlc1.mgb` and `frontend_track_dlc2.mgb`
banks. Their native linear 512-square BC3 textures contain menu frames, a photo
in the upper area and unused black space below. The export removes that frame/
space using an individually chosen 300-square crop for each circuit, retaining
recognizable buildings or turns. The square is resized uniformly to 1024 with
Lanczos: no geometric squeezing, fake detail or regenerated artwork. All twenty
crops were visually inspected together. The exporter preserves existing output
directories, records crop boxes/source and output hashes in `manifest.json`,
and creates a preview plus a ZIP containing only the upload PNGs.

The native slots stay fixed: 0–16 base circuits, slot 17 reserved, 18 Silverstone,
19 Shanghai and 20 Istanbul. The base bank's first 17 entries are photographs;
the remaining 17 are circuit outlines. Each DLC bank also contains outline
textures; these are separate from the photos used for Discord and are not
connected to Rider License's optional map path by this change.

Local export (paths refer to ignored owner-derived data):

```bash
python3 scripts/export-discord-track-assets.py \
  --base out/analysis/discord-artwork/frontend_track.decoded \
  --gp-dlc out/analysis/discord-artwork/frontend_track_dlc1.decoded \
  --twin-dlc out/analysis/discord-artwork/frontend_track_dlc2.decoded \
  --output out/discord-track-assets
```

## Native rider and bike mapping

- The primary-user lookup resolves the same profile used for class selection.
  The selected rider interface is at profile `+0xF75C4`; native `822F0318`
  copies this descriptor. The reader uses the interface itself rather than
  treating profile `+0xF75D0` as a rider-table index. That field can be -1 for a
  valid Moto2 selection. The previously consulted 124-byte rows at `82946814`
  belong to livery metadata, not the selected rider's display-name interface.
- `BuiltInRiderData` has vtable `8206BBCC`. Its virtual +64 name accessor is
  `821C2140`, which returns the display-name pointer at object +12. Its virtual
  +120 accessor `822F4830` reads bike ID at +108. These are the same accessors
  used by native `822F5810` when preparing the grid, including the Moto2 roster.
- `CustomRiderData` has vtable `82069B9C`. Its virtual +64 accessor `822EA958`
  returns the inline custom name at `*(object+60)+56`. Known adapter types are
  decoded with validated reads; no guest virtual method is invoked. An unknown
  adapter never falls back to an unrelated livery record.
- `822E8740` indexes loaded 44-byte bike records through `829F3118` / `829F311C`.
  Manufacturer and short model strings are at +4 and +16, matching native rider
  selection `822586D8`. The selected adapter's bike ID resolves the actual bike,
  including catalog entries beyond the old 100-bike boundary.
- Native grid builder `8226DBA0` appends at most 20 entries of 1824 bytes at
  `8291E130`, with count at `8291749C`. Selection-interface identity is stored
  at +1768; `822F5810` stores bike ID at +1764, and `82276268` copies the display
  name into +1596. Every mode prefers that matched local entry's valid name and
  bike over frontend metadata. Entry zero is never assumed to be the player.
  Career does not reuse a stale built-in Arcade selection; its custom adapter
  or matching actual grid supplies the name.

Pointer arithmetic, table counts, strings and UTF-8 are bounded. Only the local
selected rider is published. The opt-out, worker ownership and shutdown clearing
are preserved. Strings reuse each aligned word within a read instead of querying
the guest memory reader again for every byte; nothing is cached across captures.

## Artwork and driver verification

The focused portable probe covers normal/2010 class handling, local selection,
real manufacturer/model fields, matching a Career entry after another grid
entry, UTF-8 names and malformed text, invalid rider IDs, stale Career/Time Trial
context, event exit clearing, all twenty artwork slots, the seven mode badges,
small-badge omission in the main menu and the serialized Discord asset fields. Windows source syntax is checked
separately. These are model/source checks; live Windows rider transitions and
actual Discord asset availability still require testing with uploaded assets.

To check the image on native Windows, run a build containing this asset reference
with Discord desktop open and activity sharing enabled, then open the user's
Discord profile/activity card. A valid image should appear beside the existing
mode and elapsed-time text. If it does not, verify the exact asset key and
Application ID, allow Discord's asset cache to refresh, and restart the game
and Discord if needed. Uploading artwork alone does not change an older build.

Validation for the artwork/driver extension: portable state/protocol fixtures
passed and the Windows presence source passed a syntax-only check with the
pinned SDK headers. No application build or package was produced for this
extension. Earlier validation of the base integration cross-compiled the Windows
presence library and probe, checked application/launcher syntax, and ran the
Windows mock-pipe probe under Wine. Those earlier transport checks were not
repeated for this extension. Neither set establishes native Windows Discord or
rendered gameplay acceptance.
