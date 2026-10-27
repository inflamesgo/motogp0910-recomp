"""Short-lived Discord lobby capabilities; no Discord/NOBK credential is shared."""
import json
import secrets
import struct
import time

from security import Invalid, Limited

PEER_BASE = 0x0009000000000000
PREFIX = 'mg0910.v1.'


def valid_secret(value):
    return (isinstance(value, str) and len(value) == len(PREFIX) + 32
            and value.startswith(PREFIX)
            and all(c.isascii() and (c.isalnum() or c in '_-') for c in value[len(PREFIX):]))


def identity(value):
    if not isinstance(value, str) or len(value) != 16 or any(c not in '0123456789abcdefABCDEF' for c in value):
        raise Invalid('invalid Discord lobby identity')
    return int(value, 16)


class DiscordInvites:
    lifetime = 120
    capacity = 512

    def __init__(self, service):
        self.service = service
        self.grants = {}

    def lobby(self, host, session, inviter):
        room = self.service.rooms.get(host)
        client = self.service.clients.get(host)
        sender = self.service.clients.get(inviter)
        if (not room or not client or not sender or room.session != session
                or client.room != host or sender.room != host
                or inviter not in room.members or host not in room.members
                or time.monotonic() - room.seen > 15):
            raise Invalid('Discord lobby is no longer available')
        _, capacity, _, state = struct.unpack_from('>IIII', room.body, 16)
        if state or not 1 <= capacity <= 20:
            raise Invalid('Discord lobby is not joinable')
        return room, capacity

    def dispatch(self, credential, request):
        owner = self.service.store.owner(credential)
        account = self.service.store.db.execute('SELECT account FROM players WHERE id=?', (owner,)).fetchone()
        peer = PEER_BASE | owner
        if not account or not account[0] or peer not in self.service.clients:
            raise Invalid('Discord invitations require a connected NOBK account')
        now = time.monotonic()
        self.grants = {key: grant for key, grant in self.grants.items() if grant['until'] > now}
        action = request['action']
        output = {'supported': True}
        if action == 'discord_revoke':
            self.grants = {key: grant for key, grant in self.grants.items() if grant['from'] != peer}
        elif action == 'discord_publish':
            if not self.service.profile_limits.take(('discord-publish', peer), 1/5, 4):
                raise Limited('Discord publication limit')
            host, session = identity(request.get('host')), identity(request.get('session'))
            room, capacity = self.lobby(host, session, peer)
            if len(room.members) >= capacity:
                raise Invalid('Discord lobby is full')
            existing = next((key for key, grant in self.grants.items()
                             if grant['from'] == peer and grant['host'] == host and grant['session'] == session
                             and grant['key'] == room.body[40:56]), None)
            # One capability per publishing rider; the session and inviter are
            # revalidated on redemption, even while its TTL is refreshed.
            self.grants = {key: grant for key, grant in self.grants.items()
                           if grant['from'] != peer or key == existing}
            if existing is None:
                if len(self.grants) >= self.capacity:
                    raise Limited('Discord invitation capacity reached')
                existing = PREFIX + secrets.token_urlsafe(24)
            self.grants[existing] = {'from': peer, 'host': host, 'session': session,
                                     'key': room.body[40:56], 'until': now + self.lifetime}
            output.update(secret=existing, party_id=f'mg0910:{host:016x}:{session:016x}',
                          host=f'{host:016x}', session=f'{session:016x}', expires_in=self.lifetime)
        elif action == 'discord_join':
            if not self.service.profile_limits.take(('discord-join', peer), 1/3, 4):
                raise Limited('Discord join limit')
            secret = request.get('secret')
            if not valid_secret(secret):
                raise Invalid('invalid Discord invitation')
            grant = self.grants.get(secret)
            if not grant:
                raise Invalid('Discord invitation has expired')
            room, capacity = self.lobby(grant['host'], grant['session'], grant['from'])
            if not secrets.compare_digest(grant['key'], room.body[40:56]):
                raise Invalid('Discord lobby has changed')
            if peer in room.members and self.service.clients[peer].room == room.host:
                output['already_joined'] = True
            else:
                if len(room.members) >= capacity:
                    raise Invalid('Discord lobby is full')
                # The server supplies the session descriptor; a Discord secret
                # never contains a room key, address or profile credential.
                output['accepted'] = {'from': f"{grant['from']:016x}", 'room': room.body.hex()}
        else:
            raise Invalid('invalid Discord invitation action')
        return json.dumps(output, separators=(',', ':')).encode()
