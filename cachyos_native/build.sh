#!/bin/bash
set -euo pipefail
base=$(cd "$(dirname "$0")/.." && pwd)
out="$base/cachyos_native/build"
mkdir -p "$out/icons"
python3 - "$base" "$out" <<'PY'
from pathlib import Path
import sys
base,out=map(Path,sys.argv[1:])
s=(base/'kubuntu_native/main.cpp').read_text().replace('Kubuntu','CachyOS').replace('-kubuntu','-cachyos').replace('kubuntu-test','cachyos-test')
(out/'main.cpp').write_text(s)

PY
flags=(-std=c++17 -O2 -Wall -Wextra -Werror -fPIC -fstack-protector-strong -D_FORTIFY_SOURCE=2)
includes=(-I/usr/include/qt6 -I"$base/native_common" -I/usr/include/at-spi-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include)
for module in QtCore QtGui QtWidgets QtDBus QtNetwork; do includes+=(-I"/usr/include/qt6/$module"); done
/usr/lib/qt6/moc "${includes[@]}" "$out/main.cpp" -o "$out/main.moc"
g++ "${flags[@]}" "${includes[@]}" -I"$base/kubuntu_native" "$out/main.cpp" -o "$out/double-tap-switcher-cachyos" -lQt6Widgets -lQt6Gui -lQt6Core -lQt6DBus -lQt6Network -lX11 -lXtst -latspi -lgobject-2.0 -lglib-2.0
g++ "${flags[@]}" -I"$base/kubuntu_native" -I"$base/LiteSwitcheV1.1_source" "$base/kubuntu_native/worker.cpp" -o "$out/double-tap-reader-cachyos" -lsystemd
cp "$base/cachyos_native/assets/"*.png "$out/icons/"
echo 'Built CachyOS KDE test adapter using shared KDE sources.'
