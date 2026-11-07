#!/usr/bin/env bash
set -euo pipefail
base=$(cd -- "$(dirname -- "$0")/.." && pwd)
source "$base/installer/platform.sh"
count=0
check() {
    local expected=$1; shift
    local code=0
    dt_classify "$@" || code=$?
    [[ "$DT_ADAPTER" == "$expected" ]] || { echo "FAIL: $* -> $DT_ADAPTER"; exit 1; }
    if [[ "$expected" != unsupported ]]; then [[ $code == 0 ]]; else [[ $code == 3 ]]; fi
    count=$((count+1))
}
check ubuntu-gnome50 ubuntu 26.04 ubuntu:GNOME wayland 50 amd64
check unsupported ubuntu 26.04 GNOME x11 50 amd64
check kubuntu-kde ubuntu 26.04 KDE wayland unknown amd64
check unsupported ubuntu 24.04 KDE x11 unknown amd64
check mint-xfce linuxmint 22.3 XFCE x11 unknown amd64
check unsupported linuxmint 22.3 X-Cinnamon x11 unknown amd64
check unsupported linuxmint 22.3 MATE x11 unknown amd64
check unsupported ubuntu 24.04 ubuntu:GNOME wayland 46 amd64
check unsupported ubuntu 26.04 ubuntu:GNOME wayland 51 amd64
check unsupported fedora 43 GNOME wayland 50 amd64
check unsupported ubuntu 26.04 GNOME wayland 50 arm64
check unsupported ubuntu 26.04 GNOME tty 50 amd64
check unsupported ubuntu 26.04 unknown wayland 50 amd64
check unsupported ubuntu 26.04 NOTGNOME wayland 50 amd64
check kubuntu-kde ubuntu 26.04 KDE x11 unknown amd64
check pop-cosmic pop 24.04 COSMIC wayland unknown amd64
check unsupported pop 24.04 COSMIC x11 unknown amd64
check cachyos-kde cachyos unknown KDE wayland unknown amd64
check unsupported cachyos unknown KDE x11 unknown amd64
check unsupported linuxmint 22.3 XFCE wayland unknown amd64
echo "PASS: $count platform classifications; no install or desktop changes."
