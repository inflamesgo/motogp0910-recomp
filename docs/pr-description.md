## Changes

Prepare launcher **1.2.0** with four selectable custom gauges, the Apex friends
panel and compact pit-board notifications, native/Discord lobby invitations,
and separate 2009/2010 native leaderboard browsing. Preserve Offline/LAN, F1
(or held Back/Select), F2 settings and the original native join authority.

| Season | A | B | C |
| --- | --- | --- | --- |
| 2009 | MotoGP | 250cc | 125cc |
| 2010 | MotoGP | Moto2 | 125cc |

Leaderboard reads/uploads carry explicit category/year identity. Native menu
transitions, one-level Back navigation and class/season announcements are retained.
Multiplayer's CAREER option becomes CHAMPIONSHIP with the existing Championship
voice; single-player Career is unchanged. The gauge resources and matching
custom runtime are included, with URT2 shift-light and input/layout refinements.

Invitations have per-friend sending/sent/retry feedback and explicit accept/decline.
Incoming invitations share the friend-online card's size/theme; Discord also
supports launcher-first activation for closed-game invitations.

The updater checks stable GitHub Releases first, then the existing NOBK manifest,
and rechecks before installation. Compatible files can be combined only when all
common application files and the complete EXE/DLL sets match; conflicts install
the higher-version full package, with GitHub priority on a numeric tie. Retain
SHA-256 checks, protected game/user paths, matching-helper acknowledgement,
backup and rollback. Use GitHub's Release description for notes, draw progress
inside the download button and omit normal helper-status windows. Commits alone
do not publish updates. Game TU **0.0.1.1** and SDK/runtime pins are unchanged.

## Validation

- Local Windows x64 cross-builds/packages passed with the pinned SDK/runtime,
  including the latest 1.1.14/1.2.0 updater-UI pair. ZIP CRC, compiled version,
  binaries/runtime/resources, private-data exclusions and helper imports were
  checked. The eight later production files match that primary build source.
- The user accepted both seasons/all class menu selections, text/audio and
  forward/Back in the earlier Windows menu package, and separately reported
  one working local Discord invitation (`Joined`). GitHub update discovery
  and download/restart UI were also observed. These are scoped observations,
  not complete production/cold-launch/rollback or latest visual acceptance.
- The selective preview branch has not received a separate build or fresh full
  suite. Existing target ancestry, account/ranking fixes, CI/workspace rules,
  local tools, historical archives/screenshots and original game/player data
  are preserved under the agreed transfer boundary.

## Integration required before public release

**P0: implement scoped leaderboard storage/API and the matching website/help
corrections.** Current server source lacks the scoped stats routes/storage;
the client refuses mixed legacy boards. The menu test's missing-capability
warnings are not successful record queries.

- Implement the scoped capability/read/write and migration contract, preserving
  player IDs/records. Verify independent 2009/250cc and 2010/Moto2 records for
  the same rider/circuit; carry scope through rankings, links and public consumers.
- Complete the website checklist: accurate setup cards, no required Verify step,
  Multiplayer → Online → race type → lobby, held Back/Select, both-season About
  copy and named TU/three DLC prerequisites. Preserve the existing 20-track work.
- Supply verified immutable friend IDs for native invitations, deploy the
  matching social/proxy modules and verify real-account accept/decline/join.
  Discord has separate capability requirements; verify production/cold launch.
- **Ghost transfer/playback is not implemented here.** Native analysis/API
  planning is supplied; transport and native task/save/playback integration
  still belong to the recipient. Do not advertise it as working meanwhile.
- Build the final integrated **1.2.0** launcher/helper and publish matching
  **v1.2.0** package/hash/manifest/site details. Both publishers must retain
  this updater and coordinate increasing product versions. Verify installation,
  conflict fallback and rollback before public delivery.

Read first:

1. [Master handoff](https://github.com/inflamesgo/motogp0910-recomp/blob/preview/docs/chunchunq-handoff.md).
2. [Scoped leaderboard contract](https://github.com/inflamesgo/motogp0910-recomp/blob/preview/docs/leaderboard-season-integration.md).
3. [Critical website checklist](https://github.com/inflamesgo/motogp0910-recomp/blob/preview/docs/website-release-checklist.md).
4. [Invitations and ghosts](https://github.com/inflamesgo/motogp0910-recomp/blob/preview/docs/chunchunq-friends-invites-ghost-handoff.md).
5. [Release readiness and ownership](https://github.com/inflamesgo/motogp0910-recomp/blob/preview/docs/release-readiness.md).

No production service/database/site modification, release publication or merge
is performed by this source PR. Windows/Linux Release automation remains
explicitly deferred until the replacement repository is created.
