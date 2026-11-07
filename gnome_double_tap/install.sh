#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
sudo apt install ./dist/double-tap-switcher_1.25_all.deb
# A user prototype shadows the system copy. Back it up, then update it.
dest="${XDG_DATA_HOME:-$HOME/.local/share}/gnome-shell/extensions/double-tap@freeman4848.local"
if [[ -d "$dest" ]]; then
  backup="$dest.backup-$(date +%Y%m%d-%H%M%S)"
  cp -a -- "$dest" "$backup"
  install -m644 extension.js metadata.json "$dest/"
  mkdir -p "$dest/icons"
  install -m644 ../icons/dt-*.svg "$dest/icons/"
fi
/usr/bin/python3 - <<'PY'
from gi.repository import Gio
s = Gio.Settings.new('org.gnome.shell')
uuid = 'double-tap@freeman4848.local'
enabled = s.get_strv('enabled-extensions')
if uuid not in enabled:
    s.set_strv('enabled-extensions', enabled + [uuid])
Gio.Settings.sync()
PY
echo 'Installed. Close the old app/reader and open Double Tap Switcher.'
echo 'If GNOME still has an old DT loaded, log out and back in once.'
