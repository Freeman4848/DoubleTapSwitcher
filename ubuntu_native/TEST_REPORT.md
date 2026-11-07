# Ubuntu C++ 1.30

User confirmed 1.29 works at login and after sleep. Rollback package and checksum
are preserved under ubuntu_native/dist/rollback-1.29; local source tag is
ubuntu-1.29-working (4b7629d). No installed files were changed during this build.

Settings now offers Green (default), Dark (near-black) and Light (soft off-white).
Legacy System selection falls back to Green. Theme is persisted with Save.
The tray uses three individually bordered PopupMenuSection groups: languages,
hook controls, and support/About/Exit. Approved icon artwork is unchanged.

Verified: warnings-as-errors build; ASan/UBSan core tests; frontend lifecycle,
three palettes, preference persistence and report privacy; actual JS group
creation and item placement with mocks, password/tray actions; screenshots;
platform/installer flow tests; package checksum and APT dry-run. GNOME frame
rendering needs a live login with the updated extension; no live installation,
keyboard injection, session restart or VM changes were performed.

## Historical Checks: Ubuntu C++ 1.29

Changes: main icon without the two dashes; thicker active/inactive DT artwork;
layouts first in the tray menu, a separator before hook controls, Exit last.
Settings supports System / Light / Dark, defaults to System, and follows portal
appearance changes. Save Settings persists the appearance choice.

Activity logging and the diagnostics console have been removed. Legacy
diagnostics=true is ignored, SetOptions always receives false, the Shell bridge
does not log switches, and the reader does not print custom errors. Only the
latest error is displayed in the Settings window; no error history is stored.
Systemd/GNOME may still emit their own service or process lifecycle messages.

Report a bug opens a mail draft to freeman4848.dev@gmail.com, containing only
the static app version and blank reproduction fields. Nothing is sent by DT,
no logs are attached. Support opens https://ko-fi.com/freeman4848, copied from
the original Windows .rc resource. Both actions are available in Settings and
the tray menu.

Installer displays installed/available versions, uses an _apt-readable temporary
package copy, authenticates before stopping DT, attempts recovery on APT failure
and starts the installed app in the background. GNOME menu changes still require
logout/login once. No live installation is performed during development.

Checks: warnings-as-errors build, ASan/UBSan core tests, frontend lifecycle/theme
and persistence tests on isolated D-Bus/config, light/dark screenshot inspection,
JS password/tray tests, installer syntax/platform checks and five mocked install
flows (success, decline, preflight/auth/install failure), package checksum and
APT simulation. No real suspend/login, mail sending or donation action tested.

## Historical Checks: Ubuntu C++ 1.28 (2026-10-25)

Scope remains Ubuntu 26.04 amd64 / GNOME Shell 50. No added runtime dependencies.

Changes:
- XDG login autostart runs `--background`, without opening Settings.
- `--start` is dispatched as a hook action, not a Settings request.
- Explicit manual Stop survives suspend/resume, duplicate background launches
  and opening Settings. A new app process starts enabled as requested.
- logind PrepareForSleep closes stale input connections; resume reconnects
  after one second. Reader/bridge failures retry indefinitely with capped
  1/2/4/8/16-second backoff, rather than permanently disabling the hook.
- A five-second health check reasserts desired bridge state only while enabled;
  it does not reopen the window or override explicit Stop.
- Existing controls preserved in a redesigned, scrollable ivory/emerald UI.
- Approved main/active/inactive SVG artwork is packaged as PNG/SVG assets.
- Existing diagnostic preference, bounded console and error logging retained.

Verification: warnings-as-errors build; ASan/UBSan core tests; actual frontend
  lifecycle tests with simulated backend on an isolated D-Bus session; offscreen
  GUI screenshot inspection; actual JS protected-input tests; 14 installer
  classifications; package identity/checksums and APT dry-run.

Lifecycle cases: hidden Start and toggle, manual Stop, enabled suspend/resume,
  stopped suspend/resume, Start while sleeping, duplicate background launch,
  Settings reopening without enabling a stopped hook, twelve successive retries.

Limits: no installation, logout/login or physical suspend performed by the
  assistant; no live key injection. Actual GNOME autostart and resume behavior
  must still be checked after the user installs this build. The running DT,
  user settings and recording applications were not modified.

## Historical Checks: Ubuntu C++ 1.26

Scope: Ubuntu amd64 / GNOME Shell 50 only. Python is not shipped or required by
this package; Python was used as a local build tool. C++/Qt GUI and C++ isolated
read-only evdev worker use the existing JavaScript GNOME bridge (protocol 3).
No uinput device, keyboard-map replacement or external command per switch.

Verified locally:
- GUI and worker compiled with warnings treated as errors.
- Native protocol/detector checks passed under AddressSanitizer and UBSan.
- Offscreen GUI launch/exit passed on an isolated D-Bus session.
- APT simulation on this Ubuntu accepted the 1.26 package: one package upgraded,
  zero newly installed packages, zero removals, no unresolved dependencies.

NOT verified: installation, actual live keyboard capture, desktop switching,
GNOME tray signal handling, password fields, focus races or visible end-to-end
latency. The running installed DT and user configuration were not modified.
Existing Python-format JSON settings are read; Save writes compatible JSON with
owner-only permissions. Installer closes the old app only when the user runs it.

Only use as a local test build until live behavior is verified. Source remains
separate from the working Python application for rollback. No signed repository
or public upload was created.

## 1.27: terminal guard and readable tray icon

- User confirmed C++ 1.26 switching works; sudo protection did not.
- This machine uses sudo-rs 0.2.13. Its password reader clears ECHO and ICANON
  and sets ECHONL; the old ICANON-only condition missed that mode.
- New guard covers canonical hidden input and raw hidden input with ECHONL.
  For other raw authentication modes, a recognized pair additionally checks
  foreground sudo/su/passwd process metadata against the owned terminal device.
  No terminal contents, password strings or command arguments are read.
- Ordinary raw shells/editors are not classified as passwords just for disabling
  echo. Foreground interactive sudo sessions can conservatively pause DT while
  their terminal remains raw/no-echo; this is not universal password detection.
- Real disposable PTY tests cover echo/canonical/ECHONL combinations, unchanged
  flags and UID isolation; process-stat fixtures cover foreground/background,
  stopped processes, ordinary shells/editors and malformed metadata.
- GNOME tray uses solid green/red DT icons and an adjacent EN/RU/UA label.
  Label mapping and existing bridge guard tests pass without live input.
- Windows tray/layout work is deferred and recorded in WINDOWS_NEXT.md.
- A real sudo password prompt and live GNOME reload still require user testing;
  the installed application was not stopped or replaced during development.
- Final 1.27 checks passed: ASan/UBSan native tests, JS bridge/label tests,
  offscreen GUI launch/exit, 14 installer fixtures and read-only real detection.
  APT simulation accepts the upgrade with zero new packages and zero removals.

Reference: https://github.com/trifectatechfoundation/sudo-rs/blob/v0.2.13/src/pam/rpassword.rs
