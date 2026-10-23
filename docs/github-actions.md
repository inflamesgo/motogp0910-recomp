# Windows builds with GitHub Actions

The `Windows build` workflow is deliberately manual-only. Pushing commits,
opening pull requests and editing documentation do not start builds. Do not
dispatch the workflow until the owner requests a build. No scheduled
builds, releases or Pages deployments are configured by this workflow.

## Connect the private inputs

Compilation needs the stock `default.xex` and the extracted title-update
`default.xexp`. It does not need the full disc image or game asset directories.
The existing local copies under `assets/game-tu/` already have the required
hashes; the downloader and Windows build helper verify them again.

1. Create a ZIP containing only `default.xex` and `default.xexp` at its root.
   Keep this archive outside the tracked source files.
2. Store it in private HTTPS storage that provides a direct download URL, such
   as a signed object-storage URL. A file-sharing preview page will not work.
   Keep the URL valid through the intended build, and replace it when it expires.
3. Open the repository's **Settings > Secrets and variables > Actions**.
4. Add a **New repository secret** named `GAME_INPUTS_URL`, with the private
   direct URL as its value. Enter it in GitHub settings, never in a commit,
   workflow input, issue, public release or chat message.

Google Drive file sharing links are also supported. Upload the two-file ZIP,
enable **Anyone with the link > Viewer**, leave downloads allowed, and put the
normal file sharing URL into `GAME_INPUTS_URL`. The downloader converts it to a
direct download URL. Leave `GAME_INPUTS_TOKEN` unset for Drive. This option does
not require a GitHub token, but anyone who obtains the Drive link can download
the file; storing the link as a secret does not change its Drive permissions.
Google download quotas or an additional consent page can still block a run;
the downloader stops if it receives a web page instead of the expected ZIP.

Alternatively, use a separate private GitHub repository, for example
`inflamesgo/motogp0910-build-inputs`:

1. Create the repository as **Private**, initialized with a README. Do not add
   the original game files to its Git history.
2. Create a non-draft release with tag `inputs` and attach the two-file ZIP
   named `build-inputs.zip`. A release in a private repository remains private.
3. Under personal **Settings > Developer settings > Personal access tokens >
   Fine-grained tokens**, generate a token restricted to that private repository
   with **Contents: Read-only**. Choose an expiry and renew it when necessary.
4. In the main project's Actions repository secrets, set `GAME_INPUTS_URL` to
   `https://api.github.com/repos/inflamesgo/motogp0910-build-inputs/releases/tags/inputs`
   and `GAME_INPUTS_TOKEN` to the token. Enter the token directly in GitHub;
   do not send it in chat. Use your actual owner/repository name if different.

The downloader resolves `build-inputs.zip` from that release automatically.
`GAME_INPUTS_TOKEN` is optional for signed storage URLs, and is only accepted
for GitHub API URLs. Authorization is removed on redirects to other hosts.

The workflow receives these secrets only for the connection check and download
steps. It checks the original SHA-256 hashes before compilation. Original
inputs, generated code and intermediate build directories are not uploaded or
cached. The inputs are removed at job completion, including failures.

## Run only when requested

After the private secret is configured and the owner requests a build:

1. Open **Actions > Windows build > Run workflow**.
2. Select `main`, enter a short English **build_label** describing the changes
   (for example `discord-track-artwork`), leave **quality_checks** off for an
   ordinary build-only request, and click **Run workflow**. Other branches are
   skipped. If the label is blank, the latest commit subject supplies it.
3. Also dispatch **Quick quality checks** on `main`, as the owner requested for
   ordinary build requests. This is separate from the optional Windows probe suite.
4. Both the downloadable artifact and its application ZIP use the descriptive
   name `MotoGP0910-Windows-x64-<change-label>` (plus `.zip` for the ZIP).
   The downloaded ZIP opens directly to the launcher, `bin/`, `resources/`,
   `game/` and `dlc/`; there is no second application ZIP inside it. A workflow
   build does not create a GitHub release.

