# Season/class leaderboards: integration contract — 28 October 2026

Verified client source: `28ba765`. The user accepted native Windows menu text,
announcements and navigation. **The matching scoped service/storage/public API
is not implemented in the current repository.** This document specifies the
client's existing wire contract and required integration; it is not evidence
that these routes are deployed or that records are being stored successfully.

## Identity: six valid combinations, three native slots

| Season | Display badge/class | Category ID on wire | Native class slot | Raw season |
| --- | --- | --- | --- | --- |
| 2009 | A / MotoGP | 1 | 2 | 0 |
| 2009 | B / 250cc | 3 | 1 | 0 |
| 2009 | C / 125cc | 4 | 0 | 0 |
| 2010 | A / MotoGP | 1 | 2 | 1 |
| 2010 | B / Moto2 | 2 | 1 | 1 |
| 2010 | C / 125cc | 4 | 0 | 1 |

Reject Moto2/2009, 250cc/2010, unknown years/categories and category/view-slot
mismatches. Display letters are not category IDs. The abandoned simultaneous
four-class A/B/C/D proposal must not be revived.

Native views remain `1 + group*63 + slot*21 + track`, in 1..378. Track 17 is
reserved; DLC slots are 18 Silverstone, 19 Shanghai, 20 Istanbul. Native view ID
alone does not identify a season. Preserve group/mode semantics; do not invent
another native view block to separate Moto2.

## Existing client requests: implement exactly

All routes are authenticated POST requests to the existing verified community
HTTPS origin, with the normal profile Bearer credential and binary content
type. No player-supplied identity may override authenticated write ownership.
These are separate from the read-only website API and from `/v2/social`.

| Route | Body / successful reply |
| --- | --- |
| `/v2/stats/scoped/capabilities` | Empty request; JSON with `supported: true` and integer `protocol: 1` |
| `/v2/stats/scoped/read` | Four scope bytes followed by the legacy binary read body; legacy binary Page reply |
| `/v2/stats/scoped/write` | Four scope bytes followed by the legacy binary write body; empty successful reply |

The prefix is exactly **`[1, category_id, raw_season, 0]`**, four individual
bytes. It is not a packed integer or a 2009/2010 integer field. Require protocol
1, a zero reserved byte and a valid combination from the table. Keep existing
field byte order, string codec, sizes, bounds and response layout from
`src/online/client.cpp`, `src/lan/protocol.*` and `server/community_server.py`.

After the prefix:

- Read: U32 view, start, count; U64 around-user; U32 friend/user count followed
  by U64 peers. Page begins U32 total/count, then peer/rank/name/column records.
- Write: encoded nonce string, U32 view count, then U32 view/column count and
  U16 column, U8 type, U64 canonical bits. Retain the existing empty reply.

HTTP 404/405 on capabilities is handled as unsupported, not authentication
success. The client caches a successful capability answer, including unsupported,
for that client instance. An unsupported service raises
`Online server needs the season/class leaderboard update`; it **does not fall
back to mixed legacy boards**. Authentication/malformed responses remain errors.
Reconnect/relaunch after deploying a capability upgrade to a running test client.

Do not report support until read, write and persistent isolation are genuinely
available. Current `server/Caddyfile.example` allows only the old stats routes;
add the three exact scoped routes while preserving authentication, trusted-proxy
header rewriting, TLS, ports and existing service flags. Also update
`server/security.py::parse_headers`: its current path/length allowlist rejects
these routes before the handler runs. Capabilities has an empty request; the
legacy read/write maxima of 824/1750 bytes become 828/1754 with the four-byte
prefix. Keep every other route cap unchanged and validate the scoped body
independently of the proxy cap.

## Client capture and write behavior

`leaderboard_categories.*` owns explicit browsing selection. Scoped standalone
read/setup hooks prove their owning native board, then capture the selection.
Track-menu reads use native season state instead. `Spec.scope` copies the
identity before asynchronous work. Native slot values are raw 0/1/2, not flags
to shift again. See `leaderboard_scope_capture.*` and `leaderboard_scope_hooks.cpp`.

