#!/usr/bin/env bash
set -eu
uuid=double-tap@freeman4848.local
src=$(cd -- "$(dirname -- "$0")" && pwd)
dest="${XDG_DATA_HOME:-$HOME/.local/share}/gnome-shell/extensions/$uuid"
case "${1:-logs}" in
install)
  mkdir -p "$dest/icons"
  install -m644 "$src/metadata.json" "$src/extension.js" "$dest/"
  install -m644 "$src/../icons/"dt-*.svg "$dest/icons/"
  echo 'Installed. Log out and back in once, then run this script with start.'
  ;;
start)
  if ! gnome-extensions info "$uuid" >/dev/null 2>&1; then
    echo 'GNOME has not discovered the extension. Log out and back in, then retry.'
    exit 1
  fi
  gnome-extensions enable "$uuid"
  gnome-extensions info "$uuid"
  exec journalctl --user -f -o cat --since now --grep='\[DoubleTap\]'
  ;;
stop) gnome-extensions disable "$uuid" ;;
logs) exec journalctl --user -f -o cat --since now --grep='\[DoubleTap\]' ;;
*) echo 'Usage: run.sh install|start|stop|logs'; exit 2 ;;
esac
