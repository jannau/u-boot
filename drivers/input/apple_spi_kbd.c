// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Mark Kettenis <kettenis@openbsd.org>
 */

#include <common.h>
#include <dm.h>
#include <keyboard.h>
#include <spi.h>
#include <stdio_dev.h>
#include <asm-generic/gpio.h>
#include <linux/delay.h>

/*
 * The Apple SPI keyboard controller implements a protocol that
 * closely resembles HID Keyboard Boot protocol.  The key codes are
 * mapped according to the HID Keyboard/Keypad Usage Table.
 */

/* Modifier key bits */
#define HID_MOD_LEFTCTRL	BIT(0)
#define HID_MOD_LEFTSHIFT	BIT(1)
#define HID_MOD_RIGHTCTRL	BIT(4)
#define HID_MOD_RIGHTSHIFT	BIT(5)

/* Modifier key codes */
#define HID_KEY_LEFTCTRL	0xe0
#define HID_KEY_LEFTSHIFT	0xe1
#define HID_KEY_RIGHTCTRL	0xe4
#define HID_KEY_RIGHTSHIFT	0xe5

static const uchar hid_kbd_plain_xlate[] = {
	0xff, 0xff, 0xff, 0xff, 'a', 'b', 'c', 'd',
	'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
	'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
	'u', 'v', 'w', 'x', 'y', 'z', '1', '2',
	'3', '4', '5', '6', '7', '8', '9', '0',
	'\r', 0x1b, '\b', '\t', ' ', '-', '=', '[',
	']', '\\', '#', ';', '\'', '`',	',', '.',
	'/',
};

static const uchar hid_kbd_shift_xlate[] = {
	0xff, 0xff, 0xff, 0xff, 'A', 'B', 'C', 'D',
	'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
	'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
	'U', 'V', 'W', 'X', 'Y', 'Z', '!', '@',
	'#', '$', '%', '^', '&', '*', '(', ')',
	'\r', 0x1b, '\b', '\t', ' ', '_', '+', '{',
	'}', '|', '~', ':', '\"', '~', '<', '>',
	'?',
};

static const uchar hid_kbd_ctrl_xlate[] = {
	0xff, 0xff, 0xff, 0xff, 0x01, 0x02, 0x03, 0x04,
	0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
	0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, '1', 0x00,
	'3', '4', '5', 0x1e, '7', '8', '9', '0',
	'\r', 0x1b, '\b', '\t', ' ', 0x1f, '=', 0x1b,
	0x1d, 0x1c, '#', ';', '\'', '`', ',', '.',
	'/',
};

/* Report ID used for keyboard input reports. */
#define KBD_REPORTID	0x01

struct apple_spi_kbd_report {
	u8 reportid;
	u8 modifiers;
	u8 reserved;
	u8 keycode[6];
	u8 fn;
};

struct apple_spi_kbd_priv {
	struct gpio_desc enable;
	struct apple_spi_kbd_report old;
	struct apple_spi_kbd_report new;
};

/* Keyboard device. */
#define KBD_DEVICE	0x01

struct apple_spi_kbd_packet {
	u8 flags;
#define PACKET_READ	0x20
	u8 device;
	u16 offset;
	u16 remaining;
	u16 len;
	u8 data[246];
	u16 crc;
};

struct apple_spi_kbd_msg {
	u8 type;
#define MSG_REPORT	0x10
	u8 device;
	u8 unknown;
	u8 msgid;
	u16 rsplen;
	u16 cmdlen;
	u8 data[0];
};

static void apple_spi_kbd_service_modifiers(struct input_config *input)
{
	struct apple_spi_kbd_priv *priv = dev_get_priv(input->dev);
	u8 new = priv->new.modifiers;
	u8 old = priv->old.modifiers;

	if ((new ^ old) & HID_MOD_LEFTCTRL)
		input_add_keycode(input, HID_KEY_LEFTCTRL,
				  old & HID_MOD_LEFTCTRL);
	if ((new ^ old) & HID_MOD_RIGHTCTRL)
		input_add_keycode(input, HID_KEY_RIGHTCTRL,
				  old & HID_MOD_RIGHTCTRL);
	if ((new ^ old) & HID_MOD_LEFTSHIFT)
		input_add_keycode(input, HID_KEY_LEFTSHIFT,
				  old & HID_MOD_LEFTSHIFT);
	if ((new ^ old) & HID_MOD_RIGHTSHIFT)
		input_add_keycode(input, HID_KEY_RIGHTSHIFT,
				  old & HID_MOD_RIGHTSHIFT);
}

