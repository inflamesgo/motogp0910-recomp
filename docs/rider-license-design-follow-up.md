# Rider License design follow-up

Recorded on 2026-10-22 from the user's native screenshots and feedback.
Initially recorded as notes only. The user subsequently authorized implementation
in order, including the in-game full-bio popup described below. These items are
now implemented in source and await native visual review. No tests or builds
were run for this design pass, and the changes have not been committed or pushed.

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
- Current behavior is distinct circuits with a published personal best divided
  by 20 playable circuits, including DLC. The illustrated 17/20 is therefore
  85% track coverage, not first-place ownership or domination.
- Implemented label: **Total Track Domination** / **Toplam pist hâkimiyeti**.
  The calculation remains unchanged; it is not first-place ownership.

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

## Scope to preserve

- Keep **View profile / Profili incele**, **RIDER LICENSE / PİLOT LİSANSI** and
  **Open NOBK Online profile** naming.
- Preserve both tabs, avatars, flags, profile actions and controller controls.
- Preserve the temporary 2009 scope until the separate season integration is
  undertaken. These design notes do not authorize backend or season changes.
- Preserve the existing loading, caching and retry behavior. Bio uses the
  existing account response and cached player snapshot.
