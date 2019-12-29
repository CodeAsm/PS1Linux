/*
elf2psx: Converts ELFs to PlayStation executables
Copyright (C) GreaseMonkey, 2017, licensed under Creative Commons Zero:
https://creativecommons.org/publicdomain/zero/1.0/

I normally embed this into an assembly file and do some linker wizardry,
but I've decided to write a proper tool for it this time.

DO NOT ATTEMPT TO RUN THIS ON A NON-LITTLE-ENDIAN CPU.
Big-endian is fairly rare these days, and it also sucks.
*/

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

typedef enum region {
	REGION_NTSC_JAPAN,
	REGION_NTSC_NORTH_AMERICA,
	REGION_PAL
} region_t;

typedef struct elf_header {
	uint8_t magic[4];
	uint8_t bitness;
	uint8_t endianness;
	uint8_t elf_version_1;
	uint8_t os_abi;
	uint8_t _pad1[8];

	uint16_t elf_type;
	uint16_t cpu_type;
	uint32_t elf_version_2;
	uint32_t entry_point;
	uint32_t phdr_offs;
	uint32_t shdr_offs;
	uint32_t flags;
	uint16_t ehdr_size;
	uint16_t phdr_ent_size;
	uint16_t phdr_ent_count;
	uint16_t shdr_ent_size;
	uint16_t shdr_ent_count;
	uint16_t strtab_idx;

} __attribute__((__packed__)) elf_header_t;

typedef struct elf_program_header {
	uint32_t type;
	uint32_t offset;
	uint32_t vaddr;
	uint32_t paddr;
	uint32_t filesz;
	uint32_t memsz;
	uint32_t flags;
	uint32_t align;
} __attribute__((__packed__)) elf_program_header_t;
#define PT_LOAD 0x00000001

typedef struct psx_header {
	uint8_t magic[8];
	uint8_t _pad1[8];
	uint32_t pc;
	uint32_t gp;
	uint32_t ftext;
	uint32_t filesz;
	uint32_t unk1[2];
	uint32_t bss_offs;
	uint32_t bss_len;
	uint32_t sp_base;
	uint32_t sp_offs;
	uint32_t rsv1[5];
	uint8_t ascii_marker[0x800-0x4C];
} __attribute__((__packed__)) psx_header_t;

void show_usage(const char *arg0)
{
	printf(
		"elf2psx: Converts ELFs to PlayStation executables\n"
		"Copyright (C) GreaseMonkey, 2017, licensed under Creative Commons Zero:\n"
		"https://creativecommons.org/publicdomain/zero/1.0/\n"
		"\n"
		"usage:\n"
		"\t%s -pnj infile.elf outfile.exe\n"
		"\n"
		"use one of the -p, -n, or -j flags to denote the intended region:\n"
		"\t-j: NTSC Japan\n"
		"\t-n: NTSC North America\n"
		"\t-p: PAL\n"
		"\n"
	, arg0);
}

