# APT/PPA source-package preparation (local draft)

Run `python3 packaging/apt/prepare.py` once from a clean output location.
It refuses to overwrite an existing source directory. Outputs go to apt_dist.
Nothing is installed, signed or uploaded.

The common release is 1.39. This first Debian source package builds only the
Ubuntu 26.04 GNOME 50 adapter, not the Mint, KDE, Pop or CachyOS adapters.
One source/archive release does not mean one identical binary across desktops.
The published 1.39 archive and its VirusTotal hash are unchanged.

Completed: source-format 3.0 (quilt), Debian control/rules/changelog/copyright,
CMake source builds, generated assets and an unsigned .dsc with source archives.
No prebuilt application binary is used as source-package input.

Pending before upload:
- Install build prerequisites in an isolated build environment, not silently on host.
- Build with debhelper and Qt6 development packages; run tests and lintian.
- Test fresh install, update from 1.33, socket lifecycle and removal in Ubuntu VM.
- Compare assets/lifecycle with the existing preview; the packaging is not yet tested.
- Verify dependency licensing and GNOME extension reload/version handling.
- Generate signed .changes/.dsc using the owner's Launchpad-linked signing key.
- Owner creates Launchpad PPA and explicitly approves one upload.
- Extend packaging to other Debian-based desktop adapters after initial verification.

A PPA performs automated builds; it is not an independent application security audit.
CachyOS uses native Arch packaging, not APT. No new universal compatibility is claimed.
