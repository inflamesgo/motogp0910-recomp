# Reference projects

## FIFA 09 PC port — ChunchunQ

- Source: https://github.com/ChunchunQ/fifa09-recomp
- Default branch: `main`; public repository confirmed on 20 October 2026.
- Initial reference revision: `711da3941d81879db06ea4f2c36c3a519ac826d9`.
- Project source license at that revision: MIT; retained in
  [third_party/fifa09-launcher-LICENSE.txt](../third_party/fifa09-launcher-LICENSE.txt)
  and the launcher's embedded third-party notices.
- Online integration guide: [docs/ONLINE.md](https://github.com/ChunchunQ/fifa09-recomp/blob/main/docs/ONLINE.md).

This is the FIFA 09 port previously supplied as a launcher/update-system visual
and behavior reference. For future feature requests referring to that port,
inspect this source repository and its current implementation alongside any
supplied screenshot or executable. Relevant areas include launcher UI, update
notification/download/install/restart behavior, and NOBK online integration.

Keep FIFA-specific game logic separate from MotoGP. Record the source revision
and check the applicable source licenses before adapting code; being public does
not itself specify reuse terms. This reference entry does not add a build/runtime
dependency, automatically merge upstream changes, or change our server endpoints.

The General-page Steam shortcut feature adapts the behavior of
`launcher/SteamShortcut.cs` at the recorded revision to C++/Win32. It uses the
same binary KeyValues format, shortcut ID calculation and Steam artwork naming.
MotoGP preserves existing unrelated shortcuts and user-customized fields, chooses
one identifiable Steam account, and backs up an existing shortcuts file before
replacing it. No FIFA game files, images, credentials or server addresses are used.

Launcher update backend reference: `launcher/Updater.cs` at the same revision.
MotoGP uses its version-query, manifest, notes, numeric comparison and ZIP/hash
contract, retaining the existing native rollback as a documented client-side
fallback mechanism. The service URL stays unconfigured pending the MotoGP
operator's endpoint. This backend adaptation does not change the update UI.

The Online-page **Forgot password?** action follows `Forgot_Click` in
`launcher/MainWindow.xaml.cs` at the recorded revision: open
`https://nobkonline.com/sifremi-unuttum/` in the default browser. Account recovery
stays on the shared NOBK Online website; no reset request or password is sent by
the button itself.
