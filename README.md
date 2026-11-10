<div align="center">
<img src="icons/dt-app.svg" width="112" alt="Double Tap Switcher icon">

# Double Tap Switcher
### Your language. A double tap away.

**Formerly Lite-Switcher** · C++ native clients · Linux preview

[Download Linux 1.39](https://github.com/Freeman4848/DoubleTapSwitcher/releases/tag/v1.39-linux) · [VirusTotal archive report](https://www.virustotal.com/gui/file/d9df55d75b1d5e185ece39857b24488712966545c39a1a3fb3208352e7404251) · [GPL-2.0-or-later](LICENSING.md)
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

## Linux 1.39 lifecycle refresh

The release archive was replaced while retaining version 1.39. Its SHA256 is
`d9df55d75b1d5e185ece39857b24488712966545c39a1a3fb3208352e7404251`.
Older downloads have different hashes and lack the final GNOME removal fix.
All native package versions are now 1.39 (Arch: 1.39-1); some adapter UI labels
retain earlier versions. This supersedes the historical component-version table.

### Uninstall on Ubuntu

```bash
sudo apt remove double-tap-switcher
```

The refreshed package requests normal tray exit and disables its GNOME indicator.
The author confirmed installation, password protection and tray removal on Ubuntu.
Removal on the other supported desktops still needs a fresh manual check.
Old installed packages retain their old removal scripts; exit the old tray manually
for the first upgrade if needed. Settings are preserved.

A public Ubuntu PPA is now available; see the installation section below.

## Ubuntu APT-package preview (1.39)

[Download the Ubuntu .deb](https://github.com/Freeman4848/DoubleTapSwitcher/releases/download/v1.39-linux/double-tap-switcher_1.39-0ubuntu2_amd64.deb) · [VirusTotal package report](https://www.virustotal.com/gui/file/dac12a76a67f65f1eea87bc3d8445da9826099180cd56b8f90fd27e597ca2b36)

For Ubuntu 26.04 / GNOME 50 only. Install the downloaded file with:

```bash
sudo apt install ./double-tap-switcher_1.39-0ubuntu2_amd64.deb
```

The current Debian package version is `1.39-0ubuntu2`. Source and binary
packages pass Lintian without errors or warnings; the detector test passed. Installation, startup, password protection and
tray removal were manually confirmed on the author's Ubuntu. This is not
in the official Ubuntu archive; the public PPA is available below and official
archive packaging review has been requested.
The package attempts per-user startup and prints a logout/login instruction
if the GNOME tray cannot be loaded. A computer reboot is not required.
Antivirus results are not a security certification.

### Ubuntu UI refresh (0ubuntu2)

Removed Ko-fi from Ubuntu Settings, tray and command-line action; removed fixed
quick-layout buttons. Layout assignments remain configurable in Settings and
the tray lists system input sources. User confirmed switching and password
protection. Source build and detector test passed; Lintian produced no diagnostics.
Other adapter source changes are not yet included in their published binaries.
The common Linux archive is unchanged and still contains older components.

## Install through APT — Ubuntu PPA

For **Ubuntu 26.04 / GNOME 50 (amd64)**:

```bash
sudo add-apt-repository ppa:freeman4848/double-tap-switcher
sudo apt update
sudo apt install double-tap-switcher
```

[Published packages and build results](https://launchpad.net/~freeman4848/+archive/ubuntu/double-tap-switcher/+packages)

Launchpad built and published **1.39-0ubuntu2** successfully. The author confirmed
APT installation/reinstallation from this PPA and protected-input behavior in
password fields on their Ubuntu desktop. This is a personal repository, **not
inclusion in the official Ubuntu archive** or an independent security audit.
Other distributions/desktops should use their separately documented adapters.
If the GNOME indicator cannot load immediately, log out and back in; no reboot
is required. Future published compatible versions can be delivered through APT.

Official Ubuntu package request: [Launchpad #2173769](https://bugs.launchpad.net/ubuntu/+bug/2173769).
