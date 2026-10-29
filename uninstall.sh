#!/bin/sh
# Removes the driver, the airplane mode script and the KDE shortcuts created by install.sh.
set -eu
cd "$(dirname "$0")"

NAME=0010-Gigabyte__Gaming-A16
BIN_DIR=${XDG_BIN_HOME:-$HOME/.local/bin}

[ "$(id -u)" != 0 ] || { echo "error: run without sudo" >&2; exit 1; }

echo "== KDE shortcuts"
python3 -I scripts/a16-kde-shortcuts uninstall || true
rm -f "$BIN_DIR/a16-airplane-mode"

echo "== driver (sudo)"
sudo rm -f "/etc/udev-hid-bpf/$NAME.bpf.o" "/etc/udev/rules.d/99-hid-bpf-$NAME.rules"
sudo udevadm control --reload
for dev in $(grep -l '^HID_ID=0003:00000414:00008105$' /sys/bus/hid/devices/*/uevent 2>/dev/null | xargs -r -n1 dirname); do
	sudo udev-hid-bpf remove "$dev" 2>/dev/null || true
done

echo "Removed."
