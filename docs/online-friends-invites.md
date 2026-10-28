# NOBK friends and native lobby invitations

Current source reviewed at `28ba765` on 28 October 2026. Client and server
modules are present and included in local Windows builds; no production
deployment or two-real-account Windows acceptance is claimed. The user reports
Discord invitation success separately from native NOBK friendship acceptance.
See [the current handoff](chunchunq-friends-invites-ghost-handoff.md).
The ghost handoff describes a design, not a delivered playback implementation:
[native ghost download and API plan](ghost-download-plan.md).

## Verified original paths

MotoGP TU 0.0.1.1 routes its lobby friend UI through `82303390`. With a native
session at object `+72`, it chooses either the optional callback wrapper
`82647720` or `8239C420`, which tail-calls `XamShowFriendsUI` (`8285EB44`).
The callback wrapper returns error 120 when no callback is installed. The
pinned SDK has no working NOBK invitation implementation for this title.

The native friend enumerator consumer is `82303FE8`. It calls `825DA108` for
up to 100 friends, then enumerates 196-byte (`0xC4`) records and reads the XUID
at offset zero. The original XLIVEBASE message is `0x58020`; the SDK returns
failure. `XamUserAreUsersFriends` separately reports no friends in the pinned
SDK. These are distinct from the panel's public HTTP friend list and explain
why displaying NOBK names in F1 did not populate the native Friends filter.

Native invite handling already exists in controller `823038B0`. It obtains
an 84-byte `XINVITE_INFO` through `825DA268` / XLIVEBASE `0x58023`, validates
the host session and enters the title's existing join/confirmation flow. The
compatible notification is `XN_LIVE_INVITE_ACCEPTED` (`0x02000002`, user 0).
The SDK currently fails the accepted-info request.

An important MotoGP-specific ABI detail: `825DA0D8` writes a size of **4** for
every argument cell and stores its pointer with `extsw` / `std`. Consequently:

- Both zero-extended and sign-extended 32-bit pointers are valid.
- MotoGP's accepted-info output argument points **directly** to the 84-byte
  destination even though its cell size is 4. Do not copy FIFA's heuristic
  pointer-to-pointer handling into this wrapper.

## FIFA 09 comparison

