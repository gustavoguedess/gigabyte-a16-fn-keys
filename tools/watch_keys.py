#!/usr/bin/env python3
"""Shows the keys coming from the GIGABYTE keyboard evdev devices.

Standard library only (runs under sudo without python-evdev).
Usage: sudo python3 tools/watch_keys.py [seconds]
"""
import glob
import os
import select
import struct
import sys
import time

EV_KEY = 0x01
EVENT = struct.Struct("llHHi")
# subset of linux/input-event-codes.h relevant here
NAMES = {
    1: "ESC", 57: "SPACE", 62: "F4", 66: "F8", 125: "LEFTMETA", 29: "LEFTCTRL",
    113: "MUTE", 114: "VOLUMEDOWN", 115: "VOLUMEUP", 190: "F20", 194: "F24",
    224: "BRIGHTNESSDOWN", 225: "BRIGHTNESSUP", 248: "MICMUTE", 25: "P",
    530: "TOUCHPAD_TOGGLE", 247: "RFKILL", 42: "LEFTSHIFT", 31: "S",
    59: "F1", 60: "F2", 61: "F3", 63: "F5", 64: "F6", 65: "F7", 67: "F9",
    68: "F10", 87: "F11", 88: "F12", 191: "F21", 192: "F22", 193: "F23",
    99: "SYSRQ", 140: "CALC", 142: "SLEEP", 148: "PROG1", 149: "PROG2",
    202: "PROG3", 152: "SCREENLOCK", 212: "CAMERA", 227: "SWITCHVIDEOMODE",
    228: "KBDILLUMTOGGLE", 229: "KBDILLUMDOWN", 230: "KBDILLUMUP",
    238: "WLAN", 183: "F13", 184: "F14", 185: "F15", 186: "F16", 187: "F17",
    117: "KPEQUAL", 35: "H", 431: "DISPLAYTOGGLE", 245: "DISPLAY_OFF", 116: "POWER",
}


def main():
    seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 20
    fds = {}
    for name_path in glob.glob("/sys/class/input/event*/device/name"):
        with open(name_path) as f:
            name = f.read().strip()
        if "GIGABYTE" not in name:
            continue
        node = "/dev/input/" + name_path.split("/")[4]
        fds[os.open(node, os.O_RDONLY | os.O_NONBLOCK)] = f"{node} ({name})"
    if not fds:
        sys.exit("No GIGABYTE device found")
    for desc in fds.values():
        print("listening on", desc)
    print(f"\nPress the keys ({seconds:.0f}s)...\n")

    end = time.monotonic() + seconds
    while (left := end - time.monotonic()) > 0:
        for fd in select.select(list(fds), [], [], left)[0]:
            data = os.read(fd, EVENT.size * 64)
            for off in range(0, len(data), EVENT.size):
                _, _, etype, code, value = EVENT.unpack_from(data, off)
                if etype == EV_KEY and value in (0, 1):
                    state = "press  " if value else "release"
                    print(f"{state} KEY_{NAMES.get(code, code)}   [{fds[fd]}]")


if __name__ == "__main__":
    main()
