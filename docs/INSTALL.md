# MotoGP 09/10 — Installation and launcher guide

This guide covers the Windows package, game setup and everyday launcher
settings. For supported game content and SHA-256 values, use the
[requirements table](../README.md#required-game-content).

## Before you start

Use Windows 10 or 11 x64, a DirectX 12-capable graphics card and a controller
or keyboard. You need your own supported Xbox 360 copy, Title Update `0.0.1.1`
and all three required DLC packages. Those files are not supplied with the port.

Keep the launcher, `bin/` and `resources/` from the same Windows package
together. Extract the ZIP into a folder where your Windows account can write;
do not launch it from the archive or mix files from different releases.

## Install your game content

1. Open `MotoGP09-10Launcher.exe` and go to **General**.
2. Use **Select ISO** to extract your own game image. If it is already
   extracted, use **Browse** to choose its folder instead. That folder must
   contain `default.xex` and `xenondata/`.
3. When the button changes to **Select title update**, select the supported
   Title Update file. The launcher also accepts its verified `default.xexp`
   patch.
4. Use **Install DLC** for all three packages. Select `.zip`, `.rar` or `.7z`
   archives, or extracted folders, and wait for the operation to finish.
5. Adjust your launcher settings, choose a connection mode on **Online** and
   press **PLAY** to start the game.

The import button changes with the content still needed. All three DLC
packages are required in Offline, LAN and Online modes. **Verify game files**
is an optional troubleshooting check after setup, not a required installation
step.

## Launcher pages

| Page | Settings |
| --- | --- |
| **General** | Game content setup, launcher/game language, button prompts, **Add to Steam** and Discord activity. |
| **Graphics** | Graphics adapter, monitor, display mode, rendering resolution, window size, refresh rate, FPS limit, anti-aliasing, texture filtering and upscaling/sharpening. |
| **Advanced** | Motion blur, depth of field, shadows, bloom, vignette, particle density and draw distance, plus diagnostic options. |
| **Audio** | Master volume and muting. |
| **Controls** | Controller input, keyboard bindings and DualSense adaptive trigger settings. |
| **Online** | Offline/LAN/Online selection, account sign-in and LAN connection details. |
| **Achievements** | Progress, search, locked/unlocked filters and achievement pop-up/sound preferences. |

**Save settings** keeps your choices for the next launch. **PLAY** also saves
the current settings before opening the game. The in-game **F2** menu provides
live settings; the race continues while it is open.

### Resolution and image settings

The native base image is 720p. The 1080p preset uses FSR to upscale it; 1440p,
4K and 5K presets increase the actual rendering resolution. Larger rendering
resolutions require more graphics performance. Window size and display mode
control how the final image is presented.

FSR 1, 2 and 3 modes are selectable under **Upscaling /
sharpening**. FSR quality controls apply to FSR 2/3, and sharpness is adjustable
for the supported FSR/CAS modes. These options do not enable frame generation.

### Controllers and split-screen

Choose SDL for PlayStation controllers and DualSense adaptive triggers. The
trigger settings provide fixed resistance with adjustable strength and zones;
they do not change with engine RPM or speed. USB and Bluetooth are supported.

For keyboard play, enable **Enable keyboard** and edit the bindings under
**Controls**. Split-screen needs two controllers, or one controller with
keyboard controls enabled. Button prompts can follow your device automatically
or use a selected Xbox/PlayStation/keyboard style.

## Online play

Create an account at [nobkonline.com](https://nobkonline.com/) and sign in on
the launcher's **Online** page with **Online** selected. Your username is the
name other players see. **Remember me** signs you in on subsequent launches;
**Sign out** removes the remembered sign-in.

Start the game and select **Multiplayer → Online**, then choose a race type
and continue to the lobby. After hosting an open lobby, use **F1 → Friends**
to invite someone. Open the panel to accept or decline incoming invitations.
The controller shortcut is held
**Back** on Xbox or **Select** on PlayStation. For Discord invitations, keep
the desktop app open and enable **Show Discord activity** in General.

Offline and LAN do not need a site account. LAN players must join the same
local or virtual network.

## Updates and your files

The launcher checks for updates at startup. When an update is offered, open
the update panel and use **Update now**. Close the game before installing.
The launcher restarts after installation and preserves your settings and
remembered sign-in. Back up saved data before replacing an installation.

| Data | Default location |
| --- | --- |
| Settings | `config.json` beside the launcher |
| Saved game data | `MotoGP 09-10/` in the Windows Documents folder |
| Remembered sign-in | `%LOCALAPPDATA%\MotoGP0910\Online\nobk-account.bin` |

Custom paths can differ. The remembered sign-in is encrypted for the Windows
user; your password is not stored. Avoid sharing this file or saved profiles.

## Troubleshooting

- **Missing program file or DLL:** extract the entire Windows package again,
  keeping its folder structure. Do not replace individual DLLs with unrelated
  downloads.
- **Unsupported game, update or DLC:** compare the original input file against
  the [required hashes](../README.md#required-game-content). Hash the file
  itself, not an archive containing it.
- **Game data may be damaged:** use **General → Verify game files**. This checks
  your files without repairing or downloading them. Restore damaged content
  from your own copy.
- **Low frame rate:** reduce **Internal resolution** or demanding effects in
  **Advanced**. On a multi-GPU system, check **Graphics adapter**.
- **Cannot play online:** check the selected connection mode and sign-in.
  If an invitation no longer works, have the host send one from an open lobby.
- **Need to report a problem:** use **Open logs** or **Error report**. Reports
  remain local until you share them. Include the version and reproduction
  steps, and review personal paths/account details before sharing diagnostics.
