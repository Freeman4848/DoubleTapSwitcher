#!/usr/bin/env bash
set -euo pipefail
base=$(cd -- "$(dirname -- "$0")/.." && pwd)
out="$base/ubuntu_native/build"
sdk="${QT_SDK_ROOT:-/tmp/dt-native-sdk/root}"
qt="$sdk/usr/include/x86_64-linux-gnu/qt6"
mkdir -p "$out/icons"
[[ -f "$qt/QtWidgets/QApplication" ]] || { echo 'Qt 6 development headers required.'; exit 1; }
cxx="${CXX:-g++}"
flags=(-std=c++17 -O2 -Wall -Wextra -Werror -fPIC -fstack-protector-strong -D_FORTIFY_SOURCE=2)
includes=(-I"$qt")
for module in QtCore QtGui QtWidgets QtDBus QtNetwork; do includes+=(-I"$qt/$module"); done
LD_LIBRARY_PATH="$sdk/usr/lib/x86_64-linux-gnu" \
  "$sdk/usr/lib/qt6/libexec/moc" "${includes[@]}" \
  "$base/ubuntu_native/bridge_events.h" -o "$out/moc_bridge_events.cpp"
libs=/usr/lib/x86_64-linux-gnu
"$cxx" "${flags[@]}" "${includes[@]}" -I"$base/ubuntu_native" \
 "$base/ubuntu_native/main.cpp" "$out/moc_bridge_events.cpp" \
 "$libs/libQt6Widgets.so.6" "$libs/libQt6Gui.so.6" "$libs/libQt6DBus.so.6" \
 "$libs/libQt6Network.so.6" "$libs/libQt6Core.so.6" \
 -o "$out/double-tap-switcher"
"$cxx" "${flags[@]}" "${includes[@]}" -I"$base/ubuntu_native" \
 "$base/ubuntu_native/lifecycle_tests.cpp" "$out/moc_bridge_events.cpp" \
 "$libs/libQt6Widgets.so.6" "$libs/libQt6Gui.so.6" "$libs/libQt6DBus.so.6" \
 "$libs/libQt6Network.so.6" "$libs/libQt6Core.so.6" \
 -o "$out/lifecycle-tests"
"$cxx" "${flags[@]}" "$base/ubuntu_native/worker.cpp" \
 $(pkg-config --cflags --libs libsystemd) -o "$out/double-tap-reader"
LD_LIBRARY_PATH="$sdk/usr/lib/x86_64-linux-gnu" \
  "$sdk/usr/lib/qt6/libexec/moc" "${includes[@]}" \
  "$base/ubuntu_native/mock_bridge.h" -o "$out/moc_mock_bridge.cpp"
"$cxx" "${flags[@]}" "${includes[@]}" -I"$base/ubuntu_native" \
 "$base/ubuntu_native/mock_bridge.cpp" "$out/moc_mock_bridge.cpp" \
 "$libs/libQt6DBus.so.6" "$libs/libQt6Core.so.6" -o "$out/mock-bridge"
"$cxx" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
 "$base/ubuntu_native/tests.cpp" -o "$out/core-tests"
# Build-time conversion only: neither Python nor GTK is shipped or needed at runtime.
/usr/bin/python3 - "$base" "$out" <<'PY'
from pathlib import Path
import sys, gi
gi.require_version('GdkPixbuf', '2.0')
from gi.repository import GdkPixbuf
base, out = map(Path, sys.argv[1:])
for name in ('app', 'active', 'inactive'):
    pix = GdkPixbuf.Pixbuf.new_from_file_at_scale(str(base / 'icons' / f'dt-{name}.svg'), 256, 256, True)
    pix.savev(str(out / 'icons' / f'dt-{name}.png'), 'png', [], [])
PY
echo 'Built Ubuntu C++ GUI and read-only keyboard worker.'
