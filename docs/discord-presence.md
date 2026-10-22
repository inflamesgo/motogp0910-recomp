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
- Quick Race, Arcade, Career, Championship, Time Trial, training/tutorial,
  split screen, replay and Online/LAN event context.
- Circuit, class and 2009/2010 season when valid native event data is available.
  Career/Championship distinguish practice, qualifying and race sessions.
- Online/LAN lobby member count and capacity when the existing network bridge
  reports a current room. Room names and player identities are not published.
- Elapsed time for the current activity. This is a host wall-clock activity
  timer, including loading and pause time, rather than a lap timer.

Unknown metadata is omitted. The reserved Balaton track slot is not presented
as a playable circuit. DLC circuit slots use the existing title-update mapping.
The primary local user's selected class is used; other split-screen players
are not listed. Discord join invites and spectating are outside this feature.

## Implementation and verification

A throttled guest-frame reader copies validated native values into a small
activity snapshot. It never calls Discord or performs pipe I/O on the guest
thread. Native frontend mode supplies Arcade/Career context; the guarded race
object supplies event type and season. Leaving the event clears circuit/class
information. Lobby snapshots come from the existing LAN/Online bridge.

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

The protocol follows the public Discord local RPC handshake and SET_ACTIVITY
schema documented in the [Discord RPC reference](https://github.com/discord/discord-rpc).
No application image asset is referenced until artwork is configured in the
Developer Portal; text activity works independently of custom images.

Validation for this change: portable state/protocol fixtures passed; the Windows
presence library and probe cross-compiled; main application and launcher syntax
checks passed with the pinned Windows SDK headers. The Windows probe also passed
under Wine against its local mock pipe, covering launcher settings and the real
Windows pipe I/O path. This is supporting CPU/IPC evidence, not native Windows
Discord or rendered gameplay acceptance.
