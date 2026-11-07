from pathlib import Path
import re, shutil, subprocess, tempfile, time
base=Path(__file__).resolve().parent.parent
out=base/'cachyos_native/dist';out.mkdir(exist_ok=True)
build=base/'cachyos_native/build'
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);lib=root/'usr/lib/double-tap-switcher-cachyos';lib.mkdir(parents=True)
 for name in ['double-tap-switcher-cachyos','double-tap-reader-cachyos']:
  shutil.copy2(build/name,lib/name);(lib/name).chmod(0o755)
 shutil.copytree(build/'icons',lib/'icons')
 def write(path,text,mode=0o644):
  p=root/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text);p.chmod(mode)
 write('usr/bin/double-tap-switcher-cachyos','''#!/bin/sh
case "${XDG_SESSION_TYPE:-wayland}" in
 x11) export QT_QPA_PLATFORM=xcb ;;
 *) export QT_QPA_PLATFORM=wayland ;;
esac
exec /usr/lib/double-tap-switcher-cachyos/double-tap-switcher-cachyos "$@"
''',0o755)
 write('usr/share/applications/double-tap-switcher-cachyos.desktop','''[Desktop Entry]
Type=Application
Name=Double Tap Switcher (CachyOS TEST)
Comment=Experimental KDE adapter; test in disposable text only
Exec=/usr/bin/double-tap-switcher-cachyos
Icon=/usr/lib/double-tap-switcher-cachyos/icons/dt-app.png
Terminal=false
Categories=Utility;Accessibility;
''')
 package=(base/'kubuntu_native/package.sh').read_text()
 for suffix,marker in [('socket','SOCKET'),('@.service','SERVICE')]:
  block=re.search(r"<<'"+marker+r"'\n(.*?)\n"+marker,package,re.S).group(1)
  block=block.replace('-kubuntu','-cachyos').replace('Kubuntu','CachyOS')
  write('usr/lib/systemd/system/double-tap-switcher-cachyos'+('.socket' if suffix=='socket' else suffix),block+'\n')
 write('.INSTALL','''pre_install() {
 . /etc/os-release
 [ "$ID" = cachyos ] || { echo 'This test package requires CachyOS.'; return 1; }
}
pre_upgrade() { pre_install; }
post_install() {
 systemctl daemon-reload
 systemctl enable --now double-tap-switcher-cachyos.socket
}
post_upgrade() { post_install; }
pre_remove() {
 systemctl disable --now double-tap-switcher-cachyos.socket || true
 systemctl stop 'double-tap-switcher-cachyos@*.service' || true
}
post_remove() { systemctl daemon-reload; }
''')
 size=sum(p.stat().st_size for p in root.rglob('*') if p.is_file())
 metadata=f'''pkgname = double-tap-switcher-cachyos
pkgbase = double-tap-switcher-cachyos
pkgver = 1.37-1
pkgdesc = Experimental Double Tap Switcher for CachyOS KDE
builddate = {int(time.time())}
packager = Freeman4848
size = {size}
arch = x86_64
'''
 for dep in ['glibc','gcc-libs','qt6-base>=6.11','libx11','libxtst','systemd-libs','systemd','at-spi2-core']:
  metadata+='depend = '+dep+'\n'
 write('.PKGINFO',metadata)
 target=out/'double-tap-switcher-cachyos-1.37-1-x86_64.pkg.tar.zst'
 subprocess.run(['tar','--zstd','--owner=0','--group=0','-cf',str(target),'-C',str(root),'.PKGINFO','.INSTALL','usr'],check=True)
 print(target)
