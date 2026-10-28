# Release readiness and remaining work — 1.2.0

The user approved **1.2.0** for the PR launcher source and current release
documentation. The game's Title Update stays **0.0.1.1**; SDK/runtime pins and
historical test package versions remain unchanged. This is a source/integration
handoff, not a statement that every dependent service is deployed.

## Prepared locally

- Launcher version and synthetic update-panel version use the same **1.2.0**
  configuration. Current handoff, PR body, updater manifest example, setup and
  release instructions agree on **1.2.0** / **v1.2.0**.
- Local preview source includes dual-source update checking/selection and
  Multiplayer CHAMPIONSHIP text/native announcement. Earlier local Windows
  1.1.12/1.1.13 builds and package checks passed. A subsequent explicit request
  produced local 1.1.14/1.2.0 updater-UI test packages; native appearance/update
  acceptance still requires the user test.
- A separate matched **1.1.13** ZIP/manifest was prepared for testing the
  1.1.12 updater. Those historical files are preserved, not relabelled 1.2.0.
- The user observed the GitHub offer and supplied download/restart screenshots.
  The new local UI work uses GitHub Release-description notes, an orange
  download fill and no separate normal helper-status window. The old screenshots
  do not validate these later presentation changes or rollback.
- Website corrections are documented: exact native Multiplayer/Online path,
  no required Verify step, held Back/Select, PC/two-season About text, named TU
  and all three DLC packages, and a comparison-backed setup-page redesign.

## Still required

| Work | Owner / present state | Completion evidence |
| --- | --- | --- |
| Build/package final integrated release | Local 1.2.0 test build/package passed; recipient must verify its final integrated branch | Matching launcher/helper, final package/hash and v1.2.0 metadata/site copy; do not reuse a local hash for a differently rebuilt package |
| Real update transaction | Maintainer/user; GitHub offer and download/restart presentation observed in the user's test; complete/negative-case acceptance remains | Confirm final installed version and stable restart; test the later notes/fill/headless-helper UI and disposable-installation cancellation/conflict/rollback cases from the updater doc |
| Multiplayer CHAMPIONSHIP visual/audio | User/native Windows tester; included in previous local builds, no reported acceptance yet | Correct uppercase option and Championship voice, with single-player Career unchanged |
| Scoped leaderboard service/storage/public API | ChunchunQ/service operator; client carries scope, service still lacks the scoped routes/storage | Independent 2009/250cc and 2010/Moto2 results, scoped reads/writes, safe migration/idempotency and public consumer identity |
| Website release/help changes | Website operator; notes/copy prepared, live site not edited here | All P0 items in the website checklist implemented against the final build, including season/class separation and accurate installation instructions |
| Production invitations | Shared API/service operator plus two real accounts | Verified immutable IDs for native friends, matching service deployed, accept/decline/expiry/native join; Discord warm local success is already user-reported, cold-launch/production scenarios remain separate |
| Ghost download implementation | ChunchunQ/integration owner; only native analysis and API planning exist in this tree | Actual transport and native recording/task/save/playback implementation with validated fixtures; do not advertise as working meanwhile |

The most critical release integration remains scoped leaderboard data separation
and the matching website/API/How to play correction. A working client menu or
a successful build does not complete that backend work. Ghost planning is not
a finished transport foundation. The user asked to list these gaps explicitly;
do not silently mark them done for PR presentation.

## Deliberately deferred or not requested

Windows/Linux release automation is agreed for **after the replacement repository
is created**. The planned manual Actions workflow will build/check both platforms,
publish both packages in one Release and generate hashes/Windows metadata;
ordinary commits must not publish releases. No workflow was changed or dispatched.

Moving/hiding `achievement-ui.toml` and `hud-display.toml` was a question only;
no relocation was requested or performed. Existing LocalAppData Online identity
and remembered sign-in files must not be deleted or moved as part of this work.

See [master handoff](chunchunq-handoff.md), [PR preparation](pr-handoff.md),
[updater contract](launcher-updates.md), [scoped integration](leaderboard-season-integration.md)
and [website checklist](website-release-checklist.md).
