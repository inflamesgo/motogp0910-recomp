# Discord lobby invitations

Status reviewed on 28 October 2026 against source `28ba765`. The user reported
a successful local Discord invitation and supplied a card showing `Joined`.
This confirms that scenario, not two independent real-account installations,
cold activation or production deployment. No new live test was run for this
documentation review. See [the current handoff](chunchunq-friends-invites-ghost-handoff.md).

Invitations use Rich Presence application `1562961932061904906`, the existing
authenticated community connection and the title's native accepted-invitation
flow. No bot token, OAuth client secret or additional SDK/runtime DLL is used.

## Player flow

No Discord voice channel, Discord server or bot is required.
Both players sign in through the launcher, enable **Show Discord activity**,
open Discord desktop and use the same updated Online service. A rider in an
open, non-full lobby can send a Discord game invitation. Accepting it delivers
`ACTIVITY_JOIN`; the game resolves its opaque capability through its own
authenticated server connection and raises the native invitation notification.
The title handles the lobby transition, with its existing host handshake as the
final join authority. F1 shows pending, accepted, already-present,
unsupported-server and unavailable-invitation notices, closing its own panel
when processing the invitation.

Closed-game acceptance uses Discord's registered-game protocol to open
`MotoGP09-10Launcher.exe --discord-invite` first. The launcher selects Online and
uses its normal PLAY transaction after files, sign-in and the Discord setting
are ready. Missing files or sign-in keep the launcher visible. An expired login
returns to sign-in; transient launch errors do not loop automatically. Selecting
another connection mode cancels the automatic launch intent. An already-open
launcher receives the activation through its existing window.

The secret arrives through Discord IPC after the game connects, not through the
launch command. No secret is persisted in settings, process arguments, temporary
files or logs. If setup outlasts Discord's pending launch, or the lobby becomes
unavailable, accept again after setup or request another invitation.

Normal game startup registers the current public launcher under the current
Windows user's `Software\Classes\discord-1562961932061904906` protocol. This
needs no administrator access. Start an installation normally once before using
closed-game invitations; repeat after moving its portable directory. The
launcher retains file/DLC checks, account linking, saved settings and the crash
monitor. It does not enable Discord sharing silently.

## IPC and presence

After READY, an installed join handler subscribes once to `ACTIVITY_JOIN`. Only
that dispatch with a supported, bounded secret reaches the background queue.
Rejecting the optional subscription removes outgoing join fields while ordinary
presence continues. PING/PONG, clearing, settling and update budgets are retained.
Duplicate pending accepts are coalesced.

A joinable Online lobby adds shared `party.id`, `secrets.join` and
`instance: true` to its existing valid party-size counter. Races, menus, full
lobbies, LAN/Offline and expired/mismatched publications omit joining. Publishing
or refreshing a capability does not reset the elapsed timer. Discord never
receives a native session key, account token, user name or IP address from this
invitation payload.

## Authenticated server contract

The existing `POST /v2/social` route admits three additional JSON actions.
Normal authentication, body-size and profile request limits remain in force.
These actions do not call the shared friends API and do not require the
`--friends-api` option or the native invitation feature's immutable friend IDs.
Both accounts must still be connected and linked. An explicitly invited Discord
contact can join without also being added as a NOBK friend.

| Action | Request | Response |
| --- | --- | --- |
| `discord_publish` | `host`, `session`: 16 hexadecimal digits each | `supported`, `secret`, `party_id`, `host`, `session`, `expires_in` |
| `discord_join` | `secret` | `supported`, plus `accepted: {from, room}` or `already_joined: true` |
| `discord_revoke` | No extra fields | `supported`; removes only the caller's publications |

Secrets have prefix `mg0910.v1.` and 32 URL-safe base64 characters from 24
cryptographically random bytes. The server alone retains the capability's room
binding. Only an authenticated join response supplies the native descriptor.
Party IDs use the non-secret form `mg0910:<host>:<session>`.

The lifetime is 120 seconds, renewed about every 20 seconds while the same lobby
remains joinable. A session reuses its capability to avoid needless Discord
updates. Session/key changes produce a new one. Redemption checks expiry,
inviter/host connections and memberships, exact session and key, state, capacity
and the 15-second lobby-advertisement freshness bound. Already-present recipients
receive a no-op response. Restarting the service drops all capabilities.

There is at most one capability per publishing rider and 512 service-wide.
Publication allows four initial requests, then one per five seconds; join allows
four initial requests, then one per three seconds, in addition to route limits.
The resolver runs on a separate worker, away from Discord IPC and UI/guest
threads. Older deployments' HTTP 404/405 response means unsupported; authentication
and service failures remain failures. Validated descriptors reuse the existing
84-byte `XINVITE_INFO`, accepted-room lookup and native join notification.

## ChunchunQ deployment

Use the matching committed server sources; this review does not supply a new
server ZIP or claim the old package contains scoped stats/ghost implementations. Preserve the live database,
account aliases, account API options, ports, service identity and proxy secret.
The updated runtime files include `discord_invites.py`, `social.py` and
`community_server.py`, with their normal dependencies. No database migration,
Discord token or new website API is required. Restart the existing service with
its existing flags. The deployment's Caddy allowlist must include `/v2/social`
and `/v2/link`; the supplied example already does.

The shared friend-ID upgrade in [the earlier invitation
handoff](online-friends-invites.md) remains separate. Discord invitations can be
deployed before that native friendship work is completed.

## Acceptance and verification boundary

Before release, use two native Windows installations with Discord desktop:

- Send a real game invitation from an open lobby; accept with the receiver's
  game running and check the real lobby/session and player count.
- Repeat with the game closed. Confirm the launcher opens first, waits for
  sign-in/setup when needed, starts Online and receives the deferred join event.
  Repeat with the launcher already open and after moving the installation.
- Check expiry, host leaving/recreating a room, capacity and a race already
  started. None may redirect the receiver into a different room.
- Check LAN/Offline, menus, race activity, timer continuity and activity opt-out.

Automated server and mock-pipe checks establish protocol behavior, not actual
Discord desktop eligibility/dispatch, registered-application launch timing or
native multiplayer success. Matching client/server sources must be built and
deployed before those live acceptance checks.
