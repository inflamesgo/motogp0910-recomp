# Repository working instructions

- Write all new and updated Markdown documentation in English. Preserve exact
  localized UI labels, glyph samples, identifiers and source quotations where
  necessary to explain behavior; explanatory prose must be English.
- Development Windows packages omit README files, feature readmes, SHA256SUMS
  and separate license folders. Preserve source licenses and embedded launcher
  notices. The user will add the final release README later.
- Do not include an empty `logs/` directory in packages. Runtime logging creates
  it when needed; keep existing historical ZIPs unchanged.
- Keep original game data, user configuration, saves and personal logs out of
  commits/packages. Preserve historical release ZIPs; use unique archive names.
- Windows 10/11 x64 is the product target. Distinguish Wine/CPU/UI checks from
  native Windows GPU playthroughs and physical controller testing.
- Preserve Offline, LAN and community Online modes. Do not make speculative
  physics/handling changes or add frame generation; both are outside scope.
- Use the SDK/toolchain pins in sdk-version.json and the custom runtime manifest.
- Write commit subjects in English as `type(scope): short imperative summary`.
  Use `feat` for new features, `fix` for corrections, `ci` for Actions changes,
  and `docs`, `build`, `refactor`, `perf` or `chore` when appropriate. Keep each
  subject specific to the actual change.
- Unless the user explicitly requests otherwise, "build" means commit and push
  the authorized changes to `main`, then dispatch both `windows-build.yml`
  (`quality_checks=false`) and `quality-checks.yml` (Quick quality checks) on
  `main`. The user explicitly authorized Quick quality checks in Actions for
  these build requests; do not run local builds or tests. Include `[skip ci]`
  in the commit body to avoid duplicate push-triggered checks, then dispatch
  the two workflows explicitly. After dispatch, do not inspect Actions status,
  logs, artifacts or results. The final response should only confirm the push,
  Windows build and Quick quality checks dispatch; do not repeat that the
  results are not being monitored.

## General working principles and partner judgment

- Apply these principles to every activity: implementation, debugging, research,
  design, documentation, tool use, testing, builds, packaging, PRs, integration
  and deployment. They are not limited to validation.
- Treat the user's time, attention and token/compute budget as project resources.
  Challenge a request plainly when its likely cost, scope, technical debt or
  risk harms the goal. Explain the concrete tradeoff and recommend a better
  approach before incurring avoidable cost. Do not silently overrule an explicit
  decision, repeatedly ask for approval, or blame the user's instructions for
  poor execution choices. Exercise independent professional judgment.
- For substantial work, establish the intended outcome, necessary scope,
  proportionate effort and completion criterion before starting. Communicate
  material cost or uncertainty briefly. Simple tasks should remain simple;
  do not impose a planning ceremony or approval flow on routine choices.
- Choose the smallest reliable approach that completes the authorized goal.
  Avoid speculative abstractions, unnecessary dependencies, unrelated cleanup,
  excessive research and turning a local fix into a redesign. Address unrelated
  findings separately unless they block delivery or are clearly in scope.
- Preserve useful findings and completed work. Reuse relevant evidence instead
  of repeatedly searching, rereading, rebuilding or recreating artifacts without
  a reason. Keep tool output and progress messages focused on decisions and
  material findings, not a transcript of activity.
- Use bounded attempts and checkpoints for expensive or uncertain operations.
  If attempts produce no new evidence or costs exceed the estimate, diagnose
  the cause and change the approach rather than repeating an ineffective loop.
  Explain a material change in scope or effort promptly; elapsed time alone is
  not a reason to abandon necessary work or silently increase its scope.
- Stop adding work when the agreed outcome is met. Report what was delivered,
  how it was verified and material remaining limits without inflated claims or
  unsupported promises. Requests to improve our process should lead to focused
  safeguards, not another costly project.

## Validation-specific safeguards

- Before local validation, state the affected behavior, smallest useful checks,
  expected duration and completion criterion. "Test everything" requires a
  defined scope; it is not permission to expand indefinitely into unrelated
  legacy systems. Honor an explicitly requested full suite, but disclose its
  cost and known environment limitations before starting it.
- Start with changed-area checks and necessary integration smoke checks.
  Reserve broad regression for changes with broad dependencies, explicit user
  requests or concrete failures that justify it. Cosmetic changes do not
  automatically require server, file-integrity, ISO-import or full-launcher
  audits. Do not use assertion totals as a substitute for useful coverage.
- Use five minutes as a diagnostic checkpoint, not a promise that every test
  can finish within it. After two unsuccessful attempts, or five minutes with
  no new diagnostic evidence, stop rerunning the same expensive command.
  Inspect logs/source, classify product vs. stale-test vs. fixture/environment
  failure, and isolate the failing phase before the next run. A timeout needs
  a diagnosis; increasing its limit alone is not a fix.
- Preserve passing evidence while relevant production code and dependencies
  remain unchanged. After a fix, rerun the affected check and its actual
  dependencies. Do not restart a broad suite merely to obtain a prettier
  all-green report. Unrelated pre-existing failures must be identified rather
  than turning into an unbounded side project.
- Prefer the regression runner's `--probe` and `--launcher-phase` selections.
  It creates a new isolated fixture for each invocation. Never manually reuse
  a stateful test installation or profile; do not alter test expectations just
  to make a failure disappear. Check them against the approved product contract.
- Batch related source/test corrections before rebuilding large translation
  units. Do not run competing interactive UI tests on one display or concurrent
  builds in one build directory. Reversible read-only work may run in parallel
  when it does not invalidate timing, focus or input checks.
- If a run exceeds its estimate, give one concise update with the cause,
  completed evidence and changed next step. Do not repeatedly say "last test"
  or "almost done" without evidence. Keep raw logs in files and read only the
  relevant excerpts; avoid repeated whole-file reads and redundant narration.
- Finish when the stated acceptance checks pass and the requested build is
  ready. Report failures, skips and native-platform limits honestly; phased
  checks are not a clean full-suite run. Process/documentation feedback alone
  does not authorize another expensive build or regression campaign.
