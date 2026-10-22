# In-game rider license

The native **View profile** action in Online lobbies and leaderboards now opens a
player card using the launcher's dark palette and orange accent (`#ff6a13`).
It is titled **RIDER LICENSE** (**PİLOT LİSANSI**) and has **Overview** and
**Records** tabs. A persistent left sidebar holds the selected player's avatar,
orange account name, country flag, inline membership date and optional bio. The
right pane holds the tabs and their contents. A compact **Open NOBK Online profile** action remains
in the shared footer; **B / Esc** closes the card without a separate Back button.
A 180 ms fade-in covers
the panel, backdrop, text and photo; Windows' reduced-animation preference is
respected. The left title stripe fades over 80% of its diagonal, the middle
stripe fades only over its bottom 18%, and the right stripe is fully orange.
The ramps use smoothstep opacity over 24 seamless bands. This permanent
difference between stripes is independent of the opening animation.
The user supplied native screenshots of both tabs and confirmed that account
data and avatars load in game. The latest alignment, typography, stripe and bio
changes still require native visual review; no builds or tests were run for this
design pass. The approved concepts are implemented on an 1100 by
580 design canvas, scaled uniformly to fit the display. Text and hit targets
share that canvas; the outer window has no scroll state. Overview fits without
scrolling. Records draws complete rows with a dedicated row-based scrollbar,
keeping its tabs, filter, headings, sidebar and footer fixed. At 1920 by 1080,
Records circuit, lap-time and mode text retain their previous sizes, and six
records fit at once. Rank/population pairs now use a common size and baseline.
Overview uses moderately larger typography: 48-pixel summary values, roughly
19-pixel captions and a 50-pixel personal-best time at that resolution. Summary
columns, the tab group and rank/population pairs are centered in their respective
areas. Each rank pair shares one baseline; circuit names remain left-aligned.

## Identity and public data

Both native entry paths reach `82303260`, whose vtable entry is at `8206CB4C`.
It takes the selected player's record in `r5`, reads the XUID at offset 8 and
calls `8239C430`. That wrapper passes `r3 = local user index`, `r4 = selected
XUID` and `r5 = 0` to `XamShowGamerCardUIForXUID`. The SDK's previous stub for
that import is replaced by the client hook; original game data and generated
functions are unchanged.

The existing message getter hook replaces native bank-0 IDs 1137 and 1433
(formerly "Gamer Card") and 1416 (formerly "View gamer card") with **View profile**
or **Profili incele**, according to the game's written language. Menu action IDs
and the original message banks are preserved. The Turkish source catalogue is
kept consistent with these labels.

The community service encodes a player as `0x0009000000000000 | id`, with a
nonzero 24-bit ID. This ID is resolved through the existing public endpoint
`https://www.nobkonline.com/api/motogp/player?id=<id>`. The response ID must match
the selection before its account name is used. The current launcher account,
leaderboard rank and the site's independent account ID are never substituted.

**Open NOBK Online profile** opens
`https://nobkonline.com/profile/?u=<percent-encoded-name>`, as requested, rather
than the MotoGP-only rider page. The general profile page itself uses
`/api/fifa09/player?name=<percent-encoded-name>` for shared account metadata,
including MotoGP users. The card follows that existing public contract to read
`country`, `member_since` and `avatar`, requiring the returned `player` name to match the
resolved MotoGP account. It never treats a MotoGP ID as a FIFA or site account ID.

A supplied avatar path must belong to `/api/avatar/` on NOBK's HTTPS host.
The query string is preserved for the site's versioned image URLs. An explicit
empty avatar means no uploaded photo. When the field is absent, the legacy
`/api/avatar/u/<percent-encoded-name>` route remains a fallback. Arbitrary hosts
and HTTP redirects are rejected.

Only Online mode with the production `https://motogp.nobkonline.com` service
uses this lookup. Offline, LAN and independent services are not mapped to NOBK
players, even if their numeric identities happen to resemble a production ID.
No account token, password or community credential is sent to either public
endpoint. No backend, protocol or SDK/runtime change is required for this card.

A valid two-letter country from the shared profile is displayed when available.
The current MotoGP endpoint does not supply country. If the optional shared
lookup fails, an otherwise valid MotoGP identity and its profile action remain
available. Country is not inferred from the current user, language or IP.

