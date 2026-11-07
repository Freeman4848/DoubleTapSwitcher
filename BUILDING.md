# Build from source

Use the matching OS/desktop from README, an x86_64 machine, GCC with C++17,
Qt 6 development tools, pkg-config and the platform dependencies below.
Build scripts are included; a clean-machine dependency recipe and reproducible
build verification remain release work, not completed guarantees.

- Mint/KDE/Pop: Qt6 Core/Gui/Widgets/DBus/Network, X11/XTest, systemd development
  headers, AT-SPI2/GLib development headers. See each build.sh for exact flags.
- Icon conversion: Python3 with GI/GdkPixbuf at build time, not the C++ runtime.
- CachyOS: build.sh uses Arch Qt6 paths and reuses kubuntu_native sources.
- Ubuntu: build.sh expects QT_SDK_ROOT (default /tmp/dt-native-sdk/root),
  containing Qt6 headers and moc, with host Qt6 shared libraries. Its worker
  uses libsystemd. GNOME bridge source is gnome_double_tap/extension.js.

For example, on Mint with prerequisites available:

```bash
bash mint_native/build.sh
bash mint_native/package.sh
```

Equivalent build.sh/package.sh pairs exist for ubuntu_native, kubuntu_native,
pop_native and cachyos_native. Build each on its matching distribution.
For CachyOS the package is an Arch native package, not a .deb.
The gnome_double_tap directory also contains historical Python tooling;
current native clients use the C++ adapters.

After rebuilding all component packages, refresh installer/SHA256SUMS to their
actual hashes before running installer/build-linux-bundle.py. Preserved hashes
refer to the previously published preview, not arbitrary local rebuilds.

Installer-only tests (no sudo or installation):

```bash
bash installer/test-platform.sh
bash installer/test-install-flow.sh
```

Do not publish generated packages as independently audited or signed.
