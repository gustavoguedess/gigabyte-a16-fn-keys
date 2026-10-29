#!/usr/bin/env python3
"""Guided capture of the raw HID reports of the Gigabyte A16 keyboard (0414:8105).

Reads all the keyboard hidraw interfaces at once and logs what arrives at each
step. With sudo, it also logs the keys arriving on evdev.
Usage: [sudo] python3 tools/capture.py [output_file] [step ...]
Without steps on the command line, the default list is used.
"""
import glob
import os
import select
import subprocess
import sys
import time

from watch_keys import EV_KEY, EVENT, NAMES

VID_PID = "00000414:00008105"
STEP_SECONDS = 5

STEPS = [
    "Do NOT press anything (baseline)",
    "Press and release Fn ALONE, once",
    "Press and release F4 ALONE (no Fn), once",
    "Press Fn+F4 (mic mute), release, once",
    "Press Fn+F2 (volume down), release, once",
    "Press Fn+Esc, release, once",
    "Press Fn+Space, release, once",
    "Press Fn+F8, release, once",
    "Press Fn + the touchpad key, release, once",
    "Press Fn+F4 again, HOLDING Fn ~1s before F4",
]


def find_nodes():
    nodes = {}
    for uevent in sorted(glob.glob("/sys/class/hidraw/hidraw*/device/uevent")):
        with open(uevent) as f:
            text = f.read()
        if VID_PID not in text:
            continue
        name = uevent.split("/")[4]
        phys = next((l.split("=", 1)[1] for l in text.splitlines()
                     if l.startswith("HID_PHYS=")), "?")
        nodes[name] = phys.rsplit("/", 1)[-1]
    return nodes


def open_evdev():
    fds = {}
    for name_path in glob.glob("/sys/class/input/event*/device/name"):
        with open(name_path) as f:
            if "GIGABYTE" not in f.read():
                continue
        node = "/dev/input/" + name_path.split("/")[4]
        try:
            fds[os.open(node, os.O_RDONLY | os.O_NONBLOCK)] = node
        except PermissionError:
            return {}
    return fds


def format_evdev(data):
    keys = []
    for off in range(0, len(data) - EVENT.size + 1, EVENT.size):
        _, _, etype, code, value = EVENT.unpack_from(data, off)
        if etype == EV_KEY and value in (0, 1):
            keys.append(f"{'+' if value else '-'}KEY_{NAMES.get(code, code)}")
    return " ".join(keys)


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "capture.log"
    steps = sys.argv[2:] or STEPS
    nodes = find_nodes()
    if not nodes:
        sys.exit("No 0414:8105 hidraw found")

    fds = {}
    for name in nodes:
        fds[os.open(f"/dev/{name}", os.O_RDONLY | os.O_NONBLOCK)] = name
    evdev = open_evdev()
    if not evdev:
        print("no evdev permission (run with sudo to see the keys): hidraw only\n")

    log = open(out_path, "w")
    print(f"Interfaces: {nodes}\nOutput: {out_path}\n")
    log.write(f"# interfaces: {nodes}\n")

    interactive = sys.stdin.isatty()
    if not interactive:
        print("No interactive terminal: steps are announced by notification.\n")

    for i, step in enumerate(steps, 1):
        prompt = f"[{i}/{len(steps)}] {step}"
        if interactive:
            input(f"{prompt}. Press Enter to start...")
        else:
            subprocess.run(["notify-send", "-t", str(STEP_SECONDS * 1000),
                            "-u", "critical", "A16 capture", prompt + " NOW"])
            print(prompt)
        # drop whatever arrived while Enter was pressed
        all_fds = list(fds) + list(evdev)
        while interactive and select.select(all_fds, [], [], 0.3)[0]:
            for fd in select.select(all_fds, [], [], 0)[0]:
                os.read(fd, 1024)
        print(f"   recording {STEP_SECONDS}s... NOW")
        log.write(f"\n## {i}: {step}\n")
        t0 = time.monotonic()
        count = 0
        while (left := STEP_SECONDS - (time.monotonic() - t0)) > 0:
            for fd in select.select(all_fds, [], [], left)[0]:
                data = os.read(fd, 4096)
                if fd in evdev:
                    keys = format_evdev(data)
                    if not keys:
                        continue
                    line = f"{time.monotonic() - t0:6.3f} {evdev[fd]}: {keys}"
                else:
                    line = f"{time.monotonic() - t0:6.3f} {fds[fd]} ({nodes[fds[fd]]}): {data.hex(' ')}"
                log.write(line + "\n")
                print("   " + line)
                count += 1
        print(f"   {count} reports\n")
        log.flush()

    log.close()
    print(f"Done. Output in {out_path}")


if __name__ == "__main__":
    main()