Country codes select flags in an embedded 768 by 544 PNG atlas; no flag download
or emoji/font support is required at runtime. The atlas is rasterized from
[flag-icons](https://github.com/lipis/flag-icons/tree/ac7e0eb3259eb62aa6100c435607e2fe03896a97)
(MIT). Its attribution is in `third_party/flag-icons-LICENSE.txt` and the launcher's
embedded third-party notices. Missing country data is left hidden. Membership
dates use the shared API's Unix timestamp and localized month/year, never the
illustrative date from the concept image.

## Overview and personal records

The identity response already includes the selected player's `records` array.
The card validates and publishes an immutable record snapshot on the same
worker; switching tabs, filtering and scrolling add no HTTP calls. Invalid or
missing records do not prevent a valid identity, avatar or profile action.

Overview shows distinct circuits with a published personal best, leaderboards
where the player is first, and total published leaderboard entries. Its coverage
bar uses the 20 distinct playable circuits supplied by the mandatory base/DLC
content. Native slot 17 (reserved Balaton) is excluded; Silverstone, Shanghai and
Istanbul remain indices 18, 19 and 20. This differs deliberately from the live
site's older 17-track catalogue and its ranking metric of **tracks led**. The
first card statistic is **tracks recorded**, not tracks led; **records held**
counts first places, not all personal bests. The percentage bar is labeled
**Total Track Domination**, as explicitly selected by the user. Its calculation
remains circuits with a published personal best / 20 (17/20 = 85%); the naming
change does not turn it into a first-place ownership statistic.

Records has four columns: rank/board population on one line, circuit with
class/2009 underneath, best lap, and stored mode. Results use the site's
`display.text`; game mode comes from uploaded property 6. Unknown data uses an
ASCII hyphen to avoid a missing-glyph diamond. The empty bike column is removed.
Class labels follow published metadata, preserving the existing server-side
Moto2 integration limits. As requested, this initial view is scoped to 2009:
known 2010 rows (property 13 = 1) are excluded from the list, filters and summary;
legacy rows without a season remain in the initial 2009 presentation. The source
season property is retained for the later server integration.

The class filter is built from returned records. Empty records and failed lookups
have separate messages. Its popup has dedicated vertical padding, a smaller
corner radius, complete 24-unit selectable rows and matching height constraints;
it no longer inherits the main card's zero vertical window padding. Sorting uses
rank, then latest update. A card opened
through the verified leaderboard caller `8223EDD4` copies the current native view
from `833362D8` (written by selector `82300088`). That exact result is shown first
and highlighted; other results on the selected circuit follow. Overview also
shows the exact selected-board best when the public API exposes it. Lobby calls
have no board context and cannot inherit a previous leaderboard selection.
The public player endpoint currently lists group-0 results, so unpublished ghost
or other groups are not represented as if they were returned.

## Membership and bio

The membership date shares one line with **Member since**, for example
**Member since October 2026**. English month names can be abbreviated when a
long label would not fit; the text does not become smaller to force a fit.
Turkish uses **Üyelik: Ekim 2026**. Missing membership information is hidden.

The existing shared public account response also contains `bio`. It is read
after the account name has been verified and uses the same request and cache as
the avatar metadata; no new endpoint, download or periodic refresh is added.
Only plain UTF-8 text is displayed, up to 300 Unicode characters / 1200 bytes.
Line breaks are preserved, tabs become spaces and embedded ASCII controls are
excluded. Empty, oversized or unavailable bios remain hidden. Bio contents are
never written into diagnostics.

The sidebar shows a word-wrapped three-line preview below the membership date.
If more text remains, a clickable **...** button at the end of the last line
opens a centered **About me** modal with the full text. **Y** on the gamepad or
**R** on the keyboard can open it for any nonempty bio. Preview wrapping preserves
UTF-8 character boundaries, including when a single word exceeds the column.

The modal sizes to the text up to a bounded height; only its text area scrolls
for unusually long paragraphs or many explicit line breaks. Mouse wheel,
scrollbar, D-pad and keyboard arrows can scroll it. A **Close** button,
**A / Enter** or **B / Esc** closes the bio while retaining the underlying card,
its selected tab and Records scroll position. Closing input cannot also open
the website or dismiss the card in the same frame. Tabs, filters and profile
actions are blocked while the modal is open. Each reopening starts the text at
the top, and a new player selection resets its open state.

## Avatar loading and fallback

Name lookup starts only when the card opens, on one worker. The account name,
records and profile action become available before the optional account metadata
and avatar download end. There is no periodic network refresh. Stale responses
from a closed card or another selected player cannot replace the current card's
name or image.

The worker reuses a WinHTTP session and connection across identity, metadata,
avatar and subsequent player requests, allowing HTTPS keep-alive instead of
recreating the entire session for each download. Requests use asynchronous
WinHTTP completion callbacks with a cancellable worker wait. Closing the card,
selecting another player or shutting down cancels the current request; the next
player does not wait for a closed card's network timeout. Request context and
read buffers remain alive until the final handle-closing callback. Each HTTP
exchange has a five-second deadline shared by send, headers and body reads,
instead of accumulating a separate timeout for every stage.

The two supplied native logs show the first identity request timing out, followed
by a successful manual reopen of the same player; later lookups in the second
log complete in roughly 0.3 seconds including metadata and photo. This points to
an initial-request problem, but the old combined send/receive timing cannot
identify DNS, proxy discovery, TLS or server response as the cause. The first
identity request now gets a two-second initial attempt and one automatic retry
for transient transport failures, using only the remainder of the same
five-second total budget. Cancellation, HTTP error responses, identity mismatch
and malformed JSON do not trigger this retry. Later player lookups do not add
retries, and there is no background warm-up or polling request.

A bounded, memory-only cache retains up to eight profiles. A complete profile
reopened within 60 seconds is shown immediately without HTTP. An older or partial
cached profile can be shown for up to five minutes while the worker refreshes it
in the background. The selected leaderboard view is always taken from the new
opening, never from the cached card. Refresh failures keep the available cached
profile visible; a verified account rename clears the previous account's photo.
Partial or failed loads are eligible for another attempt on reopening. The cache
does not survive game shutdown, and a first visit still depends on network and
server response time. These changes have not been built or tested locally.

Avatar downloads are limited to 512 KiB and image dimensions to 1024 by 1024
before decoding. HTTP errors, missing photos, invalid or oversized images leave
the default helmet visible; they do not disable an otherwise resolved profile.
Windows Imaging Component decodes JPEG and PNG on the worker. The site accepts
JPG, PNG and WEBP uploads but can serve a re-encoded JPEG; decoding uses the
actual image format rather than the upload extension. No optional WEBP codec
is required by the card. Texture creation and release
use the existing immediate renderer on the UI thread. The full photo fits the
avatar square without stretching or cropping. Avatars are not written to disk.

## Diagnostics

Card events use the existing game log with the `PLAYER_CARD` prefix. No separate
logging service, polling loop or debug UI is added. Each opening gets a request
number that connects the selected XUID/player ID to these stages:

- `identity`: MotoGP ID-to-name lookup and returned field availability.
- `account`: shared profile lookup, country and avatar source.
- `avatar`: HTTP result, JPEG/PNG decode dimensions and UI texture upload.
- `records`: parsed personal-best rows, circuits and first places.
- `flags`: embedded atlas decode/upload failures.

HTTP events include the fixed host/path, status, content type, byte count,
duration and Windows error code when relevant. `http_phase` events report
cumulative setup, send-completion and response-header timings; `http_result`
reports total time through the final body read. `cache` records immediate hits and freshness, and
`cancelled` distinguishes dismissal from a network failure. `http_retry` records
the single initial-connection retry and its remaining time budget. Logs distinguish
an account with no photo, HTTP errors, decode failures and responses discarded after
closing or changing cards. They do not record response bodies, passwords, tokens
or cookies.

For a missing avatar/country, open that player's card, wait for loading to finish,
then share the newest game log from the install's `logs/` directory. Search for
`PLAYER_CARD` to follow the request. A default helmet is expected for an account
whose public profile explicitly reports no photo.

If the player name cannot be resolved, the card explains the failure and keeps
**B / Esc** available; its profile button is disabled. It never opens a guessed
username. There is no new modal error box.

## Controls and lifecycle

- **A / Enter** opens the selected player's general NOBK profile in the browser.
- **B / Esc** closes the card; there is no visible Back button.
- **Y / R** opens the full bio when present; the preview's **...** does the same.
- With the bio open, **A / Enter**, **B / Esc** or **Close** closes only that modal.
- **LB / RB** or **Q / E** changes tabs.
- **X / F** cycles the Records class filter.
- **D-pad / Up / Down** scrolls Records or the open bio; a held D-pad repeats at
  a bounded rate.
- Mouse controls support tabs, the class dropdown, wheel/scrollbar navigation,
  the bio preview and the compact profile action. The license itself never
  scrolls; scrolling is limited to Records rows or the bio modal's text area.

The card uses a separate instance of the existing settings input gate. Opening
F1 and the card is serialized so concurrent requests cannot acquire both gates.
The opening press must return to neutral before card actions become active. Focus
changes rearm this guard, and dismissal remains blocked from the underlying
menu until held inputs are released. The existing guest-frame observer polls
only an active/held gate; no device or HTTP work is added while it is inactive.
The card sends the usual `XN_SYS_UI` open/close notification, prevents opening
F1 settings over it, and participates in the existing quit/keystroke gate.

## FIFA 09 reference and limits

The inspected reference is
[ChunchunQ/fifa09-recomp at `711da394`](https://github.com/ChunchunQ/fifa09-recomp/blob/711da3941d81879db06ea4f2c36c3a519ac826d9/patches/nobk-sdk-1.3.0.patch).
Its social channel supplies friend/account metadata including country and
avatar paths. Avatar download and UI-thread texture creation are separate.
This card uses the shared public avatar service without importing FIFA's
social protocol, friends list, invitations or SDK patch.

The current card does not add account editing, friends, invites or replay
transfers. Native Windows review should cover both tabs, class filtering,
gamepad/keyboard scrolling, country flags, inline membership date, short/long/empty
and multiline bios, bio modal input isolation, the three distinct stripe ramps
and selected-board ordering, as well as a lobby member,
multiple leaderboard rows including the local player, accounts with/without a
photo, failed lookups, opening/dismissal presses and switching to the browser
and back. These checks remain pending for the latest source. Development continues
on `main`, separate from the frozen `preview` handoff branch. The Windows Actions
workflow must be dispatched with `quality_checks=false` for the requested
build-only run; push-triggered tests are skipped for this change. Per the user's
standing instruction, dispatch the build and do not monitor its result.
