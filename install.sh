#!/bin/sh
# Installs the Fn key driver for the Gigabyte GAMING A16 keyboard (0414:8105).
# Run as a regular user: sudo is only used to install the driver.
#   ./install.sh                 driver + airplane mode + KDE shortcuts
#   ./install.sh --no-shortcuts  driver and airplane mode script only
set -eu
cd "$(dirname "$0")"

NAME=0010-Gigabyte__Gaming-A16.bpf.o
BIN_DIR=${XDG_BIN_HOME:-$HOME/.local/bin}
AIRPLANE=$BIN_DIR/a16-airplane-mode

die() { echo "error: $*" >&2; exit 1; }

[ "$(id -u)" != 0 ] || die "run without sudo (the script asks for your password when needed)"
kver=$(uname -r | cut -d. -f1,2)
[ "$(printf '%s\n6.12\n' "$kver" | sort -V | head -1)" = 6.12 ] || die "kernel $kver: 6.12 or newer is required"
UHB=$(command -v udev-hid-bpf) || die "udev-hid-bpf not found (install the udev-hid-bpf package)"

devices=$(grep -l '^HID_ID=0003:00000414:00008105$' /sys/bus/hid/devices/*/uevent 2>/dev/null | xargs -r -n1 dirname)
[ -n "$devices" ] || echo "warning: keyboard 0414:8105 not found; the driver will load when it shows up"

# a local build (./build.sh) takes precedence over the prebuilt object
if [ -f "build/$NAME" ]; then
	OBJ=build/$NAME
elif [ -f "dist/$NAME" ]; then
	OBJ=dist/$NAME
else
	./build.sh
	OBJ=build/$NAME
fi

echo "== driver (sudo)"
sudo "$UHB" install --force --prefix "$(dirname "$(dirname "$UHB")")" "$OBJ"
# load it now, no reboot needed (--replace avoids stacking on an already loaded version)
for dev in $devices; do
	sudo "$UHB" add --replace "$dev" "/etc/udev-hid-bpf/$NAME" 2>/dev/null || true
done

echo "== airplane mode: $AIRPLANE"
install -Dm755 scripts/a16-airplane-mode "$AIRPLANE"

if [ "${1:-}" != "--no-shortcuts" ]; then
	echo "== KDE shortcuts"
	python3 -I scripts/a16-kde-shortcuts install "$AIRPLANE"
fi

echo
echo "Done. Test it: Fn alone does nothing, Fn+F4 mutes the microphone."
