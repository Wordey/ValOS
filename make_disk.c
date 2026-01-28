#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <uchar.h>
#include <string.h>

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

// GPT Partition Entry
typedef struct {
	GUID partition_type_guid;
	GUID unique_guid;
	uint64_t starting_lba;
	uint64_t ending_lba;
	uint64_t attributes;
	char16_t name[36];
} __attribute__ ((packed)) GPT_Partition_Entry;

// FAT32 VBR (aka. Volume Boot Record)
typedef struct {
	uint8_t BS_jmpBoot[3];
	uint8_t BS_OEMName[8];
	uint16_t BPB_BytesPerSec;
	uint8_t BPB_SecPerClus;
	uint16_t BPB_RsvdSecCnt;
	uint8_t BPB_NumFATs;
	uint16_t BPB_RootEntCnt;
	uint16_t BPB_TotSec16;
	uint8_t BPB_Media;
	uint16_t BPB_FATSz16;
	uint16_t BPB_SecPerTrk;
	uint16_t BPB_NumHeads;
	uint32_t BPB_HiddSec;
	uint32_t BPB_TotSec32;
	uint32_t BPB_FATSz32;
	uint16_t BPB_ExtFlags;
	uint16_t BPB_FSVer;
	uint32_t BPB_RootClus;
	uint16_t BPB_FSInfo;
	uint16_t BPB_BkBootSec;
	uint8_t BPB_Reserved[12];
	uint8_t BS_DrvNum;
	uint8_t BS_Reserved;
	uint8_t BS_BootSig;
	uint8_t BS_VolID[4];
	uint8_t BS_VolLab[11];
	uint8_t BS_FilSysType[8];

	uint8_t boot_code[510-90];
	uint16_t bootsect_sig;
} __attribute__ ((packed)) VBR;

typedef struct {
	uint32_t FSI_LeadSig;
	uint8_t FSI_Reserved[480];
	uint32_t FSI_StructSig;
	uint32_t FSI_Free_Count;
	uint32_t FSI_Nxt_Free;
	uint8_t FSI_Reserved2[12];
	uint32_t FSI_TrailSig;
} __attribute__ ((packed)) FSInfo;

// FAT32 Directory Entry
typedef struct {
	uint8_t DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t DIR_NTRes;
	uint8_t DIR_CrtTimeTenth;
	uint16_t DIR_CrtTime;
	uint16_t DIR_CrtDate;
	uint16_t DIR_LstAccDate;
	uint16_t DIR_FstClusHI;
	uint16_t DIR_WrtTime;
	uint16_t DIR_WrtDate;
	uint16_t DIR_FstClusLO;
	uint32_t DIR_FileSize;
} __attribute__ ((packed)) FAT32_Dir_Entry; 

// FAT32 Dir Attributes
typedef enum {
	ATTR_READ_ONLY	= 0x01,
	ATTR_HIDDEN	= 0x02,
	ATTR_SYSTEM	= 0x03,
	ATTR_VOLUME_ID	= 0x08,
	ATTR_DIRECTORY	= 0x10,
	ATTR_ARCHIVE	= 0x20,
	ATTR_LONG_NAME	= ATTR_READ_ONLY | ATTR_HIDDEN |
			  ATTR_SYSTEM    | ATTR_VOLUME_ID,
} FAT32_Attributes;

//---------------------|
//---Enums, Constans---|
//---------------------|
enum {
	GPT_TABLE_ENTRY_SIZE = 128,
	NUMBER_OF_GPT_TABLE_ENTRIES = 128,
	GPT_TABLE_SIZE = 16384,
	ALIGNMENT = 1048576,
};

const GUID ESP_GUID = { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B,
			{ 0x00, 0xA0, 0xC9, 0x3B} };

const GUID BDP_GUID = { 0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0,
			{ 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } };

//-----------------------------|
//-----------Values------------|
//-----------------------------|
char* image_name = "ValOS.img";						// Name of Disk
uint64_t lba_size = 512;						// Size of LBA
uint64_t esp_size = 1024*1024*33; 					//33Mib
uint64_t data_size = 1024*1024*1;					//1Mib
uint64_t image_size = 0;						// I calculate It later
uint64_t image_size_lbas = 0, esp_size_lbas = 0, data_size_lbas = 0,	// 
	 gpt_table_lbas = 0;						// Sizes in LBAs