int main(int argc, char *argv[])
{
	int i;
	uint32_t addr;

	const char *region_flag_str;
	const char *fname_elf;
	const char *fname_psx;
	region_t region_flag;
	elf_header_t ehdr;
	elf_program_header_t *phdrs;
	elf_program_header_t *phdr;
	psx_header_t psxh;
	uint8_t blank_page[0x800];
	uint8_t *tmp_copy_buf;
	FILE *elf;
	FILE *psx;

	uint32_t target_ftext;
	uint32_t target_edata;
	uint32_t target_aedata;

	// Read arguments
	if (argc <= 3) {
		fprintf(stderr, "not enough arguments\n");
		show_usage(argv[0]);
		return 1;
	}

	region_flag_str = argv[1];
	fname_elf = argv[2];
	fname_psx = argv[3];

	// Read region flag
	if(!strcmp(region_flag_str, "-j")) {
		region_flag = REGION_NTSC_JAPAN;
	} else if(!strcmp(region_flag_str, "-n")) {
		region_flag = REGION_NTSC_NORTH_AMERICA;
	} else if(!strcmp(region_flag_str, "-p")) {
		region_flag = REGION_PAL;
	} else {
		fprintf(stderr, "invalid region flag\n");
		show_usage(argv[0]);
		return 1;
	}

	// Open source ELF
	elf = fopen(fname_elf, "rb");
	if (elf == NULL) {
		perror("fopen(elf)");
		return 1;
	}

	// Read header
	if(fread(&ehdr, sizeof(ehdr), 1, elf) != 1) {
		perror("fread(elf)");
		goto fail_close_elf;
	}

	// Check magic
	if(memcmp(ehdr.magic, "\x7F""ELF", 4)) {
		fprintf(stderr, "invalid ELF magic\n");
		goto fail_close_elf;
	}

	// Check if we can actually read the ELF contents
	if(ehdr.bitness != 1) {
		fprintf(stderr, "not a 32-bit ELF\n");
		goto fail_close_elf;
	}
	if(ehdr.endianness != 1) {
		fprintf(stderr, "not a little-endian ELF\n");
		goto fail_close_elf;
	}
	if(ehdr.phdr_ent_size != 0x0020) {
		fprintf(stderr, "unusual PHdr entry size\n");
		goto fail_close_elf;
	}

	// Check if what we're about to do makes sense
	if(ehdr.cpu_type != 8) {
		fprintf(stderr, "not a MIPS ELF\n");
		goto fail_close_elf;
	}
	if(ehdr.elf_type != 2) {
		fprintf(stderr, "not an executable ELF\n");
		goto fail_close_elf;
	}

	// Spew out some info
	// (we'll assume you're trying 
	printf("ELF version: %02X\n", ehdr.elf_version_1);
	printf("ELF version: %08X\n", ehdr.elf_version_2);
	printf("OS ABI:      %02X\n", ehdr.os_abi);
	printf("ELF type:    %04X\n", ehdr.elf_type);
	printf("CPU type:    %04X\n", ehdr.cpu_type);
	printf("Entry point: %08X\n", ehdr.entry_point);
	printf("PHdr offset: %08X\n", ehdr.phdr_offs);
	printf("PHdr esize:  %04X\n", ehdr.phdr_ent_size);
	printf("PHdr ecount: %04X\n", ehdr.phdr_ent_count);

	// Load program headers
	phdrs = malloc(sizeof(*phdr)*ehdr.phdr_ent_count);
	if(fseek(elf, ehdr.phdr_offs, SEEK_SET) != 0) {
		perror("fseek(elf phdr)");
		goto fail_free_phdr_elf;
	}
	if(fread(phdrs, sizeof(*phdr)*ehdr.phdr_ent_count, 1, elf) != 1) {
		perror("fread(elf phdrs)");
		goto fail_free_phdr_elf;
	}

	// Find start and end of program
	target_ftext = 0xFFFFFFFF;
	target_edata = 0x00000000;
	target_aedata = 0x00000000;
	for(i = 0; i < ehdr.phdr_ent_count; i++) {
		phdr = &phdrs[i];

		printf("%05u: %p %08X %08X %08X %08X %08X %08X %08X %08X\n"
			, i
			, phdr
			, phdr->type
			, phdr->offset
			, phdr->vaddr
			, phdr->paddr
			, phdr->filesz
			, phdr->memsz
			, phdr->flags
			, phdr->align
		);

		if(phdr->type == PT_LOAD && phdr->filesz > 0) {
			if(phdr->vaddr < target_ftext) {
				target_ftext = phdr->vaddr;
			}
			if(phdr->vaddr + phdr->filesz > target_edata) {
				target_edata = phdr->vaddr + phdr->filesz;
			}
		}
	}

	// Sanity-check the range
	if(target_ftext > target_edata) {
		fprintf(stderr, "couldn't find any PT_LOAD segments!\n");
		goto fail_free_phdr_elf;
	}
/*	if(target_ftext < 0x80010000) {
		fprintf(stderr, "text segment starts too early\n");
		goto fail_free_phdr_elf;
	}
*/	if(target_edata > 0x80200000) {
		fprintf(stderr, "data segment ends too late\n");
		goto fail_free_phdr_elf;
	}
	if((target_ftext & 0x7FF) != 0) {
		fprintf(stderr, "text segment start not 2KB-aligned\n");
		goto fail_free_phdr_elf;
	}

	// Get 8KB-aligned end
	target_aedata = (target_edata + 0x7FF) & ~0x7FF;
	printf("-----\n");
	printf("File     memory range: %08X -> %08X\n", target_ftext, target_edata);
	printf("Adjusted memory range: %08X -> %08X\n", target_ftext, target_aedata);

	// Open destination PS-X EXE
	psx = fopen(fname_psx, "w+b");
	if(psx == NULL) {
		perror("fopen(psx)");
		goto fail_free_phdr_elf;
	}

	// Prepare header
	memset(&psxh, 0, sizeof(psxh));
	memcpy(psxh.magic, "PS-X EXE", 8);
	psxh.pc = ehdr.entry_point;
	psxh.gp = 0;
	psxh.ftext = target_ftext;
	psxh.filesz = target_aedata - target_ftext;
	psxh.bss_offs = 0;
	psxh.bss_len = 0;
	psxh.sp_base = 0x801FFFF0;
	psxh.sp_offs = 0;

	switch(region_flag) {
		case REGION_NTSC_JAPAN:
			strncpy(psxh.ascii_marker,
				"Sony Computer Entertainment Inc. for Japan area",
				sizeof(psxh.ascii_marker));
			break;

		case REGION_NTSC_NORTH_AMERICA:
			strncpy(psxh.ascii_marker,
				"Sony Computer Entertainment Inc. for North America area",
				sizeof(psxh.ascii_marker));
			break;

		case REGION_PAL:
			strncpy(psxh.ascii_marker,
				"Sony Computer Entertainment Inc. for Europe area",
				sizeof(psxh.ascii_marker));
			break;
	}

	// Write header
	if(fwrite(&psxh, sizeof(psxh), 1, psx) != 1) {
		perror("fwrite(psx head)");
		goto fail_close_psx;
	}

	// Zerofill EXE space
	memset(&blank_page, 0, 0x800);
	for(i = 0; i < psxh.filesz; i += 0x800) {
		if(fwrite(&blank_page, 0x800, 1, psx) != 1) {
			perror("fwrite(psx zero)");
			goto fail_close_psx;
		}
	}

	// Apply PT_LOAD sections
	for(i = 0; i < ehdr.phdr_ent_count; i++) {
		phdr = &phdrs[i];

		if(phdr->type != PT_LOAD) {
			continue;
		}

		if(phdr->filesz == 0) {
			continue;
		}

		// Calculate output address
		addr = phdr->vaddr - target_ftext;
		if(addr < 0 || addr >= psxh.filesz || addr+phdr->filesz > psxh.filesz) {
			fprintf(stderr, "PT_LOAD destination out of range\n");
			goto fail_close_psx;
		}

		// Seek
		if(fseek(elf, phdr->offset, SEEK_SET)) {
			perror("fseek(psx copy - elf)");
			goto fail_close_psx;
		}

		if(fseek(psx, addr + 0x800, SEEK_SET)) {
			perror("fseek(psx copy - psx)");
			goto fail_close_psx;
		}

		// Allocate
		tmp_copy_buf = malloc(phdr->filesz);

		// Copy
		if(fread(tmp_copy_buf, phdr->filesz, 1, elf) != 1) {
			perror("fread(psx copy)");
			goto fail_free_tmp_copy_buf;
		}
		if(fwrite(tmp_copy_buf, phdr->filesz, 1, psx) != 1) {
			perror("fwrite(psx copy)");
			goto fail_free_tmp_copy_buf;
		}
		free(tmp_copy_buf);
	}

	// Close files
	fclose(psx);
	fclose(elf);

	free(phdrs);
	return 0;

	// FAILURES
fail_free_tmp_copy_buf:
	free(tmp_copy_buf);
fail_close_psx:
	fclose(psx);
fail_free_phdr_elf:
	free(phdrs);
fail_close_elf:
	fclose(elf);
	return 1;
}

