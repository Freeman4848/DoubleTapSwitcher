#!/usr/bin/env python3
"""Prepare an unsigned Ubuntu source package; never install or upload."""
from pathlib import Path
import shutil, subprocess, tarfile, sys
base=Path(__file__).resolve().parents[2]
out=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else base/'apt_dist'; out.mkdir(parents=True,exist_ok=True)
root=out/'double-tap-switcher-1.39'
if root.exists(): raise SystemExit('Source directory already exists; inspect it before rebuilding.')
root.mkdir()
for folder in ['ubuntu_native','icons']:
 for f in (base/folder).iterdir():
  if f.is_file() and f.suffix in ['.cpp','.h','.svg']:
   dest=root/folder/f.name;dest.parent.mkdir(exist_ok=True);shutil.copy2(f,dest)
for name in ['DoubleTapDetector.h']:
 dest=root/'LiteSwitcheV1.1_source'/name;dest.parent.mkdir(exist_ok=True);shutil.copy2(base/'LiteSwitcheV1.1_source'/name,dest)
bridge=root/'gnome_double_tap';bridge.mkdir();shutil.copy2(base/'gnome_double_tap/extension.js',bridge/'extension.js')
shutil.copy2('/usr/share/common-licenses/GPL-2',root/'COPYING')
(root/'LICENSE-NOTICE').write_text('Copyright 2025-2026 Freeman4848. This source release is GPL-2.0-or-later. Earlier MIT grants remain valid.\n')
(root/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.16)
project(double_tap_switcher LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
add_executable(double-tap-switcher ubuntu_native/main.cpp ubuntu_native/bridge_events.h)
target_link_libraries(double-tap-switcher PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::DBus Qt6::Network)
add_executable(double-tap-reader ubuntu_native/worker.cpp)
target_link_libraries(double-tap-reader PRIVATE PkgConfig::SYSTEMD)
add_executable(core-tests ubuntu_native/tests.cpp)
enable_testing()
add_test(NAME detector COMMAND core-tests)
install(TARGETS double-tap-switcher double-tap-reader DESTINATION lib/double-tap-switcher)
''')
# Archive upstream sources before adding Debian-specific packaging.
with tarfile.open(out/'double-tap-switcher_1.39.orig.tar.gz','w:gz') as t:t.add(root,arcname=root.name)
d=root/'debian';(d/'source').mkdir(parents=True)
def write(name,text,mode=None):
 p=d/name;p.write_text(text)
 if mode:p.chmod(mode)
write('source/format','3.0 (quilt)\n')
write('control','''Source: double-tap-switcher
Section: utils
Priority: optional
Maintainer: Freeman4848 <freeman4848.dev@gmail.com>
Build-Depends: debhelper-compat (= 13), cmake, pkgconf, qt6-base-dev, libsystemd-dev, python3:any, python3-gi, gir1.2-gdkpixbuf-2.0
Standards-Version: 4.7.0
Homepage: https://github.com/Freeman4848/DoubleTapSwitcher
Rules-Requires-Root: no

Package: double-tap-switcher
Architecture: amd64
Depends: ${shlibs:Depends}, ${misc:Depends}, systemd, qt6-qpa-plugins, gnome-shell (>= 50), gnome-shell (<< 51), libglib2.0-bin
Description: double-tap keyboard layout switching for GNOME 50
 Native C++ frontend and keyboard reader with a GNOME Shell bridge.
 This build targets Ubuntu 26.04 GNOME Wayland, not every Linux desktop.
''')
write('changelog','''double-tap-switcher (1.39-0ubuntu2) resolute; urgency=medium

  * Initial source-package draft for the common 1.39 release.
  * Package the existing Ubuntu GNOME adapter; no behavior changes intended.

 -- Freeman4848 <freeman4848.dev@gmail.com>  Sun, 08 Nov 2026 12:00:00 +0200
''')
write('rules','''#!/usr/bin/make -f
export DEB_BUILD_MAINT_OPTIONS = hardening=+all
%:
\tdh $@ --buildsystem=cmake

override_dh_auto_install:
\tdh_auto_install
\tpython3 debian/install-assets.py
''',0o755)
write('prerm', '#!/bin/sh\nset -e\n'+(base/'packaging/linux/graceful-exit.sh').read_text()+'\ncase \"${1:-}\" in remove|upgrade|deconfigure) dt_quit_sessions double-tap-switcher double-tap-switcher-cpp-control ;; esac\n'+(base/'packaging/linux/disable-gnome-indicator.sh').read_text().replace('/usr/bin/gnome-extensions', 'gnome-extensions')+'\n#DEBHELPER#\n',0o755)
write('postinst', '#!/bin/sh\nset -e\n#DEBHELPER#\n'+(base/'packaging/linux/gnome-postinstall.sh').read_text().replace('/usr/bin/gnome-extensions', 'gnome-extensions'),0o755)
write('manpages', 'debian/double-tap-switcher.1\n')
write('double-tap-switcher.1', (base/'packaging/apt/double-tap-switcher.1').read_text())
write('copyright','''Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: Double Tap Switcher
Source: https://github.com/Freeman4848/DoubleTapSwitcher

Files: *
Copyright: 2025-2026 Freeman4848
License: GPL-2+
 This source release is licensed under GNU GPL version 2 or any later version.
 On Debian systems the GPL version 2 is in /usr/share/common-licenses/GPL-2.
 Earlier releases under MIT retain their historical permissions.
''')
for name in ['double-tap-switcher.socket','double-tap-switcher@.service']:
 s=(base/'gnome_double_tap/packaging'/name).read_text()
 if name.endswith('.service'):s=s.replace('/usr/bin/python3 -I -B /usr/lib/double-tap-switcher/reader_service.py','/usr/lib/double-tap-switcher/double-tap-reader')
 write(name,s)
# debhelper manages socket lifecycle; do not copy historical shell maintainer scripts.
write('install-assets.py', '''from pathlib import Path
import shutil,json
import gi
gi.require_version('GdkPixbuf','2.0')
from gi.repository import GdkPixbuf
root=Path('debian/double-tap-switcher')
def put(path,data,mode=0o644):
 p=root/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data);p.chmod(mode)
