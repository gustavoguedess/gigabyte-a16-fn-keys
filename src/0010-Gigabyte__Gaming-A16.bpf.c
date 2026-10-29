// SPDX-License-Identifier: GPL-2.0-only
/* Fn key fix for the Gigabyte GAMING A16 internal keyboard (0414:8105).
 */

#include "vmlinux.h"
#include "hid_bpf.h"
#include "hid_bpf_helpers.h"
#include <bpf/bpf_tracing.h>

#define VID_GIGABYTE	0x0414
#define PID_A16_KBD	0x8105

HID_BPF_CONFIG(
	HID_DEVICE(BUS_USB, HID_GROUP_GENERIC, VID_GIGABYTE, PID_A16_KBD)
);

/*
 * The keyboard exposes five USB interfaces. Two of them matter here:
 *
 * input0 (77 byte descriptor): the boot keyboard, report ID 1, 37 bytes.
 *   byte 1 modifiers, bytes 3..8 a 6-key usage array, bytes 9..36 a bitmap
 *   of usages 0x00..0xdf. The firmware reports the Fn key itself as usage
 *   0x6f (F20), which desktops map to "Microphone Mute". Some Fn combos are
 *   Windows shortcuts typed by the firmware while Fn is still reported:
 *     Fn+F8   Meta+P            (Windows "project", key shows "LCD")
 *     Fn+F9   Ctrl+Meta+F24     (Windows touchpad toggle)
 *     Fn+F11  Meta+Shift+S      (Windows snipping tool)
 *     Fn+F12  Meta+H            (Windows voice typing)
 *   The G key sends usage 0x67 (Keypad =).
 *
 * input2 (202 byte descriptor): mouse, consumer control, wireless radio
 *   (report ID 7, Fn+F10) and a vendor collection (usage page 0xff02,
 *   report ID 4, 3 bytes) with one-shot events (no release report):
 *     04 00 00 92  Fn+F4 (microphone mute)
 *     04 00 00 84  Fn+F7 (fan)
 *     04 00 00 81  Fn+F9 (touchpad, also typed as Ctrl+Meta+F24)
 *     04 00 00 91  G     (also typed as Keypad =)
 *     04 00 00 93  Fn+G
 *     04 00 01 95  Fn+Esc (Fn lock, handled by the firmware)
 *     04 01 18 00  Fn+Space
 *
 * This program:
 *   - drops usage 0x6f from input0 reports, so Fn alone sends nothing;
 *   - while Fn is held, holds back the modifiers typed by the firmware until
 *     the macro key arrives, and turns Meta+P (Fn+F8) into F13;
 *   - turns G into F15;
 *   - appends a collection (report ID 0x10) to the input2 descriptor and
 *     injects press+release of its keys from the one-shot reports:
 *     Fn+F4 -> KEY_MICMUTE, Fn+F10 -> F14 (instead of KEY_RFKILL, which
 *     desktops treat as Wi-Fi only), Fn+G -> F16, Fn+F7 -> F17.
 *
 * F13..F17 are free keys (XF86Tools, XF86Launch5..8) meant for desktop
 * shortcuts.
 */

#define KBD_RDESC_SIZE		77
#define KBD_REPORT_ID		1
#define KBD_REPORT_SIZE		37
#define KBD_ARRAY_START		3
#define KBD_ARRAY_END		9
#define KBD_BITMAP_START	9

#define USAGE_P			0x13
#define USAGE_KP_EQUAL		0x67
#define USAGE_F13		0x68
#define USAGE_F14		0x69
#define USAGE_F15		0x6a
#define USAGE_F16		0x6b
#define USAGE_F17		0x6c
#define USAGE_FN		0x6f	/* F20 */
#define USAGE_LEFT_META		0xe3
#define USAGE_FIRST_MOD		0xe0

#define MISC_RDESC_SIZE		202
#define VENDOR_REPORT_ID	4
#define VENDOR_REPORT_SIZE	4
#define VENDOR_FN_F7		0x84
#define VENDOR_FN_F4		0x92
#define VENDOR_FN_G		0x93
#define RADIO_REPORT_ID		7
#define RADIO_REPORT_SIZE	2

#define EXTRA_REPORT_ID		0x10
#define EXTRA_MICMUTE		(1 << 0)
#define EXTRA_F14		(1 << 1)
#define EXTRA_F16		(1 << 2)
#define EXTRA_F17		(1 << 3)

static const __u8 extra_rdesc[] = {
	0x05, 0x01,		/* Usage Page (Generic Desktop) */
	0x09, 0x80,		/* Usage (System Control) */
	0xa1, 0x01,		/* Collection (Application) */
	0x85, EXTRA_REPORT_ID,	/*  Report ID (0x10) */
	0x15, 0x00,		/*  Logical Minimum (0) */
	0x25, 0x01,		/*  Logical Maximum (1) */
	0x75, 0x01,		/*  Report Size (1) */
	0x09, 0xa9,		/*  Usage (System Microphone Mute) */
	0x95, 0x01,		/*  Report Count (1) */
	0x81, 0x02,		/*  Input (Data,Var,Abs) */
	0x05, 0x07,		/*  Usage Page (Keyboard) */
	0x09, USAGE_F14,	/*  Usage (F14) */
	0x09, USAGE_F16,	/*  Usage (F16) */
	0x09, USAGE_F17,	/*  Usage (F17) */
	0x95, 0x03,		/*  Report Count (3) */
	0x81, 0x02,		/*  Input (Data,Var,Abs) */
	0x95, 0x04,		/*  Report Count (4) */
	0x81, 0x03,		/*  Input (Cnst,Var,Abs) */
	0xc0,			/* End Collection */
};

