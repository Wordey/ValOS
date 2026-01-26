#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

//--------------------|
//------Typedefs------|
//--------------------|
// GUID(aka. UUID)
typedef struct {
	uint8_t time_low;
} __attribute__ ((packed)) GUID;

//MBR Partition
typedef struct {
	uint8_t boot_indicator;
	uint8_t starting_chs[3];
	uint8_t os_type;
	uint8_t ending_chs[3];
	uint32_t starting_lba;
	uint32_t size_lba;
} __attribute__ ((packed)) MBR_Partition;

// MBR (aka. Master Boot Record)
typedef struct {
	uint8_t boot_code[440];
	uint16_t unknown;
	uint32_t mbr_signature; // Unique MBR Disk Signature
	MBR_Partition partition[4];
	uint16_t boot_signature;
} __attribute__ ((packed)) MBR;

// GPT Header
typedef struct {
	uint8_t signature[8];
	uint32_t revision;
	uint32_t header_size;
	uint32_t header_crc32;
	uint32_t reserved;
	uint64_t my_lba;
	uint64_t alternate_lba;
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	GUID disk_guid;
	uint64_t partition_table_lba;
	uint32_t number_of_entries;
	uint32_t size_of_entry;
	uint32_t partition_table_crc32;

	uint8_t reserved_2[512-92];
} __attribute__ ((packed)) GPT_Header;

//-----------------------------|
//-----------Values------------|
//-----------------------------|
char* image_name = "ValOS.img";
uint64_t lba_size = 512;
uint64_t esp_size = 1024*1024*33; 	//33Mib
uint64_t data_size = 1024*1024*1;	//1Mib
uint64_t image_size = 0;		// I calculate It later
uint64_t image_size_lbas = 0, esp_size_lbas = 0, data_size_lbas = 0;

//-----------------------------|
//----Convert-Bytes-To-LBAs----|
//-----------------------------|
uint64_t bytes_to_lbas(const uint64_t bytes) {
	return (bytes / lba_size) + (bytes % lba_size > 0 ? 1 : 0);
}

//-----------------------------|
//----Pad-0s-to-full-LBAs------|
//-----------------------------|
void write_full_lba(FILE* image) {
	uint64_t zero_sector[512];
	for (uint8_t i = 0; i < (lba_size - sizeof zero_sector) / sizeof zero_sector; i++) {
		fwrite(&zero_sector, sizeof zero_sector, 1, image);
	}
}

//---------------------------|
//---Generate-New-GUID-------|
//---------------------------|
uint64_t generate_guid() {
}

//---------------------------|
//---Write-Protective-MBR----|
//---------------------------|
bool write_mbr(FILE* image) {
	uint64_t mbr_image_lbas = image_size_lbas;
	if (mbr_image_lbas > 0xFFFFFFFF) mbr_image_lbas = 0x100000000;

	MBR mbr = {
		.boot_code = { 0 },
		.mbr_signature = 0,
		.unknown = 0,
		.partition[0] = {
			.boot_indicator = 0,
			.starting_chs = { 0x00, 0x02, 0x00 },
			.os_type = 0xEE,
			.ending_chs = { 0xFF, 0xFF, 0xFF },
			.starting_lba = 0x0000001,
			.size_lba = mbr_image_lbas - 1,
		},
		.boot_signature = 0xAA55,
	};

	if (fwrite(&mbr, 1, sizeof mbr, image) != sizeof mbr) {
		return false;
	}
	//write_full_lba(image);

	return true;
}

//---------------------------------|
//---Write-GPT-Headers-&&-Tables---|
//---------------------------------|
bool write_gpts(FILE* image) {
	GPT_Header primary_gpt = {
		.signature = { "EFI PART" },
		.revision = 0x00010000,
		.header_size = 92,
		.header_crc32 = 0,	// I calculate it later
		.reserved = 0,
		.my_lba = 1,
		.alternate_lba = image_size_lbas - 1,
		.first_usable_lba = 1 + 1 + 32,		// MBR + GPT + primary gpt table
		.last_usable_lba = image_size_lbas - 1 - 1 - 32,	// image_size_lbas - MBR - GPT - primary gpt table
		.disk_guid = generate_guid(),
	};

	return true;
}

//---------------------|
//--------Main---------|
//---------------------|
int main(void) {
	FILE* image = fopen(image_name, "wb+");

	if (!image) {
		fprintf(stderr, "ERROR: Cannot create new disk image!\n");
		return EXIT_FAILURE;
	}

	//Set values
	image_size = esp_size + data_size + (1024*1024); // ESP + DATA + extra padding
	image_size_lbas = bytes_to_lbas(image_size);

	// Write Prtoective MBR
	if (!write_mbr(image)) {
		fprintf(stderr, "ERROR: Cannot create Protective MBR For disk!\n");
		return EXIT_FAILURE;
	}

	// Write GPT Headers && Tables
	if (!write_gpts(image)) {
		fprintf(stderr, "ERROR: Cannot create GPT Headers && Tables For disk!\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
