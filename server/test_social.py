import asyncio
import io
import json
from pathlib import Path
import secrets
import struct
import time
import unittest
from unittest.mock import patch

from community_server import Service, Store, Room, PEER_BASE, Invalid, Limits, load_schemas, text
from social import Social


class SocialTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        store = Store(':memory:', load_schemas(Path(__file__).with_name('leaderboards.json')))
        self.addCleanup(store.db.close)
        self.service = Service(store, 30821)
        self.social = self.service.social
        self.social.friends_api = 'http://127.0.0.1/friends?name='
        self.lookup_calls = 0
        def lookup(name, account):
            self.lookup_calls += 1
            return {'Alice': {202: 'Bob'}, 'Bob': {101: 'Alice'}, 'Eve': {}}[name]
        self.social.lookup = lookup
        self.keys = [secrets.token_bytes(32) for _ in range(3)]
        self.peers = []
        for i, name in enumerate(('Alice', 'Bob', 'Eve')):
            store.link(self.keys[i], (101,202,303)[i], name)
            raw = self.service.connect(self.keys[i], name)
            self.peers.append(struct.unpack_from('>Q', raw)[0])
        self.make_room()

    def make_room(self, session=42, capacity=2, state=0):
        peer = self.peers[0]
        body = struct.pack('>QQIIIIII', session, peer, 0x21, capacity, 1, state, 0, 0) + b'K'*16 + text('Qatar') + b'\0\0\0\0'
        room = Room(session, peer, body, members={peer})
        self.service.rooms[peer] = room
        self.service.clients[peer].room = peer
        return room

    async def request(self, who, action='poll', **fields):
        return json.loads(await self.social.dispatch(self.keys[who], json.dumps({'action': action, **fields}).encode()))

    async def send(self):
        await self.request(0, 'send', to=f'{self.peers[1]:016x}')
        reply = await self.request(1)
        return reply['invites'][0]['id']

    async def test_real_friend_ids_cached_and_distinct_from_fifa_ids(self):
        reply = await self.request(0)
        self.assertTrue(reply['can_invite'])
        self.assertEqual(reply['friends'], [{'name': 'Bob', 'peer': f'{self.peers[1]:016x}', 'online': True}])
        self.assertEqual(self.peers[1] & 0xFFFFFFFFFF000000, PEER_BASE)
        await self.request(0)
        self.assertEqual(self.lookup_calls, 1)
        # A legacy profile with the same public name is not an account friend.
        self.service.store.connect(secrets.token_bytes(32), 'Bob')
        self.assertEqual((await self.request(0))['friends'], reply['friends'])

    async def test_invite_accept_returns_server_owned_room_once(self):
        key = await self.send()
        reply = await self.request(1, 'accept', id=key)
        self.assertEqual(reply['accepted']['from'], f'{self.peers[0]:016x}')
        self.assertEqual(bytes.fromhex(reply['accepted']['room']), self.service.rooms[self.peers[0]].body)
        self.assertEqual(reply['invites'], [])
        with self.assertRaises(Invalid):
            await self.request(1, 'accept', id=key)

    async def test_duplicate_click_does_not_duplicate_invites(self):
        key = await self.send()
        await self.request(0, 'send', to=f'{self.peers[1]:016x}')
        self.assertEqual((await self.request(1))['invites'], [{'id': key, 'from': 'Alice'}])

    async def test_other_rider_cannot_answer_or_invite_nonfriend(self):
        key = await self.send()
        with self.assertRaises(Invalid):
            await self.request(2, 'accept', id=key)
        with self.assertRaises(Invalid):
            await self.request(0, 'send', to=f'{self.peers[2]:016x}')

    async def test_decline_does_not_disclose_room_or_trigger_join(self):
        key = await self.send()
        reply = await self.request(1, 'decline', id=key)
        self.assertNotIn('accepted', reply)
        self.assertEqual(reply['invites'], [])

    async def test_expired_replaced_closed_racing_and_full_lobbies_reject(self):
        key = await self.send()
        self.social.invites[key]['until'] = time.monotonic()-1
        with self.assertRaises(Invalid):
            await self.request(1, 'accept', id=key)
        for change in ('replaced', 'closed', 'racing', 'full'):
            with self.subTest(change=change):
                self.service.profile_limits = Limits()
                self.make_room()
                self.social.invites.clear()
                key = await self.send()
                if change == 'replaced': self.make_room(session=43)
                elif change == 'closed': self.service.rooms.clear()
                elif change == 'racing': self.make_room(state=1)
                else: self.service.rooms[self.peers[0]].members.add(self.peers[2])
                with self.assertRaises(Invalid):
                    await self.request(1, 'accept', id=key)

    async def test_no_friend_service_means_explicitly_unsupported(self):
        self.social.friends_api = None
        self.assertEqual(await self.request(0), {'supported': False})

    async def test_offline_friend_cannot_be_invited(self):
        del self.service.clients[self.peers[1]]
        self.assertFalse((await self.request(0))['friends'][0]['online'])
        with self.assertRaises(Invalid):
            await self.request(0, 'send', to=f'{self.peers[1]:016x}')

    async def test_lobby_guest_can_invite_without_becoming_the_host(self):
        room = self.make_room(capacity=3)
        room.members.add(self.peers[2])
        self.service.clients[self.peers[2]].room = room.host
        self.social.lookup = lambda name, account: {202: 'Bob'} if name == 'Eve' else {101: 'Alice',303: 'Eve'}
        await self.request(2, 'send', to=f'{self.peers[1]:016x}')
        key = (await self.request(1))['invites'][0]['id']
        reply = await self.request(1, 'accept', id=key)
        self.assertEqual(reply['accepted']['from'], f'{self.peers[2]:016x}')
        self.assertEqual(struct.unpack_from('>Q', bytes.fromhex(reply['accepted']['room']), 8)[0], self.peers[0])

    async def test_display_only_or_wrong_owner_friend_api_cannot_authorize_invites(self):
        self.social.lookup = Social.lookup.__get__(self.social, Social)
        for data in ({'friends': [{'username': 'Bob'}]}, {'account_id': 303, 'friends': [{'account_id': 202, 'username': 'Bob'}]}):
            with self.subTest(data=data), patch('social.urllib.request.urlopen', return_value=io.BytesIO(json.dumps(data).encode())):
                reply = await self.request(0)
                self.assertFalse(reply['supported'])
                self.assertEqual(reply['reason'], 'friends_identity_upgrade_required')
                self.assertEqual(self.social.invites, {})

    async def test_offline_friend_rename_keeps_the_same_native_player_id(self):
        with self.service.store.db:
            self.service.store.db.execute('UPDATE players SET name=? WHERE account=?', ('OldBob',202))
        friend = (await self.request(0))['friends'][0]
        self.assertEqual(friend, {'name': 'Bob', 'peer': f'{self.peers[1]:016x}', 'online': True})

    async def test_real_http_route_requires_profile_auth_and_accepts_social_json(self):
        server = await asyncio.start_server(self.service.http, '127.0.0.1', 0)
        self.addAsyncCleanup(server.wait_closed)
        self.addCleanup(server.close)
        port = server.sockets[0].getsockname()[1]
        async def post(key):
            reader, writer = await asyncio.open_connection('127.0.0.1', port)
            raw = b'{"action":"poll"}'
            writer.write(b'POST /v2/social HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer ' + key.hex().encode() + b'\r\nContent-Type: application/octet-stream\r\nContent-Length: ' + str(len(raw)).encode() + b'\r\n\r\n' + raw)
            await writer.drain()
            response = await asyncio.wait_for(reader.read(), 3)
            writer.close()
            await writer.wait_closed()
            return response.split(b'\r\n\r\n', 1)
        headers, body = await post(self.keys[0])
        self.assertTrue(headers.startswith(b'HTTP/1.1 200'))
        self.assertEqual(json.loads(body)['friends'][0]['name'], 'Bob')
        headers, body = await post(secrets.token_bytes(32))
        self.assertTrue(headers.startswith(b'HTTP/1.1 400'))
        self.assertNotIn(b'friends', body)


if __name__ == '__main__':
    unittest.main()
