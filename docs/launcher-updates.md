# Launcher updates

The Windows launcher checks two independent publishers, starting with the
maintainer's GitHub Releases and then the existing NOBK manifest. It compares
available full Windows packages before installation, preserving the existing
backup, rollback and launcher-restart handshake. No account token is sent to
these public update endpoints.

The approved PR/release source version is **1.2.0**. Earlier local test packages
remain **1.1.12** and **1.1.13**, with their original matching manifests/hashes;
they are not 1.2.0 builds. The dual-source implementation is a
local change after main `28ba765` and published preview `f5250a9`; those older
commits and the earlier Windows test package do not contain it. Source syntax
checks are not native update/installation acceptance. A subsequent explicit
local-build request produced the latest Windows package; build/package checks
passed, without a real update transaction.

## Approved release version and publication

`updates.h::kConfiguration.current_version` is set to **1.2.0**, as approved by
the user. Before publishing, rebuild the integrated branch and use **1.2.0** /
**v1.2.0** consistently in the compiled launcher, release tag, manifest and
website download details. Publish the final package before its metadata, with
the SHA-256 of that exact ZIP. An unchanged 1.1.12 offer cannot update existing
1.1.12 installations. Changing metadata alone while shipping an old-version
launcher causes repeated offers. Game Title Update **0.0.1.1** is independent.

Both publishers must use a coordinated, increasing numeric product version
sequence. These are full builds of the same application, not independently
comparable client/backend component versions. Each must retain this updater and
its matching helper; installing a higher-version older fork can otherwise remove
features, including GitHub update checking. Separate repositories do not remove
that release-integration responsibility.

## Sources and selection

Configuration is in `src/launcher/updates.h`:

1. `https://api.github.com/repos/inflamesgo/motogp0910-recomp/releases?per_page=20`
2. `https://nobkonline.com/downloads/motogp/version.json`

The launcher always checks both in this order so simultaneous offers can be
compared. A source failure does not suppress a valid offer from the other.
Checks run on a worker once per launcher opening and again when **Update now**
is pressed. There are no periodic retries. Opening the notification panel alone
does not contact either source. A later recheck cannot silently substitute a
lower version than the one already offered in that session.

| Available updates newer than the installed launcher | Action |
| --- | --- |
| GitHub only | Install its verified complete package |
| NOBK only | Install its verified complete package |
| Both, same ZIP digest | Download the higher-version offer once |
| Both, different ZIP digests | Download and verify both, compare their extracted application files |
| Different contents at any common application path, or different EXE/DLL sets | Install only the higher-version complete package |
| Every common application file and the complete EXE/DLL sets are identical | Add allowed files unique to the other package to the higher-version payload |
| Same numeric version with conflicting packages | GitHub wins the tie; no mixed binaries |
| Neither newer | No update icon |

The merge is a union of compatible files. It never merges C++ changes, rewrites
EXE/DLL bytes or guesses which conflicting resource is right. Even different
JSON/text/font files at the same application-owned path force the full-package
fallback. A new executable/DLL found in only one package also prevents merging.
Most separately compiled full releases will therefore use the higher-version
package, exactly as the user approved. Bringing both publishers' code features
into a new executable still requires integrating source and compiling it.

Both candidates must be complete valid Windows releases. A bad checksum,
malformed/traversing ZIP, missing required program file or cancelled companion
download aborts before installation; it is not treated as an ordinary content
conflict. Existing files are preserved. If a publisher disappears after the
startup offer, retry rather than silently installing an older announced version.

Comparison retains FIFA's existing numeric rule: two to four numeric components,
compare major/minor/build, zero for a missing build, ignore the fourth revision.
Publishers must increment one of the compared components; revision-only releases
will not update. GitHub tags may use a leading `v` or `V`, stripped before
comparison. Prerelease suffixes are unsupported.

## GitHub publication contract

Only the newest 20 returned stable, non-draft GitHub Releases are considered;
commits, tags without a release, drafts, prereleases and source-code ZIPs do not
qualify. Keep the current stable Windows release within this bounded list.
Each release must have exactly one full package asset named
`MotoGP0910-Windows-x64-<change-label>.zip`. Multiple matching Windows packages are
ambiguous and skipped. Do not upload a prepared application ZIP inside another
Actions ZIP: the downloadable asset must open directly to the application.

The package needs a SHA-256. Either GitHub's asset metadata provides
`digest: "sha256:<64 hexadecimal characters>"`, or attach **version.json** with
the schema below. When attached, its version must numerically match the release
tag, its URL must exactly match the selected ZIP's `browser_download_url`, and
its checksum must agree with any GitHub digest. The launcher's notes now use the
actual GitHub Release description, preserving its text/line breaks in the plain
text viewer. The same description is shown in either launcher language. Only
when that description is empty/missing do attached manifest notes supply the
English/Turkish fallback. Descriptions are capped at 64 KiB and truncated on a
UTF-8 boundary with an ellipsis. The highest eligible release must validate before it is offered.
Do not put authentication credentials or game files in release assets.

## NOBK manifest contract

Keep the existing FIFA-compatible lowercase JSON fields:

```json
{
  "version": "1.2.0",
  "url": "https://nobkonline.com/downloads/motogp/MotoGP0910-Windows-x64-dual-source-updates.zip",
  "sha256": "<actual ZIP SHA-256: 64 hexadecimal characters>",
  "notes": {
    "en": "Short release notes.",
    "tr": "Kısa sürüm notları."
  }
}
```

