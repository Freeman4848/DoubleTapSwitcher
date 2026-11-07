# Kubuntu KDE / Wayland 1.32 TEST

Experimental separate adapter: KDE layout D-Bus API, evdev double-tap detection,
restricted Backspace-only uinput device. Active-layout triggers disabled via
KDE layoutChanged notifications. Terminal hidden-input protection is best effort;
GUI password fields are NOT detected. Use disposable text only.

Build: bash kubuntu_native/build.sh
Package: bash kubuntu_native/package.sh
Do not run the host DT hook while testing inside VirtualBox.