#define FIXED_MISC_RDESC_SIZE	(MISC_RDESC_SIZE + sizeof(extra_rdesc))

SEC(HID_BPF_RDESC_FIXUP)
int BPF_PROG(a16_rdesc_fixup, struct hid_bpf_ctx *hctx)
{
	__u8 *data = hid_bpf_get_data(hctx, 0, HID_MAX_DESCRIPTOR_SIZE);

	if (!data)
		return 0; /* EPERM check */

	if (hctx->size != MISC_RDESC_SIZE || data[MISC_RDESC_SIZE - 1] != 0xc0)
		return 0;

	__builtin_memcpy(data + MISC_RDESC_SIZE, extra_rdesc, sizeof(extra_rdesc));

	return FIXED_MISC_RDESC_SIZE;
}

static void clear_array(__u8 *data)
{
	int i;

	for (i = KBD_ARRAY_START; i < KBD_ARRAY_END; i++)
		data[i] = 0x00;
}

/*
 * The firmware types its macros one key per report (Meta, then Ctrl, then
 * the key) with Fn still down. Hold the modifiers back until a real key
 * shows up, so a macro arrives as one report and can be rewritten whole.
 */
static void fix_fn_macro(__u8 *data)
{
	bool has_key = false, has_p = false, has_meta = false;
	int i;

	for (i = KBD_ARRAY_START; i < KBD_ARRAY_END; i++) {
		if (data[i] == USAGE_P)
			has_p = true;
		else if (data[i] == USAGE_LEFT_META)
			has_meta = true;
		if (data[i] && data[i] < USAGE_FIRST_MOD)
			has_key = true;
	}

	if (!has_key) {
		clear_array(data);
	} else if (has_p && has_meta) {
		clear_array(data);
		data[KBD_ARRAY_START] = USAGE_F13;
	}
}

static int fix_keyboard_report(__u8 *data)
{
	bool fn = false;
	int i;

	for (i = KBD_ARRAY_START; i < KBD_ARRAY_END; i++) {
		if (data[i] == USAGE_FN) {
			data[i] = 0x00;
			fn = true;
		} else if (data[i] == USAGE_KP_EQUAL) {
			data[i] = USAGE_F15;
		}
	}
	data[KBD_BITMAP_START + USAGE_FN / 8] &= ~(1 << (USAGE_FN % 8));

	if (fn)
		fix_fn_macro(data);

	return 0;
}

/*
 * Injecting reuses the buffer of the event being processed, so the original
 * report is clobbered afterwards: drop it.
 */
static int send_extra_keys(struct hid_bpf_ctx *hctx, __u8 keys)
{
	__u8 report[2] = { EXTRA_REPORT_ID, keys };

	hid_bpf_try_input_report(hctx, HID_INPUT_REPORT, report, sizeof(report));
	report[1] = 0x00;
	hid_bpf_try_input_report(hctx, HID_INPUT_REPORT, report, sizeof(report));

	return -1;
}

static int fix_vendor_report(struct hid_bpf_ctx *hctx, __u8 *data)
{
	switch (data[3]) {
	case VENDOR_FN_F4:
		return send_extra_keys(hctx, EXTRA_MICMUTE);
	case VENDOR_FN_G:
		return send_extra_keys(hctx, EXTRA_F16);
	case VENDOR_FN_F7:
		return send_extra_keys(hctx, EXTRA_F17);
	}

	return 0;
}

static int fix_radio_report(struct hid_bpf_ctx *hctx, __u8 *data)
{
	if (!(data[1] & 0x01))
		return 0;

	return send_extra_keys(hctx, EXTRA_F14);
}

SEC(HID_BPF_DEVICE_EVENT)
int BPF_PROG(a16_device_event, struct hid_bpf_ctx *hctx)
{
	__u8 *data;

	switch (hctx->size) {
	case KBD_REPORT_SIZE:
		data = hid_bpf_get_data(hctx, 0, KBD_REPORT_SIZE);
		if (data && data[0] == KBD_REPORT_ID)
			return fix_keyboard_report(data);
		break;
	case VENDOR_REPORT_SIZE:
		data = hid_bpf_get_data(hctx, 0, VENDOR_REPORT_SIZE);
		if (data && data[0] == VENDOR_REPORT_ID)
			return fix_vendor_report(hctx, data);
		break;
	case RADIO_REPORT_SIZE:
		data = hid_bpf_get_data(hctx, 0, RADIO_REPORT_SIZE);
		if (data && data[0] == RADIO_REPORT_ID)
			return fix_radio_report(hctx, data);
		break;
	}

	return 0;
}

HID_BPF_OPS(gigabyte_a16) = {
	.hid_device_event = (void *)a16_device_event,
	.hid_rdesc_fixup = (void *)a16_rdesc_fixup,
};

SEC("syscall")
int probe(struct hid_bpf_probe_args *ctx)
{
	switch (ctx->rdesc_size) {
	case KBD_RDESC_SIZE:
	case MISC_RDESC_SIZE:
	case FIXED_MISC_RDESC_SIZE:
		ctx->retval = 0;
		break;
	default:
		ctx->retval = -EINVAL;
	}

	return 0;
}

char _license[] SEC("license") = "GPL";
