# Desktop-aware installer

`./install-linux.sh --check` is the default, read-only mode. It detects distro,
release, desktop tokens, X11/Wayland, architecture and GNOME major version;
checks package identity and checksum; never stops applications or installs.

`./install-linux.sh --install` first resolves dependencies with APT simulation,
asks for confirmation, then closes the old application and installs the matching
package without permitting removals. The old Ubuntu entry point delegates here.

Only Ubuntu 26.04 amd64 / GNOME 50 has a selectable C++ package. KDE and
Mint/Xfce/Cinnamon/MATE explicitly stop before APT; the failed experimental
Python KDE package is NOT selected. Other desktops/releases are rejected.

14 classification fixtures passed, including KDE, three Mint desktop variants,
wrong GNOME version, non-Ubuntu GNOME, ARM, tty and deceptive desktop names.
Real detection matched the current Ubuntu/Wayland environment. No installation
or live hook test was performed. Adapter implementation and verification for
other desktops remain outstanding; this is not universal Linux support.