This is a schema example, not a published offer. NOBK requests append
`?v=<major.minor.build>` (or `&v=` after an existing query) and use
`User-Agent: NOBK-Launcher/<major.minor.build>`, without a request body or account
credential. Package URLs require HTTPS and a `.zip` suffix; query strings after
that suffix do not qualify. Localized notes fall back to English and then a
no-notes message. Missing/invalid package metadata is not installable.

For a GitHub-attached manifest use the GitHub ZIP asset's exact browser URL,
not the example NOBK URL. The existing NOBK endpoint/schema needs no GitHub API
hosting or new merge endpoint. Old installed launchers cannot discover GitHub
until this launcher/helper pair is delivered by their existing updater or a
manual complete-package installation.

## User interface and transaction

The existing orange download icon opens the 390 × 360 logical-pixel panel with
version, publisher, notes and **Update now**. When two packages are available,
the panel explains the compatibility comparison/full-package fallback. The
normal launch path is unchanged. Game activity disables Update now; native
process enumeration is checked again before replacement, and the game is never
terminated. Startup check failures remain quiet.

Downloads and metadata run off the window thread. Cancellation remains available
during checking/downloading. Single-package downloads show a percentage when
length is known; dual downloads show progress without a misleading per-package
reset. A clipped orange fill inside the download button follows known progress;
unknown length uses a moving segment. A 33 ms repaint timer runs only while the
busy update panel is open. After verification, settings are saved and controls
are disabled during helper preflight. There is no second installation confirmation.

After Update now, normal installation, restart and rollback do not create the
separate helper status/progress window. The helper remains headless through
normal completion, preserving its checks and restart acknowledgement. Real
failures still report an error instead of silently disappearing. This is not
automatic installation at launcher startup: Update now remains the user's action.
Presentation is owned by the installed launcher/helper starting the transaction;
an older version can show its old notes/progress window once when upgrading to
this version. New release assets cannot retroactively change that running UI.

The static-runtime helper is copied from the installed `bin/` to a unique
`MotoGP0910-update-<random-token>` directory under Windows TEMP. It validates
both ZIP hashes again, extracts separately with built-in PowerShell/.NET,
compares case-insensitive application paths and stages the permitted union.
A protocol-2 selection receipt proves that the running helper understood the
job before the old launcher authorizes replacement. An outdated helper fails
preflight instead of silently ignoring the second package. Ship launcher and
helper from the same final build.

Only explicit application-owned paths are installable. This includes the four
approved converted `gauge.json` resources and Open Sans Regular/SemiBold/Bold.
Game files, DLC, settings, credentials, saves, logs and unrelated files are not
installed from update ZIPs. Configured data roots and links/junctions are checked
before any application replacement. Both standard package roots and one
outer directory are accepted; a complete set of launcher, game, UI, helper and
runtime files is required in each candidate.

Changed files are backed up and hashed before the readiness signal. Following
commit and old-launcher exit, replacements use flushed scratch files and atomic
rename with hash verification. The new launcher must create/paint its window,
acknowledge the helper and remain alive briefly. Handled failures restore old
files and remove newly introduced files. The helper may stop only the failed
launcher it started, never the game. Failed rollback retains the recovery backup.
Files absent from a package are not generally deleted. Forced termination/power
loss remains outside this handled-error recovery path.

Limits: 128 KiB per manifest; 1 MiB for the GitHub list; at most 20 releases and
64 assets per release; 512 MiB per ZIP (up to two); 4,096 entries, 1 GiB per file
and 2 GiB expanded per ZIP. HTTPS certificate validation remains enabled and
HTTPS-to-HTTP redirects are forbidden. Metadata deadlines are eight seconds
per request; each package has a ten-minute deadline, checked between bounded
WinHTTP calls. Helper preflight allows three minutes for one package and six
for two. The installation directory must be writable with space for downloads,
both extracted payloads, backups and scratch files. No elevation is requested.

## Validation and release gate

The six changed source units passed Windows-target compiler syntax checks on
28 October, along with focused native label/cue and documentation checks. A subsequent explicit local-build request completed the Windows build and
package/runtime/inclusion checks. Real installation, rollback and native Windows
audio acceptance were not performed for these changes. Historical Windows/Wine update-preview checks
from 20 October cover the earlier panel only, not dual-source behavior.

Before public release, use a disposable Windows installation to verify both
single-source offers, simultaneous disjoint resources with identical program
files, a conflicting binary/resource selecting the higher complete package,
equal-version GitHub priority, an unreachable source, a newer release appearing
before Update now, wrong hashes, unsafe ZIP paths, cancellation, outdated helper,
failed restart and rollback. Confirm credentials, saves/config and game/DLC
files remain untouched. Do not test destructive update cases on a real profile.

`MotoGP09-10Launcher.exe --preview-update` retains the existing synthetic panel
preview. Its displayed version now follows `kConfiguration.current_version`
(**1.2.0**) instead of hardcoding an old sample version. It does not check either
server, download packages or validate this
update transaction. Source: `updates.*`, `update_panel.inc`, `update_install.cpp`,
`update_helper.cpp` and `update_protocol.h`.
