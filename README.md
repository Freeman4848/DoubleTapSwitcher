# Double Tap Switcher

Formerly **Lite-Switcher**.

Switch keyboard layouts by double-tapping a configured key.

## Linux 1.39 preview

Download the Linux archive from Releases, extract it, and run `bash install.sh`
as your desktop user. Administrative approval is requested for package installation.
Run `bash install-linux.sh --check` for a read-only compatibility check.

One distribution bundle selects separate native adapters, not one universal binary.
Supported profiles are limited to the tested OS/desktop combinations described in
`TEST_STATUS.txt` inside the archive. Other Linux environments are not certified.
Component package versions differ intentionally; Ubuntu remains 1.33.

Password protection is best effort and depends on accessibility metadata.
Known limitations include global pauses from background hidden-input terminals,
incomplete suspend/login coverage, and a conflicting CachyOS Live layout helper.
Read the included test status before installing. SHA256 verifies integrity, not
publisher identity. Packages are unsigned; no independent security certification.

Windows is not included in this release. Source publication and APT/PPA packaging
are planned separately. No source license is granted by this placeholder README.
