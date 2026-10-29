# Investigation

How the problem was understood and solved. To install the driver, see the [README](../README.md).

Test machine: Gigabyte GAMING A16 3VH, Bazzite (Fedora 44, KDE Plasma 6, Wayland), kernel 7.2.

## The problem

The internal keyboard (USB `0414:8105`, "GIGABYTE USB-HID Keyboard") sends **`KEY_F20`** as soon as
**Fn** is pressed, alone or with another key. KDE, GNOME and X11 map F20 to **Microphone Mute**
(`XF86AudioMicMute`). As a result:

- Fn alone mutes the microphone.
- Every Fn combo (Fn+F2 to lower the volume, for example) mutes it too.
- Fn+F4, the key with the microphone icon, sends exactly the same `F20` as Fn alone; F4 adds nothing.
  So the two cannot be told apart with shortcuts.

On Windows, the Gigabyte Control Center bridges this. Gigabyte offers nothing similar for Linux.
Other users reported the same problem with no solution:
[Fedora Discussion – Gigabyte GAMING A16 3WH](https://discussion.fedoraproject.org/t/the-fn-key-isnt-working-properly-fedora-44-workstation-gigabyte-gaming-a16-3wh/199094).

## Workarounds tried first

- **hwdb** (`KEYBOARD_KEY_7006f=reserved` for `evdev:input:b0003v0414p8105*`): Fn stops muting, but so
  does Fn+F4, and evdev no longer even emits the `MSC_SCAN` for the key.
- **KDE shortcuts:** removing *Microphone Mute* from *Mute Microphone* fixes Fn alone, but leaves only
  `Meta+Volume Mute` to mute the microphone.

Neither tells Fn+F4 apart from Fn, because evdev only sees the translated events. The open question
was whether the raw HID reports carry the difference.

## The interfaces

The keyboard exposes five USB interfaces:

| hidraw | Interface | Descriptor | Content |
|---|---|---|---|
| hidraw3 | input0 | 77 bytes | Boot keyboard, report ID 1, 37 bytes: modifiers, 6-key array, bitmap of usages 0x00–0xdf |
| hidraw4 | input1 | 34 bytes | Vendor page 0xFF00, 64-byte input/output |
| hidraw5 | input2 | 202 bytes | Mouse (6), Consumer (3), vendor 0xFF02 (4), keyboard bitmap (5), vendor 0xFF89 feature (0x5a), Wireless Radio (7) |
| hidraw6 | input3 | 31 bytes | Vendor page 0xFF01, 64 bytes, feature report |
| hidraw7 | input4 | 327 bytes | LampArray (RGB lighting) |

## What the keyboard sends

Captured with `tools/capture.py`, which reads all hidraw nodes at once (and evdev, with sudo).
Raw logs are in [`captures/`](captures/).

The vendor report 4 on input2 carries the special keys as one-shot events, with no release report:

| Key | input0 (report 1) | input2 |
|---|---|---|
| Fn | usage `0x6f` (F20) in the array | — |
| Fn+F4 (microphone) | `0x6f` | `04 00 00 92` |
| Fn+F7 (fan) | `0x6f` | `04 00 00 84` |
| Fn+F8 (LCD) | `0x6f`, then `Meta+P` | — |
| Fn+F9 (touchpad) | `0x6f`, then `Ctrl+Meta+F24` | `04 00 00 81` |
| Fn+F10 (airplane) | `0x6f` | report 7: `07 01` (`KEY_RFKILL`) |
| Fn+F11 (print) | `0x6f`, then `Meta+Shift+S` | — |
| Fn+F12 (microphone) | `0x6f`, then `Meta+H` | — |
| G | usage `0x67` (Keypad =) | `04 00 00 91` |
| Fn+G | `0x6f` | `04 00 00 93` |
| Fn+Esc (Fn Lock, firmware) | `0x6f` | `04 00 01 95` |
| Fn+Space | `0x6f` | `04 01 18 00` |

Fn+F1–F3 (audio) and Fn+F5/F6 (brightness) arrive as Consumer Control keys and already work.

The Windows shortcuts (Fn+F8/F9/F11/F12) are typed by the firmware one key per report, with Fn (`0x6f`)
still in the array and the modifiers **in the array** as usages `0xe0`–`0xe7`. A real modifier comes in
the modifier byte instead, which is how the driver tells a typed `Meta+P` apart from the firmware's.

So Fn+F4 is distinguishable after all: through the vendor interface, which Linux ignores.

## The driver

`src/0010-Gigabyte__Gaming-A16.bpf.c` is a HID-BPF program loaded by
[udev-hid-bpf](https://gitlab.freedesktop.org/libevdev/udev-hid-bpf). It attaches to every interface of
`0414:8105`; `probe()` keeps only input0 and input2, by descriptor size. The `probe() ... failed`
warnings printed by `udev-hid-bpf add` are those rejections, not errors.

- **input0:** removes usage `0x6f`. While Fn is held, it holds back the firmware's modifiers until the
  macro key arrives, so each macro is delivered as a single report, and rewrites `Meta+P` as F13.
  It replaces G's Keypad `=` with F15.
- **input2:** `rdesc_fixup` appends a System Control collection (report ID `0x10`) with
  *System Microphone Mute* (Generic Desktop `0xA9`, mapped to `KEY_MICMUTE` by hid-input) and F14, F16,
  F17. On a one-shot report, `hid_bpf_try_input_report()` (kernel ≥ 6.12) injects a press and a release.
  The injection reuses the buffer of the event being processed, so the original report is dropped
  afterwards.

F13–F17 are free keys (`XF86Tools`, `XF86Launch5`–`8`) for desktop shortcuts. Fn+F10 does not keep
`KEY_RFKILL` because KDE handles it as Wi-Fi only; the airplane mode script blocks all radios instead.

There is no fan control: no hwmon fan device is exposed, and the ACPI WMI interfaces
(`ABBC0F6x-8EA1-11D1-00A0-C90629100000`) have no matching driver. Fn+F7 is mapped to the KDE power
profile switch, which drives the AMD PMF `platform_profile` (low-power, balanced, performance).

## Notes for Bazzite / Fedora Atomic

- `/usr` is read-only. `udev-hid-bpf install` writes to `/etc/udev-hid-bpf/` and `/etc/udev/rules.d/`,
  which works.
- There is no clang on the host; `build.sh` builds in a Fedora container with podman.

## Related projects

- [Luminous418/Gigabyte-Gaming-A16-Linux-KB-Drivers](https://github.com/Luminous418/Gigabyte-Gaming-A16-Linux-KB-Drivers): keyboard RGB lighting through HID LampArray (tested on `0414:8104`, should work on `8105`). Notes that the keyboard also sends `KEY_F20` on Fn+Space.
- [aasray123/gigabyte-keyboard-ctl](https://github.com/aasray123/gigabyte-keyboard-ctl)
- Kernel patch mapping Lenovo's microphone mute to `KEY_F20`: [LKML](https://lkml.rescloud.iu.edu/2105.1/06281.html)
