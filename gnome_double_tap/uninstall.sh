#!/usr/bin/env bash
set -euo pipefail
gnome-extensions disable double-tap@freeman4848.local || true
if [[ -x /usr/bin/double-tap-switcher ]]; then
  /usr/bin/double-tap-switcher --quit || true
fi
sudo apt remove double-tap-switcher
echo 'Removed. The disabled prototype and saved settings are preserved.'
