# Double Tap Switcher 1.2 — GNOME 50 / Wayland

## Installation

From the project root:

```sh
./gnome_double_tap/install.sh
```

Finish the OBS recording first. Installation does not log you out, restart GNOME,
or stop the existing DT. Stop the old terminal reader with Ctrl+C, log out and back
in once, then open **Double Tap Switcher** from Applications. Do not run both readers.
The existing user extension is backed up before being updated. The UUID stays the
same, so there is one DT indicator after the next login. No manual DT removal is
required before upgrading.

## Use

The first launch starts recognition and shows settings. Close the window to leave
recognition running. DT → Settings reopens it. Start / Stop pauses recognition;
Restart Hook reconnects the reader; Exit closes the app and reader connection.
Manual layout selection in DT still works while recognition is stopped.

Defaults: QQ → English, WW → Russian, FF → Ukrainian. Configure up to five distinct
physical keys (Q W E R T A S D F G), their installed GNOME layouts, and a 100–1000 ms
double-tap interval. Save applies settings immediately. No layouts are added and
system shortcuts, IBus engines and GNOME keyboard settings are not overwritten.

The console contains switch results/errors only. Copy All copies its retained
history (up to 2000 lines). It does not log typed text. No network access or upload
is performed by the app. Original active/inactive icons are preserved.

## Keyboard access

Installation needs administrator authorization. Normal launches do not ask for
sudo or a password. A root-owned, systemd socket-activated reader opens keyboard
devices. It checks the connecting user's local unlocked session through logind,
recognizes only the configured double taps and sends only target layout IDs to
the user's application. It does not grab the keyboard or transmit raw keystrokes.
Closing the connection stops its reader instance. The socket remains available.
No user is added to the input group; device permissions remain unchanged.

Unlock/session state is checked every 500 ms and the GNOME bridge also rejects
switches on the lock screen. GNOME's virtual keyboard injects two Backspaces;
this is not transactional with application input. Test cursor movement, text
selection, terminal programs and your usual editors before relying on it widely.
Password fields cannot currently be identified reliably: pause DT before entering
secrets containing a configured repeated letter. Recognition is not claimed to
be harmless in every application. Bluetooth devices lacking a by-path keyboard
entry are not currently discovered. USB hot-plug is checked every two seconds.

## Removal / rollback

```sh
./gnome_double_tap/uninstall.sh
```

This disables DT, stops the app and removes the system package/service. The
prototype copy, its backup and user settings are preserved. The old 0.1.0 package
is retained in dist for manual rollback; stop the new app before restoring it.
The installer is the supported way to upgrade a machine with a user prototype;
installing the .deb alone cannot update that user's shadowing extension copy.

## Verification

### Protected input (1.2-2)

Automatic double-tap deletion and switching are blocked when GNOME reports a
password, PIN, private or hidden-text input. Shell password entries are checked
as well. Unknown IBus contexts are blocked until content metadata arrives;
manual layout selection and system shortcuts remain available. Reopen/refocus a
normal text field if an application does not publish its initial input type.

The reader also pauses while ANY PTY owned by the desktop user has canonical
input enabled and echo disabled (the usual sudo/read -s password mode). It
checks flags only, never reads PTY contents, and clears pending double taps.
This intentionally pauses DT outside that terminal too, until echo returns.
Raw-mode editors are excluded to avoid disabling DT whenever Vim is open.
Applications that hide passwords themselves without publishing metadata, or
use raw-mode terminal prompts, are NOT guaranteed to be detected. This is a
best-effort editing safeguard, not a security boundary: the evdev service still
receives hardware events and discards protected terminal batches.

Reference implementations: [GNOME content metadata](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/50.1/js/misc/inputMethod.js)
and [terminal flags](https://man7.org/linux/man-pages/man3/termios.3.html).
The package is built locally; live browser/sudo behavior needs verification
after installing and reloading the extension (log out/in on Wayland).

## Test Commands

### Release 1.24

- Diagnostics are off by default; errors remain visible. Optional diagnostic
  events contain timing/status only, never typed text or window titles. No log
  file is created by the application. The reader sends no raw key records.
- Protected-input detection is an explicit saved setting, enabled by default.
  Turning it off deliberately permits DT in password prompts; this does not
  make input private from the raw evdev reader. Detection limitations above
  still apply. GNOME shows protected/unknown contexts separately from stopped.
- Context changes reset pending reader pairs. A disconnected reader/startup
  failure retries with 1/2/4/8/16-second backoff; Stop/Exit cancel retries.
- Existing settings gain safe defaults without losing key/layout bindings.
- Requires extension protocol 3. Reload the extension after installation by
  logging out/in on Wayland; do not update during a recording or busy session.
- Only GNOME 50 is supported. KDE, Cinnamon/Xfce and generic X11 backends are
  still future work, not implemented or advertised as compatible here.

Version 1.23 uses new local SVG DT artwork: neutral application, green active
and red stopped icons. Original Lite Switcher artwork is preserved in `icons/`.
Launching the application starts the reader automatically; reopening a stopped
instance also starts it. This does not add desktop-login autostart.

```sh
python3 -m unittest discover -s gnome_double_tap/tests -q
node gnome_double_tap/tests/test_password_guard.cjs
./gnome_double_tap/packaging/build.sh
```

SHA256SUMS is in dist. Only GNOME 50 is declared supported. Reported bridge round
trips of the previous prototype: 1.4–3.1 ms. That measures command exchange, not
visible text latency. This release still needs user testing after login; no live
recording, desktop keyboard settings or installed app is changed by building it.
