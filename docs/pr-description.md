## Changes

Add four selectable custom gauges (MotoGP 17, MotoGP 06, URT2 and URT3),
including the converted resources, settings and URT2 shift-light refinements.
Update the Apex NOBK.FRIENDS panel, startup strip, achievements and compact
pit-board notifications, with controller hints and the related control/layout fixes.

Add native NOBK and Discord lobby invitation source. F1 provides explicit
accept/decline actions and per-friend sending/sent/retry feedback; incoming
invitations share the friend-online card's size and theme. Keep the native
session join authority and the separate authenticated invitation workers.
Discord activity also includes the updated circuit/mode/rider presentation
and launcher-first activation for closed-game invitations.

Separate native leaderboard browsing into **2009 SEASON** and **2010 SEASON**:

| Season | A | B | C |
| --- | --- | --- | --- |
| 2009 | MotoGP | 250cc | 125cc |
| 2010 | MotoGP | Moto2 | 125cc |

Preserve native transitions and one-level Back navigation, class-specific
headings and existing class/season announcer audio. Client queries/uploads now
carry explicit category/year identity. Include track-menu mode isolation and
Time Trial duplicate-query handling without altering the native score units.

## Validation

- Local Windows x64 source build and packaging checks passed with the pinned
  SDK/custom runtime. The user confirmed both seasons, all class menu selections,
  text, audio and repeated forward/Back navigation on Windows; their debug log
  agrees with those paths and normal shutdown.
- The user separately reported a working local Discord invitation, with Discord
  displaying `Joined`. This is one scenario, not full production acceptance.
- Temporary menu debug traces were removed after that test. This selective
  `preview` transfer has not been rebuilt or run through a fresh full suite.
- Existing preview history and upstream account/ranking/updater changes are
  retained. Local validation tools, CI/workspace rules, historical ZIP changes,
  screenshots and original game/player data are excluded.

## Integration required before public release

**The most critical remaining work is the scoped leaderboard backend and the
matching website/API/How to play update.** Current server source does not yet
implement the new scoped stats routes/storage. The client refuses mixed legacy
boards; the menu test's missing-capability warnings are not successful queries.

- Implement scoped storage, capability/read/write routes and conservative
  migration; preserve player IDs and surviving records. Then verify independent
  2009/250cc and 2010/Moto2 results for the same rider/circuit.
- Carry season/class identity through website queries, rankings, board/rider
  links and public consumers. Preserve the already-completed 20-track DLC
  catalogue. Correct the old `Back + Start` instructions and verify installation
  steps/download details against the matching final release.
- Supply verified immutable owner/friend IDs for native NOBK invitations, deploy
  the matching social/proxy source and check real-account send/accept/decline.
  Discord uses a separate capability path; verify production and cold launch.
- Ghost download is **not delivered as working replay transport/playback**.
  The current tree contains native analysis/API planning. ChunchunQ must implement
  or recover transport and complete native format/task/save/playback validation
  before enabling it. An earlier synthetic transport success is not native replay.

Operator read order:

1. [Master handoff](https://github.com/inflamesgo/motogp0910-recomp/blob/preview/docs/chunchunq-handoff.md).
2. [Scoped leaderboard contract](https://github.com/inflamesgo/motogp0910-recomp/blob/preview/docs/leaderboard-season-integration.md).
3. [Critical website and setup checklist](https://github.com/inflamesgo/motogp0910-recomp/blob/preview/docs/website-release-checklist.md).
4. [Invitations and ghost ownership](https://github.com/inflamesgo/motogp0910-recomp/blob/preview/docs/chunchunq-friends-invites-ghost-handoff.md).

No production service/database/site changes, release publication or real updater
transaction are claimed by this source transfer. Launcher remains 1.1.12;
the MotoGP manifest is already configured. The final integrated branch needs
its own Windows build and the remaining native/service acceptance checks.
