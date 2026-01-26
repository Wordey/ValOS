#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

//--------------------|
//------Typedefs------|
//--------------------|
// GUID(aka. UUID)
typedef struct {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_ver;
	uint8_t clock_seq_hi_and_res;
	uint8_t clock_seq_low;
	uint8_t node[6];
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
uint32_t crc_table[256];		// CRC table. don't touch!

//-----------------------------|
//----Convert-Bytes-To-LBAs----|
//-----------------------------|
uint64_t bytes_to_lbas(const uint64_t bytes) {
	return (bytes / lba_size) + (bytes % lba_size > 0 ? 1 : 0);
}

//-----------------------------|
//----Pad-0s-to-full-LBAs------|
//-----------------------------|
void write_full_lba(FILE *image) {
	uint8_t zero_sector[512];
	for (uint8_t i = 0; i < (lba_size - sizeof zero_sector) / sizeof zero_sector; i++)
		fwrite(zero_sector, sizeof zero_sector, 1, image);
}

//---------------------------|
//---Generate-New-GUID-------|
//---------------------------|
GUID generate_guid(void) {
	uint8_t rand_arr[16] = { 0 };

	for (uint8_t i = 0; i < sizeof rand_arr; i++) {
		rand_arr[i] = rand() % (UINT8_MAX + 1);
	}

	// GUIDs fill out
	GUID result = {
		.time_low 		= *(uint32_t *)&rand_arr[0],
		.time_mid 	 	= *(uint16_t *)&rand_arr[4],
		.time_hi_and_ver 	= *(uint16_t *)&rand_arr[6],
		.clock_seq_hi_and_res	= rand_arr[8],
		.clock_seq_low		= rand_arr[9],
		.node			= {	rand_arr[10], rand_arr[11], rand_arr[12], rand_arr[13],
						rand_arr[14], rand_arr[15] },
	};

	// versions bits fill out
	result.time_hi_and_ver &= ~(1 << 15);
	result.time_hi_and_ver |= (1 << 14);
	result.time_hi_and_ver &= ~(1 << 13);

	// variants bits fill out
	result.clock_seq_hi_and_res |= (1 << 7);
	result.clock_seq_hi_and_res |= (1 << 6);
	result.clock_seq_hi_and_res &= ~(1 << 5);
	

	return result;
}

//------------------------------|
//----Create-New-CRC32-Table----|
//------------------------------|
void create_new_crc32_table(void) {
  uint32_t c;

  for (uint32_t n = 0; n < 256; n++) {
    c = (uint32_t) n;
    for (int32_t k = 0; k < 8; k++) {
      if (c & 1)
        c = 0xedb88320L ^ (c >> 1);
      else
        c = c >> 1;
    }
    crc_table[n] = c;
  }
}

//------------------------------|
//----Create-New-CRC32-Table----|
//------------------------------|
uint32_t calculate_crc32_table(void *buf, int32_t len) {
	static bool made_crc_table = false;
	uint32_t c = 0xFFFFFFFFL;
	uint8_t *bufp = buf;

	if (!made_crc_table) {
		create_new_crc32_table();
		made_crc_table = true;
	}

 	for (int32_t n = 0; n < len; n++) {
		c = crc_table[(c ^ bufp[n]) & 0xff] ^ (c >> 8);
  	}

	return c ^ 0xFFFFFFFFL;
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
	write_full_lba(image);

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
		.partition_table_lba = 2,
		.number_of_entries = 128,
		.size_of_entry = 128,
		.partition_table_crc32 = 0, // I calculate it later

		.reserved_2 = { 0 },
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

	// Seed random number generation
	srand(time(NULL));

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
