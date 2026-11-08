#!/bin/sh
# Called by package management as root; control requests run as each session user.
dt_quit_sessions() {
 dt_name=$1 dt_control=$2
 for dt_runtime in /run/user/[0-9]*; do
  [ -d "$dt_runtime" ] || continue
  dt_uid=${dt_runtime##*/}
  case "$dt_uid" in *[!0-9]*|'') continue ;; esac
  [ "$(stat -c %u "$dt_runtime")" = "$dt_uid" ] || continue
  [ -S "$dt_runtime/$dt_control" ] || continue
  dt_user=$(getent passwd "$dt_uid" | cut -d: -f1)
  [ -n "$dt_user" ] || continue
  timeout 5s runuser -u "$dt_user" -- env QT_QPA_PLATFORM=offscreen \
   XDG_RUNTIME_DIR="$dt_runtime" DBUS_SESSION_BUS_ADDRESS="unix:path=$dt_runtime/bus" \
   "/usr/lib/$dt_name/$dt_name" --quit || return 1
  dt_count=0
  while [ -S "$dt_runtime/$dt_control" ]; do
   dt_count=$((dt_count + 1))
   [ "$dt_count" -lt 50 ] || {
    echo 'DT did not exit; close its tray/dialogs and retry.' >&2; return 1;
   }
   sleep 0.1
  done
 done
}
