#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

//--------------------|
//------Typedefs------|
//--------------------|
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

//---------------------------|
//---Write-Protective-MBR----|
//---------------------------|
bool write_mbr(FILE* image) {
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
			.size_lba = image_size_lbas - 1,
		},
		.boot_signature = 0xAA55,
	};

	if (fwrite(&mbr, 1, sizeof mbr, image) != sizeof mbr) {
		return false;
	}

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

	if (!write_mbr(image)) {
		fprintf(stderr, "ERROR: Cannot create Protective MBR Fot disk!\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
