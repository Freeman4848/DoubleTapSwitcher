from pathlib import Path
import hashlib, json, shutil, subprocess, tarfile, tempfile
base=Path(__file__).resolve().parent.parent
version='1.39';name=f'DoubleTapSwitcher-{version}-Linux-x86_64'
out=base/'linux_dist';out.mkdir(exist_ok=True)
profiles=['ubuntu-gnome50','mint-xfce','kubuntu-kde','pop-cosmic','cachyos-kde']
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp)/name;root.mkdir();components=[]
 def copy(relative):
  src=base/relative;dest=root/relative;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
 for path in ['install-linux.sh','installer/platform.sh','installer/releases.sh','installer/SHA256SUMS']:
  copy(path)
 for profile in profiles:
  cmd='source installer/releases.sh; DT_ADAPTER="$1"; dt_release; printf "%s\\n%s\\n%s\\n" "$DT_PACKAGE" "$DT_RELEASE" "$DT_FILE"'
  package,release,path=subprocess.check_output(['bash','-c',cmd,'bash',profile],cwd=base,text=True).splitlines()
  copy(path);components.append({'adapter':profile,'package':package,'version':release,'file':path})
 (root/'install.sh').write_text('#!/bin/bash\nset -euo pipefail\ncd -- "$(dirname -- "$0")"\nexec bash ./install-linux.sh --install\n');(root/'install.sh').chmod(0o755)
 status='''Double Tap Switcher Linux bundle 1.39 - RELEASE CANDIDATE

Lifecycle update: graceful tray exit requested before package removal/upgrade.
Live removal tests for this packaging update remain pending.

One installer selecting separate native adapters; NOT a single universal binary.
Ubuntu 26.04 GNOME 50 Wayland: prior live tests; 1.39 package rebuilt and mock/core tested, not installed on the host.
Mint 22.3 Xfce X11: 1.39 packaging over tested 1.38 adapter; switching, protected GUI and hidden-terminal tests passed.
Kubuntu 26.04 KDE: 1.39 packaging over tested 1.37 adapter; protected-input tests passed in Wayland and XWayland; native X11 session pending.
Pop!_OS 24.04 legacy COSMIC: 1.39 packaging over tested 1.36 adapter; protected-input tests passed in Wayland and XWayland.
CachyOS KDE: 1.39 packaging over tested 1.35 adapter; protected-input tests passed in Wayland/XWayland and hidden TTY; kxkb2locale1 remains paused.

CachyOS helper compatibility is NOT fixed: installer asks before temporarily pausing it.
Newer COSMIC protocol versions, other desktops and CPU architectures are not certified.
Password guards depend on accessibility metadata and are not universally certified.
Background hidden terminals can pause the entire hook; that issue is still open. Login/sleep recovery needs review.
Pop VM sleep hung. Saved VM state is not an OS suspend test.
Disable the host DT hook when testing DT inside VirtualBox.
Component version numbers are preserved; bundle version is independent.
SHA256 checks integrity, not publisher identity. No signing or external publication.

Check without changes: bash ./install-linux.sh --check
Install as desktop user: bash ./install.sh
'''
 (root/'TEST_STATUS.txt').write_text(status)
 (root/'release.json').write_text(json.dumps({'bundle_version':version,'status':'release-candidate','password_guard_missing':[],'architecture':'x86_64','components':components,'conditional_adapter':'cachyos-kde'},indent=2)+'\n')
 checks=[]
 for p in sorted(root.rglob('*')):
  if p.is_file():checks.append(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(root).as_posix()+'\n')
 (root/'SHA256SUMS').write_text(''.join(checks))
 subprocess.run(['sha256sum','-c','--status','SHA256SUMS'],cwd=root,check=True)
 target=out/(name+'.tar.gz')
 with tarfile.open(target,'w:gz') as archive:archive.add(root,arcname=name)
 with tarfile.open(target,'r:gz') as archive:
  files={m.name:m for m in archive.getmembers() if m.isfile()}
  for line in checks:
   expected,relative=line.rstrip().split('  ',1)
   assert hashlib.sha256(archive.extractfile(files[name+'/'+relative]).read()).hexdigest()==expected
 digest=hashlib.sha256(target.read_bytes()).hexdigest()
 target.with_suffix('.gz.sha256').write_text(digest+'  '+target.name+'\n')
 print(f'Built and verified: {target} ({target.stat().st_size} bytes, {len(checks)} files)')
