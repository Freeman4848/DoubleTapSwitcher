from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app import Application, GLib


class ReleaseTests(unittest.TestCase):
    def test_diagnostics_off_keeps_errors_only(self):
        app = SimpleNamespace(config={'diagnostics': False}, logs=[])
        with patch('builtins.print') as output:
            Application.log(app, 'switch event')
            self.assertEqual(app.logs, [])
            output.assert_not_called()
            Application.log(app, 'Reader disconnected', error=True)
            self.assertEqual(len(app.logs), 1)
            output.assert_called_once()

    def test_diagnostics_opt_in(self):
        app = SimpleNamespace(config={'diagnostics': True}, logs=[])
        with patch('builtins.print'):
            Application.log(app, 'current=us')
        self.assertIn('current=us', app.logs[0])

    def test_stop_cancels_retry_and_stale_callback(self):
        app = Application()
        app.want_running = True
        app.status, app.start_button = Mock(), Mock()
        with patch('app.GLib.timeout_add_seconds', return_value=42) as timer, \
             patch('app.GLib.source_remove') as remove, \
             patch.object(app, 'call'), patch.object(app, 'set_icon'):
            app.schedule_retry()
            timer.assert_called_once_with(1, app.retry_start)
            app.stop()
            remove.assert_called_once_with(42)
            with patch.object(app, 'start') as start:
                self.assertEqual(app.retry_start(), GLib.SOURCE_REMOVE)
                start.assert_not_called()

    def test_retries_back_off_and_stop_after_five(self):
        app = Application()
        app.want_running = True
        app.status, app.start_button = Mock(), Mock()
        delays = []
        with patch('app.GLib.timeout_add_seconds', side_effect=lambda n, cb: delays.append(n) or 42):
            for _ in range(6):
                app.schedule_retry()
                app.retry_source = None
        self.assertEqual(delays, [1, 2, 4, 8, 16])
        self.assertFalse(app.want_running)

    def test_context_change_resets_reader_without_logging(self):
        app = SimpleNamespace(running=True, connection=Mock())
        Application.on_context_changed(app)
        app.connection.sendall.assert_called_once_with(b'R')

    def test_stale_reader_event_does_not_switch(self):
        app = SimpleNamespace(running=True, generation=2, switch=Mock())
        Application.on_target(app, 1, 'ru')
        app.switch.assert_not_called()
