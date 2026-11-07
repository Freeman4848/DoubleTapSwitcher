import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import settings
from evdev_bridge import Detector

class SettingsTests(unittest.TestCase):
    def test_defaults_and_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.dict(os.environ, {'XDG_CONFIG_HOME': directory}):
                self.assertEqual(settings.load(), settings.DEFAULT)
                data = {'threshold_ms': 310, 'bindings': [{'key': 'D', 'layout': 'us+intl'}]}
                settings.save(data)
                self.assertEqual(settings.load(), settings.validate(data))
                self.assertEqual(settings.config_path().stat().st_mode & 0o777, 0o600)

    def test_reject_invalid_configurations(self):
        invalid = [[], {}, {'threshold_ms': True, 'bindings': []},
            {'threshold_ms': 99, 'bindings': [{'key': 'Q', 'layout': 'us'}]},
            {'threshold_ms': 250, 'bindings': []},
            {'threshold_ms': 250, 'bindings': [None]},
            {'threshold_ms': 250, 'bindings': [{'key': 'X', 'layout': 'us'}]},
            {'threshold_ms': 250, 'bindings': [{'key': 'Q', 'layout': 'us; rm'}]},
            {'threshold_ms': 250, 'bindings': [{'key': 'Q', 'layout': 'us'}] * 2},
            {'threshold_ms': 250, 'bindings': [{'key': 'Q', 'layout': 'us'}] * 6}]
        for data in invalid:
            with self.subTest(data=data), self.assertRaises(ValueError):
                settings.validate(data)

    def test_custom_key_and_threshold(self):
        detector = Detector({32: 'ua'}, 400)
        self.assertIsNone(detector.feed(1, 32, 1, 0))
        detector.feed(1, 32, 0, .01)
        self.assertEqual(detector.feed(1, 32, 1, .35), 'ua')

    def test_options_migrate_and_validate(self):
        old = {'threshold_ms': 250, 'bindings': [{'key': 'Q', 'layout': 'us'}]}
        self.assertFalse(settings.validate(old)['diagnostics'])
        self.assertTrue(settings.validate(old)['protect_input'])
        self.assertFalse(settings.validate({**old, 'protect_input': False})['protect_input'])
        self.assertTrue(settings.validate({**old, 'diagnostics': True})['diagnostics'])
        for key in ('protect_input', 'diagnostics'):
            with self.assertRaises(ValueError):
                settings.validate({**old, key: 'false'})

    def test_autorepeat_breaks_pair(self):
        detector = Detector()
        detector.feed(1, 16, 1, 0)
        detector.feed(1, 16, 2, .1)
        detector.feed(1, 16, 0, .15)
        self.assertIsNone(detector.feed(1, 16, 1, .2))
