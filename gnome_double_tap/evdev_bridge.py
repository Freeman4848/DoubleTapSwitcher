#!/usr/bin/env python3
"""Read keyboard events separately; send only detected targets to GNOME."""
import glob
import json
import os
import select
import struct
import sys
import time

TARGETS = {16: 'us', 17: 'ru', 33: 'ua'}
MODIFIERS = {29, 97, 56, 100, 125, 126}
EVENT = struct.Struct('@llHHi')

class Detector:
    def __init__(self, targets=None, threshold_ms=250):
        self.last = None
        self.down = set()
        self.targets = TARGETS if targets is None else targets
        self.threshold = threshold_ms / 1000

    def feed(self, device, code, value, timestamp):
        key = (device, code)
        if value == 0:
            self.down.discard(key)
            return None
        if value == 2:
            self.last = None
            return None
        if value != 1 or key in self.down:
            return None
        self.down.add(key)
        if code not in self.targets or any(k[1] in MODIFIERS for k in self.down):
            self.last = None
            return None
        previous = self.last
        self.last = (device, code, timestamp)
        if previous and previous[:2] == (device, code):
            if 0 <= timestamp - previous[2] <= self.threshold:
                self.last = None
                return self.targets[code]
        return None

def read_devices():
    handles = []
    seen = set()
    for path in glob.glob('/dev/input/by-path/*-event-kbd'):
        actual = os.path.realpath(path)
        if actual in seen:
            continue
        seen.add(actual)
        try:
            fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
            handles.append(fd)
            print(f'keyboard: {path}', file=sys.stderr)
        except OSError as error:
            print(error, file=sys.stderr)
    if not handles:
        sys.exit('No readable keyboards; use run-evdev.sh.')
    detector = Detector()
    try:
        while True:
            readable, _, _ = select.select(handles, [], [])
            for fd in readable:
                data = os.read(fd, EVENT.size * 64)
                for offset in range(0, len(data), EVENT.size):
                    sec, usec, kind, code, value = EVENT.unpack_from(data, offset)
                    if kind == 1:
                        target = detector.feed(fd, code, value, sec + usec / 1e6)
                        if target:
                            print(json.dumps({'target': target}), flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        for fd in handles:
            os.close(fd)

def consume():
    import gi
    gi.require_version('Gio', '2.0')
    from gi.repository import Gio, GLib
    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    for line in sys.stdin:
        target = json.loads(line)['target']
        start = time.monotonic()
        try:
            reply = bus.call_sync('org.gnome.Shell', '/org/freeman4848/DoubleTap',
                'org.freeman4848.DoubleTap', 'Switch',
                GLib.Variant('(sb)', (target, True)), GLib.VariantType('(s)'),
                Gio.DBusCallFlags.NONE, 1000, None)
            print(f'target={target} current={reply.unpack()[0]} '
                  f'roundtrip_ms={(time.monotonic()-start)*1000:.1f}', flush=True)
        except GLib.Error as error:
            print(f'ERROR {error.message}', flush=True)

if __name__ == '__main__':
    if '--read' in sys.argv:
        read_devices()
    else:
        consume()
