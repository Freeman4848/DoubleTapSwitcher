# DT_GNOME_REMOVE_INDICATOR: leave enabled during upgrades, hide on removal.
case "${1:-}" in
 remove|deconfigure)
 for dt_runtime in /run/user/[0-9]*; do
  [ -d "$dt_runtime" ] || continue
  dt_uid=${dt_runtime##*/}
  case "$dt_uid" in *[!0-9]*|'') continue ;; esac
  [ "$(stat -c %u "$dt_runtime")" = "$dt_uid" ] || continue
  [ -S "$dt_runtime/bus" ] || continue
  dt_user=$(getent passwd "$dt_uid" | cut -d: -f1)
  [ -n "$dt_user" ] || continue
  # This extension belongs to the desktop session, not the terminated client.
  if ! timeout 5s runuser -u "$dt_user" -- env \
   XDG_RUNTIME_DIR="$dt_runtime" DBUS_SESSION_BUS_ADDRESS="unix:path=$dt_runtime/bus" \
   /usr/bin/gnome-extensions disable double-tap@freeman4848.local; then
   echo 'DT: GNOME indicator could not be disabled; log out to unload it.' >&2
  fi
 done
 ;;
esac
