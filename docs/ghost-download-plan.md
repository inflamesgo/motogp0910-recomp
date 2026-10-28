# Native ghost download: analysis and proposed API

Current status reviewed on 28 October 2026 at `28ba765`. **This document is
native analysis and a proposed API/lifecycle, not implemented ghost download or
playback.** The user authorized infrastructure work in the earlier session and
reported a successful synthetic transport check; those later transport/fixture
sources are absent from this source anchor. Current `server/`, `src/online/`,
CMake and packaging contain no ghost route/storage/task/save/playback delivery.
Do not describe them as a complete shipped infrastructure needing only wiring.

ChunchunQ's integration must implement or recover that layer and verify the
native binding below. [Current feature ownership](chunchunq-friends-invites-ghost-handoff.md)
and [season/class identity](leaderboard-season-integration.md) are companion
contracts. No new game execution, fixture capture, service deployment or format
validation was performed during this documentation review.

## What the game already does

Analysis targets the supplied MotoGP TU 0.0.1.1 executable and its generated
functions. Keep the original game responsible for recording, save integration,
compatibility checks and racing against a ghost.

| Stage | Verified native path |
| --- | --- |
| Confirm overwrite | `8223CD00`, messages 1560/1561 |
| Start selected download | `8223CC30` → `8223CB68` → `822DAE50` → provider `822FFD58` |
| Identify remote object | Selected record XUID at `+8`; `825D95D0` builds its server path |
| Local upload path | `825D9538` builds the primary user's storage path |
| Build storage path | XLIVEBASE app `0xFC`, message `0x58035` |
| Start download/upload | `825DA398` / `825DA678`, original asynchronous XLIVE task machinery |
| Poll progress | `8223E630` → `822DAA08` → `822FF218`; `825D9530` → `825DB668`, message `0x58032` |
| Commit downloaded save | `822DAF00`, then original track/content/season compatibility logic |
| Offer race-now/start | `8223CDD0` (1562/1563), callback `8223C698` |

The current SDK lacks `0x58035` and `0x58032`; the community service has no ghost
object routes. The original starter opens its progress modal without propagating
a failed start. The no-start bookkeeping stays zero; progress remains zero
instead of reaching 100 or -1. This explains the reported infinite loading in
static analysis; it is not a captured trace of the user's session.

The repair must address both binary transport **and** the start/failure/completion
contract. Returning fake progress or reporting success with an empty buffer would
feed invalid data into the original save/ghost reader.

## Payload facts and confidence limits

- The **online** builder `822DB128` creates exactly **0x81AC = 33,196 bytes**:
  header/metadata `0x1AC`, followed by a fixed `0x8000`-byte recording body.
- Magic at `+0` is big-endian `0x47485354` (`GHST`); checksum is at `+4`;
  version at `+8` is **14**. A missing source causes version zero, not a valid
  empty ghost.
- Metadata is serialized at `+0x18` (`822D9DA0`) and `+0x190` (`822DB710`).
  The latter includes buffer/pointer/count fields; `822DB758` rebases native
  recording state on load. Do not treat these bytes as trusted addresses.
- `822DB1D0` checks magic/version/checksum, copies the body into the current
  recorder and applies its rebasing routine. Field meanings and all sample
  pointer/count constraints still need complete validation before remote files
  should be accepted.
- Native checksum `823983D0` is a byte/table routine called with seed zero and
  the checksum word temporarily zeroed. Verify the table, polynomial and final
  transform against real fixtures; do not assume a default `zlib.crc32` call is
  identical.
- Download capacity is **0x81C4 = 33,220**, larger than the actual online payload.
  It is a buffer capacity, not a stored object length.
- **Correction to the earlier working notes:** `822DAFD0` uses `lis r5,1` for
  its separate local body and `0x101AC` for checksum length. That is a
  **0x10000-byte local body / 0x101AC = 65,964-byte local payload**, not
  `0x1000` / `0x11AC`. This distinction was rechecked against generated code;
  online storage must not accept that local form as if it were `0x81AC`.
- Native names include G125/G250/GMGP-style class names and `_%02d` circuit
  suffixes (`822DAB98`), plus UserGhosts/NetGhosts save categories. Their complete
  class/season/period mapping is not established. Preserve native names as
  metadata, not as filesystem paths or the sole identity of a record.

`822DB280` preserves the local ghost before online qualification. Provider
`822FFA60` reads the two ghost statistics views and checks rank against 100;
`822FFC08` uploads to the qualifying view list. This supports a top-100 native
publication policy, not a promise that every lap or every leaderboard row has
a downloadable file.

## Proposed record identity

A ghost must describe the **same published best** as the selected row. Keying
only by player and track lets a later/slower lap or another season replace the
file for an earlier best.

Use an immutable record reference derived server-side from:

- Persistent MotoGP player ID (not FIFA/site XUID, IP or display name).
- Native view ID and applicable period identifier.
- Canonical published score/rating and relevant native properties, including
  track, class, mode bit field and season property 13.
- A stable record revision/digest produced by the server's canonical data.

The existing view layout has 21 track slots, with reserved slot 17 and DLC
18–20. Do not compact the hole or infer a 2009 season when property 13 is
missing. Time Trial/Arcade/Career/Championship filtering and ghost publication
views must remain distinct. The category/year namespace is now agreed in the scoped integration contract.
Ghost publication/naming/period binding still needs native upload/download
fixtures; sharing native slot 1 does not permit sharing 250cc/Moto2 objects.

