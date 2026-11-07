# Local verification — 2026-10-20

Release: 1.2, GNOME 50.1 / Wayland.

- 17 tests pass: default/custom double taps, timeout, repeats, modifiers,
  intervening keys, device separation, triple taps, settings validation and
  persistence, session authorization, socket event delivery and disconnect.
- GTK settings window rendered offscreen. Five bindings, three quick-switch
  buttons, settings save/reload and log clearing verified without a live switch.
- The logind query recognizes the current unlocked local graphical session.
- JavaScript syntax, desktop entry, shell script syntax and systemd unit
  definitions checked successfully. Package builds with dpkg-deb.
- Original PNG icons included; square launcher SVG embeds the original active
  image without stretching or changing the artwork.

No installation, GNOME reload, real keystroke injection or session restart was
performed during this build. OBS and the currently installed prototype were not
modified. The packaged systemd service has not yet been tested as an installed
root service; its event delivery was exercised with a socket pair and simulated
keyboard records. GUI bridge calls in the window test were simulated.

After the recording: install, stop the old terminal reader, log out/in, launch
Double Tap Switcher. Verify EN/RU/UA, custom bindings, Stop/Restart/Exit, reopening
Settings from DT, and Super+Space after Exit in a disposable editor/terminal.
Also verify no password prompt on subsequent launches. Application input deletion
and every editor/password field are not certified; see README limitations.

GitHub publication and VirusTotal submission were not performed.

## Protected-input update: 1.2-2 (2026-10-21)

- 23 Python tests passed, including real disposable PTY echo/canonical toggles,
  automatic recovery, unchanged terminal flags, UID isolation and disappearing PTYs.
- Node tests exercise actual extension methods with mocked Shell interfaces:
  PASSWORD/PIN/PRIVATE/HIDDEN_TEXT, unknown contexts, Shell password entries,
  manual selection and normal automatic switching/deletion recovery.
- Local PTY scan: 200 calls, median 0.031 ms, maximum 0.250 ms; not an end-to-end
  desktop latency measurement.
- Built `dist/double-tap-switcher_1.2-2_all.deb`; live installation, extension
  reload and real browser/sudo verification NOT performed. Installed DT untouched.

## Icons and startup: 1.23

- 25 Python tests passed; first launch and reopening both request hook startup.
- Extension tests passed, including Exit hides / Start restores the indicator.
- Three SVG icons rendered and visually checked at 256 px and 18 px.
- Original icons retained; package rebuilt without installing or reloading Shell.

## Release hardening: 1.24

- 33 Python tests passed, plus actual extension-method tests in Node.
- Added diagnostics opt-in/error retention, settings migration/type checking,
  retry/backoff cancellation, stale callbacks and context reset coverage.
- Disposable PTY/socket tests confirm protected terminal events do not leave
  the worker and pending taps do not cross a protected batch. No real password
  was entered or captured; real browser/sudo detection remains unverified.
- Built `dist/double-tap-switcher_1.24_all.deb`; no installed files, running
  application, user settings, GNOME session or virtual machine were changed.
- Windows portable detector passed ASan/UBSan (12 edge cases, four keys).
  Windows tray/resume code and new ICO resources require native build/runtime
  verification: MinGW is currently unavailable; no new Windows EXE was built.
- End-to-end desktop latency, elevated applications, Windows 10/11 and other
  Linux desktops still require real-environment testing. No publication done.
