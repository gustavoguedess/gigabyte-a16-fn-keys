#!/bin/sh
# Loads the HID-BPF program from build/ TEMPORARILY (gone after a reboot or
# "sudo udev-hid-bpf remove <device>") and shows the keys received.
# Usage: sudo sh tools/test-load.sh
set -eu
cd "$(dirname "$0")/.."

[ "$(id -u)" = 0 ] || { echo "run with sudo"; exit 1; }

udev-hid-bpf -v add --replace - build/0010-Gigabyte__Gaming-A16.bpf.o
sleep 1
echo
udev-hid-bpf list-bpf-programs
echo
python3 -I tools/watch_keys.py 60