Reference: [ChunchunQ/fifa09-recomp](https://github.com/ChunchunQ/fifa09-recomp),
revision `4dba1063d742911a81c81aa53347e463c2dc75d5`, inspected in
`patches/nobk-sdk-1.3.0.patch` (`online_social.h`, `xam_net.cpp`, `xam_ui.cpp`,
`apps/xlivebase_app.cpp`). The root project license is MIT; its SDK patch also
contains existing SDK notices. No FIFA artwork, game files or server credentials
are included here; this is an independent MotoGP implementation.

FIFA implements a separate relay UDP social channel (request/reply types 10/11)
with `hi`, `inv`, `ans`, `host` and join requests. Its friend picker sends host
session information; accepting fills the 84-byte structure and broadcasts the
native notification. Its patched Xbox friends enumerator remains an empty list:
the overlay/social transport and native enumeration are separate mechanisms.

MotoGP uses its own authenticated MGO2 relay, persistent MotoGP player IDs and
native sessions. FIFA packets and site/FIFA XUIDs cannot be substituted for
MotoGP peers. This change follows the accepted-info/notification pattern while
using the existing MotoGP HTTPS profile authentication and room authority.

## Implemented source changes

- `online/social.*`: one bounded background HTTPS worker, pending actions,
  friendship/identity snapshots, incoming invitations and accepted native room.
  Polling is every 3 seconds while supported, or 15 seconds when unavailable.
  Guest-frame/UI code performs no HTTP request.
- `online/social_guest.*`: bounded friend enumeration, signed-pointer argument
  decoding and the accepted-info handler. Deferred enumeration rechecks writable
  memory; closing a handle removes its size registration.
- `lan/runtime.cpp`: connects the native lobby Invite action to F1's friend
  picker, supplies native friendship queries and manages the worker lifecycle.
  Offline and LAN keep their original paths.
- `lan/guest_bridge.cpp`: retains the accepted server room as a join candidate.
  The original keyed Node join and native join/leave flow remain authoritative.
- `nobk_panel.cpp`: Invite buttons for online MotoGP friends while in an open
  lobby; the native Invite entry makes A/Enter invite the selected friend.
  Incoming rows use explicit accept/decline tickets. Clicking a row selects
  it; the accept ticket or A/Enter accepts, and X/Delete or the decline ticket
  declines. PlayStation uses Cross/Square. Duplicate answers remain disabled
  until completion/failure. Accepting closes F1 so the game can handle its notification. Existing
  F1 profile opening remains available outside the invite picker.

The existing public `/api/account/friends_of?name=...` remains the relationship
source for both the panel and server, with the stable-identity extension below.
The server resolves only **linked** MotoGP accounts by immutable account ID into
`0x0009000000000000 | player_id`; a reused/renamed handle or an unlinked profile
with a matching name cannot become another friend. Native friend enumeration supplies
actual MotoGP XUIDs to the original Friends leaderboard logic. The social feature itself does not change ranking/upload units. The same
source revision now requires [scoped class/season statistics](leaderboard-season-integration.md),
whose service implementation is still pending; native Friends filtering must
be accepted against that matching deployed service.

The native invite structure keeps the primary local profile XUID as invitee,
the sending MotoGP peer as inviter, and the real room host in `XSESSION_INFO`.
A lobby guest may invite their friends without being misidentified as host.

## Server contract and deployment

New route: **POST `/v2/social`**, with existing `Authorization: Bearer <profile
credential>`, `Content-Type: application/octet-stream`, and a UTF-8 JSON body
of at most 1024 bytes. No launcher token/password is added to the game or panel.

| Action | Request fields | Result |
| --- | --- | --- |
| `poll` | `action` | Capability, mapped friends, online flags, invitation list, whether this rider can invite from their current lobby |
| `send` | `action`, `to` (16-character MotoGP peer hex) | Current snapshot after queuing one invitation |
| `accept` | `action`, `id` | Current snapshot plus `accepted: {from, room}`; `room` is the exact server-owned native room bytes encoded as hex |
| `decline` | `action`, `id` | Current snapshot with that invitation removed |

Responses use `supported`, `can_invite` (true for a member of a joinable lobby),
`friends: [{name, peer, online}]`, and `invites: [{id, from}]`. With the feature
disabled the response is `{"supported":false}`. The inviter need not own the room.
Legacy servers returning HTTP 404 or 405 for the optional `/v2/social` route are
also treated as unsupported. The public F1 friends list remains available;
the invite picker explains the missing capability. Authentication errors and
other service failures still surface as invitation-service errors, and required
routes retain strict HTTP failure handling.

Friend lookup is cached for 30 seconds and performed off the UDP/event loop.
At most eight lookups run concurrently. Native identity lookups reuse the existing
unique `players.account` index; no social database migration is needed.
Invitation state is transient: expiry 120 seconds,
at most 16 per recipient and 512 service-wide. Sending is limited to four initial
requests and then one per five seconds per player. Duplicate clicks do not
create duplicate invitations. Existing HTTP/profile limits also apply.

Every operation requires a connected, linked account. Sending requires an online
NOBK friend and actual membership in an open lobby. Accept checks the recipient,
friendship, inviter membership, host, exact session ID, capacity, race state and
fresh room advertisement. Clients cannot submit a room key or arbitrary address
to direct another player. The existing authenticated join handshake still
decides membership. Restarting the service discards pending invitations.

For the operator:

1. Preserve the database, profile aliases, proxy key, domain and current service
   options. Review the supplied source diff before replacing files.
2. Extend the shared friends API with **numeric immutable account IDs**:

   ```json
   {
     "account_id": 101,
     "friends": [
       {"account_id": 202, "username": "Bob", "country": "TR", "avatar": "", "online": false}
     ]
   }
   ```

   The top-level ID must identify the account named by the request; each friend
   ID must identify that relationship's actual NOBK account, not a MotoGP player
   ID or already-offset FIFA XUID. Valid range is 1 through 2^31-1. Keep existing
   display fields and name-based callers compatible. The earlier inspection found no immutable IDs; this review did not
   re-query a personal friend list. Verify the actual deployed response: **this website/account-service extension is an
   operator prerequisite, not implemented in this repository**. Name-only or
   mismatched-owner replies return `supported:false` with reason
   `friends_identity_upgrade_required`; no invitation is authorized from them.
3. Install the updated `community_server.py`, `security.py` and new `social.py`.
   Run the supplied focused tests with Python 3.11+.
4. Add this option to the **existing** service command:

   ```text
   --friends-api "https://nobkonline.com/api/account/friends_of?name="
   ```

   The equivalent trusted loopback endpoint may be used when the shared account
   API runs on the same host. Keep `--account-api` and `--require-account` as
   already configured. Without `--friends-api`, invitations stay disabled.
5. Allow `/v2/social` through the existing Caddy API matcher. Preserve proxy
   authentication/header rewriting and the current domain. The existing 2 KB
   request cap is sufficient. The example now includes `/v2/link` as well.
6. Restart during a suitable maintenance window; existing rooms are transient.
7. Ship a client built from the matching source, then perform native Windows
   acceptance with two separate linked accounts.

## Focused verification and remaining acceptance

The server checks use isolated in-memory databases and local HTTP, not real
players or production invitations. They cover mapping/caching, a duplicate
legacy name, send/accept/decline, duplicate clicks, forged recipient, nonfriend,
offline recipient, expiry, changed/closed/racing/full room, a guest inviting to
another host's room, feature disabled, a name-only/wrong-owner account response,
an offline friend's renamed handle, and the real HTTP route/auth boundary.
Existing server core and selected HTTP/deployment security checks are retained.

The CPU codec probe checks exact native invite offsets, distinct inviter/host,
local profile identity, relay endpoint, room key, invite flag, friend stride,
UTF-8 names and canonical argument-pointer extensions. Earlier changed-source syntax checks and later local Windows build/package
checks are recorded separately. Neither establishes a real native NOBK
invitation or Friends leaderboard read with two linked production accounts.

Native acceptance must include: host and guest invitations, decline/expiry,
busy/full/replaced rooms, receiving in menus and during gameplay, native leave
confirmation, controller and mouse input, F1 close/reopen, Friends/global board
comparison, Offline/LAN behavior, and closing the game during an HTTP operation.
