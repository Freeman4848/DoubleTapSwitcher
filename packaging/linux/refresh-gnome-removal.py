from pathlib import Path
import tempfile,subprocess as S,hashlib
base=Path(__file__).resolve().parents[2]
pkg=base/'ubuntu_native/dist/double-tap-switcher_1.39_amd64.deb'
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);S.run(['dpkg-deb','--raw-extract',str(pkg),str(root)],check=True)
 p=root/'DEBIAN/prerm';text=p.read_text()
 marker='# DT_GNOME_REMOVE_INDICATOR'
 if marker not in text:
  text += '\n'+(base/'packaging/linux/disable-gnome-indicator.sh').read_text()
 p.write_text(text);p.chmod(0o755)
 S.run(['sh','-n',str(p)],check=True)
 S.run(['dpkg-deb','--root-owner-group','--build',str(root),str(pkg)],check=True)
p=base/'installer/SHA256SUMS';lines=p.read_text().splitlines()
relative=pkg.relative_to(base).as_posix()
p.write_text('\n'.join(hashlib.sha256(pkg.read_bytes()).hexdigest()+'  '+relative if l.endswith('  '+relative) else l for l in lines)+'\n')
