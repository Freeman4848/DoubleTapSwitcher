import json
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app import Application, GLib, Gio

class BootstrapTests(unittest.TestCase):
    def test_reopening_stopped_app_starts_hook(self):
        events = []
        app = SimpleNamespace(window=SimpleNamespace(present=lambda: events.append('present')),
                              start=lambda: events.append('start'))
        Application.do_activate(app)
        self.assertEqual(events, ['start', 'present'])

    def test_first_launch_starts_hook(self):
        app = Application()
        bus = SimpleNamespace(signal_subscribe=Mock(return_value=1))
        with patch('app.Gtk.ApplicationWindow') as window, \
             patch('app.Gio.bus_get_sync', return_value=bus), \
             patch.object(app, 'build_content', return_value=None), \
             patch.object(app, 'hold'), patch.object(app, 'start') as start:
            app.do_activate()
            start.assert_called_once_with()
            window.return_value.show_all.assert_called_once_with()

    def test_missing_bridge_enables_extension_and_retries(self):
        app = Application()
        app.bus = SimpleNamespace(call_sync=Mock(return_value=GLib.Variant('(b)', (True,))))
        missing = GLib.Error.new_literal(Gio.dbus_error_quark(), 'Object missing',
                                        Gio.DBusError.UNKNOWN_METHOD)
        state = {'version': 3, 'sources': [], 'current': 'us'}
        with patch.object(app, 'call', side_effect=[missing, GLib.Variant('(s)', (json.dumps(state),))]):
            self.assertEqual(app.bridge_state(), state)
        self.assertEqual(app.bus.call_sync.call_args.args[3], 'EnableExtension')

    def test_failed_enable_reports_login_requirement(self):
        app = Application()
        app.bus = SimpleNamespace(call_sync=Mock(return_value=GLib.Variant('(b)', (False,))))
        missing = GLib.Error.new_literal(Gio.dbus_error_quark(), 'Object missing',
                                        Gio.DBusError.UNKNOWN_METHOD)
        with patch.object(app, 'call', side_effect=missing):
            with self.assertRaisesRegex(ValueError, 'Log out'):
                app.bridge_state()
