# Rider License design follow-up

Recorded on 2026-10-22 from the user's native screenshots and feedback.
Initially recorded as notes only. The user subsequently authorized implementation
in order, including the in-game full-bio popup described below. These items
were implemented in commit `0c3aae0`, followed by the requested Actions build
dispatch. The subsequent helmet artwork, sharpness, metric and Personal Best
changes were committed and pushed as `1b8e105`, with Windows build and Quick
quality checks dispatched. No local builds or tests were run; workflow results
were not inspected.

## Final design acceptance

On 2026-10-22 the user accepted the Game Card design at `1b8e105` as final.
Preserve its layout, typography, artwork and interaction pattern as the baseline
for future fixes. Further visual redesign requires new user direction. This
decision supersedes the pending visual-design decisions recorded below; it
does not assert independent runtime validation or resolve the documented
missing DLC outlines and deferred lobby/season integration.

The user subsequently reopened the decision to add the site's overall rider
position, then explicitly limited the naming to **Rider Rank** and **Tracks
Held**. The resulting source extension places Rider Rank in the header's
right-hand space, preserving the three existing summary columns. The former
**Leaderboards** caption becomes **Boards Entered**, since it counts separate
circuit/class personal-best entries rather than unique circuits ridden. Rider
Rank follows the website's `withTracks` ordering from the public ranking and
view-leader endpoints, with a shared 60-second cache and loading/failure states.
It does not use the raw API `pos` without the website's reordering. This local
extension is included in the subsequent local CPU/UI validation.

The user also requested removal of temporary Rider License debug logs. Routine
HTTP/lifecycle/cache/success/cancellation traces and their log-only metadata are
removed. Actionable failure warnings remain, including failed font or texture
creation. Profile loading, retry/cancellation and cache behavior are preserved;
the subsequent validation covers parsing, drawing and input isolation without
restoring routine diagnostics.

The user subsequently requested a consistent footer center when changing tabs.
The full controller hint group now uses the right content pane's fixed center;
its measured text width accounts for Records' extra D-pad hint and the optional
bio hint. The profile button keeps its existing position. The user then approved
the Personal Best spacing adjustment: circuit name, lap time and a single
class/2009/mode line now share the center of the middle column, with equal
insets between its dividers. The missing-lap message uses that same center.
The existing circuit outline and rank/date columns retain their positions.
No extra metrics are added. Subsequent local checks are recorded in
[the validation report](rider-license-validation.md).

## Subsequent feedback

- Applied in the latest source pass: remove **(Y / R)** from the **Read full bio** tooltip,
  since the same shortcut already appears in the footer. Keep the actual
  shortcut and footer hint. The tooltip should read **Read full bio** /
  **Biyografinin tamamını oku**. Keyboard bindings remain functional.
- Replace the geometric generic helmet with the user's side-profile racing
  helmet reference. The edited transparent PNG has an off-white shell, charcoal
  visor and light silver outline to remain visible on the dark card. It is
  embedded in the Windows executable and replaces the old generated geometry.
  The icon has no opaque square backdrop or surrounding avatar frame; uploaded
  profile photos keep their existing framing. Native review is pending.
- Applied after the user's sharpness comparison with F1: dedicated preloaded
  profile font sizes and pixel-aligned drawing replace arbitrary scaling of
  the 17-pixel font. SDK/runtime and F1 fonts remain unchanged.
- Remove the bio modal's entire shortcut footer and the class filter's X/F hint.
  Show only controller hints in the main license footer; keep keyboard controls.
- Size the class popup for every option with extra padding, without a scrollbar.
- Add **Set** from the existing record `updated` timestamp, with relative time
  and a full-date hover tooltip. No new request is required.
- Correct the metric definition: the user selected **distinct circuits led / 20**
  including DLC. The first caption is now **Tracks Held**, and Total Track
  Domination uses that same numerator (16/20 = 80% in the supplied example).
- Subsequently approved and implemented for Personal Best: selected-circuit
  outline, lap/class/2009/mode, rank/board population and set date. Public NOBK
  outline filenames map explicitly to native base-circuit indices; artwork is
  optional, loaded after profile data, cancellable and cached per circuit for
  the session. It never substitutes another circuit's map or personal best.
  No published DLC outlines or directly usable local DLC artwork were found;
  Silverstone, Shanghai and Istanbul show a missing-map state with their lap
  data intact. The source assets were searched without committing game data.
- The live public player endpoint returned HTTP 403 from this workspace during
  the consistency review. The metric comparison uses the user's supplied
  screenshots, the saved site JavaScript and the server API/write-path source;
  a fresh live response was not verified here. Existing cache freshness and the
  temporary 2009 scope remain unchanged.

## Alignment and consistency

- Review the inconsistent, left-heavy appearance across the profile menus and
  both Rider License tabs. Center summary values and their captions within
  their intended columns and establish consistent spacing and baselines.
- Correct awkward rank layouts, including the Overview rank and Records
  rank/board-population pairs. Treat each pair as one aligned visual group;
  avoid detached denominators or mismatched vertical positions.
