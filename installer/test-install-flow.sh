#!/usr/bin/env bash
# Use real package validation, but replace every modifying desktop/root command.
set -euo pipefail
base=$(cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/bin" "$work/home" "$work/data"
cat > "$work/bin/apt-get" <<'EOF'
#!/bin/bash
if [[ $1 == --simulate ]]; then
    echo preflight >> "$DT_TEST_LOG"
    [[ ${!#} == /tmp/double-tap-install.*/*.deb ]] || exit 90
    [[ $(stat -c %a "${!#}") == 644 ]] || exit 91
    [[ $DT_TEST_CASE != preflight-failure ]] || exit 42
else
    echo install >> "$DT_TEST_LOG"
    [[ $DT_TEST_CASE != install-failure ]] || exit 44
fi
EOF
cat > "$work/bin/sudo" <<'EOF'
#!/bin/bash
if [[ $1 == -v ]]; then
    echo auth >> "$DT_TEST_LOG"
    [[ $DT_TEST_CASE != auth-failure ]] || exit 43
else
    "$@"
fi
EOF
cat > "$work/bin/double-tap-switcher" <<'EOF'
#!/bin/bash
case $1 in
    --quit) echo quit >> "$DT_TEST_LOG" ;;
    --background) echo start >> "$DT_TEST_LOG" ;;
    *) exit 92 ;;
esac
EOF
cat > "$work/bin/gnome-extensions" <<'EOF'
#!/bin/bash
echo enable >> "$DT_TEST_LOG"
EOF
cat > "$work/bin/pgrep" <<'EOF'
#!/bin/bash
[[ $DT_TEST_CASE != not-running ]]
EOF
cat > "$work/bin/dpkg-query" <<'EOF'
#!/bin/bash
if [[ $DT_TEST_CASE == already-installed ]]; then printf 'installed 1.39';
else printf 'installed 1.29'; fi
EOF
chmod +x "$work/bin/"*
export PATH="$work/bin:$PATH" HOME="$work/home" XDG_DATA_HOME="$work/data"
export XDG_CURRENT_DESKTOP=ubuntu:GNOME XDG_SESSION_TYPE=wayland
export DT_TEST_LOG="$work/events"
count=0
for DT_TEST_CASE in success decline preflight-failure auth-failure install-failure already-installed not-running; do
    export DT_TEST_CASE
    : > "$DT_TEST_LOG"
    answer=y; [[ $DT_TEST_CASE != decline ]] || answer=n
    result=0
    bash "$base/install-linux.sh" --install <<< "$answer" > "$work/output" 2>&1 || result=$?
    case $DT_TEST_CASE in
        success) expected=0; events='preflight auth quit install enable start' ;;
        decline) expected=0; events=preflight ;;
        preflight-failure) expected=42; events=preflight ;;
        auth-failure) expected=43; events='preflight auth' ;;
        install-failure) expected=1; events='preflight auth quit install start' ;;
        already-installed) expected=0; events='' ;;
        not-running) expected=0; events='preflight auth install enable start' ;;
    esac
    # The background restart must complete before inspecting its mock event.
    if [[ $events == *start ]]; then
        for ((i=0;i<50;i++)); do grep -qx start "$DT_TEST_LOG" && break; sleep .02; done
    fi
    [[ $result == "$expected" && $(paste -sd' ' "$DT_TEST_LOG") == "$events" ]] || {
        echo "FAIL: $DT_TEST_CASE (exit $result)"; cat "$DT_TEST_LOG"; exit 1;
    }
    ((count+=1))
done
echo "PASS: $count mocked installer flows; no sudo, installation or app changes."