The shared naming helper lowercases the label, removes a Conventional Commit
prefix and replaces spaces/punctuation with hyphens. Version numbers, dates,
commit hashes and Actions run IDs are not appended automatically. When dispatching
on the owner's behalf, choose an explicit meaningful label for the delivered work.
The packaging script validates the named application ZIP locally on the runner,
then Actions uploads only the staged application files in `out/package-windows/`.
GitHub creates the single downloadable ZIP from those files. Artifact uploads
omit empty directories, so the workflow adds empty hidden `.keep` files only to
the Actions staging copies of `game/` and `dlc/` and includes hidden files from
that staging directory. Local application ZIPs retain genuinely empty setup
directories. Historical artifacts with nested ZIPs are unchanged; this applies
to future builds. Updater manifests still need a publicly downloadable application
ZIP URL; an authenticated Actions web page is not a download endpoint.

Local packaging uses the same convention, for example:

```powershell
python scripts/package-windows.py --build-dir out/build/win-amd64-release --label discord-track-artwork
```

Without `--label`, the latest commit subject supplies the description; source ZIP
checkouts without Git history must provide a label. The legacy explicit
`--archive` filename override remains available. Existing local archives are
preserved by adding `-2`, `-3`, etc. on a filename collision. Naming a future
build does not authorize starting one.

The workflow uses a fresh Windows 2025 runner, LLVM 20.1.8, the SDK pin from
`sdk-version.json` and the matching custom runtime manifest. The Windows helper
accepts `-PreparedInputs` for the verified XEX/patch pair; the existing
`-GameRoot` / `-TitleUpdatePath` path remains available for local builds.

By default tests and game launch are skipped with `-SkipTests`. A successful
build-only job establishes compilation and packaging, not gameplay validation. Artifacts
expire after seven days. Standard GitHub-hosted runner compute is free for
public repositories; private repositories use the owner's Actions allowance.

## Enable automatic builds later

Automatic Windows builds are not configured. Changing that policy requires a
separate user request; the existing manual workflow is deliberate. Quick quality
checks already has a main push trigger, as described below.
Avoid pull-request triggers that expose private inputs to unreviewed code.

## Quality checks

`Quick quality checks` runs after every push to `main`, on pull requests targeting
`main`, and when manually dispatched. It checks Python syntax and runs the existing
server protocol, storage and security regression suites on both Linux and Windows.
It does not use private inputs, download SDKs, build the game or access the live
community service. Failed checks are reported on the commit and Actions run.
For an ordinary requested build, put `[skip ci]` in the commit body to suppress
duplicate push-triggered checks, then dispatch both workflows explicitly. Do not
inspect run status/logs/artifacts afterward unless the user separately asks.
Documentation-only maintenance does not require a Windows build or local tests.

For the manual Windows CPU/UI suite, open **Windows build > Run workflow** and
enable **quality_checks**. The build helper then builds the selected existing
probe targets and runs `scripts/verify-windows.py` before packaging. Failures
stop the build; JSON summaries distinguish passed, failed and skipped probes.
Summaries and verified runtime hashes are uploaded as a separate seven-day artifact.
No game inputs, raw memory dumps or entire test working folders are uploaded.

The current private archive contains only the XEX and patch. Full-disc
`localization_probe`, `launcher_probe` and `launcher_layout_probe` checks therefore
remain explicitly skipped rather than being run against fake disc assets.
The other CPU/software UI probes can exercise the patched title, configuration,
input defaults, engine settings, prompt selection and runtime integration.
Physical controllers, real GPU output, complete launcher interaction and actual
races still need their own native validation. This manual suite has not yet
been run on the GitHub Windows runner; its first result is not assumed to pass.

## GitHub Pages is separate

To stop the generated `pages-build-deployment` workflow, open **Settings >
Pages**, leave **Source** as **Deploy from a branch**, select **None** in the
branch selector and click **Save**. This disables website publishing without
disabling the Windows build workflow or deleting repository files.

## Historical workflow evidence

The first requested build confirmed Google Drive input download and SHA-256
checks, then exposed an older Windows PowerShell runtime-staging command
dependency. Runtime staging now computes the same SHA-256 with .NET directly.
[Run 39305063341](https://github.com/inflamesgo/motogp0910-recomp/actions/runs/39305063341)
then completed the native Windows build, packaging and artifact upload in
7 minutes 30 seconds. Its application source is `d6e405a`; subsequent quality
workflow changes do not alter the application binaries.

[Quick-check run 39305310774](https://github.com/inflamesgo/motogp0910-recomp/actions/runs/39305310774)
passed all 60 existing server tests on Windows and all 60 on Linux, plus Python
syntax checks. The optional Windows CPU/UI suite remains unrun on Actions.