static void apple_spi_kbd_service_key(struct input_config *input, int i,
				      int released)
{
	struct apple_spi_kbd_priv *priv = dev_get_priv(input->dev);
	u8 *new;
	u8 *old;

	if (released) {
		new = priv->new.keycode;
		old = priv->old.keycode;
	} else {
		new = priv->old.keycode;
		old = priv->new.keycode;
	}

	if (memscan(new, old[i], sizeof(priv->new.keycode)) ==
	    new + sizeof(priv->new.keycode))
		input_add_keycode(input, old[i], released);
}

static int apple_spi_kbd_check(struct input_config *input)
{
	struct udevice *dev = input->dev;
	struct apple_spi_kbd_priv *priv = dev_get_priv(dev);
	struct apple_spi_kbd_packet packet;
	struct apple_spi_kbd_msg *msg;
	struct apple_spi_kbd_report *report;
	int i, ret;

	memset(&packet, 0, sizeof(packet));

	ret = dm_spi_claim_bus(dev);
	if (ret < 0)
		return ret;

	/*
	 * The keyboard controller needs delays after asserting CS#
	 * and before deasserting CS#.
	 */
	dm_spi_xfer(dev, 0, NULL, NULL, SPI_XFER_BEGIN);
	udelay(100);
	dm_spi_xfer(dev, sizeof(packet) * 8, NULL, &packet, 0);
	udelay(100);
	dm_spi_xfer(dev, 0, NULL, NULL, SPI_XFER_END);

	dm_spi_release_bus(dev);

	/*
	 * The keyboard controlled needs a delay between subsequent
	 * SPI transfers.
	 */
	udelay(250);

	msg = (struct apple_spi_kbd_msg *)packet.data;
	report = (struct apple_spi_kbd_report *)msg->data;
	if (packet.flags == PACKET_READ && packet.device == KBD_DEVICE &&
	    msg->type == MSG_REPORT && msg->device == KBD_DEVICE &&
	    msg->cmdlen == sizeof(struct apple_spi_kbd_report) &&
	    report->reportid == KBD_REPORTID) {
		memcpy(&priv->new, report,
		       sizeof(struct apple_spi_kbd_report));
		apple_spi_kbd_service_modifiers(input);
		for (i = 0; i < sizeof(priv->new.keycode); i++) {
			apple_spi_kbd_service_key(input, i, 1);
			apple_spi_kbd_service_key(input, i, 0);
		}
		memcpy(&priv->old, &priv->new,
		       sizeof(struct apple_spi_kbd_report));
		return 1;
	}

	return 0;
}

static int apple_spi_kbd_probe(struct udevice *dev)
{
	struct apple_spi_kbd_priv *priv = dev_get_priv(dev);
	struct keyboard_priv *uc_priv = dev_get_uclass_priv(dev);
	struct stdio_dev *sdev = &uc_priv->sdev;
	struct input_config *input = &uc_priv->input;
	int ret;

	ret = gpio_request_by_name(dev, "spien-gpios", 0, &priv->enable,
				   GPIOD_IS_OUT);
	if (ret < 0)
		return ret;

	/* Reset the keyboard controller. */
	dm_gpio_set_value(&priv->enable, 1);
	udelay(5000);
	dm_gpio_set_value(&priv->enable, 0);
	udelay(5000);

	/* Enable the keyboard controller. */
	dm_gpio_set_value(&priv->enable, 1);
	udelay(50000);

	input->dev = dev;
	input->read_keys = apple_spi_kbd_check;
	input_add_table(input, -1, -1,
			hid_kbd_plain_xlate, ARRAY_SIZE(hid_kbd_plain_xlate));
	input_add_table(input, HID_KEY_LEFTSHIFT, HID_KEY_RIGHTSHIFT,
			hid_kbd_shift_xlate, ARRAY_SIZE(hid_kbd_shift_xlate));
	input_add_table(input, HID_KEY_LEFTCTRL, HID_KEY_RIGHTCTRL,
			hid_kbd_ctrl_xlate, ARRAY_SIZE(hid_kbd_ctrl_xlate));
	strcpy(sdev->name, "spikbd");

	return input_stdio_register(sdev);
}

static const struct keyboard_ops apple_spi_kbd_ops = {
};

static const struct udevice_id apple_spi_kbd_of_match[] = {
	{ .compatible = "apple,spi-hid-transport" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(apple_spi_kbd) = {
	.name = "apple_spi_kbd",
	.id = UCLASS_KEYBOARD,
	.of_match = apple_spi_kbd_of_match,
	.probe = apple_spi_kbd_probe,
	.priv_auto = sizeof(struct apple_spi_kbd_priv),
	.ops = &apple_spi_kbd_ops,
};
