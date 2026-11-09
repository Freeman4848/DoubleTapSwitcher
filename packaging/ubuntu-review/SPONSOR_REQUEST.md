# Draft: request for initial Ubuntu packaging guidance/review

Subject: Request for initial review: double-tap-switcher 1.39 (GNOME 50)

Hello Ubuntu developers,

I am the author of Double Tap Switcher (formerly Lite-Switcher), a GPL-2.0-or-later
keyboard layout utility. I would like guidance and sponsorship review for possible
inclusion in Ubuntu, rather than only distributing it through a personal PPA.

This source package targets Ubuntu GNOME 50. It includes a C++/Qt frontend,
a GNOME Shell JavaScript bridge and a privileged socket-activated input reader.
Other desktop adapters exist upstream, but are not part of this Ubuntu package.

Repository: https://github.com/Freeman4848/DoubleTapSwitcher
Source descriptor:
https://github.com/Freeman4848/DoubleTapSwitcher/releases/download/v1.39-linux/double-tap-switcher_1.39-0ubuntu2.dsc
The accompanying upstream and Debian source archives are attached to the release.

Completed checks:
- Source build in an Ubuntu 26.04 container; detector test passed.
- Lintian on the source descriptor and binary package: no errors or warnings.
- Manual installation, switching, password protection, startup and tray removal
  checked on my Ubuntu desktop.

Important limitations for review:
- Privileged input access and session IPC need an independent security review.
- Password protection is best effort, not a security boundary.
- Background hidden terminal input can pause switching globally.
- GNOME bridge version/reload handling and current-development-series compatibility
  need further review. The current changelog targets resolute, not a claim that a
  new package can be admitted to that released series through the normal route.
- Clean-VM lifecycle and autopkgtest coverage remain to be expanded.
- The source package is currently unsigned; signing/upload details need coordination.

I understand that a clean Lintian result or antivirus report does not guarantee
acceptance. Would this be appropriate for Ubuntu-specific packaging, and which
current development series and review workflow should I target?
I am willing to maintain the package and address review feedback.

Thank you,
Freeman4848
freeman4848.dev@gmail.com

---
DRAFT ONLY: no bug, email, Matrix message or sponsorship request has been sent.
