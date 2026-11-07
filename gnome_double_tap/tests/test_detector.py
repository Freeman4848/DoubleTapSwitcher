import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from evdev_bridge import Detector

class DetectorTests(unittest.TestCase):
    def tap(self, d, key, t, device=1):
        result = d.feed(device, key, 1, t)
        d.feed(device, key, 0, t + .001)
        return result

    def test_layouts(self):
        for key, target in [(16, 'us'), (17, 'ru'), (33, 'ua')]:
            d = Detector()
            self.assertIsNone(self.tap(d, key, 0))
            self.assertEqual(self.tap(d, key, .2), target)

    def test_timeout(self):
        d = Detector()
        self.tap(d, 16, 0)
        self.assertIsNone(self.tap(d, 16, .251))

    def test_intervening_key(self):
        d = Detector()
        self.tap(d, 16, 0)
        self.tap(d, 30, .1)
        self.assertIsNone(self.tap(d, 16, .2))

    def test_modifiers(self):
        for modifier in [29, 97, 56, 100, 125, 126]:
            d = Detector()
            d.feed(1, modifier, 1, 0)
            self.tap(d, 16, .1)
            self.assertIsNone(self.tap(d, 16, .2))

    def test_repeat(self):
        d = Detector()
        d.feed(1, 16, 1, 0)
        self.assertIsNone(d.feed(1, 16, 2, .1))
        self.assertIsNone(d.feed(1, 16, 1, .2))

    def test_devices_separate(self):
        d = Detector()
        self.tap(d, 16, 0, 1)
        self.assertIsNone(self.tap(d, 16, .1, 2))

    def test_triple(self):
        d = Detector()
        self.tap(d, 16, 0)
        self.assertEqual(self.tap(d, 16, .1), 'us')
        self.assertIsNone(self.tap(d, 16, .2))

if __name__ == '__main__':
    unittest.main()
