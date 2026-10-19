# Windows builds with GitHub Actions

The `Windows build` workflow is deliberately manual-only. Pushing commits,
opening pull requests and editing documentation do not start builds. Do not
dispatch the workflow until the owner requests the first build. No scheduled
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
2. Select `main` and click **Run workflow**. Other branches are skipped.
3. Download the Windows artifact from the completed run. It contains the
   uniquely named application ZIP; it does not create a GitHub release.

The workflow uses a fresh Windows 2025 runner, LLVM 20.1.8, the SDK pin from
`sdk-version.json` and the matching custom runtime manifest. The Windows helper
accepts `-PreparedInputs` for the verified XEX/patch pair; the existing
`-GameRoot` / `-TitleUpdatePath` path remains available for local builds.

Tests and game launch are explicitly skipped with `-SkipTests`. A successful
job establishes compilation and packaging, not gameplay validation. Artifacts
expire after seven days. Standard GitHub-hosted runner compute is free for
public repositories; private repositories use the owner's Actions allowance.

## Enable automatic builds later

After the first requested build succeeds, a `push` trigger restricted to
`main` can be added in a separate change. There is no such trigger today.
Avoid pull-request triggers that expose private inputs to unreviewed code.

## GitHub Pages is separate

To stop the generated `pages-build-deployment` workflow, open **Settings >
Pages**, leave **Source** as **Deploy from a branch**, select **None** in the
branch selector and click **Save**. This disables website publishing without
disabling the Windows build workflow or deleting repository files.

The initial workflow wiring has not been executed or validated on a GitHub
Windows runner. Secret configuration and the first owner-requested build are
still required before calling the pipeline operational.
