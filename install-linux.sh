#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
base=$(cd -- "$(dirname -- "$0")" && pwd)
source "$base/installer/platform.sh"
source "$base/installer/releases.sh"
mode=${1:---check}
[[ $# -le 1 && ( "$mode" == --check || "$mode" == --install ) ]] || {
    echo 'Usage: ./install-linux.sh [--check|--install]'; exit 2;
}
supported=0
dt_detect && supported=1
printf 'System: %s %s; desktop=%s; session=%s; architecture=%s\n' \
    "$DT_DISTRO" "$DT_VERSION" "$DT_DESKTOP" "$DT_SESSION" "$DT_ARCH"
printf 'Adapter: %s\n%s\n' "$DT_ADAPTER" "$DT_REASON"
[[ "$supported" == 1 ]] || { echo 'Nothing installed or changed.'; exit 3; }
dt_release
if [[ "$DT_ADAPTER" == mint-xfce ]]; then echo "WARNING: this Mint component has no GUI password protection; testing only."; fi
package="$base/$DT_FILE"
[[ -f "$package" ]] || { echo 'Matching package is missing; nothing installed.'; exit 4; }
checks=$(awk -v file="$DT_FILE" '$2 == file { print }' "$base/installer/SHA256SUMS")
[[ -n "$checks" ]] && (cd "$base" && printf '%s\n' "$checks" | sha256sum -c --status) || {
    echo 'Checksum mismatch or missing manifest entry; nothing installed.'; exit 4;
}
installed=''
if [[ "$DT_ADAPTER" == cachyos-kde ]]; then
    read -r name version < <(pacman -Qp "$package" 2>/dev/null)
    [[ "$name" == "$DT_PACKAGE" && "$version" == "$DT_RELEASE" ]] || exit 4
    installed=$(pacman -Q "$DT_PACKAGE" 2>/dev/null | awk '{print $2}' || true)
else
    [[ $(dpkg-deb -f "$package" Package) == "$DT_PACKAGE" && \
       $(dpkg-deb -f "$package" Version) == "$DT_RELEASE" && \
       $(dpkg-deb -f "$package" Architecture) == "$DT_ARCH" ]] || {
        echo 'Package identity does not match; nothing installed.'; exit 4;
    }
    value=$(dpkg-query -W -f='${db:Status-Status} ${Version}' "$DT_PACKAGE" 2>/dev/null || true)
    [[ "$value" != installed\ * ]] || installed=${value#installed }
fi
echo "Package: $package"
echo "Installed: ${installed:-not installed}; available: $DT_RELEASE"
if [[ "$mode" == --check ]]; then
    echo 'Detection only: no applications stopped, no installation performed.'
    exit 0
fi
[[ $EUID != 0 ]] || { echo 'Run as the desktop user, without sudo.'; exit 2; }
if [[ -n "$installed" ]]; then
    if [[ "$DT_ADAPTER" == cachyos-kde ]]; then comparison=$(vercmp "$installed" "$DT_RELEASE")
    elif dpkg --compare-versions "$installed" ge "$DT_RELEASE"; then comparison=0
    else comparison=-1; fi
    if (( comparison >= 0 )); then echo 'This version or a newer version is already installed; no changes.'; exit 0; fi
fi
# Private homes must not force APT to bypass its _apt download sandbox.
staging=$(mktemp -d /tmp/double-tap-install.XXXXXX)
trap 'rm -rf -- "$staging"' EXIT
chmod 755 "$staging"
install -m644 "$package" "$staging/$(basename "$package")"
package="$staging/$(basename "$package")"
if [[ "$DT_ADAPTER" == cachyos-kde ]]; then pacman -U --print "$package"
else apt-get --simulate --no-remove --no-install-recommends install "$package"; fi
pause_sync=0
if [[ "$DT_ADAPTER" == cachyos-kde ]] && systemctl --user is-active --quiet app-kxkb2locale1@autostart.service; then
    echo 'WARNING: the Live layout-sync helper conflicts with DT. This build is conditional, not certified with that helper running.'
    read -r -p 'Temporarily pause that user service for testing? [y/N] ' answer
    [[ "$answer" == y || "$answer" == Y ]] || exit 0
    pause_sync=1
fi
if [[ "$DT_ADAPTER" == pop-cosmic ]]; then
    echo 'WARNING: this adapter was tested with the legacy COSMIC compositor only.'
fi
if [[ "$DT_ADAPTER" != ubuntu-gnome50 ]]; then
    echo 'Experimental adapter: GUI password fields and login/sleep recovery are not certified.'
fi
read -r -p 'Install this package and close the old DT? [y/N] ' confirm
[[ "$confirm" == y || "$confirm" == Y ]] || exit 0
sudo -v
was_running=0
if pgrep -u "$(id -u)" -f "^/usr/lib/$DT_PACKAGE/$DT_CLIENT( |$)" >/dev/null; then
    was_running=1; "$DT_CLIENT" --quit
fi
if [[ "$DT_ADAPTER" == cachyos-kde ]]; then
    if ! sudo pacman -U --needed --noconfirm "$package"; then
        if (( was_running )); then "$DT_CLIENT" >/dev/null 2>&1 & fi
        echo 'Installation failed; no helper service was paused.'; exit 1
    fi
    if (( pause_sync )); then
        systemctl --user stop app-kxkb2locale1@autostart.service
        echo 'Restore Live sync later: systemctl --user start app-kxkb2locale1@autostart.service'
    fi
elif ! sudo apt-get -y --no-remove --no-install-recommends install "$package"; then
    if (( was_running )); then "$DT_CLIENT" --background >/dev/null 2>&1 & fi
    echo 'Installation failed; attempted to restart only a previously running DT.'; exit 1
fi
if [[ "$DT_ADAPTER" == ubuntu-gnome50 ]]; then
    uuid=double-tap@freeman4848.local
    dest="${XDG_DATA_HOME:-$HOME/.local/share}/gnome-shell/extensions/$uuid"
    if [[ -d "$dest" ]]; then
        cp -a -- "$dest" "$dest.backup-$(date +%Y%m%d-%H%M%S)"
        install -m644 "/usr/share/gnome-shell/extensions/$uuid/extension.js" \
            "/usr/share/gnome-shell/extensions/$uuid/metadata.json" "$dest/"
        mkdir -p "$dest/icons"
        install -m644 "/usr/share/gnome-shell/extensions/$uuid/icons/"dt-*.svg "$dest/icons/"
    fi
    gnome-extensions enable "$uuid" || true
    "$DT_CLIENT" --background >/dev/null 2>&1 &
    echo 'Installed. Log out/in once to load the updated GNOME bridge; do not restart the PC.'
else
    "$DT_CLIENT" >/dev/null 2>&1 &
    echo 'Installed and started. Test in disposable text with the host hook disabled.'
fi
