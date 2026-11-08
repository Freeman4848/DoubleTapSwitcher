# Linux 1.39 lifecycle packaging refresh

New prerm/pre_remove and upgrade hooks send the same --quit control request as
normal Exit, as the owner of each active /run/user session. No kill -9, no root
GUI launch, no stored input. Session-user requests use fixed installed binary
paths. A five-second timeout aborts package removal if the tray does not exit.

The existing tested Mint/KDE/Pop/CachyOS executables are preserved; Ubuntu was
rebuilt with version labels 1.39. Package versions are 1.39 (Arch 1.39-1).
Some existing adapter UI labels retain their historical component version.

Existing installations still have their old removal scripts: installing this
refresh cannot retroactively alter a script already executing from an older
package. Exit the old tray manually for that first upgrade if needed.

All rewritten shell hooks pass sh -n; platform classification (20 cases) and
mock installer flows (7 cases) pass. Actual GUI removal across all desktops is
not yet retested. Archive hashes and all package hashes are refreshed.
The formerly published archive is preserved in linux_dist/archive-before-graceful-exit.

repack-139.py is a one-shot release preparation helper, not an installation tool.
Do not rerun on its output; it expects the original component packages.
Nothing is uploaded automatically. Windows artifacts are unchanged.
