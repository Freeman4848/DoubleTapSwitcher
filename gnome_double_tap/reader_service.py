#!/usr/bin/python3
"""Socket-activated keyboard reader. Only double-tap targets leave this process."""
import glob
import json
import os
from pathlib import Path
import select
import socket
import struct
import sys
import termios
import time

# The packaged directory and its modules are root-owned, never user imports.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from evdev_bridge import Detector, EVENT, MODIFIERS
from settings import KEYS, validate


def hidden_terminal_input(uid):
    """Inspect flags only, never read terminal contents or change their mode.

    Conservatively pause for ANY owned canonical no-echo PTY (sudo/read -s).
    Raw-mode editors also disable echo, but must not suspend normal DT usage.
    """
    for path in glob.glob('/dev/pts/[0-9]*'):
        fd = None
        try:
            fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY |
                         os.O_NOFOLLOW)
            if os.fstat(fd).st_uid != uid:
                continue
            flags = termios.tcgetattr(fd)[3]
            if flags & termios.ICANON and not flags & termios.ECHO:
                return True
        except (OSError, termios.error):
            continue
        finally:
            if fd is not None:
                os.close(fd)
    return False


def active_local_user(bus, uid):
    from gi.repository import Gio, GLib
    sessions = bus.call_sync('org.freedesktop.login1', '/org/freedesktop/login1',
        'org.freedesktop.login1.Manager', 'ListSessions', None,
        GLib.VariantType('(a(susso))'), Gio.DBusCallFlags.NONE, 1000, None)
    for _, owner, _, seat, path in sessions.unpack()[0]:
        if owner != uid or not seat:
            continue
        result = bus.call_sync('org.freedesktop.login1', path,
            'org.freedesktop.DBus.Properties', 'GetAll',
            GLib.Variant('(s)', ('org.freedesktop.login1.Session',)),
            GLib.VariantType('(a{sv})'), Gio.DBusCallFlags.NONE, 1000, None)
        props = result.unpack()[0]
        if (props.get('Active') and not props.get('Remote') and
                not props.get('LockedHint') and
                props.get('Type') in ('wayland', 'x11') and
                props.get('Class') == 'user'):
            return True
    return False


def serve(peer, erase=None):
    from gi.repository import Gio
    _, uid, _ = struct.unpack('3i', peer.getsockopt(
        socket.SOL_SOCKET, socket.SO_PEERCRED, struct.calcsize('3i')))
    bus = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)
    if uid == 0 or not active_local_user(bus, uid):
        raise PermissionError('An unlocked local desktop session is required.')
    peer.settimeout(3)
    raw = bytearray()
    while not raw.endswith(b'\n'):
        chunk = peer.recv(1)
        if not chunk or len(raw) >= 2048:
            raise ValueError('Invalid configuration frame.')
        raw.extend(chunk)
    config = validate(json.loads(raw))
    if erase is not None:
        erase.prepare()
    targets = {KEYS[b['key']]: b['layout'] for b in config['bindings']}
    detector = Detector(targets=targets, threshold_ms=config['threshold_ms'])
    devices, known = {}, set()
    allowed, last_check, last_scan = True, time.monotonic(), 0
    peer.settimeout(1)
    erase_until = 0
    try:
        while True:
            now = time.monotonic()
            if now - last_check >= .5:
                new_allowed = active_local_user(bus, uid)
                if not new_allowed or new_allowed != allowed:
                    detector = Detector(targets, config['threshold_ms'])
                allowed, last_check = new_allowed, now
            if now - last_scan >= 2:
                for path in glob.glob('/dev/input/by-path/*-event-kbd'):
                    path = os.path.realpath(path)
                    if path in known:
                        continue
                    try:
                        fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
                        devices[fd] = path
                        known.add(path)
                    except OSError:
                        continue
                last_scan = now
            ready, _, _ = select.select([peer, *devices], [], [], .1)
            if peer in ready:
                # Client EOF immediately stops this instance and closes devices.
                commands = peer.recv(1024)
                if not commands:
                    return
                for command in commands:
                    if command == ord('R'):
                        detector = Detector(targets, config['threshold_ms'])
                        erase_until = 0
                    elif command == ord('D') and erase is not None:
                        permitted = (time.monotonic() <= erase_until and
                            active_local_user(bus, uid) and
                            not any(key[1] in MODIFIERS for key in detector.down) and
                            not (config['protect_input'] and hidden_terminal_input(uid)))
                        erase_until = 0
                        if permitted:
                            erase()
                            peer.sendall(b'{"erased":true}\n')
                        else:
                            peer.sendall(b'{"erase_error":"Deletion cancelled: stale or protected input"}\n')
                    else:
                        raise ValueError('Reconnect to change settings.')
            for fd in ready:
                if fd == peer:
                    continue
                try:
                    data = os.read(fd, EVENT.size * 64)
                except BlockingIOError:
                    continue
                except OSError:
                    data = b''
                if not data:
                    known.discard(devices.pop(fd))
                    os.close(fd)
                    detector = Detector(targets, config['threshold_ms'])
                    continue
                if not allowed or (config['protect_input'] and hidden_terminal_input(uid)):
                    detector = Detector(targets, config['threshold_ms'])
                    erase_until = 0
                    continue
                for offset in range(0, len(data), EVENT.size):
                    sec, usec, kind, code, value = EVENT.unpack_from(data, offset)
                    if kind == 1:
                        if value == 1:
                            erase_until = 0
                        target = detector.feed(fd, code, value, sec + usec / 1e6)
                        if target:
                            erase_until = time.monotonic() + .75
                            peer.sendall((json.dumps({'target': target}) + '\n').encode())
    finally:
        for fd in devices:
            os.close(fd)


if __name__ == '__main__':
    with socket.socket(fileno=0) as connection:
        try:
            serve(connection)
        except Exception as error:
            print(f'Reader stopped: {type(error).__name__}: {error}', file=sys.stderr)
