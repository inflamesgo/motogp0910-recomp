# In-game player card

The native **Gamer Card** action in Online lobbies and leaderboards now opens a
player card using the launcher's dark palette and orange accent (`#ff6a13`).
It shows the selected player's account name, country when available and NOBK
avatar, with **Open NOBK profile** and **Back** actions. A 180 ms fade-in covers
the panel, backdrop, text and photo; Windows' reduced-animation preference is
respected. The initial Windows build was reviewed by the user in game. The
subsequent metadata, JPEG, theme, animation and logging changes still require
native Windows review; no automated or gameplay tests were run for this update.

## Identity and public data

Both native entry paths reach `82303260`, whose vtable entry is at `8206CB4C`.
It takes the selected player's record in `r5`, reads the XUID at offset 8 and
calls `8239C430`. That wrapper passes `r3 = local user index`, `r4 = selected
XUID` and `r5 = 0` to `XamShowGamerCardUIForXUID`. The SDK's previous stub for
that import is replaced by the client hook; original game data and generated
functions are unchanged.

The community service encodes a player as `0x0009000000000000 | id`, with a
nonzero 24-bit ID. This ID is resolved through the existing public endpoint
`https://www.nobkonline.com/api/motogp/player?id=<id>`. The response ID must match
the selection before its account name is used. The current launcher account,
leaderboard rank and the site's independent account ID are never substituted.

**Open NOBK profile** opens
`https://nobkonline.com/profile/?u=<percent-encoded-name>`, as requested, rather
than the MotoGP-only rider page. The general profile page itself uses
`/api/fifa09/player?name=<percent-encoded-name>` for shared account metadata,
including MotoGP users. The card follows that existing public contract to read
`country` and `avatar`, requiring the returned `player` name to match the
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

## Avatar loading and fallback

Name lookup starts only when the card opens, on one worker. The account name
and profile action become available before the optional avatar download ends.
There is no periodic network refresh. Stale responses from a closed card or
another selected player cannot replace the current card's name or image.

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

HTTP events include the fixed host/path, status, content type, byte count,
duration and Windows error code when relevant. Logs distinguish an account with
no photo, HTTP errors, decode failures and responses discarded after closing or
changing cards. They do not record response bodies, passwords, tokens or cookies.

For a missing avatar/country, open that player's card, wait for loading to finish,
then share the newest game log from the install's `logs/` directory. Search for
`PLAYER_CARD` to follow the request. A default helmet is expected for an account
whose public profile explicitly reports no photo.

If the player name cannot be resolved, the card explains the failure and keeps
**Back** available; its profile button is disabled. It never opens a guessed
username. There is no new modal error box.

## Controls and lifecycle

- **A / Enter** opens the selected player's general NOBK profile in the browser.
- **B / Esc** or the **Back** button closes the card.
- Mouse controls use the same two visible actions.

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

The current card does not add a full records table, account editing, friends,
invites or replay transfers. Native Windows review should cover a lobby member,
multiple leaderboard rows including the local player, accounts with/without a
photo, failed lookups, opening/dismissal presses and switching to the browser
and back. These checks remain pending for the latest source. Development continues
on `main`, separate from the frozen `preview` handoff branch. The Windows Actions
workflow must be dispatched with `quality_checks=false` for the requested
build-only run; push-triggered tests are skipped for this change.
