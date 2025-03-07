// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2021 Mark Kettenis <kettenis@openbsd.org>
 */

#include <common.h>
#include <dm.h>
#include <efi_loader.h>

#include <asm/armv8/mmu.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/system.h>

DECLARE_GLOBAL_DATA_PTR;

/* Apple M1 */

static struct mm_region t8103_mem_map[] = {
	{
		/* I/O */
		.virt = 0x200000000,
		.phys = 0x200000000,
		.size = 2UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x380000000,
		.phys = 0x380000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x500000000,
		.phys = 0x500000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x680000000,
		.phys = 0x680000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x6a0000000,
		.phys = 0x6a0000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x6c0000000,
		.phys = 0x6c0000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* RAM */
		.virt = 0x800000000,
		.phys = 0x800000000,
		.size = 8UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Framebuffer */
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

/* Apple M1 Pro/Max */

static struct mm_region t6000_mem_map[] = {
	{
		/* I/O */
		.virt = 0x280000000,
		.phys = 0x280000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x380000000,
		.phys = 0x380000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x580000000,
		.phys = 0x580000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x5a0000000,
		.phys = 0x5a0000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x5c0000000,
		.phys = 0x5c0000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x700000000,
		.phys = 0x700000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0xb00000000,
		.phys = 0xb00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0xf00000000,
		.phys = 0xf00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x1300000000,
		.phys = 0x1300000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* RAM */
		.virt = 0x10000000000,
		.phys = 0x10000000000,
		.size = 16UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Framebuffer */
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map;

int board_init(void)
{
	return 0;
}

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

extern long fw_dtb_pointer;

void *board_fdt_blob_setup(int *err)
{
	/* Return DTB pointer passed by m1n1 */
	*err = 0;
	return (void *)fw_dtb_pointer;
}

void build_mem_map(void)
{
	ofnode node;
	fdt_addr_t base;
	fdt_size_t size;
	int i;

	if (of_machine_is_compatible("apple,t8103"))
		mem_map = t8103_mem_map;
	else if (of_machine_is_compatible("apple,t6000"))
		mem_map = t6000_mem_map;
	else if (of_machine_is_compatible("apple,t6001"))
		mem_map = t6000_mem_map;
	else
		panic("Unsupported SoC\n");

	/* Find list terminator. */
	for (i = 0; mem_map[i].size || mem_map[i].attrs; i++)
		;

	/* Align RAM mapping to page boundaries */
	base = gd->bd->bi_dram[0].start;
	size = gd->bd->bi_dram[0].size;
	size += (base - ALIGN_DOWN(base, SZ_4K));
	base = ALIGN_DOWN(base, SZ_4K);
	size = ALIGN(size, SZ_4K);

	/* Update RAM mapping */
	mem_map[i - 2].virt = base;
	mem_map[i - 2].phys = base;
	mem_map[i - 2].size = size;

	node = ofnode_path("/chosen/framebuffer");
	if (!ofnode_valid(node))
		return;

	base = ofnode_get_addr_size(node, "reg", &size);
	if (base == FDT_ADDR_T_NONE)
		return;

	/* Align framebuffer mapping to page boundaries */
	size += (base - ALIGN_DOWN(base, SZ_4K));
	base = ALIGN_DOWN(base, SZ_4K);
	size = ALIGN(size, SZ_4K);

	/* Add framebuffer mapping */
	mem_map[i - 1].virt = base;
	mem_map[i - 1].phys = base;
	mem_map[i - 1].size = size;
}

void enable_caches(void)
{
	build_mem_map();

	icache_enable();
	dcache_enable();
}

u64 get_page_table_size(void)
{
	return SZ_256K;
}

int board_late_init(void)
{
	unsigned long base;
	unsigned long top;
	u32 status = 0;

	/* Reserve 4M each for scriptaddr and pxefile_addr_r at the top of RAM
	 * at least 1M below the stack. */
	top = gd->start_addr_sp - CONFIG_STACK_SIZE - SZ_8M - SZ_1M;
	top = ALIGN_DOWN(top, SZ_8M);

	status |= env_set_hex("scriptaddr", top + SZ_4M);
	status |= env_set_hex("pxefile_addr_r", top);

	/* somewhat based on the Linux Kernel boot requirements:
	 * align by 2M and maximal FDT size 2M */
	base = ALIGN(gd->ram_base, SZ_2M);

	status |= env_set_hex("fdt_addr_r", base);
	status |= env_set_hex("kernel_addr_r", base + SZ_2M);
	status |= env_set_hex("ramdisk_addr_r", base + SZ_128M);
	status |= env_set_hex("loadaddr", base + SZ_2G);

	if (status)
		log_warning("late_init: Failed to set run time variables\n");

	return 0;
}
