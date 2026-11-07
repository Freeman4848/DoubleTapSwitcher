#!/usr/bin/env bash
set -euo pipefail
. /etc/os-release
[[ ${ID:-} == linuxmint && ${XDG_SESSION_TYPE:-} == x11 && ${XDG_CURRENT_DESKTOP^^} == *XFCE* ]] || {
 echo 'Only Linux Mint Xfce / X11 is supported by this test.'; exit 2;
}
command -v double-tap-switcher-mint >/dev/null || { echo 'Install the Mint test package first.'; exit 3; }
work=$(mktemp -d)
setxkbmap -query > "$work/query"
xkbcomp -xkb "$DISPLAY" "$work/map.xkb" 2>/dev/null
cleanup() {
 double-tap-switcher-mint --quit || true
 python3 - "$work/query" <<'PY'
from pathlib import Path
import subprocess,sys
values=dict(line.split(':',1) for line in Path(sys.argv[1]).read_text().splitlines() if ':' in line)
values={k.strip():v.strip() for k,v in values.items()}
args=['setxkbmap']
for name in ['rules','model','layout','variant']:args+=['-'+name,values.get(name,'')]
args+=['-option','']
if values.get('options'):args+=['-option',values['options']]
subprocess.run(args,check=True)
PY
 xkbcomp "$work/map.xkb" "$DISPLAY" 2>/dev/null || true
 rm -rf -- "$work"
}
trap cleanup EXIT
trap 'exit 130' INT TERM
echo 'TEST ONLY: no browser/password-field protection. Use a disposable text document.'
echo 'US/RU/UA layouts are temporary; Exit restores the previous keymap.'
setxkbmap -layout us,ru,ua -variant ',,'
double-tap-switcher-mint
