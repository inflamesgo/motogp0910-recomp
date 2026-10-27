# Invitation UI and local-test observations

## Status — 27 October 2026

The user reports that Discord invitations work in their local test and supplied
a Discord invitation card showing `Joined`. This is useful user-reported local
evidence, not proof of production deployment, closed-game activation or every
two-player join scenario. Their F1 screenshots also show a sent invitation and
an incoming invitation from a synthetic friend.

The ghost console says `Ghost transfer ownership, protocol, failure and
cancellation checks passed`. This is a successful synthetic transport check.
The following line explicitly excludes native ghost playback. Do not interpret
it as an error, or claim that native download/save/playback is ready.

## Approved F1 presentation — implemented in source

The approved second concept uses clipped-corner pit tickets in the existing
Apex Friends rows. The panel, backdrop, navigation, avatars and country columns
retain their design. The first concept's plain orange text action was rejected.

- Place a charcoal `INVITE` ticket with an orange outline, clipped corner and
  arrow at the right edge of each eligible friend row. The activity column
  fits beside it without moving the existing avatar, name or country columns.
- Hover/focus strengthens the outline. The ticket uses its own hit target,
  separate from profile opening. The native invite picker's A/Enter action
  still sends an invitation to the selected friend.
- Show `SENDING…` in that same action slot while a request is pending.
- After the server acknowledges success, show a muted check and `SENT` on
  that friend's row. Remove the unrelated global `Invitation sent.` heading.
  This means sent, not delivered/read/accepted.
- The existing social worker keeps feedback per peer across polls, clears it
  on a changed room host/session/key or loss of eligibility, and discards a
  queued send when its room changed before dispatch. Concurrent sends to the
  same peer are blocked; failures show a localized `RETRY` ticket. A `SENT`
  ticket may be clicked again to send another invitation through the existing
  server deduplication rules. It is a historical acknowledgement, not a claim
  that an invitation remains pending or has been accepted.

Incoming invitations now occupy a dedicated row above ordinary friends with
the inviter's circular portrait, `LOBBY INVITATION` heading, name and subtitle.
The approved `ACCEPT` and `DECLINE` pit tickets appear side by side: orange
outline/check for accept, muted outline/cross for decline. Only controller
hints sit beneath their respective buttons: A/X for Xbox, Cross/Square for
PlayStation. Keyboard prompt style hides this hint line; keyboard input remains
available.
Clicking the row selects it; only the explicit accept button or A/Enter joins.
Delete/X (Square on PlayStation) declines. Both tickets are disabled while the
same invitation is being answered; duplicate answers are rejected by the local
queue, and failure clears the pending state so the player can retry.

Profile opening on ordinary friend rows remains unchanged. Labels use the
existing fonts, English/Turkish text and viewport scaling. Neither the server
API nor the native acceptance ABI changes.

## Incoming pit board — implemented in source

Reuse the approved friend-online notification's 404 × 108 reference-pixel
geometry at 1080p: charcoal surface, chamfered right corners, orange left stripe,
portrait, muted outline and countdown line. Change the content to:

- Orange heading: `LOBBY INVITE`.
- White name: the inviter, using the existing UTF-8-safe truncation.
- Muted prompt: `F1 · Respond`, using the configured panel key in production.

It uses the existing right-side entrance and exit behavior, queue, Online gating,
photo cache and passive input contract. Opening F1 resolves responses in the
panel; the toast itself does not capture gameplay controls. A disappearing
invitation stops its active notification. The subtitle is kept to one line
with UTF-8-safe truncation so custom key labels cannot enlarge the 404 × 108
card. Synthetic bots still cannot host a real race.

Implementation files are `src/nobk_panel.cpp`, `src/nobk_friend_toast.h`,
`src/online/social.{h,cpp}` and the room-provider hookup in `src/lan/runtime.cpp`.
Source review covers separate hit targets, server-acknowledged feedback,
pending-send/answer deduplication, separate accept/decline controls, room
invalidation and shared toast geometry. The 28 October local Windows x64
builds include these edits and passed packaging checks. Dedicated native
Windows acceptance of the revised invitation visuals and real-account
send/accept/decline remains outstanding; the user's subsequent acceptance
covers the leaderboard menus, text and audio.

## Production acceptance gate

Local success is not a 100% production guarantee. Deploy matching client and
service versions and verify the actual proxy routes, authentication/account
linking, unchanged room authority and network configuration. Native NOBK friend
invitations additionally require the shared account API's verified immutable
account IDs; the local fixture supplying synthetic identities does not establish
that production provides them. Discord invitation capabilities have a separate
authenticated path and do not depend on that friends-API identity extension.

Before release, use two real linked accounts to check send, receive,
accept/decline, an expired/closed room, and a real lobby join through the deployed
service. Check Discord warm acceptance and cold launch separately. Keep ghost
playback explicitly unavailable until its native format/task/save/playback
binding is completed and verified with a real recording.

See [the integration handoff](chunchunq-friends-invites-ghost-handoff.md),
[native invitations](online-friends-invites.md) and
[Discord invitations](discord-invites.md).
