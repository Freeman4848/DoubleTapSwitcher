#!/usr/bin/env bash
set -euo pipefail
base=$(cd -- "$(dirname -- "$0")/.." && pwd)
out="$base/pop_native/build"
mkdir -p "$out/icons"
cxxflags=(-std=c++17 -O2 -Wall -Wextra -Werror -fPIC -fstack-protector-strong -D_FORTIFY_SOURCE=2)
$(qmake6 -query QT_INSTALL_LIBEXECS)/moc "$base/pop_native/main.cpp" -o "$out/main.moc"
g++ -I"$out" "${cxxflags[@]}" "$base/pop_native/main.cpp" \
 $(pkg-config --cflags --libs Qt6Widgets Qt6DBus Qt6Network atspi-2 gobject-2.0) -o "$out/double-tap-switcher-pop"
g++ "${cxxflags[@]}" "$base/pop_native/worker.cpp" \
 $(pkg-config --cflags --libs libsystemd) -o "$out/double-tap-reader-pop"
/usr/bin/python3 - "$base" "$out" <<'PY'
from pathlib import Path
import sys, gi
gi.require_version('GdkPixbuf','2.0')
from gi.repository import GdkPixbuf
base,out=map(Path,sys.argv[1:])
for name in ('app','active','inactive'):
    image=GdkPixbuf.Pixbuf.new_from_file_at_scale(str(base/'icons'/f'dt-{name}.svg'),256,256,True)
    image.savev(str(out/'icons'/f'dt-{name}.png'),'png',[],[])
PY
echo 'Built experimental Pop!_OS GUI and input worker.'
