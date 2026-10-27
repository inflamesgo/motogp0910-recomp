import asyncio
import json
from pathlib import Path
import secrets
import struct
import time
import unittest

from community_server import Service, Store, Room, Invalid, Limited, load_schemas, text
from discord_invites import PREFIX, valid_secret
from social import Social
from security import Limits


class DiscordInviteTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        store = Store(':memory:', load_schemas(Path(__file__).with_name('leaderboards.json')))
        self.addCleanup(store.db.close)
        self.service = Service(store, 30821)
        self.keys = [secrets.token_bytes(32) for _ in range(3)]
        self.peers = []
        for i, name in enumerate(('Alice', 'Bob', 'Eve')):
            store.link(self.keys[i], (101, 202, 303)[i], name)
            self.peers.append(struct.unpack_from('>Q', self.service.connect(self.keys[i], name))[0])
        self.make_room()

    def make_room(self, session=42, capacity=3, state=0, key=b'K'*16):
        peer = self.peers[0]
        body = struct.pack('>QQIIIIII', session, peer, 0x21, capacity, 1, state, 0, 0) + key + text('Qatar') + b'\0\0\0\0'
        room = Room(session, peer, body, members={peer})
        self.service.rooms[peer] = room
        self.service.clients[peer].room = peer
        return room

    async def request(self, who, action, **fields):
        return json.loads(await self.service.social.dispatch(self.keys[who], json.dumps({'action': action, **fields}).encode()))

    async def publish(self, who=0, session=42):
        return await self.request(who, 'discord_publish', host=f'{self.peers[0]:016x}', session=f'{session:016x}')

    async def test_independent_of_native_friend_api_and_no_private_descriptor_in_presence(self):
        self.assertFalse((await self.request(0, 'poll'))['supported'])
        published = await self.publish()
        self.assertTrue(published['supported'])
        self.assertTrue(valid_secret(published['secret']))
        self.assertEqual(set(published), {'supported', 'secret', 'party_id', 'host', 'session', 'expires_in'})
        self.assertEqual(published['expires_in'], 120)
        self.assertEqual(published['party_id'], f'mg0910:{self.peers[0]:016x}:000000000000002a')
        # Discord friends need not also be NOBK friends: possession of an
        # explicit invitation capability is redeemed by an authenticated peer.
        accepted = await self.request(2, 'discord_join', secret=published['secret'])
        self.assertEqual(accepted['accepted']['from'], f'{self.peers[0]:016x}')
        self.assertEqual(bytes.fromhex(accepted['accepted']['room']), self.service.rooms[self.peers[0]].body)

    async def test_refresh_is_stable_and_same_lobby_has_common_party_id(self):
        first = await self.publish()
        self.assertEqual((await self.publish())['secret'], first['secret'])
        room = self.service.rooms[self.peers[0]]
        room.members.add(self.peers[2]); self.service.clients[self.peers[2]].room = room.host
        guest = await self.publish(2)
        self.assertNotEqual(guest['secret'], first['secret'])
        self.assertEqual(guest['party_id'], first['party_id'])
        joined = await self.request(1, 'discord_join', secret=guest['secret'])
        self.assertEqual(joined['accepted']['from'], f'{self.peers[2]:016x}')

    async def test_expired_revoked_and_restarted_capabilities_fail(self):
        secret = (await self.publish())['secret']
        self.service.social.discord.grants[secret]['until'] = time.monotonic()-1
        with self.assertRaises(Invalid): await self.request(1, 'discord_join', secret=secret)
        secret = (await self.publish())['secret']
        await self.request(2, 'discord_revoke')
        self.assertIn('accepted', await self.request(1, 'discord_join', secret=secret))
        await self.request(0, 'discord_revoke')
        with self.assertRaises(Invalid): await self.request(1, 'discord_join', secret=secret)
        secret = (await self.publish())['secret']
        self.service.social = Social(self.service)
        with self.assertRaises(Invalid): await self.request(1, 'discord_join', secret=secret)

    async def test_current_membership_state_capacity_and_session_are_authoritative(self):
        for change in ('closed', 'racing', 'full', 'stale', 'replaced', 'inviter_left', 'host_left', 'key_changed'):
            with self.subTest(change=change):
                self.service.social.discord.grants.clear()
                self.service.profile_limits = Limits(100)
                room = self.make_room(capacity=2)
                secret = (await self.publish())['secret']
                if change == 'closed': self.service.rooms.clear()
                elif change == 'racing': self.make_room(state=1)
                elif change == 'full': room.members.add(self.peers[2])
                elif change == 'stale': room.seen = time.monotonic()-16
                elif change == 'replaced': self.make_room(session=43)
                elif change == 'inviter_left': self.service.clients[self.peers[0]].room = 0
                elif change == 'host_left': room.members.clear()
                else: self.make_room(key=b'J'*16)
                with self.assertRaises(Invalid): await self.request(1, 'discord_join', secret=secret)

    async def test_reused_session_id_with_changed_key_gets_a_new_capability(self):
        first = (await self.publish())['secret']
        self.make_room(key=b'J'*16)
        second = (await self.publish())['secret']
        self.assertNotEqual(first, second)
        with self.assertRaises(Invalid): await self.request(1, 'discord_join', secret=first)

    async def test_cannot_publish_another_lobby_or_join_without_linked_connection(self):
        with self.assertRaises(Invalid): await self.publish(1)
        with self.assertRaises(Invalid): await self.publish(session=43)
        secret = (await self.publish())['secret']
        self.service.store.db.execute('UPDATE players SET account=NULL WHERE id=?', (self.peers[1] & 0xFFFFFF,))
        with self.assertRaises(Invalid): await self.request(1, 'discord_join', secret=secret)
        del self.service.clients[self.peers[2]]
        with self.assertRaises(Invalid): await self.request(2, 'discord_join', secret=secret)

    async def test_already_joined_is_a_noop_and_closed_races_are_rejected(self):
        secret = (await self.publish())['secret']
        room = self.service.rooms[self.peers[0]]
        room.members.add(self.peers[1]); self.service.clients[self.peers[1]].room = room.host
        self.assertTrue((await self.request(1, 'discord_join', secret=secret))['already_joined'])
        self.make_room(state=1)
        with self.assertRaises(Invalid): await self.request(1, 'discord_join', secret=secret)

    async def test_limits_and_secret_validation(self):
        for invalid in (None, '', 'x'*128, PREFIX+'/'*32, PREFIX+'é'*32, PREFIX+'A'*31):
            self.assertFalse(valid_secret(invalid))
        secret = (await self.publish())['secret']
        for _ in range(3): await self.publish()
        with self.assertRaises(Limited): await self.publish()
        for _ in range(4): await self.request(1, 'discord_join', secret=secret)
        with self.assertRaises(Limited): await self.request(1, 'discord_join', secret=secret)

    async def test_capability_pool_is_bounded_but_existing_publication_can_refresh(self):
        self.service.social.discord.capacity = 1
        first = await self.publish()
        self.assertEqual(first['secret'], (await self.publish())['secret'])
        room = self.service.rooms[self.peers[0]]
        room.members.add(self.peers[2]); self.service.clients[self.peers[2]].room = room.host
        with self.assertRaises(Limited): await self.publish(2)

    async def test_actual_http_publish_join_and_profile_authentication(self):
        server = await asyncio.start_server(self.service.http, '127.0.0.1', 0)
        self.addAsyncCleanup(server.wait_closed); self.addCleanup(server.close)
        port = server.sockets[0].getsockname()[1]
        async def post(credential, request):
            reader, writer = await asyncio.open_connection('127.0.0.1', port)
            raw = json.dumps(request).encode()
            writer.write(b'POST /v2/social HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer ' + credential.hex().encode() + b'\r\nContent-Type: application/octet-stream\r\nContent-Length: ' + str(len(raw)).encode() + b'\r\n\r\n' + raw)
            await writer.drain(); response = await asyncio.wait_for(reader.read(), 3)
            writer.close(); await writer.wait_closed()
            return response.split(b'\r\n\r\n', 1)
        headers, body = await post(self.keys[0], {'action': 'discord_publish', 'host': f'{self.peers[0]:016x}', 'session': '000000000000002a'})
        self.assertTrue(headers.startswith(b'HTTP/1.1 200'))
        secret = json.loads(body)['secret']
        headers, body = await post(self.keys[1], {'action': 'discord_join', 'secret': secret})
        self.assertTrue(headers.startswith(b'HTTP/1.1 200')); self.assertIn('accepted', json.loads(body))
        headers, body = await post(secrets.token_bytes(32), {'action': 'discord_join', 'secret': secret})
        self.assertTrue(headers.startswith(b'HTTP/1.1 400')); self.assertNotIn(b'accepted', body)


if __name__ == '__main__':
    unittest.main()
