# Xbox 360 DLC import

The launcher supports the three supplied MotoGP 09/10 marketplace packages:

| Content | Native marker | Payload |
| --- | --- | --- |
| Free GP Class Season 2010 Update | `DLCPack01.meta` | 187 files |
| Twin Track Pack | `DLCPack02.meta` | 20 files, including Istanbul and Shanghai track data |
| Free 2010 Moto2 and 125 Update | `DLCPack03.meta` | 341 files |

All three inspected containers have Xbox title ID `434307F0`, marketplace content
type `00000002`, and a `LIVE` STFS header. They are **additional content**, not
title-update patches: they are not renamed to `default.xexp`, merged into
`xenondata`, or used to replace `default.xex`. The supported base game and title
update remain prerequisites. Original game/DLC files are not distributed with
the launcher.

## User flow

When the base game and title update are present, **General → Install DLC** appears
beside **Verify game files**. Choose **Select archive…** for a ZIP, RAR or 7z file,
or **Select folder…** for the main folder obtained by unpacking the archive.
The importer searches nested folders and installs all supported packages found
in that selection; there is no need to select the three files separately.
The current archive flow accepts the supplied package IDs and directory layout;
encrypted archives are rejected. A separately installed archiver is unnecessary.

The operation runs on a worker. Cancellation is available while reading and
extracting; the short publication phase disables the cancel button. The launcher
waits for cleanup if closed. Game launch, verification/ISO import, Steam shortcut
changes and launcher installation cannot start through the launcher during an
import. An external running copy of the game also blocks publication.

Content becomes visible on the next game start. The native game enumerates
marketplace packages and checks their `DLCPack0N.meta` files. Offline, LAN and
Online use the same local content store. Multiplayer participants still need
compatible installed content; the original game's compatibility checks remain.

## Storage and preservation

The destination follows the same configured `user_data_root` as the runtime,
using the existing launcher user-root resolver. Its default on Windows is
`Documents/motogp0910`. Marketplace content is shared across local game profiles:

```text
USER_DATA_ROOT/
  0000000000000000/434307F0/
    00000002/PACKAGE_ID/...extracted content...
    Headers/00000002/PACKAGE_ID.header
```

The `.header` uses the pinned SDK's `XCONTENT_AGGREGATE_DATA` ABI and preserves
the package's own display name, file name and license bits. The GP update has
mask zero; the other two supplied packages have mask one. No global license
setting is forced or entitlement fabricated.

The importer verifies each package's size and SHA-256 before handing it to the
existing SDK STFS reader, checks its title/content type, and verifies payload
counts, total sizes and recognition markers. Paths are checked before writing.
Extraction is staged under a unique directory on the destination filesystem.
The complete selection is prepared before any package is published. Inputs are
opened for reading and are never modified. Existing game data, patches, settings,
save files and DLC packages are not overwritten.

If an installed package is byte-identical, it is retained. If its files or
metadata differ, installation stops with an explanation. Newly published paths
are journaled and removed on a cooperative cancellation or publication failure.
A process kill/power loss can leave staging or a partially published selection;
there is no claim of crash-atomic installation across multiple directories.
Only newly created paths participate in rollback.
Imports sharing the same user-data root are serialized with a named mutex so
two local launcher installations cannot publish or roll back each other's work.

## Archive dependency and validation limits

The portable reader is the pinned, unmodified official **7-Zip 26.04 x64** console
program and DLL. The launcher verifies both executable hashes before starting
the reader, lists the archive before extraction, accepts only the expected DLC
files, and extracts those files flat into staging. It never runs an executable
from the selected archive. Formats, permissions and password errors are reported
without changing installed content. Original RAR/7z package samples were not
provided; those paths use the same reader and selection rules.

Packaging includes the reader, its complete corresponding source and embedded
license notices. The launcher updater allows only those specific reader/source
resource paths. SDK and game-code pins are unchanged.

The supplied ZIP was downloaded and its three packages were extracted with the
existing inspection tool. Their metadata, payload inventories and native marker
names were checked against the recompiled content-discovery routine and pinned
SDK source. Reader hashes and x64 PE headers were verified against the official
distribution. These are artifact/source inspections. The revised launcher has
**not been built or run**, and no native Windows installation or gameplay result
is claimed. Imported DLC assets, including their language scripts, remain
unmodified. Native visual/gameplay review and RAR/7z sample imports remain pending.