Race uploads do not take identity from the last browsed menu. `PrepareWrite`
requires each native view's property 13 (canonical int32, raw 0 or 1), derives
the category from that view's slot and groups writes by scope. Validate both
the prefix and stored season/track/mode properties on the server. For example,
same-rider 2009/250cc and 2010/Moto2 on the same track must coexist despite
sharing a native view ID.

One original submission nonce can be used across several scope groups. Make
retry/idempotency storage scope-aware: a `(player, nonce)`-only dedup table would
discard later groups. Preserve atomic best-score/metadata/timestamp updates and
the existing retry bounds. No new retry or offline upload queue is delivered by
the menu change.

Preserve raw time/rating conventions: property 2 is 60 Hz lap ticks, property 3
is `-(ticks*100)`, greatest is best. Keep full mode property 6, including
Championship bit `0x10000000`, track property 7 and season property 13. Never
replace them with rounded website times or infer upload season from UI labels.

## Storage migration: operator implementation, preserve data

Current source uses `scores` primary key `(player, view)` and public API SQL
partitioned by view alone. Merely adding labels or filtering that one surviving
row cannot restore the other season's discarded best. A compatible new store
must distinguish at least player, view and scope, with corresponding read/rank
and replay-dedup identities. This is a requirement, not a supplied SQL migration.

1. Preserve a verified backup of the production database and configuration.
   Do not delete `scores`, player/account IDs, aliases or existing timestamps.
2. Classify surviving rows using valid property 13 and the native slot. Keep
   missing/invalid/ambiguous rows in a clearly identified legacy collection;
   do not silently assume 2009, duplicate a row into both years or relabel all
   middle-class rows Moto2. Record provenance and migration counts.
3. Import only valid combinations, preserving exact score/properties/updated
   values. Define the older-client write policy explicitly. Legacy writes must
   never overwrite another season or bypass scoped validation.
4. Make the migration repeatable and reversible; compare counts and sample
   values before and after. Existing overwritten records require an earlier
   backup or new valid submissions, not invented reconstruction.
5. Deploy compatible storage, routes and proxy before advertising the client
   capability. Retain old-route behavior for callers covered by the agreed
   compatibility policy. Do not claim that current source already does this.

## Public API and consumer changes

`server/motogp_api.py` currently returns group-0 public boards with view-only
identity. Add explicit year/category to listings, board/rider/recent queries,
ranking partitions, totals, pagination and stable links. Final additive public
field/query names must be agreed with the website operator; they are **not**
the already-implemented binary prefix contract. Do not publish a guessed URL
schema as if the client supported it.

Retain stable existing routes/IDs for legacy consumers or version them with a
defined compatibility path. Every board row and ranking must use the requested
scope, not a season label attached after global ranking. Carry scope through
Records, Live, Riders, held-track flags, player pages and direct board links.

F1 public records and Rider License use these public APIs, separately from native
stats. Rider License already includes all published seasons, showing known years
from property 13. Its `ReadResults` and `ReadRanking` still deduplicate by view
alone, and the selected personal-best context carries only a view. Update these
identities and validations with the scoped public contract; simply returning
two years for one view would drop/reject duplicate view IDs or select the wrong
year. No such public-consumer upgrade is delivered by the native menu hook.

## End-to-end acceptance before release

- Same player/track: submit 2009 250cc and 2010 Moto2; each must remain readable,
  ranked and labelled independently after reconnect and service restart.
- Repeat for MotoGP/125cc across both seasons and at least one DLC circuit.
  Check raw views/properties and all supported native board groups.
- Faster/equal/slower repeats must retain the correct best and its timestamp;
  retries must not drop scope groups or duplicate aggregate writes.
- Check Friends/global, around-user, pagination, mode predicates, ranking totals
  and direct public board/rider links within the same selected scope.
- Reject forged/mismatched scope, invalid property 13 and incompatible servers;
  preserve Offline/LAN and old-client behavior within the agreed policy.

The user's latest menu test produced five missing-capability warnings, not five
successful queries. Menu entry explains why queries occurred; it does not prove
the required service is installed. No result-storage acceptance is claimed.