- Preserve readable text alignment for names, circuit names and prose rather
  than centering every string indiscriminately.
- Implemented: centered tab group, summary columns and paired rank/population
  values on one baseline. Circuit names and prose retain left alignment.

## Explain the percentage bar

- Keep the percentage bar, but add a clear label explaining its meaning.
- The user explicitly selected **Total Track Domination** after considering
  and rejecting **Track Coverage** and **Track Familiarisation**.
- The original behavior counted circuits with any published personal best.
  The subsequent screenshot review exposed the mismatch with the site's Tracks
  metric. The user selected circuits led / 20, including DLC, for this follow-up.
- Implemented label: **Total Track Domination** / **Toplam pist hâkimiyeti**.
  The current calculation counts each circuit with at least one first-place
  board once; 16 circuits led / 20 = 80% in the supplied example.

## Records dropdown

- Fix the visual issue shown in the expanded class filter: the bottom option
  appears clipped against the popup's lower edge, above the first record row.
- Inspect popup height, option padding, clipping and its relationship to the
  underlying rows before choosing a fix; the screenshot alone does not prove
  the technical cause.
- Preserve the Records pane's current overall density, typography and row
  layout except for the requested alignment and dropdown corrections.
- Source inspection found that the popup inherited the card's zero vertical
  padding. It now has its own padding, corner radius, complete selectable row
  heights and matching height constraints.

## Membership date

- Put the date directly beside **Member since** on the same line, for example
  **Member since October 2026**, instead of two separate lines.
- Keep the localized equivalent readable without shrinking the text
  excessively or allowing it to overflow the sidebar.
- Implemented a single membership line. Long English month names can use a
  shortened month if needed; Turkish uses **Üyelik: Ekim 2026**.

## Header stripe gradient

- The current effect is still rejected. The requested change is a permanent
  difference between the three stripes, not merely a panel opening animation.
- From the viewer's left to right:
  - Left stripe: more than half of its length fades into the background.
  - Middle stripe: only a small part fades.
  - Right stripe: fully colored, solid orange.
- Use the supplied native purple/blue menu headers as the visual reference for
  the diagonal geometry and smooth blending into the background.
- Do not apply a similar full-length fade to all three stripes again. The
  current small opacity multiplier difference between stripes does not achieve
  the requested appearance.
- Keep the launcher's orange palette. The separate panel opening animation
  remains independent of this static stripe treatment.
- Implemented an 80% fade length on the left, 18% in the middle and a completely
  solid right stripe. Native appearance remains to be reviewed.

## Overview density and typography

- Text has become too small and leaves excessive empty space in Overview.
- The user is open to a smaller window, moderately larger text, useful extra
  information, or a balanced combination. A final choice has not been made.
- Prefer first assessing moderate Overview-only typography and spacing changes
  within the current window: a global resize could unnecessarily disrupt
  Records, whose density the user wants preserved.
- Keep the Overview legible and balanced without returning to the earlier
  oversized or cramped layout. Do not fill space with redundant captions.
- Implemented moderately larger Overview values, captions and personal-best
  details, with balanced spacing. The window size, Records circuit/lap/mode
  typography, six-row capacity and row scrolling remain unchanged; rank pairs
  have a common size and baseline.

## Optional account bio

- Confirmed `bio` in the existing shared public account response. Display it
  below membership without adding an HTTP request. The website allows up to
  300 characters; the client also bounds Unicode characters and UTF-8 bytes.
- Implemented a three-line wrapped preview with a clickable **...** when it
  overflows. The user requested a dedicated in-game full-bio window instead of
  relying on the website action to read the remainder.
- **...** or **Y / R** opens the full bio. **Close**, **A / Enter** or **B / Esc**
  closes only the bio, retaining the license and its tab/Records position.
- The modal bounds its height and scrolls only its text when necessary. Long
  words, line breaks and multibyte characters are handled without shrinking the
  text or making the entire card scroll.
- Empty or unavailable bios are hidden. No bio content is logged, and this
  optional field does not disable the rest of the profile.
- Added shared embedded Noto emoji artwork for 5,709 sequences / 4,068 sprites.
  Both preview and full text preserve supported emoji clusters, flags and skin
  tones without modifying account text, adding HTTP requests or changing the
  pinned SDK ABI. Future emoji outside this catalogue need an asset update.
- Launcher version `1.1.11` is approved for this Rider License release. The
  update manifest endpoint remains unconfigured.

## Scope to preserve

- Keep **View profile / Profili incele**, **RIDER LICENSE / PİLOT LİSANSI** and
  **Open NOBK Online profile** naming.
- Preserve both tabs, avatars, flags, profile actions and controller controls.
- Preserve the temporary 2009 scope until the separate season integration is
  undertaken. These design notes do not authorize backend or season changes.
- Preserve the existing loading, caching and retry behavior. Bio uses the
  existing account response and cached player snapshot.
