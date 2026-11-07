import json
import os
from pathlib import Path
import socket
import sys
import threading
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import reader_service
import settings

class Reply:
    def __init__(self, value):
        self.value = value
    def unpack(self):
        return self.value

class Bus:
    def __init__(self, **properties):
        self.props = dict(Active=True, Remote=False, LockedHint=False,
                          Type='wayland', Class='user')
        self.props.update(properties)
    def call_sync(self, _name, _path, _interface, method, *_args):
        if method == 'ListSessions':
            return Reply(([('session', os.getuid(), 'user', 'seat0', '/session')],))
        return Reply((self.props,))

class ReaderTests(unittest.TestCase):
    def test_protected_terminal_drops_events_and_clears_pair(self):
        server, client = socket.socketpair()
        read_fd, write_fd = os.pipe()
        protected, seen, errors = [True], threading.Event(), []
        def guard(_uid):
            seen.set()
            return protected[0]
        def run():
            try:
                reader_service.serve(server)
            except Exception as error:
                errors.append(error)
        def send(events):
            os.write(write_fd, b''.join(reader_service.EVENT.pack(1, t, 1, 16, value)
                                       for t, value in events))
        try:
            with patch('gi.repository.Gio.bus_get_sync', return_value=Bus()), \
                 patch.object(reader_service, 'active_local_user', return_value=True), \
                 patch.object(reader_service, 'hidden_terminal_input', side_effect=guard), \
                 patch.object(reader_service.glob, 'glob', return_value=['/test/keyboard']), \
                 patch.object(reader_service.os, 'open', side_effect=lambda *_: os.dup(read_fd)):
                client.settimeout(.1)
                client.sendall((json.dumps(settings.DEFAULT) + '\n').encode())
                thread = threading.Thread(target=run, daemon=True)
                thread.start()
                send([(0, 1), (10000, 0), (20000, 1), (30000, 0)])
                self.assertTrue(seen.wait(2))
                with self.assertRaises(socket.timeout):
                    client.recv(1024)
                protected[0] = False
                send([(40000, 1), (50000, 0)])
                with self.assertRaises(socket.timeout):
                    client.recv(1024)
                send([(100000, 1)])
                client.settimeout(2)
                self.assertEqual(json.loads(client.recv(1024)), {'target': 'us'})
                client.shutdown(socket.SHUT_WR)
                thread.join(2)
                self.assertFalse(thread.is_alive())
                self.assertEqual(errors, [])
        finally:
            server.close()
            client.close()
            os.close(read_fd)
            os.close(write_fd)

    def test_socket_delivers_only_recognized_target(self):
        server, client = socket.socketpair()
        read_fd, write_fd = os.pipe()
        errors = []
        def run():
            try:
                reader_service.serve(server)
            except Exception as error:
                errors.append(error)
        try:
            with patch('gi.repository.Gio.bus_get_sync', return_value=Bus()), \
                 patch.object(reader_service, 'active_local_user', return_value=True), \
                 patch.object(reader_service.glob, 'glob', return_value=['/test/keyboard']), \
                 patch.object(reader_service.os, 'open', side_effect=lambda *_: os.dup(read_fd)):
                client.settimeout(2)
                client.sendall((json.dumps(settings.DEFAULT) + '\n').encode())
                thread = threading.Thread(target=run, daemon=True)
                thread.start()
                events = [(0, 16, 1), (10000, 16, 0), (200000, 16, 1)]
                os.write(write_fd, b''.join(reader_service.EVENT.pack(1, t, 1, key, value)
                    for t, key, value in events))
                message = client.recv(1024)
                self.assertEqual(json.loads(message), {'target': 'us'})
                client.shutdown(socket.SHUT_WR)
                thread.join(2)
                self.assertFalse(thread.is_alive())
                self.assertEqual(errors, [])
        finally:
            server.close()
            client.close()
            os.close(read_fd)
            os.close(write_fd)

    def test_active_local_user(self):
        self.assertTrue(reader_service.active_local_user(Bus(), os.getuid()))
        for props in [dict(Active=False), dict(Remote=True), dict(LockedHint=True),
                      dict(Type='tty'), dict(Class='greeter')]:
            self.assertFalse(reader_service.active_local_user(Bus(**props), os.getuid()))

    def test_unknown_user(self):
        self.assertFalse(reader_service.active_local_user(Bus(), os.getuid() + 1))

    def test_denied_before_device_access(self):
        server, client = socket.socketpair()
        try:
            with patch('gi.repository.Gio.bus_get_sync', return_value=Bus()), \
                 patch.object(reader_service, 'active_local_user', return_value=False), \
                 patch.object(reader_service.glob, 'glob') as scan:
                with self.assertRaises(PermissionError):
                    reader_service.serve(server)
                scan.assert_not_called()
        finally:
            server.close()
            client.close()

    def test_disconnect_stops_worker(self):
        server, client = socket.socketpair()
        errors = []
        def run():
            try:
                reader_service.serve(server)
            except Exception as error:
                errors.append(error)
        try:
            with patch('gi.repository.Gio.bus_get_sync', return_value=Bus()), \
                 patch.object(reader_service, 'active_local_user', return_value=True), \
                 patch.object(reader_service.glob, 'glob', return_value=[]):
                client.sendall((json.dumps(settings.DEFAULT) + '\n').encode())
                client.shutdown(socket.SHUT_WR)
                thread = threading.Thread(target=run, daemon=True)
                thread.start()
                thread.join(2)
                self.assertFalse(thread.is_alive())
                self.assertEqual(errors, [])
        finally:
            server.close()
            client.close()
