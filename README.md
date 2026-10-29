# Gigabyte GAMING A16 Fn keys on Linux

[Português (Brasil)](README.pt-BR.md)

HID-BPF driver that fixes the Fn keys of the **Gigabyte GAMING A16** internal keyboard
(USB `0414:8105`, "GIGABYTE USB-HID Keyboard").

**The problem:** on Linux the keyboard sends `F20` whenever **Fn** is pressed. KDE, GNOME and X11
treat F20 as *Microphone Mute*, so Fn alone, or any Fn combo, mutes the microphone. On Windows this
is handled by the Gigabyte Control Center, which does not exist for Linux.
Problem report: [Fedora Discussion](https://discussion.fedoraproject.org/t/the-fn-key-isnt-working-properly-fedora-44-workstation-gigabyte-gaming-a16-3wh/199094).

> Tested on a GAMING A16 3VH, Bazzite (Fedora 44), kernel 7.2, KDE Plasma 6.7 (Wayland).
> Other A16 models with the same keyboard (`0414:8105`) should work. If you try it, please open an issue with the result.

## What changes

| Key | Without the driver | With the driver |
|---|---|---|
| Fn alone | mutes the microphone | nothing |
| Fn+F4 (microphone) | same as Fn alone | mutes the microphone |
| Fn+F7 (fan) | nothing | cycles the power profile ¹ |
| Fn+F8 (LCD) | `Meta+P` (switch display) | turns the screen off ¹ |
| Fn+F9 (touchpad) | toggles the touchpad | unchanged |
| Fn+F10 (airplane) | turns off Wi-Fi only | airplane mode: Wi-Fi and Bluetooth ¹ |
| Fn+F11 (print) | screen snip (Spectacle) | unchanged |
| G | types `=` | free key for shortcuts |
| Fn+G | nothing | free key for shortcuts |

¹ On KDE, `install.sh` sets these shortcuts up for you. Elsewhere, see [Other desktops](#other-desktops).

There is no direct fan control on Linux for this model. Fn+F7 switches the power profile
(power saver, balanced, performance), which changes how the fan behaves.

## Requirements

- Kernel ≥ 6.12 with HID-BPF (`CONFIG_HID_BPF=y`, default on Fedora and Arch; Ubuntu from 25.04)
- [`udev-hid-bpf`](https://gitlab.freedesktop.org/libevdev/udev-hid-bpf) (package `udev-hid-bpf` on Fedora/Bazzite)
- `python3`, `rfkill` and `gdbus` (usually already installed)

## Install

```sh
git clone <url of this repository> gigabyte-a16-fn
cd gigabyte-a16-fn
./install.sh
```

Run it **without sudo**: it asks for your password only to install the driver. It:

1. installs the driver to `/etc/udev-hid-bpf/` with a rule in `/etc/udev/rules.d/` (works on
   read-only `/usr` systems such as Bazzite and Silverblue) and loads it right away, no reboot needed;
2. installs the airplane mode script to `~/.local/bin/a16-airplane-mode`;
3. on KDE Plasma, sets the Fn+F7, Fn+F8 and Fn+F10 shortcuts. Shortcuts you already have are kept.

Driver only, no shortcuts: `./install.sh --no-shortcuts`.

**Uninstall:** `./uninstall.sh` (removes the driver, the script and the shortcuts it created).

### Building it yourself (optional)

A prebuilt driver ships in `dist/`. To build: `./build.sh`. It uses the system `clang` and
`bpftool`, or builds inside a `podman` container if they are missing.
`install.sh` prefers what is in `build/`. Run `install.sh` again after rebuilding.

## Other desktops

The driver works on any desktop; only the shortcuts are KDE-specific. The new keys arrive as:

| Key | Emitted key | X11/Wayland name | Suggestion |
|---|---|---|---|
| Fn+F7 | `KEY_F17` | `XF86Launch8` | cycle power profile |
| Fn+F8 | `KEY_F13` | `XF86Tools` | turn screen off |
| Fn+F10 | `KEY_F14` | `XF86Launch5` | `~/.local/bin/a16-airplane-mode` |
| G | `KEY_F15` | `XF86Launch6` | free |
| Fn+G | `KEY_F16` | `XF86Launch7` | free |

On KDE the actions live in **System Settings → Keyboard → Shortcuts**: *Power Management →
Turn Off Screen / Switch Power Profile*. When you assign *Tools*, KDE warns that it opens
System Settings; reassigning is fine.

## How it works

The keyboard has 5 USB interfaces. The driver (`src/0010-Gigabyte__Gaming-A16.bpf.c`) acts on two:

- **input0 (keyboard):** drops the F20 sent for Fn. The Windows shortcuts the firmware types while Fn
  is held (`Meta+P`, `Ctrl+Meta+F24`…) are delivered whole; Fn+F8's `Meta+P` becomes F13. G's `=` becomes F15.
- **input2:** the firmware sends the special keys (Fn+F4, Fn+F7, Fn+G…) in a vendor report that Linux
  ignores. The driver appends a report with those keys to the HID descriptor and injects them with
  `hid_bpf_try_input_report()`.

Details, captures and the investigation history: [docs/investigation.md](docs/investigation.md).

### Diagnostic tools

- `tools/capture.py`: guided capture of raw HID reports (and evdev, with sudo).
- `tools/watch_keys.py`: shows incoming keys (`sudo python3 tools/watch_keys.py`).
- `tools/test-load.sh`: loads the driver from `build/` until the next reboot and shows the keys.

## Related projects

- [Luminous418/Gigabyte-Gaming-A16-Linux-KB-Drivers](https://github.com/Luminous418/Gigabyte-Gaming-A16-Linux-KB-Drivers): keyboard RGB lighting.
- [aasray123/gigabyte-keyboard-ctl](https://github.com/aasray123/gigabyte-keyboard-ctl)

## License

[GPL-2.0-only](LICENSE)
