# MotoGP How to play: content and design review — 28 October 2026

The user requested comparison with these live references:

- [FIFA 09](https://nobkonline.com/fifa09/how-to-play/).
- [WWE SVR 2009](https://nobkonline.com/svr09/how-to-play/).
- [NFS Most Wanted](https://nobkonline.com/nfsmw/how-to-play/).
- [Current MotoGP page](https://nobkonline.com/motogp/how-to-play/).

All four pages were opened read-only in Chromium at a 1440 × 1000 viewport.
The rendered sections, headings, DOM text and layout were inspected. Mobile
behavior is a recommendation to verify, not a completed mobile audit. Local
captures/receipts are under ignored `out/analysis/how-to-play-comparison/`.
No production website or game UI was changed.

## What the references actually do better

| Reference | Useful pattern | Apply to MotoGP |
| --- | --- | --- |
| FIFA 09 | Download and adding the game are separate numbered steps; file input has its own short instruction | Split MotoGP's one overloaded download/import paragraph into actual actions |
| WWE SVR09 | A prominent release/download strip before setup; distinct requirements, supported-game details, new install, existing install and controls | Make download/version visible early and separate a new installation from an update |
| NFS MW | Clear download card, short sequential setup cards, code-styled exact file/button labels, separate existing-player guidance and controls | Give each step one task and a clear outcome; make file names and menu labels easy to scan |

These references still use text cards. They do not all contain launcher setup
screenshots; adding current MotoGP launcher screenshots is our recommendation,
not a claimed reference feature.

MotoGP already shares the site's typography, rounded step cards and navigation
language. A wholesale site-theme replacement is unnecessary. Its weaker points
are the overloaded instructions, download buried inside step two, a large white
hero/logo area and the weak separation between setup and game/lobby instructions.
The game gallery and manufacturer branding below the guide do not teach setup.
Do not diagnose a broken CSS layout merely because this older content template
is less effective than the newer reference pages.

## Required content corrections

1. Remove **Verify game files** from the public normal setup sequence entirely.
   The user reserves it for damaged files/startup troubleshooting and does not
   want a replacement verification paragraph in the guide. Keep the function
   in the launcher; this is a website correction.
2. Replace the current Race paragraph with the user-confirmed native path:
   **game main menu → Multiplayer → Online → desired race type → lobby screen**.
   There is no Play button in the game's main menu. Do not retain the current
   shortcut from Play to Player Match or invent further submenu names.
3. Explain lobby creation/join and leaderboard submission separately. The phrase
   “Your timed results are saved … on their own” is insufficient: describe
   supported modes and the required successful Online service connection.
   Do not claim the missing scoped backend already accepts results.
4. Replace Back + Start with held Back/Select. Keep F1/F2 roles clear and
   explain that account sign-in is for Online, not Offline/LAN.
5. Distinguish the launcher release number from game TU 0.0.1.1. Give an exact
   checklist for the user's ISO, supported TU and the three required DLC packages.
   Explain what is bundled versus user-supplied without repeating long warnings.
6. Use the final published package's actual version/size/link. The operator must
   bump and rebuild the launcher for the new release as already documented.

## Proposed page order

1. **Compact MotoGP hero.** Retain the official logo and orange/charcoal identity.
   Use one coherent racing image and restrained dark treatment, with enough
   contrast for the logo. Reduce the large white/empty area; do not obscure the
   artwork with several competing banners or replace the game's branding.
2. **Download card.** A visible Windows download button, current version and
   archive size, plus a brief “No game files included” note. Keep it above the
   long setup guide, following the useful NFS/WWE pattern.
3. **Before starting.** Compact OS/GPU/input requirements and a separate
   checklist for the ISO, TU and three DLC packages. Keep the package names
   legible rather than burying all three in the download paragraph.
4. **New installation.** Short numbered cards: download/extract; select ISO;
   select title update; install DLC; choose mode/sign in for Online; enter
   **Multiplayer → Online → race type → lobby** once the game is open. Each
   card states one action and what the player should see next. Explain that
   the same General-page action changes labels as prerequisites are completed.
5. **Already installed?** A separate compact update path explaining the launcher
   updater and preservation of existing game data/settings/saves. Do not copy
   another game's “extract over everything” advice without verifying MotoGP's
   installation and update behavior.
6. **Controls and common questions.** Clear F1 panel, held Back/Select and F2
   settings hints, then concise FAQ about Online sign-in, Offline/LAN and setup
   issues. Keep race-record and ghost claims limited to implemented features.

Move the general game description/gallery toward the main game page or after
the useful setup/FAQ content. Prefer a current General-page image showing the
relevant action, an Online-page sign-in image and a native menu/lobby image.
Use the matching release's own UI; do not fabricate labels or include personal
accounts/credentials. One focused screenshot per useful step is enough.

## Visual acceptance

Use consistent orange step numbers, heading/body spacing and padding. Long
instructions should become separate cards or a short checklist rather than a
single bold-text paragraph. Preserve the site's existing fonts and readable
body contrast. File/button names should be visually distinct, with short UI
labels reproduced exactly in English and Turkish.

Verify that the first viewport makes the product/download purpose clear, steps
stay in a logical order on narrow screens, no text/button clips and screenshots
remain readable. A first-time player should be able to identify the files they
need, follow the actual launcher labels, reach the correct native Online menu
and understand the resulting lobby without guessing. This review specifies
the redesign; it does not claim the operator has implemented it.

See the [release checklist](website-release-checklist.md) and
[master handoff](chunchunq-handoff.md). Further user corrections should be
collected there before finalizing the operator's public copy.

## About the game: current content correction

The user also requested replacing the original About the game description.
Use the draft in the release checklist: identify the community PC port and
original developer/publisher separately, describe both seasons with their
correct classes and the full 20-location supplied content. Do not present
17 base circuits or 72 base-season riders as the complete installation totals.

The supported-game table must also name the matching Title Update `0.0.1.1`
and each of the three required DLC packages. See the release checklist's
“supported game and required packages” table for the exact copy and identifiers.
Separate these original-game prerequisites from the PC download's version and
size; keep Title ID, Media ID and Title Update in separately labelled fields.
