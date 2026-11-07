<div align="center">
<img src="icons/dt-app.svg" width="112" alt="Double Tap Switcher icon">

# Double Tap Switcher
### Your language. A double tap away.

**Formerly Lite-Switcher** · C++ native clients · Linux preview

[Download Linux 1.39](https://github.com/Freeman4848/DoubleTapSwitcher/releases/tag/v1.39-linux) · [VirusTotal archive report](https://www.virustotal.com/gui/file/7e29a40de0a9284851ad9b202e480b7f4bba7af3de50e20b4a7b4e71bd89a563) · [GPL-2.0-or-later](LICENSING.md)
</div>

---

## Keep your hands on the keyboard

Double-tap a configured key to select its assigned keyboard layout.
Typical mappings: **QQ → English**, **WW → Russian**, **FF → Ukrainian**.
Use Settings to configure assignments and the double-tap interval.
That interval is the time between presses, not the switching latency.

- Native C++ desktop clients and platform-specific keyboard adapters.
- Tray controls, layout identification and configurable mappings.
- Already-active layouts do not consume the matching trigger.
- Best-effort protected-input guards; no typed-text log.

## Install

Download and extract **DoubleTapSwitcher-1.39-Linux-x86_64.tar.gz** from Releases.
In the extracted directory, check compatibility without changing the system:

```bash
bash install-linux.sh --check
```

Then install as your normal desktop user:

```bash
bash install.sh
```

The installer requests administrative approval when needed. Read the included
`TEST_STATUS.txt` first. Follow any logout/login instructions for GNOME.
Do not run the desktop application as root.

## Tested profiles

| Distribution / desktop | Coverage | Component |
| --- | --- | --- |
| Ubuntu 26.04 / GNOME 50 | Wayland | 1.33 |
| Mint 22.3 / Xfce | X11, GUI password and hidden-terminal checks | 1.38 |
| Kubuntu 26.04 / KDE | Wayland and XWayland; older core X11 checks | 1.37 |
| Pop!_OS 24.04 / legacy COSMIC | Wayland and XWayland | 1.36 |
| CachyOS / KDE | Wayland and XWayland, conditional Live-helper compatibility | 1.35-1 |

**One download, separate native adapters.** The bundle is version 1.39;
component versions intentionally differ. This does not support every Linux
configuration. XWayland client tests are not native X11-session tests.
Windows is not included in this release.

## Security and limitations

Password protection relies on accessibility and terminal metadata and is not
a security boundary. Unknown controls, custom applications and focus races
require further testing; no independent security certification is claimed.
Background hidden-input terminals can pause the whole hook. Sleep/login recovery
coverage is incomplete; native KDE X11 password tests remain pending.
CachyOS Live's conflicting layout helper requires temporary suspension.

The linked VirusTotal report concerns the **archive**, not a certificate for
every executable inside it. SHA256 confirms integrity, not publisher identity.
Packages are unsigned. Disable the host DT when testing another DT in a VM.

## Source and build

See [BUILDING.md](BUILDING.md). The source tree includes platform adapters,
shared headers, packaging scripts and tests; generated binaries are release assets.
Build on the matching target distribution. Builds are not claimed bit-for-bit
reproducible against the already published preview binaries.

## License

This source release is **GPL-2.0-or-later**: GNU GPL version 2 or any later
version, at the recipient's option. See [LICENSE](LICENSE) and
[licensing notes](LICENSING.md), including historical MIT distributions.

## Contact and support

[Report an issue](https://github.com/Freeman4848/DoubleTapSwitcher/issues) ·
[Email](mailto:freeman4848.dev@gmail.com) ·
[Support on Ko-fi](https://ko-fi.com/freeman4848)

Please include OS, desktop, session type and component version in bug reports.
Never include real passwords or private keyboard input.
