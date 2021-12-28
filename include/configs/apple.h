#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>

#define CONFIG_SYS_LOAD_ADDR	0x880000000
#define CONFIG_SYS_MALLOC_LEN	SZ_64M

#define CONFIG_SYS_PCI_64BIT

#define CONFIG_SYS_SDRAM_BASE	0x880000000

#define CONFIG_LNX_KRNL_IMG_TEXT_OFFSET_BASE	CONFIG_SYS_TEXT_BASE

/* Environment */
#define ENV_DEVICE_SETTINGS \
	"stdin=serial,usbkbd,spikbd\0" \
	"stdout=serial,vidconsole\0" \
	"stderr=serial,vidconsole\0"

#define ENV_MEM_LAYOUT_SETTINGS \
	"fdt_addr_r=0x960100000\0" \
	"kernel_addr_r=0x960200000\0"

#if CONFIG_IS_ENABLED(CMD_NVME)
	#define BOOT_TARGET_NVME(func) func(NVME, nvme, 0)
#else
	#define BOOT_TARGET_NVME(func)
#endif

#if CONFIG_IS_ENABLED(CMD_USB)
	#define BOOT_TARGET_USB(func) func(USB, usb, 0)
#else
	#define BOOT_TARGET_USB(func)
#endif

#define BOOT_TARGET_DEVICES(func) \
	BOOT_TARGET_NVME(func) \
	BOOT_TARGET_USB(func)

#include <config_distro_bootcmd.h>

#define CONFIG_EXTRA_ENV_SETTINGS \
	ENV_DEVICE_SETTINGS \
	ENV_MEM_LAYOUT_SETTINGS \
	BOOTENV

#endif
