# Critical website release checklist — 28 October 2026

**Owner: ChunchunQ / NOBK website and service operator. This is highest-priority
release work, not optional documentation polish. Do not publish the update as
fully integrated while season/class record identity or the matching player
instructions remain incorrect.** No production site was edited in this review.

## What was actually rechecked

Read-only HTTP 200 responses were obtained on 28 October for:

- [Live](https://nobkonline.com/motogp/).
- [Riders](https://nobkonline.com/motogp/ranking/).
- [Records](https://nobkonline.com/motogp/records/).
- [How to play](https://nobkonline.com/motogp/how-to-play/).
- Shared `/assets/motogp.js?v=9`, `/assets/motogp/tracks.json` and public
  `/api/motogp/views`.

This was response/source inspection, not a browser interaction or production
database audit. Local receipts and response hashes are under ignored
`out/analysis/handoff-review-20261028/`. JavaScript SHA-256:
`805dcadf65710beab495a94a6581fcfc769e686c73f06cf67d50071d3bdb3630`.
Recheck at deployment because this is a dated observation.

| Area | Current observation | Required action |
| --- | --- | --- |
| DLC catalogue | v9 includes native 18/19/20, reserved 17 is null; `tracks.json` has 20 entries and the denominator counts non-null tracks | Preserve it; the old 17-track-only finding is superseded |
| Badges | `clsBadge` maps MotoGP to A, 250cc or Moto2 to B, 125cc to C | This matches the agreed season-relative letters; retain the separate class names and data identities |
| Records and links | Board links use only `?v=<view>`; card filtering/labels have no explicit season identity | Add scope to queries, links, rendering and filters together with the backend |
| Public board metadata | The observed response contains 55 populated boards, class labels MotoGP/250cc/125cc and no top-level season field | A point-in-time listing, not proof that no Moto2 result exists; provide explicit scoped identities, not relabelled mixed rows |
| How to play | Correct state-dependent ISO/TU/DLC/verify sequence is already present; `Back + Start` remains | Keep the correct sequence; correct controller instructions and publish the detailed steps below |
| Download copy | Page advertises 1.1.12 / 38 MB | Match the actually published release archive/version/size; do not substitute an unapproved version or a local test ZIP |

The 20 artwork mappings do not by themselves validate every image/layout or
the website's distance/turn values. Preserve the verified native index mapping;
do not restore the suspect duplicated DLC-script facts described in the master
handoff.

## P0: separate seasons and classes throughout the site

Implement the [scoped service/public API integration](leaderboard-season-integration.md)
first or in one coordinated deployment. The required display is:

| Season | A | B | C |
| --- | --- | --- | --- |
| 2009 | MotoGP | 250cc | 125cc |
| 2010 | MotoGP | Moto2 | 125cc |

There is no fourth simultaneous class or D badge in this agreed model. Shared
B letters are intentional; shared 250cc/Moto2 records are not.

- Provide a visible 2009/2010 scope, and carry it into Records search/cards,
  board headers, every board link, Riders ranking, Live/latest records, player
  profile rows, totals and held-track flags. Never infer year from `_2009`
  asset keys, the current calendar year or class B alone.
- Preserve rankings computed within the requested scope. A global rank with
  a filtered label is not a season-specific rank. Keep any deliberate overall
  ranking explicitly named and separate; do not silently redefine Rider Rank.
- Extend public API metadata, storage queries and URLs together. Existing
  view-only metadata cannot distinguish six combinations. Keep a defined
  compatibility path for legacy API consumers, including F1 and Rider License.
- Preserve Donington/2009 and Silverstone/2010 context, the two historical
  bonus circuits and reserved slot 17. Do not globally replace old names/art.
- Back up and classify legacy rows conservatively. No display fix can recover
  a historical best already overwritten in the view-only store.

Acceptance: show independent same-rider 2009/250cc and 2010/Moto2 results on the
same track in native UI, public API and site; repeat across years for MotoGP and
125cc. Check direct links and browser back/forward, not only the listing page.

## P0: rewrite How to play as actionable, matching-release steps

The existing short sequence is substantially correct. Do not call the whole
sequence broken or ask for nonexistent new buttons. Expand it from the current
launcher code (`iso_import_ui.inc`, `dlc_ui.inc`, `main.cpp`) as follows, in
English and Turkish:

1. **Prepare owned files.** Supply an Xbox 360 disc ISO, the supported TU
   0.0.1.1 package and all three free DLC packages: GP Class Season 2010 Update,
   2010 Moto2 and 125 Update, Twin Track Pack. Game/TU/DLC files are not included
   or downloaded by the PC package. Launcher 1.1.12 is a separate version.
2. **Extract the complete PC package.** Keep launcher, `bin/`, `resources/`,
   `game/` and `dlc/` together. Open `MotoGP09-10Launcher.exe` from the root;
   do not run an isolated EXE or mix DLLs from an older release.
3. **Import the disc.** On General, use **Select ISO** when game files are
   missing. It extracts into the portable game directory on a worker. Already
   valid files cause the same action to advance, so not everyone sees every
   label. Do not tell users to supply a second destination or download data.
4. **Import the supported title update.** The same action becomes **Select
   title update** when needed. Use the verified original TU package; explain
   invalid/missing input inline. It does not update the launcher's version.
5. **Install all DLC.** The same action becomes **Install DLC**. Select the
   supported ZIP/RAR/7z archives or extracted package folders until the inline
   missing list is empty. All three are required in Offline, LAN and Online,
   including direct EXE startup. Successful import need not open a success dialog.
6. **Verify and choose mode.** Use **Verify game files**, then choose Online,
   LAN or Offline on the Online page. Online is the default and requires NOBK
   sign-in/PC linking. Offline/LAN require no site account. Do not promise that
   a failed Online connection automatically falls back to Offline.
7. **Sign in for Online.** Explain Create account, Forgot password?, Remember
   me and My account. Retain the same shared NOBK account identity used by the
   other games. Records follow the linked account; display names alone do not
   prove ownership. Do not instruct users to delete profiles or saves.
8. **Play and use the panel.** Press Play. Use Player Match to host/search/join.
   Open NOBK.FRIENDS with **F1** by default, or hold **Back** on Xbox / **Select**
   on PlayStation prompts. **Remove `Back + Start`**. Respect a customized panel
   key. The startup strip and friend/invitation notifications are Online-only;
   they are not missing features in Offline/LAN. F2 opens live settings.
9. **Describe new Online features accurately.** Host a joinable lobby to invite
   eligible NOBK friends; accept/decline in F1. Native friends require the shared
   immutable-ID extension and matching service. Discord desktop/sharing plus
   the launcher activity option and matching service enable Discord invitations;
   joining a Discord voice server is not a prerequisite. Ghost replay download
   must not be advertised as working while native binding is pending.
10. **Publish truthful record and update guidance.** Explain season/class scope
    after the scoped service is deployed. Automatic records require successful
    Online uploads; menu access is not proof. Updates use the configured MotoGP
    manifest, but the operator must publish and verify the actual package before
    promising a working update. Keep download version/size/checksum consistent.

For older Documents-based DLC installations, explain reimport or moving the
existing `434307F0` directory with `Headers` into portable `dlc/`; do not imply
automatic import or ask users to erase save data. Distinguish base 17 circuits /
72 riders from the 84-entry 2010 roster and 20 supplied circuit locations.

## Final operator sign-off

Review all four pages and linked board/player pages in both languages against
the matching final Windows build. Check a clean installation and one existing
installation, six valid season/class combinations, direct links, ranking
denominators and invitation/help text. Mark completed DLC catalogue work as
verified rather than repeatedly reopening it. The source PR can be reviewed
with explicit integration tasks; public release readiness requires these P0
data and instruction corrections, not just a merged client menu.
