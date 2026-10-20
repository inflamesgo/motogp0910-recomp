# 7-Zip archive reader

Unmodified x64 `7z.exe` and `7z.dll`, version **26.04** (5 October 2026),
extracted from the official Windows distribution. They read the player's local
DLC archives; the Windows installer is never run. No system-wide installation,
PATH change, shell extension or network access is required at runtime.

The publisher's download page is https://www.7-zip.org/download.html and the
release is https://github.com/ip7z/7zip/releases/tag/26.04. Download digests were
checked against the release's GitHub asset digests. `manifest.json` records
upstream URLs and SHA-256/size pins for the exact distributed files.

`License.txt` is reproduced in the launcher's embedded third-party notices.
The complete corresponding source is retained as `7zip-source.tar.xz` and
included in Windows packages at `resources/archive/7zip-source.tar.xz` alongside
the reader. This includes the upstream license texts and build instructions.
The RAR decoder is used only for reading archives; it must not be used to develop
a RAR/WinRAR-compatible compression implementation.

To update, download an official release, verify its publisher-provided digests,
extract the two unmodified x64 files, retain that exact version's complete source
and license, and update the manifest, runtime checksum pins and embedded notices
together. Review format support and the native archive-reader behavior before
distributing a revised package.
