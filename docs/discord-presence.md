# Discord Rich Presence

The Windows game publishes local activity to the Discord desktop app using
Application ID `1562961932061904906`. The application's name in Discord Developer
Portal determines the displayed game title; set it to **MotoGP 09/10**.
No bot token, client secret, online account or game server is required.

**General → Discord → Show Discord activity** enables this feature by default.
The English/Turkish launcher persists `discord_presence_enabled` in the shared
configuration and forwards it to the game. Changes apply on the next launch.
Discord must allow activity sharing in its own Activity Privacy settings.

## Displayed information

- Main menu, or the selected mode's menus, without stale race details.
- Native Settings, Leaderboards and Multiplayer screens, plus the F2 settings
  overlay. Split Screen setup retains its own status when choosing Career or
  Arcade; network setup remains Online/LAN rather than a single-player mode.
- Quick Race, Arcade, Career, Championship, Time Trial, training/tutorial,
  split screen, replay and Online/LAN event context.
- Circuit photograph during an active event, with a small game-mode badge.
  Hovering the photograph shows circuit, class, season and session metadata.
- Arcade, Career, Championship and Time Trial display `Mode - Rider name` in
  the first activity row and the actual bike manufacturer/model in the second.
  Career uses the matching local entry's native name. Circuit, class, season
  and practice/qualifying/race context remain available on image hover. A missing
  rider name leaves just the mode in the heading; it never substitutes the track.
  Missing bike data retains the existing class/year/session fallback. Other
  modes retain their previous text layout.
- Main-menu/lobby activity returns to the large game logo and clears rider/bike
  and track artwork. Mode menus and Online/LAN lobbies retain their mode badge;
  the main menu has no small image or small-image hover label.
- Online/LAN lobby member count and capacity through Discord's native party
  counter when the existing network bridge reports a current room. The custom
  status is `In lobby`, for example `In lobby (1 of 20)`; Discord controls the
  final counter formatting. The old `1 / 20 riders` text row is removed.
  Room names and player identities are not published.
- Elapsed time for the current activity. This is a host wall-clock activity
  timer, including loading and pause time, rather than a lap timer.

Unknown metadata is omitted. The reserved Balaton track slot is not presented
as a playable circuit. DLC circuit slots use the existing title-update mapping.
The primary local user's selected class, rider and bike are used; other
split-screen players are not listed. Discord join invites and spectating are outside this feature.

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
or invalid counts omit the counter. No room ID or join/spectate secret is needed
for this display. Count changes participate in activity comparison, so joins and
leaves update within the existing half-second capture/five-second publish limits
without resetting the activity timer. Entering a race or leaving a lobby removes
the party metadata; race rider/bike text is independent of lobby membership.
Native names and model strings are copied with bounded, validated UTF-8 reads.
No console function is invoked and no guest memory is written by this reader.

A background worker connects to the standard Windows `discord-ipc-0` through
`discord-ipc-9` named pipes. It sends the version 1 handshake, waits for READY,
responds to PING, frames bounded JSON messages and publishes changed activities
at most once per five seconds. An unavailable or closed Discord client causes
silent retries every fifteen seconds. Pipe transfers have a 250 ms timeout;
shutdown wakes the worker, clears activity and closes the connection. The window
close path also stops the worker because the pinned SDK can bypass OnShutdown.
There are no new third-party SDKs, runtime DLLs or game-data modifications.

`discord_presence_probe` checks native-state fixtures, mode transitions, missing
metadata, RPC framing and serialization. On Windows it also checks launcher
persistence/argument forwarding and a mock named-pipe peer, including fragmented
READY, PING/PONG, activity changes and clearing. It runs without a real Discord
account. These checks do not establish acceptance by the actual Discord desktop
client or live-game state transitions; verify those on native Windows with the
configured application before release.

