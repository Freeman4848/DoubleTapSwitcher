#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if ! gdbus introspect --session --dest org.gnome.Shell \
    --object-path /org/freeman4848/DoubleTap |
    grep -Fq 'interface org.freeman4848.DoubleTap'; then
  echo 'DT bridge is not loaded. Log out of Ubuntu, log back in, then retry.'
  exit 1
fi
# Privilege is limited to the reader; the D-Bus client stays in the user session.
sudo /usr/bin/python3 -u evdev_bridge.py --read |
  /usr/bin/python3 -u evdev_bridge.py
