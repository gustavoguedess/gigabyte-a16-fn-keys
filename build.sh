#!/bin/sh
# Builds the HID-BPF program. Uses the system clang + bpftool when available,
# otherwise builds inside a container (podman), e.g. on Bazzite/Fedora Atomic.
# Output: build/0010-Gigabyte__Gaming-A16.bpf.o
set -eu
cd "$(dirname "$0")"

UHB=ext/udev-hid-bpf
UHB_COMMIT=bbaad639d4b2efe2a909e8b7c594b91e87704ab8

if [ ! -d "$UHB" ]; then
	git clone -q --filter=blob:none https://gitlab.freedesktop.org/libevdev/udev-hid-bpf.git "$UHB"
	git -C "$UHB" checkout -q "$UHB_COMMIT"
fi

compile='
	set -e
	for f in src/*.bpf.c; do
		o=build/$(basename "$f" .c).o
		clang -std=gnu11 -fno-stack-protector -O2 -target bpf -g -c \
			-fms-extensions -Wno-microsoft-anon-tag -Wall \
			-I ext/udev-hid-bpf/src/bpf "$f" -o "$o.tmp"
		bpftool gen object "$o" "$o.tmp"
		rm "$o.tmp"
		echo "ok: $o"
	done
'

mkdir -p build
if command -v clang >/dev/null && command -v bpftool >/dev/null && [ -f /usr/include/bpf/bpf_helpers.h ]; then
	sh -c "$compile"
else
	podman run --rm -v "$PWD":/src:Z -w /src registry.fedoraproject.org/fedora:44 sh -c "
		dnf -q -y install clang libbpf-devel bpftool >/dev/null
		$compile"
fi