uint32_t crc_table[256];						// CRC table. don't touch!
uint64_t align_lba = 0, esp_lba = 0, data_lba = 0;			// First lba values



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

//--------------------------|
//----Get-Next-Aligned-LBA--|
//--------------------------|
uint64_t get_next_aligned_lba(const uint64_t lba) {
	return lba - (lba % align_lba) + align_lba;
}

//------------------------------------|
//----Get-New-Date/Time-For-FAT32-----|
//------------------------------------|
void get_fat_dir_entry_time_and_and_date(uint16_t *in_time, uint16_t *in_date) {
	time_t curr_time;
	curr_time = time(NULL);
	struct tm tm = *localtime(&curr_time);
	if (tm.tm_sec == 60) tm.tm_sec = 59;
	*in_time = tm.tm_hour << 11 | tm.tm_min << 5 | (tm.tm_sec / 2);
	*in_date = ((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday;
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
		.header_crc32 = 0,						// I calculate it later
		.reserved = 0,
		.my_lba = 1,
		.alternate_lba = image_size_lbas - 1,
		.first_usable_lba = 1 + 1 + gpt_table_lbas,			// MBR + GPT + primary gpt table
		.last_usable_lba = image_size_lbas - 1 - 1 - gpt_table_lbas,	// image_size_lbas - MBR - GPT - primary gpt table
		.disk_guid = generate_guid(),
		.partition_table_lba = 2,
		.number_of_entries = 128,
		.size_of_entry = 128,
		.partition_table_crc32 = 0, // I calculate it later

		.reserved_2 = { 0 },
	};

	// Fill out primary table partition entries
	GPT_Partition_Entry gpt_table[NUMBER_OF_GPT_TABLE_ENTRIES] = {
		// ESP (aka. EFI system partition)
		{
			.partition_type_guid = ESP_GUID,
			.unique_guid = generate_guid(),
			.starting_lba = esp_lba,
			.ending_lba = esp_lba + esp_size_lbas - 1,
			.attributes = 0,
			.name = u"ESP"
		},
		// BDP (aka. Basic data partition)
		{
			.partition_type_guid = BDP_GUID,
			.unique_guid = generate_guid(),
			.starting_lba = data_lba,
			.ending_lba = data_lba + data_size_lbas - 1,
			.attributes = 0,
			.name = u"BDP",
		},
	};

	// Fill out primary header crc32 values
	primary_gpt.partition_table_crc32 = calculate_crc32_table(gpt_table, sizeof gpt_table);
	primary_gpt.header_crc32 = calculate_crc32_table(&primary_gpt, primary_gpt.header_size);

	// Write primary header to file
	if(fwrite(&primary_gpt, 1, sizeof primary_gpt, image) != sizeof primary_gpt)
		return false;
	write_full_lba(image);

	// Write primary gpt table to file
	if(fwrite(&gpt_table, 1, sizeof gpt_table, image) != sizeof gpt_table)
		return false;

	// secondary's gpt's Fill out
	GPT_Header secondary_gpt = primary_gpt;

	secondary_gpt.header_crc32 = 0;
	secondary_gpt.partition_table_crc32 = 0;
	secondary_gpt.my_lba = primary_gpt.alternate_lba;
	secondary_gpt.alternate_lba = primary_gpt.my_lba;
	secondary_gpt.partition_table_lba = image_size_lbas - 1 - gpt_table_lbas;

	// Fill out secondary header crc32 values
	secondary_gpt.partition_table_crc32 = calculate_crc32_table(gpt_table, sizeof gpt_table);
	secondary_gpt.header_crc32 = calculate_crc32_table(&secondary_gpt, secondary_gpt.header_size);


	// Go to position of secondary gpt
	fseek(image, (secondary_gpt.partition_table_lba * lba_size), SEEK_SET);

	// Write primary gpt table to file
	if(fwrite(&secondary_gpt, 1, sizeof secondary_gpt, image) != sizeof secondary_gpt)
		return false;
	write_full_lba(image);

	// Write Backups
	fseek(image, secondary_gpt.partition_table_lba * lba_size, SEEK_SET);
	if (fwrite(&gpt_table, 1, sizeof gpt_table, image) != sizeof gpt_table)
		return false;

	fseek(image, secondary_gpt.my_lba * lba_size, SEEK_SET);
	if (fwrite(&secondary_gpt, 1, sizeof secondary_gpt, image) != sizeof secondary_gpt)
		return false;


	return true;
}

//-----------------------------|
//---Write-ESP-With-FAT32-FS---|
//-----------------------------|
bool write_esp(FILE* image) {
	// ------------------------------ reserved sector region ------------------------------
	const uint8_t reserved_sector = 32;

	// Fill out VBR
	VBR vbr = {
		.BS_jmpBoot = { 0xEB, 0x00, 0x90 },
		.BS_OEMName = { "VALOS   " },
		.BPB_BytesPerSec = lba_size,
		.BPB_SecPerClus = 1,
		.BPB_RsvdSecCnt = reserved_sector,
		.BPB_NumFATs = 2,
		.BPB_RootEntCnt = 0,
		.BPB_TotSec16 = 0,
		.BPB_Media = 0xF8,
		.BPB_FATSz16 = 0,
		.BPB_SecPerTrk = 0,
		.BPB_NumHeads = 0,
		.BPB_HiddSec = esp_lba - 1,
		.BPB_TotSec32 = esp_size_lbas,
		.BPB_FATSz32 = (align_lba - reserved_sector) / 2,
		.BPB_ExtFlags = 0,
		.BPB_FSVer = 0,
		.BPB_RootClus = 2,
		.BPB_FSInfo = 1,
		.BPB_BkBootSec = 6,
		.BPB_Reserved = { 0 },
		.BS_DrvNum = 0x80,
		.BS_Reserved = 0,
		.BS_BootSig = 0x29,
		.BS_VolID = { 0 },
		.BS_VolLab = { "NO NAME    " },
		.BS_FilSysType = { "FAT32   " },

		.boot_code = { 0 },
		.bootsect_sig = 0xAA55,
	};

	// Fill out FSInfo Sector
	FSInfo fsinfo = {
		.FSI_LeadSig = 0x41615252,
		.FSI_Reserved = { 0 },
		.FSI_StructSig = 0x61417272,
		.FSI_Free_Count = 0xFFFFFFFF,
		.FSI_Nxt_Free = 0xFFFFFFFF,
		.FSI_Reserved2 = { 0 },
		.FSI_TrailSig = 0xAA550000,
	};

	// Write VBR && FSInfo
	fseek(image, esp_lba * lba_size, SEEK_SET);
	if (fwrite(&vbr, 1, sizeof vbr, image) != sizeof vbr) {
		fprintf(stderr, "Error on step writing VBR 1");
		return false;
	}
	write_full_lba(image);

	if (fwrite(&fsinfo, 1, sizeof fsinfo, image) != sizeof fsinfo) {
		fprintf(stderr, "Error on step writing SYSINFO SEC 1");
		return false;
	}
	write_full_lba(image);

	// Go to Backup Sector
	fseek(image, (esp_lba + vbr.BPB_BkBootSec) * lba_size, SEEK_SET);

	// Write VBR && FSInfo
	fseek(image, esp_lba * lba_size, SEEK_SET);
	if (fwrite(&vbr, 1, sizeof vbr, image) != sizeof vbr) {
		fprintf(stderr, "Error on step writing VBR 2");
		return false;
	}
	write_full_lba(image);

	if (fwrite(&fsinfo, 1, sizeof fsinfo, image) != sizeof fsinfo) {
		fprintf(stderr, "Error on step writing SYSINFO SEC 2");
		return false;
	}
	write_full_lba(image);


	// ----------------------------------- FAT region --------------------------------------
	// Write FATs
	const uint32_t fat_lba = esp_lba + vbr.BPB_RsvdSecCnt;
	for (uint8_t i = 0; i < vbr.BPB_NumFATs; i++) {
		fseek(image, (fat_lba  + (i * vbr.BPB_FATSz32)) * lba_size, SEEK_SET);

		uint32_t cluster = 0;
	
		// Cluster 0; FAT identifier, lowest 8 bit are the media type/byte
		cluster = 0xFFFFFF00 | vbr.BPB_Media;
		fwrite(&cluster, sizeof cluster, 1, image);

		// Cluster 1; EOC marker
		cluster = 0xFFFFFFFF;
		fwrite(&cluster, sizeof cluster, 1, image);

		// Cluster 2; Root directory
		cluster = 0xFFFFFFFF;
		fwrite(&cluster, sizeof cluster, 1, image);

		// Cluster 3; /EFI
		cluster = 0xFFFFFFFF;
		fwrite(&cluster, sizeof cluster, 1, image);

		// Cluster 4; /EFI/BOOT
		cluster = 0xFFFFFFFF;
		fwrite(&cluster, sizeof cluster, 1, image);
	}

	// ----------------------------------- DATA region --------------------------------------
	// Write data
	const uint32_t data_lba = fat_lba + (vbr.BPB_NumFATs * vbr.BPB_FATSz32);
	fseek(image, data_lba * lba_size, SEEK_SET);

	// Root Dir entries
	// '/EFI'
	FAT32_Dir_Entry dir_ent = {
		.DIR_Name = { "EFI        " },
		.DIR_Attr = ATTR_DIRECTORY,
		.DIR_NTRes = 0,
		.DIR_CrtTimeTenth = 0,
		.DIR_CrtTime = 0,
		.DIR_CrtDate = 0,
		.DIR_LstAccDate = 0,
		.DIR_FstClusHI = 0,
		.DIR_WrtTime = 0,
		.DIR_WrtDate = 0,
		.DIR_FstClusLO = 3,
		.DIR_FileSize = 0
	};

	uint16_t create_time = 0, create_date = 0;
	get_fat_dir_entry_time_and_and_date(&create_time, &create_date);

	dir_ent.DIR_CrtTime = create_time;
	dir_ent.DIR_CrtDate = create_date;
	dir_ent.DIR_WrtTime = create_time;
	dir_ent.DIR_WrtDate = create_date;

	fwrite(&dir_ent, sizeof dir_ent, 1, image);

	// EFI Dir
	fseek(image, (data_lba + 1) * lba_size, SEEK_SET);
	memcpy(dir_ent.DIR_Name, ".          ", 11);
	fwrite(&dir_ent, sizeof dir_ent, 1, image);
	memcpy(dir_ent.DIR_Name, "..         ", 11);
	dir_ent.DIR_FstClusLO = 0;
	fwrite(&dir_ent, sizeof dir_ent, 1, image);
	memcpy(dir_ent.DIR_Name, "BOOT       ", 11);
	dir_ent.DIR_FstClusLO = 4;
	fwrite(&dir_ent, sizeof dir_ent, 1, image);

	// EFI/BOOT dir
	fseek(image, (data_lba + 2) * lba_size, SEEK_SET);
	memcpy(dir_ent.DIR_Name, ".          ", 11);
	fwrite(&dir_ent, sizeof dir_ent, 1, image);
	memcpy(dir_ent.DIR_Name, "..         ", 11);
	dir_ent.DIR_FstClusLO = 3;
	fwrite(&dir_ent, sizeof dir_ent, 1, image);


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
	gpt_table_lbas = GPT_TABLE_SIZE / lba_size;

	const uint64_t padding = (ALIGNMENT * 2 + (lba_size * ((gpt_table_lbas*2)) + 1 + 2));
	image_size = esp_size + data_size + padding; // ESP + DATA + extra padding
	image_size_lbas = bytes_to_lbas(image_size);
	align_lba = ALIGNMENT / lba_size;
	esp_lba = align_lba;
	esp_size_lbas = bytes_to_lbas(esp_size);
	data_size_lbas = bytes_to_lbas(data_size);
	data_lba = get_next_aligned_lba(esp_lba + esp_size_lbas);
	image_size_lbas = data_lba + data_size_lbas + 34;

	// Need for no errors in sgdisk DON'T TOUCH!
	fseek(image, (image_size_lbas * lba_size) - 1, SEEK_SET);
    	fputc(0, image);
    	rewind(image);



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

	// Write ESP with FAT32 FS
	if (!write_esp(image)) {
		fprintf(stderr, "ERROR: Cannot write ESP With FAT32 For disk!\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
