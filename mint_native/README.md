# Double Tap Switcher Mint 1.32 - TEST ONLY

1.32: XKB layout notifications update the reader's enabled-trigger mask.
Bindings assigned to the current layout are excluded before double-tap detection.
The final layout check remains as a race safeguard. Set `DT_TRACE=1` for
layout-mask and dispatched-trigger diagnostics (no typed text).

Separate experimental C++/Qt6 build for Mint 22.3 Xfce / X11, not the Ubuntu
GNOME application. Built on Mint/Noble to avoid newer Ubuntu ABI dependencies.

GUI/password fields are NOT detected. Use only in disposable plain text inside
this test VM; stop the hook before entering passwords. Terminal protection is
best effort. This is not a production release and does not start at OS login.

The GUI starts the hook at launch. Q/W/F default to US/RU/UA; configure all three
layouts in Mint Keyboard Settings first. Missing layouts are errors, never
silently redirected to US. The input worker requires an unlocked local desktop
session and is read-only; the desktop client uses the existing XTEST keyboard for Backspace. No global layouts are rewritten.

Runtime requires Qt6; compiler/development packages are build-time only.
The package refuses installation outside Mint 22.3 and conflicts with the GNOME
package. It does not install a GNOME extension. GUI settings are separate.

Build on Mint: bash mint_native/build.sh, then bash mint_native/package.sh.
Uninstall: exit the test application, then apt remove double-tap-switcher-mint.

1.31: if the assigned layout is already active, the double-tap is ordinary
text: no switching and no trigger deletion. The check runs before XTest input.
Switching from a different layout still removes the trigger and selects its
assigned language. Query failure cancels deletion rather than guessing state.