The native lobby-counter fixtures cover Online and LAN serialization, joins and
leaves, invalid counts, clearing on race/menu transitions and unchanged custom
status. These are state/protocol checks; the live Discord UI remains a native
Windows acceptance check after a build containing the change.
The heading fixtures also cover all four rider-first modes, the custom Career
name, missing rider/bike metadata, circuit/session hover text and the preserved
Quick Race layout.

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
| ArcadeMainMenu / ArcadeClassMenu | `8201789C` / `82017374` | Arcade immediately on entry |
| TimetrialChooseClass | `82030184` | Time Trial immediately on entry |
| MultiplayerMultiplayer | `8203A0C4` | Multiplayer root |
| MultiplayerOnline | `8203AAEC` | Online, or LAN for community LAN configuration |
| MP_Splitscreen_GameMode / RaceMode | `8203CF6C` / `8203CFB4` | Split Screen |
| OptionsOptions | `8203411C` | Settings |
| MultiplayerLeaderboards | `82036F14` | Leaderboards |
| MultiplayerLobby | `8203799C` | Online/LAN lobby with real room counts |

Common season/class/bike selection screens inherit the last recognized menu
context instead of interpreting the reused Career/Arcade frontend value as a
single-player mode. Returning to MainMenu clears that history. When there is no
recognized screen and no valid event, the activity falls back to Main menu.
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
returns. The portable model/protocol checks and Windows syntax-only checks cover
the changed reader/provider integration; they do not replace native Windows
menu navigation with a real Discord client. No application build is produced
by these checks.

The protocol follows the public Discord local RPC handshake and SET_ACTIVITY
schema documented in the [Discord RPC reference](https://github.com/discord/discord-rpc).
Menus and unknown/reserved circuits use `motogp_logo`. Valid active circuits
use the explicit `track_*` keys returned by `TrackAsset`. `ModeAsset` supplies
`small_image` for the seven configured modes; `small_text` is the mode's name.
`large_text` contains the full circuit and available class/year/session.
Discord hosts the artwork. No game
texture bank, extracted PNG or decompressor is bundled with the application.

## Mode badge setup

Upload the seven PNGs in `resources/discord/` to **Rich Presence > Art Assets**,
using each filename without `.png` as the exact key:

| Mode | Asset key | Artwork |
| --- | --- | --- |
| Time Trial | `mode_time_trial` | Stopwatch |
| Championship | `mode_championship` | Trophy |
| Career | `mode_career` | Racing helmet with an upward arrow |
| Arcade | `mode_arcade` | Lightning bolt |
| Online | `mode_online` | Globe with connected nodes |
| Quick Race | `mode_quick_race` | Checkered flag |
| LAN | `mode_lan` | Two connected monitors |

These are the approved white/orange designs on a dark background, exported as
1024-square PNGs by slicing the generated atlas and uniformly resizing/padding;
they were not redrawn during export. They are Portal upload sources and are not
bundled as runtime dependencies. Keep `motogp_logo` for the large main-menu image
and existing `track_*` assets for race photos. Unspecified modes have no small
badge. Lobby Online/LAN state takes precedence over a lingering menu choice;
active events use the same resolved mode as their displayed activity text.

The focused model probe checks the seven mappings, hover labels, actual-event
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

- The existing primary-user lookup resolves the same profile used for the class.
  The selected interface is at profile `+0xF75C4`, selected bike record pointer at
  `+0xF75CC` and native rider ID at `+0xF75D0`. Native `822F0318` copies this
  twenty-byte selection descriptor; `822F44C0` resolves its rider/bike metadata.
- Native `822B1608` indexes the rider table at `82946810` (count) / `82946814`
  (pointer) in 124-byte rows. The key at row `+4` is mapped to the 2009/2010
  English roster's display name by `roster.h`. Keys, rather than numeric IDs,
  avoid confusing IDs reused across seasons; unknown keys are omitted.
- `822E8740` indexes the loaded 44-byte bike records through `829F3118` /
  `829F311C`. The manufacturer and short model strings are at `+4` and `+16`,
  matching the pair rendered by native rider selection `822586D8`.
- Native grid builder `8226DBA0` appends 1824-byte entries at `8291E130`, with
  count at `8291749C`. The selection-interface identity is stored at `+1768`;
  `822F5810` stores the actual bike ID at `+1764`, and `82276268` copies the
  native display name into the buffer at `+1596`. The reader matches the local
  selection pointer instead of assuming grid entry zero is the local rider.
  Career uses that matched entry's name; an unmatched Career selection never
  falls back to a stale Arcade rider or bike. A matching event bike overrides
  the frontend selection. Missing/invalid metadata falls back to class/year.

Pointer arithmetic, table counts, strings and UTF-8 are bounded. Only the local
selected rider is published. The existing opt-out, half-second capture throttle,
worker ownership, five-second publish limit and shutdown clearing are preserved.

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