put('usr/bin/double-tap-switcher','#!/bin/sh\\nexport QT_QPA_PLATFORM=xcb\\nexec /usr/lib/double-tap-switcher/double-tap-switcher "$@"\\n',0o755)
for name in ['app','active','inactive']:
 p=root/'usr/lib/double-tap-switcher/icons'/('dt-'+name+'.png');p.parent.mkdir(parents=True,exist_ok=True)
 image=GdkPixbuf.Pixbuf.new_from_file_at_scale('icons/dt-'+name+'.svg',256,256,True);image.savev(str(p),'png',[],[])
put('usr/share/icons/hicolor/scalable/apps/double-tap-switcher.svg',Path('icons/dt-app.svg').read_text())
folder='usr/share/gnome-shell/extensions/double-tap@freeman4848.local/'
put(folder+'extension.js',Path('gnome_double_tap/extension.js').read_text())
put(folder+'metadata.json',json.dumps({'uuid':'double-tap@freeman4848.local','name':'Double Tap Switcher','description':'C++ client and GNOME bridge','shell-version':['50'],'version':13,'version-name':'1.39'}))
for p in Path('icons').glob('dt-*.svg'):put(folder+'icons/'+p.name,p.read_text())
desktop='[Desktop Entry]\\nType=Application\\nName=Double Tap Switcher\\nExec=/usr/bin/double-tap-switcher\\nIcon=double-tap-switcher\\nTerminal=false\\nCategories=Utility;\\n'
put('usr/share/applications/double-tap-switcher.desktop',desktop)
put('etc/xdg/autostart/double-tap-switcher.desktop',desktop.replace('Exec=/usr/bin/double-tap-switcher','Exec=/usr/bin/double-tap-switcher --background')+'OnlyShowIn=GNOME;\\nX-GNOME-Autostart-enabled=true\\n')
''')
subprocess.run(['dpkg-source','-b',str(root)],cwd=out,check=True)
print('Unsigned source draft ready; binary build and clean-VM installation remain untested.')
