import os
from pathlib import Path
import pty
import sys
import termios
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from reader_service import hidden_terminal_input


class PasswordGuardTests(unittest.TestCase):
    def setUp(self):
        self.master, self.slave = pty.openpty()
        self.path = os.ttyname(self.slave)
        self.scan = patch('reader_service.glob.glob', return_value=[self.path])
        self.scan.start()

    def tearDown(self):
        self.scan.stop()
        os.close(self.slave)
        os.close(self.master)

    def mode(self, echo, canonical=True):
        attrs = termios.tcgetattr(self.slave)
        attrs[3] = (attrs[3] | termios.ICANON) if canonical else (attrs[3] & ~termios.ICANON)
        attrs[3] = (attrs[3] | termios.ECHO) if echo else (attrs[3] & ~termios.ECHO)
        termios.tcsetattr(self.slave, termios.TCSANOW, attrs)

    def test_hidden_prompt_and_recovery(self):
        self.mode(True)
        self.assertFalse(hidden_terminal_input(os.getuid()))
        self.mode(False)
        before = termios.tcgetattr(self.slave)
        self.assertTrue(hidden_terminal_input(os.getuid()))
        self.assertEqual(termios.tcgetattr(self.slave), before)
        self.mode(True)
        self.assertFalse(hidden_terminal_input(os.getuid()))

    def test_raw_editor_not_password_prompt(self):
        self.mode(False, canonical=False)
        self.assertFalse(hidden_terminal_input(os.getuid()))

    def test_other_user_not_inspected(self):
        self.mode(False)
        self.assertFalse(hidden_terminal_input(os.getuid() + 1))

    def test_disappearing_terminal(self):
        with patch('reader_service.os.open', side_effect=FileNotFoundError):
            self.assertFalse(hidden_terminal_input(os.getuid()))
