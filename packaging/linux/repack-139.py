"""Repackage tested adapters with graceful package-removal lifecycle only."""
from pathlib import Path
import subprocess as S,tempfile,hashlib,re
base=Path(__file__).resolve().parents[2]
helper=(base/'packaging/linux/graceful-exit.sh').read_text()
profiles=[('ubuntu','double-tap-switcher','cpp','1.39'),('mint','double-tap-switcher-mint','mint-test','1.38'),('kubuntu','double-tap-switcher-kubuntu','kubuntu-test','1.37'),('pop','double-tap-switcher-pop','pop-test','1.36')]
for folder,name,control,old in profiles:
 src=base/f'{folder}_native/dist/{name}_{old}_amd64.deb'
 with tempfile.TemporaryDirectory() as tmp:
  root=Path(tmp);S.run(['dpkg-deb','--raw-extract',str(src),str(root)],check=True)
  p=root/'DEBIAN/prerm';original=p.read_text()
  command=f'dt_quit_sessions {name} double-tap-{control}-control' if folder!='ubuntu' else 'dt_quit_sessions double-tap-switcher double-tap-switcher-cpp-control'
  insertion=helper+'\ncase "${1:-}" in remove|upgrade|deconfigure) '+command+' ;; esac\n'
  text=original.replace('set -e','set -e\n'+insertion,1)
  if folder=='ubuntu':text+='\n'+(base/'packaging/linux/disable-gnome-indicator.sh').read_text()
  p.write_text(text);p.chmod(0o755)
  c=root/'DEBIAN/control';text=c.read_text();text=re.sub(r'^Version:.*$', 'Version: 1.39',text,flags=re.M)
  text=re.sub(r'^Depends: ', 'Depends: util-linux, coreutils, ',text,flags=re.M);c.write_text(text)
  S.run(['sh','-n',str(p)],check=True)
  dest=base/f'{folder}_native/dist/{name}_1.39_amd64.deb'
  S.run(['dpkg-deb','--root-owner-group','--build',str(root),str(dest)],check=True,stdout=S.DEVNULL)
  print(dest.relative_to(base))
# Arch: preserve tested executables and adjust lifecycle metadata.
src=base/'cachyos_native/dist/double-tap-switcher-cachyos-1.35-1-x86_64.pkg.tar.zst'
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);S.run(['tar','--zstd','--no-same-owner','-xf',str(src),'-C',str(root)],check=True)
 p=root/'.INSTALL';text=p.read_text()
 command='dt_quit_sessions double-tap-switcher-cachyos double-tap-cachyos-test-control'
 text=helper+'\n'+text.replace('pre_remove() {','pre_remove() {\n '+command+' || return 1',1)
 # Preserve existing validation and stop tray before upgrades too.
 text+='\n# Graceful upgrade cleanup, same request as the desktop Exit action.\n'
 text=re.sub(r'pre_upgrade\(\) \{', 'pre_upgrade() {\n '+command+' || return 1; ',text,count=1)
 p.write_text(text);S.run(['sh','-n',str(p)],check=True)
 p=root/'.PKGINFO';text=re.sub(r'^pkgver = .*$', 'pkgver = 1.39-1',p.read_text(),flags=re.M);text+='depend = util-linux\ndepend = coreutils\n';p.write_text(text)
 dest=base/'cachyos_native/dist/double-tap-switcher-cachyos-1.39-1-x86_64.pkg.tar.zst'
 names=[p.name for p in root.iterdir() if p.name!='.MTREE']
 S.run(['tar','--zstd','--owner=0','--group=0','-cf',str(dest),'-C',str(root),*names],check=True)
 print(dest.relative_to(base))
