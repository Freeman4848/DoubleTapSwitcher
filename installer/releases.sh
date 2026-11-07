#!/bin/bash
# Select only locally tested package identities; no downloads or desktop changes.
dt_release() {
    case "$DT_ADAPTER" in
        ubuntu-gnome50) DT_PACKAGE=double-tap-switcher; DT_RELEASE=1.33; DT_FILE=ubuntu_native/dist/double-tap-switcher_1.33_amd64.deb ;;
        mint-xfce) DT_PACKAGE=double-tap-switcher-mint; DT_RELEASE=1.38; DT_FILE=mint_native/dist/double-tap-switcher-mint_1.38_amd64.deb ;;
        kubuntu-kde) DT_PACKAGE=double-tap-switcher-kubuntu; DT_RELEASE=1.37; DT_FILE=kubuntu_native/dist/double-tap-switcher-kubuntu_1.37_amd64.deb ;;
        pop-cosmic) DT_PACKAGE=double-tap-switcher-pop; DT_RELEASE=1.36; DT_FILE=pop_native/dist/double-tap-switcher-pop_1.36_amd64.deb ;;
        cachyos-kde) DT_PACKAGE=double-tap-switcher-cachyos; DT_RELEASE=1.35-1; DT_FILE=cachyos_native/dist/double-tap-switcher-cachyos-1.35-1-x86_64.pkg.tar.zst ;;
        *) return 3 ;;
    esac
    DT_CLIENT=$DT_PACKAGE
}
