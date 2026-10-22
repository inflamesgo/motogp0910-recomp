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
