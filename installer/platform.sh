#!/usr/bin/env bash
# Pure classification: do not install packages or change the desktop here.
dt_read_os_release() {
    local key value
    DT_DISTRO=unknown DT_VERSION=unknown
    while IFS='=' read -r key value; do
        value=${value%\"}; value=${value#\"}
        value=${value%\'}; value=${value#\'}
        case "$key" in
            ID) DT_DISTRO=$value ;;
            VERSION_ID) DT_VERSION=$value ;;
        esac
    done < "$1"
}

dt_classify() {
    local distro=$1 version=$2 desktop=":${3^^}:" session=$4 shell=$5 arch=$6
    DT_ADAPTER=unsupported
    DT_REASON='No verified adapter for this desktop.'
    if [[ "$arch" != amd64 ]]; then
        DT_REASON='Only amd64 has a built package; no matching architecture.'
    elif [[ "$session" != wayland && "$session" != x11 ]]; then
        DT_REASON='Run from the graphical desktop; X11/Wayland session is unknown.'
    elif [[ "$distro" == linuxmint && "$version" == 22.3 && "$desktop" == *:XFCE:* && "$session" == x11 ]]; then
        DT_ADAPTER=mint-xfce
        DT_REASON='Tested Mint Xfce/X11 adapter.'
        return 0
    elif [[ "$distro" == ubuntu && "$version" == 26.04 && "$desktop" == *:KDE:* ]]; then
        DT_ADAPTER=kubuntu-kde
        DT_REASON='KDE password protection tested on Wayland/XWayland; native X11 session pending.'
        return 0
    elif [[ "$distro" == cachyos && "$desktop" == *:KDE:* && "$session" == wayland ]]; then
        DT_ADAPTER=cachyos-kde
        DT_REASON='Conditional: the Live kxkb2locale1 helper conflicts with direct switching.'
        return 0
    elif [[ "$distro" == pop && "$version" == 24.04 && "$desktop" == *:COSMIC:* && "$session" == wayland ]]; then
        DT_ADAPTER=pop-cosmic
        DT_REASON='Tested legacy COSMIC adapter; newer COSMIC layout protocol is not certified.'
        return 0
    elif [[ "$desktop" == *:GNOME:* ]]; then
        if [[ "$distro" == ubuntu && "$version" == 26.04 && "$shell" == 50 && "$session" == wayland ]]; then
            DT_ADAPTER=ubuntu-gnome50
            DT_REASON='Tested Ubuntu GNOME 50 Wayland adapter with JavaScript bridge.'
            return 0
        fi
        DT_REASON='The GNOME build targets Ubuntu 26.04 / GNOME 50 / Wayland only.'
    fi
    return 3
}

dt_detect() {
    dt_read_os_release /etc/os-release
    DT_DESKTOP=${XDG_CURRENT_DESKTOP:-unknown}
    DT_SESSION=${XDG_SESSION_TYPE:-unknown}
    if command -v dpkg >/dev/null; then
        DT_ARCH=$(dpkg --print-architecture)
    else
        DT_ARCH=$(uname -m)
        case "$DT_ARCH" in x86_64) DT_ARCH=amd64 ;; aarch64) DT_ARCH=arm64 ;; esac
    fi
    DT_SHELL=unknown
    if [[ ":${DT_DESKTOP^^}:" == *:GNOME:* ]] && command -v gnome-shell >/dev/null; then
        local description
        description=$(gnome-shell --version 2>/dev/null) || description=''
        if [[ "$description" =~ ([0-9]+)\. ]]; then DT_SHELL=${BASH_REMATCH[1]}; fi
    fi
    dt_classify "$DT_DISTRO" "$DT_VERSION" "$DT_DESKTOP" "$DT_SESSION" "$DT_SHELL" "$DT_ARCH"
}
