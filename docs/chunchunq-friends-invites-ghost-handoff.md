# Friends/invitations and ghost handoff — 23 October 2026

Prepared against MotoGP fork commit `d952868`. The supplied patch is local source
work, not a published commit or built game. Review/merge it against your current
branch before applying it. No live service deployment or player invitations were
performed.

## Contents and status

| Area | Delivered | Still required |
| --- | --- | --- |
| Native Invite → F1 friend picker | Client source | Matching server/API deployment and two-player native Windows acceptance |
| Incoming invitations | Server/client source, F1 accept/decline, native join notification/info | Native menu/race/leave-prompt and controller acceptance |
| Friends leaderboard | Real NOBK-account → MotoGP-peer mapping and native friend enumeration/query source | Shared API identity fields, then native Friends/global comparison |
| Ghost download | Verified native analysis and proposed API/storage/task lifecycle | Agree unresolved format/record/period details, then implement and verify separately |

The shared F1 source patch also carries the local drawn server-status indicator
and the row-scroll correction: hovering no longer continually centers a row;
explicit keyboard/D-pad navigation can still reveal selection. It excludes the
pending Discord changes and the General-page Discord checkbox relocation.

## Read these two documents

- [Friends and invitations: code paths, API, deployment and acceptance](online-friends-invites.md).
- [Ghost download: native evidence, API proposal and unresolved details](ghost-download-plan.md).

The **shared NOBK friends endpoint needs a compatible identity extension**:
numeric `account_id` at the root and in each friend entry. The inspected public
endpoint currently returns display names alone. The prepared social service
deliberately reports unsupported until it can verify immutable IDs and the list
owner. This account-service change is not present in the public MotoGP repository
and must be made by its operator. Existing public/F1 callers can ignore the new
fields. Do not authorize invitations by matching mutable display names.

Install the updated community service/security module plus new `social.py`, add
`--friends-api` to the existing service command and allow `/v2/social` through
the existing authenticated Caddy route. Keep the domain, UDP ports, proxy secret,
SQLite identity/record data and account-link flags. Full steps are in the invite
document. No new launcher password/token or runtime DLL is introduced.

## Local verification supplied

The focused server run covers the new social behavior, actual local HTTP/profile
authentication, existing server core, and selected HTTP/deployment security tests.
The CPU probe checks the native invitation/friend binary layout, profile/peer/host
distinction and signed argument-pointer representation. Six changed Windows
translation units passed syntax checks. The bundle's `verification.json` records
this scope and source hashes.

Reproduction from the project root:

```text
python3 -m unittest discover -s server -p test_social.py
python3 -m unittest discover -s server -p test_community_server.py
```

`online_social_probe` is an explicit, excluded-from-default-build CMake target.
It does not start a game, contact production or send an invitation.

No application build, live Windows game join, native Friends leaderboard UI,
physical controller run or production API extension has been verified. Those
remain acceptance gates before publishing this feature as working.

## Package use

`source.patch` contains the related client/server/build-description changes and
documentation against the fork baseline. Apply it once after review; the
separate `server/` directory contains full operator-facing files for comparison
or deployment. It is not a second patch to apply on top of identical content.
The archive includes no game data, saves, profiles, databases, proxy keys,
credentials, application binaries or nested archives.

The interface is independent of the social transport: notification position,
layout, duration, sounds and F1 placement may be changed locally while preserving
the API, identity validation and native acceptance/join contract. FIFA's current
reference UI uses a top-right ten-second notification, then F1/held Back to open
pending invites and Enter/A or Delete/X to answer; MotoGP retains its own F1
design and configurable input gates.
