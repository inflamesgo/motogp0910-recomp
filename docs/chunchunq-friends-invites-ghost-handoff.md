# Friends, Discord invitations and ghosts — 28 October 2026

Source anchor: fork `main` at **`28ba765`**. Use the committed repository and
[PR transfer manifest](pr-handoff.md), not the older `d952868` patch/bundle
instructions. This document reflects current files; older verification receipts
and local fixtures are not supplied artifacts unless they are actually present.
No live service, website, account or database was modified by this review.

## What is delivered and what is still owned by integration

| Feature | Current source | Evidence / remaining work |
| --- | --- | --- |
| Native Invite → F1 friend picker | `social.*`, `social_guest.*`, room bridge and server social module | Native codecs/protocol checks exist; production friend IDs and two-real-account join acceptance remain |
| F1 outgoing invitation tickets | Per-peer pending/sent/retry, duplicate-send guard and room invalidation | Included in local Windows builds; dedicated live invitation/controller acceptance remains |
| Incoming invitation | Compact 404 × 108 pit board, F1 accept/decline and answer deduplication | Presentation implemented; no single-PC fake peer proves a real race join |
| Native Friends leaderboard | Verified account-ID → MotoGP-peer enumeration and query path | Shared immutable identity extension and deployed scoped stats are required |
| Discord invitation | Local IPC, capability publication/redemption, native accept and launcher activation; server module present | User reports a working local invitation with Discord showing `Joined`; cold launch/production/two-account acceptance remains separate |
| Ghost transport/playback | Native evidence and proposed API only in current source | Earlier synthetic transport success was reported, but its later source/fixture is absent; no tracked ghost routes/task/save/playback binding can be delivered from this anchor |

The interface uses the existing Apex/pit-board theme. Incoming toast position,
size, portrait and animation match friend-online notifications. F1 has explicit
`ACCEPT` and `DECLINE` tickets; clicking the row only selects it. Xbox A/X and
PlayStation Cross/Square hints are shown, while keyboard actions still work.
Outgoing tickets show `INVITE`, `SENDING…`, acknowledged `SENT` or `RETRY`.
A sent acknowledgement does not prove delivery, acceptance or room membership.

The startup strip uses F1 by default and `Controller: hold Back` for Xbox or
`Controller: hold Select` for PlayStation prompts. Keyboard style omits the
controller hint. Online-only notifications/panel startup must not be enabled
in Offline/LAN merely to exercise the UI.

## Native invitation deployment

Read [the native invitation contract](online-friends-invites.md). Preserve the
existing authenticated profile, account links, database, proxy key/domain and
UDP ports. The shared friends endpoint must provide immutable numeric
`account_id` for both the requested owner and each friend. Names alone do not
authorize invitations or map FIFA/site XUIDs to MotoGP peers.

Current files include `server/community_server.py`, `security.py`, `social.py`
and `discord_invites.py`. Configure the existing `--friends-api` option and
allow `/v2/social` through the trusted proxy; retain `/v2/link` and account
options. Native social support stays unavailable until the service can verify
friend identities. No new launcher password/token or runtime DLL is required
for the invitation protocol itself.

Native invite info uses the title's existing 84-byte `XINVITE_INFO`, distinct
local invitee/sending peer/real host identities and accepted-room notification.
A guest may invite someone to their host's lobby. Keep the server-owned exact
session/key, room freshness/state/capacity and final keyed join authority.

## Discord is a separate capability path

Read [Discord deployment and acceptance](discord-invites.md). It uses the same
`/v2/social` route but does not depend on `--friends-api` or the immutable
friend-list extension. Connected linked accounts, a joinable advertised lobby,
Discord desktop/sharing and the enabled activity option are still required.
No Discord bot token, OAuth secret, voice channel or Discord server is required.

Deploy the matching module/service/proxy before expecting the local behavior
on production. The user's successful local scenario is useful evidence, not a
100% guarantee of account/proxy/closed-launch timing in another deployment.
Test an open game and a closed game separately; closed acceptance must go
through the launcher, retaining normal setup/sign-in/PLAY validation.

## Ghost work assigned to ChunchunQ

Read [native analysis and the proposed API](ghost-download-plan.md). The earlier
console message `Ghost transfer ownership, protocol, failure and cancellation
checks passed` was a successful **synthetic** check, not a failure and not native
playback. It does not prove those transport/fixture sources are in this commit.
The current tree has no `record_scopes.py`, ghost storage routes, task adapter
or native save/playback implementation. Do not claim the recipient only needs
to connect an already-delivered complete infrastructure layer.

Use the agreed season/class identity and an immutable published-record revision.
Verify online GHST format/checksum/pointer-count bounds with real fixtures,
then implement authenticated bounded transport, task failure/cancellation,
original save integration and two-client on-track replay. Preserve unavailable
results for records that never had a recording; never fabricate progress or a
valid empty ghost. Keep this feature explicitly unavailable until that work passes.

## Verification handoff

The selective transfer includes `server/test_social.py` and
`server/test_discord_invites.py`. The historical codec probe
`tools/online_social_probe.cpp` belongs to the main source branch and is excluded
from this transfer along with newly added local probe targets. Historical
reports describe focused protocol/HTTP/codec checks; they are not a fresh full
suite run at `28ba765`. Reproduction commands are in the feature documents.

Local Windows packages include the invitation UI and season menu changes.
The user's latest acceptance and log cover **leaderboard** text/audio/navigation,
not every invitation screen or physical controller. Live native acceptance
must still cover real-account send/receive/decline/expiry, renamed identities,
full/closed/recreated rooms, native leave confirmation, menu/gameplay receive,
Friends/global boards, shutdown and Offline/LAN.

Also complete [the scoped service contract](leaderboard-season-integration.md)
and [critical website/help work](website-release-checklist.md). Native menu
acceptance does not prove that the server accepted or stored leaderboard queries.
No new source archive, build, tests or PR is created by the documentation review.
The user subsequently authorized the selective `preview` commit/push; no
production deployment is included.
