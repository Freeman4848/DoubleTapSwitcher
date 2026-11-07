# CachyOS KDE 1.32 - TEST

Uses the shared kubuntu_native C++ KDE adapter and root input worker.
The build generates isolated CachyOS names/settings/socket; other ports unchanged.
Native Arch package, not a converted Debian package. No repository publication.

Build inside CachyOS: bash cachyos_native/build.sh
Package: python3 cachyos_native/package.py
Needs GCC, Qt6 development files/tools, X11/XTest and systemd headers.
Desktop entry: Double Tap Switcher (CachyOS TEST).
GUI password-field protection and sleep/resume are not certified.
Disable the host Ubuntu hook while testing inside VirtualBox.

Live-session conflict: kxkb2locale1 must currently be paused for reliable tests.
See test_reports/cachyos-wayland/REPORT.md; this is not a final compatibility fix.
