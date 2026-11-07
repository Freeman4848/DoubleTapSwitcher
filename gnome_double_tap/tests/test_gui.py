"""Offscreen GUI test. No desktop switching, reader or clipboard access."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app import Application, GLib, Gtk
import settings

class GuiTests(unittest.TestCase):
    def test_settings_and_log_window(self):
        if not Gtk.init_check()[0]:
            self.skipTest('No display available for offscreen GTK')
        with tempfile.TemporaryDirectory() as directory, \
             patch.dict(os.environ, {'XDG_CONFIG_HOME': directory}):
            app = Application()
            window = Gtk.OffscreenWindow()
            window.set_default_size(650, 660)
            window.add(app.build_content())
            sources = [{'id': id, 'name': id} for id in ['us', 'ru', 'ua']]
            result = GLib.Variant('(s)', (json.dumps(
                {'version': 3, 'sources': sources, 'current': 'us'}),))
            with patch.object(app, 'call', return_value=result):
                self.assertTrue(app.refresh_sources())
                self.assertEqual(len(app.rows), 5)
                self.assertEqual(len(app.quick.get_children()), 3)
                app.threshold.set_value(310)
                app.save_settings()
                self.assertEqual(settings.load()['threshold_ms'], 310)
                self.assertEqual(settings.load()['bindings'], settings.DEFAULT['bindings'])
                app.clear_logs()
                self.assertEqual(app.logs, [])
                window.show_all()
                while Gtk.events_pending():
                    Gtk.main_iteration_do(False)
                self.assertIsNotNone(window.get_pixbuf())
            window.destroy()
