# Building MotoGP 09/10

The player setup is in [docs/INSTALL.md](docs/INSTALL.md). This guide builds the
port from source; it does not download the game, its Title Update or its DLC.

## Windows x64

Install Visual Studio 2022 or newer with the x64 C++ tools and Windows SDK,
Clang 20 or newer targeting Windows/MSVC, CMake 3.25+ and Ninja. The script can
find Visual Studio's bundled Clang, CMake and Ninja. Python 3 is needed for
packaging.

Use the original Xbox 360 executable and the supported Title Update from your
own copy. The script checks their hashes before generating code:

| Input | SHA-256 |
| --- | --- |
| Original `default.xex` | `54245417e996ed0aa561a5a803fd810e87684bd0b43082f316db711b3ee786ae` |
| Title Update package | `441df6cc2e20ac8aeeed475138c1714d477f965423d8c5c9b0202e3a18d8141f` |
| Extracted `default.xexp` | `0419205ba10dc65eaa7b0bb62abc64cada46119ece5ed749e4ba20c06a6bc079` |

From the repository root, run PowerShell:

```powershell
.\scripts\build-windows.ps1 -GameRoot '<game-folder>' -TitleUpdatePath '<title-update-file>' -Jobs 4
```

Replace the angle-bracket placeholders with your own paths, keeping the quotes.
`GameRoot` must contain the original `default.xex`. The helper extracts the
Title Update, stages the verified executable and patch in `assets/game-tu/`,
downloads the pinned SDK, runs code generation and builds the application.

If you already have both verified files in one directory, use:

```powershell
.\scripts\build-windows.ps1 -PreparedInputs '<build-inputs-folder>' -Jobs 4
```

Outputs are in `out/build/win-amd64-release/`. Run the launcher with the complete
game data and all three DLC packages described in the installation guide.
Compiling the executable alone does not supply playable game data.

For later incremental builds from the same configured environment:

```powershell
cmake --build --preset win-amd64-release --parallel 4
```

To rebuild only the launcher, including its UI DLL and update helper:

```powershell
cmake --build --preset win-amd64-release --target motogp0910-launcher --parallel 4
```

Create a Windows package with:

```powershell
python scripts/package-windows.py --label launcher-polish
```

The ZIP is written to `out/dist/`. The label describes the change; existing
archives are preserved. The packager verifies the runtime, required resources
and embedded license notices, and leaves original game data out.

## SDK

[sdk-version.json](sdk-version.json) pins SDK `0.10.0.24-dev.gbd833a2` at source
commit `bd833a2ab69d305f99ddf57418fc38ffec8af93d`. Use its exact package hashes.
The Windows runtime is separately pinned by
[third_party/rexglue-runtime/manifest.json](third_party/rexglue-runtime/manifest.json).
Keep its matching DLLs, source patches and provenance hashes together; an
arbitrary newer SDK or runtime is not a drop-in replacement.

## Contributions and attribution

Keep original inputs, generated game code, accounts, saves, logs and build
outputs out of commits. Change hand-written sources or the recompilation
manifest rather than editing generated game functions. Keep fixes focused and
describe how the affected behavior was checked. Report build/platform limits
instead of treating compilation as a gameplay test.

The project builds on [ReXGlue](https://github.com/rexglue/rexglue-sdk), including
its Xenia-derived components. ChunchunQ contributed support and integration for
[NOBK Online](https://nobkonline.com/).
MotoGP 09/10 was developed by Monumental Games and published by Capcom.

Dependency terms remain in their supplied files:

| Component | License or notice |
| --- | --- |
| ReXGlue and runtime dependencies | [Runtime notices](third_party/rexglue-runtime/licenses/) |
| NOBK Online launcher integration | [MIT notice](third_party/nobk-launcher-LICENSE.txt) |
| SDL controller database | [License](third_party/SDL_GameControllerDB/LICENSE.txt) |
| AMD FidelityFX | [License](third_party/fidelityfx/LICENSE.txt) |
| 7-Zip, including the unRAR restriction and corresponding source | [License](third_party/7zip/License.txt), `third_party/7zip/7zip-source.tar.xz` |
| Open Sans | [License](third_party/open-sans/LICENSE.txt), [font notices](third_party/launcher-fonts/NOTICE.txt) and [OFL](third_party/launcher-fonts/OFL.txt) |
| nlohmann/json | [MIT license](third_party/nlohmann/LICENSE.MIT) |
| toml++ | [MIT license](third_party/tomlplusplus-LICENSE.txt) |
| stb | [License](third_party/stb/LICENSE.txt) |
| Flag icons | [MIT license](third_party/flag-icons-LICENSE.txt) |

The launcher embeds [third-party notices](resources/launcher/third-party-notices.txt).
Original project code is available under the [MIT License](LICENSE).
Third-party components retain their own licenses; game content and artwork
are not covered by the project license.
