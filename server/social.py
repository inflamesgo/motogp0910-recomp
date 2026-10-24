"""Authenticated MotoGP friends and short-lived lobby invitations.

Runs on the service event loop. Only the public NOBK friend lookup runs in a
worker; database access and room ownership decisions stay on the event loop.
"""
import asyncio
import json
import secrets
import struct
import time
import urllib.parse
import urllib.request

from security import Invalid, Limited

PEER_BASE = 0x0009000000000000


class IdentityUpgrade(Invalid):
    pass


class Social:
    def __init__(self, service):
        self.service = service
        self.friends_api = None
        self.cache = {}
        self.fetching = {}
        self.invites = {}

    def lookup(self, name, account):
        request = urllib.request.Request(
            self.friends_api + urllib.parse.quote(name, safe=''),
            headers={'User-Agent': 'MotoGP0910-Social/1'})
        with urllib.request.urlopen(request, timeout=3) as response:
            raw = response.read(65537)
        if len(raw) > 65536:
            raise Invalid('friend response too large')
        data = json.loads(raw)
        # The existing display-only API has names alone. Sending invitations
        # requires immutable account IDs, including proof of whose list this is.
        if not isinstance(data, dict) or type(data.get('account_id')) is not int or data['account_id'] != account:
            raise IdentityUpgrade('shared friends API needs stable account ids')
        rows = data.get('friends') if isinstance(data, dict) else None
        if not isinstance(rows, list) or len(rows) > 100:
            raise Invalid('invalid friend service response')
        names = {}
        for row in rows:
            name = row.get('username') if isinstance(row, dict) else None
            friend = row.get('account_id') if isinstance(row, dict) else None
            if type(friend) is not int or not 0 < friend < 2**31:
                raise IdentityUpgrade('shared friends API needs stable account ids')
            if not isinstance(name, str) or not 1 <= len(name.encode('utf-8')) <= 15:
                raise Invalid('invalid friend service response')
            names[friend] = name
        return names

    async def friends(self, owner, name, account):
        now = time.monotonic()
        entry = self.cache.get(owner)
        if entry and entry[0] > now:
            return entry[1]
        # Concurrent poll/send requests share one bounded lookup per account.
        task = self.fetching.get(owner)
        if task is None:
            if len(self.fetching) >= 8:
                raise Limited('friend service busy')
            task = asyncio.create_task(asyncio.to_thread(self.lookup, name, account))
            self.fetching[owner] = task
            def completed(done):
                if self.fetching.get(owner) is done:
                    self.fetching.pop(owner, None)
            task.add_done_callback(completed)
        try:
            names = await asyncio.shield(task)
        except IdentityUpgrade:
            raise
        except (OSError, ValueError):
            raise Limited('friend service unavailable') from None
        finally:
            if task.done():
                self.fetching.pop(owner, None)
        self.cache = {key: value for key, value in self.cache.items() if value[0] > now}
        if len(self.cache) >= self.service.max_clients:
            raise Limited('friend cache capacity reached')
        self.cache[owner] = (now + 30, names)
        return names

    def room(self, host, session=None):
        room = self.service.rooms.get(host)
        client = self.service.clients.get(host)
        if not room or not client or client.room != host or time.monotonic() - room.seen > 15:
            raise Invalid('host lobby is no longer available')
        if session is not None and room.session != session:
            raise Invalid('host lobby has changed')
        _, capacity, _, state = struct.unpack_from('>IIII', room.body, 16)
        if state or len(room.members) >= capacity:
            raise Invalid('host lobby is racing or full')
        return room

    def member_room(self, peer):
        client = self.service.clients.get(peer)
        room = self.room(client.room if client else 0)
        if peer not in room.members:
            raise Invalid('rider is not in this lobby')
        return room

    async def dispatch(self, credential, body):
        if not self.friends_api:
            return json.dumps({'supported': False}).encode()
        if len(body) > 1024:
            raise Invalid('social request too large')
        request = json.loads(body)
        if not isinstance(request, dict) or request.get('action') not in ('poll', 'send', 'accept', 'decline'):
            raise Invalid('invalid social action')
        owner = self.service.store.owner(credential)
        name, account = self.service.store.db.execute('SELECT name,account FROM players WHERE id=?', (owner,)).fetchone()
        peer = PEER_BASE | owner
        if not account or peer not in self.service.clients:
            raise Invalid('social actions require a connected NOBK account')
        try:
            names = await self.friends(owner, name, account)
        except IdentityUpgrade:
            return json.dumps({'supported': False, 'reason': 'friends_identity_upgrade_required'}).encode()
        # Account/profile may have disconnected or switched while HTTP was pending.
        if self.service.store.owner(credential) != owner or peer not in self.service.clients:
            raise Invalid('online account changed')
        # Resolve immutable account IDs, even if the public handle was renamed
        # while this account was offline. The existing unique index is reused.
        rows = self.service.store.db.execute(
            'SELECT id,account FROM players WHERE account IN (' + ','.join('?' for _ in names) + ')',
            tuple(names)).fetchall() if names else []
        friends = {PEER_BASE | key: names[account_id] for key, account_id in rows if key != owner}
        now = time.monotonic()
        self.invites = {key: value for key, value in self.invites.items() if value['until'] > now}
        action = request['action']
        accepted = None
        if action == 'send':
            if not self.service.profile_limits.take(('social-send', peer), 1/5, 4):
                raise Limited('invitation limit')
            target = request.get('to')
            if not isinstance(target, str) or len(target) != 16 or any(c not in '0123456789abcdefABCDEF' for c in target):
                raise Invalid('invalid invitation recipient')
            target = int(target, 16)
            if target not in friends or target not in self.service.clients:
                raise Invalid('recipient must be an online NOBK friend')
            room = self.member_room(peer)
            if target in room.members:
                raise Invalid('friend is already in this lobby')
            # Repeated clicks refresh no state and create no duplicate notification.
            if not any(v['from'] == peer and v['to'] == target and v['session'] == room.session for v in self.invites.values()):
                if len(self.invites) >= 512 or sum(v['to'] == target for v in self.invites.values()) >= 16:
                    raise Limited('invitation capacity reached')
                self.invites[secrets.token_hex(8)] = {'from': peer, 'host': room.host, 'to': target, 'name': name, 'session': room.session, 'until': now + 120}
        elif action in ('accept', 'decline'):
            key = request.get('id')
            invitation = self.invites.get(key) if isinstance(key, str) else None
            if not invitation or invitation['to'] != peer:
                raise Invalid('invitation expired or belongs to another rider')
            if action == 'accept':
                if invitation['from'] not in friends:
                    raise Invalid('inviter is no longer a NOBK friend')
                room = self.room(invitation['host'], invitation['session'])
                if invitation['from'] not in room.members:
                    raise Invalid('inviter has left this lobby')
                accepted = {'from': f"{invitation['from']:016x}", 'room': room.body.hex()}
            del self.invites[key]
        pending = []
        for key, invitation in self.invites.items():
            if invitation['to'] == peer:
                try:
                    room = self.room(invitation['host'], invitation['session'])
                    if invitation['from'] not in room.members:
                        continue
                except Invalid:
                    continue
                pending.append({'id': key, 'from': invitation['name']})
        try:
            self.member_room(peer)
            can_invite = True
        except Invalid:
            can_invite = False
        output = {'supported': True, 'can_invite': can_invite, 'friends': [{'name': value, 'peer': f'{key:016x}', 'online': key in self.service.clients} for key, value in friends.items()], 'invites': pending}
        if accepted:
            output['accepted'] = accepted
        return json.dumps(output, separators=(',', ':'), ensure_ascii=False).encode()