The current service's statistics write reply is empty. Preserve that legacy
contract; add separate preparation/capability requests rather than silently
changing existing native stats results or extending the eight display columns.

## Proposed API (not implemented)

Use the existing verified HTTPS origin and profile credential; no new password,
launcher token, peer-provided URL or public filesystem route. All routes below
are **POST**, consistent with the existing control API. Names and field schemas
are proposals to agree before coding.

| Proposed route | Request | Reply / purpose |
| --- | --- | --- |
| `/v2/ghosts/capabilities` | Empty or small versioned request | Service version, GHST format 14, exact maximum payload, supported view/period policies |
| `/v2/ghosts/prepare` | Current view and canonical record reference for the authenticated owner | Short-lived, single-use upload ticket bound to that precise published best |
| `/v2/ghosts/commit` | Upload ticket plus the exact native `0x81AC` bytes | Validate and atomically publish the object for the ticket's unchanged record |
| `/v2/ghosts/read` | Selected rider's MotoGP ID, view, period and canonical record reference | Exact native payload, or a typed missing/stale/incompatible response |

Preparation follows completion of the existing record upload. The client must
also verify that its current ghost metadata describes that exact record;
merely obtaining a ticket for some older server best is insufficient. Commit
rechecks the best/revision to handle concurrent new records. A late transfer
cannot overwrite a newer best's ghost. Ownership always comes from the existing
credential, never a submitted player ID or filename.

Retain the payload exactly, with SHA-256/length in service metadata for stored
object integrity. SHA-256 is independent of the native GHST checksum; both have
different purposes. Read rechecks the selected record reference and stored
integrity before returning bytes. A stale object must not be substituted for a
different season, circuit, mode or record.

No object exists for historical records whose recording was never uploaded.
Those downloads must fail promptly with a clear unavailable result. A future
validated owner migration may import matching local ghosts; lap-time statistics
alone cannot reconstruct a rider's recorded inputs/trajectory.

### Storage and admission

- Keep ghost objects/metadata in explicitly configured persistent storage;
  never in package assets, user save folders or arbitrary submitted paths.
- Enforce exact online length, magic/version, native checksum and complete
  metadata/body pointer/count bounds before publishing. Record compatibility
  must also be checked. Header-only validation is insufficient.
- Authenticate writes, keep short ticket expiry and reject replay. Limit
  upload/read rates, per-player object counts/bytes, total bytes, pending work
  and response time. Use atomic publication and clean up abandoned temporary
  objects after bounded expiry.
- Retain current objects while their referenced records remain current. Define
  retention/collection explicitly; avoid deleting unrelated persistent records.
- The HTTP parser and Caddy currently cap ordinary requests below the 33 KB
  payload size. Add explicit ghost route limits (ticket + **33,196** bytes),
  preserving all existing route caps/proxy authentication. Do not raise every
  endpoint's limit to accommodate ghosts.
- Monthly views are a separate unresolved contract: define the actual period
  used by native queries and service ranking before implementing expiry/read
  selection. Do not silently serve an all-time ghost for a monthly row.

## Client task and UI lifecycle

Adapt the original storage/provider interface after verifying its full argument
and completion layouts. The path builder should produce a bounded internal
object identity/handle, not a remote URL or host filesystem path. Keep the
existing native recording/qualification/save/race code.

1. Validate/copy upload input and operation arguments before returning from a
   native call; its XDK argument lists are stack-backed.
2. Execute HTTPS on owned workers. Keep guest output/overlapped lifetime under
   the original asynchronous contract and revalidate memory before completion.
3. Expose explicit pending/success/failure/cancelled states. Check the native
   starter's result before opening its downloading modal; failed starts and
   missing objects must terminate instead of retaining zero progress.
4. Only successful, exact-length validated bytes may complete as a downloaded
   ghost and enter `822DAF00`. Preserve native compatibility and overwrite/race
   confirmation; do not modify local saves to conceal a failed transfer.
5. Bound transfer deadlines and handle cancellation, disconnect, menu exit and
   shutdown. Never issue a late callback or write into released guest buffers.
6. With an old/disabled server, preserve local ghosts and fail the Online
   download clearly. Offline/LAN must not start production storage requests.

## Acceptance before enabling

First obtain native fixtures for: one validated upload/download pair, two seasons
on a reused class slot, Moto2, a DLC circuit, top-100 qualification, both ghost
categories and a real stored record revision. Verify the checksum and every
pointer/count field used by the reader against these fixtures.

Then check missing and stale records, slow/failed/cancelled transfers, truncated
and oversized payloads, bad magic/version/checksum, hostile pointer/count values,
wrong owner/view/season/mode, upload replay and a new best arriving during commit.
Compare stored/downloaded bytes and demonstrate original on-track ghost playback
between **two native Windows clients**. Source inspection and an HTTP round trip
do not establish successful replay.

Implement or recover the transport source explicitly; no such layer is bundled
with this design. Deliver the service capability and client failure handling first; enable real
objects only after format and identity validation pass. This plan intentionally
leaves unresolved native details explicit instead of inventing a working binary
contract.
