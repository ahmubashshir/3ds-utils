#ifndef _NCCH_H_
#define _NCCH_H_

#include <stdio.h>
#include "types.h"
#include "keyset.h"
#include "filepath.h"
#include "ctr.h"
#include "exefs.h"
#include "romfs.h"
#include "exheader.h"
#include "settings.h"

typedef enum
{
	NCCHTYPE_EXHEADER = 1,
	NCCHTYPE_EXEFS = 2,
	NCCHTYPE_ROMFS = 3,
	NCCHTYPE_LOGO = 4,
	NCCHTYPE_PLAINRGN = 5,
} ctr_ncchtypes;

typedef enum
{
	NCCHCRYPTO_NONE		= 0,		//< already decrypted
	NCCHCRYPTO_FIXED	= 1,		//< fixed key crypto, used for SDK-made application titles and very very old system titles
	NCCHCRYPTO_SECURE	= (1<<1),	//< hardware generated key
	NCCHCRYPTO_BROKEN	= 0xFF		//< Internal: failed to generate key, but encryption still used
} ctr_ncchcryptotype;

typedef struct
{
	u8 signature[0x100];
	u8 magic[4];
	u8 contentsize[4];
	u8 titleid[8];
	u8 makercode[2];
	u8 version[2];
	u8 seedcheck[4];
	u8 programid[8];
	u8 reserved1[0x10];
	u8 logohash[0x20];
	u8 productcode[0x10];
	u8 extendedheaderhash[0x20];
	u8 extendedheadersize[4];
	u8 reserved2[4];
	u8 flags[8];
	u8 plainregionoffset[4];
	u8 plainregionsize[4];
	u8 logooffset[4];
	u8 logosize[4];
	u8 exefsoffset[4];
	u8 exefssize[4];
	u8 exefshashregionsize[4];
	u8 reserved4[4];
	u8 romfsoffset[4];
	u8 romfssize[4];
	u8 romfshashregionsize[4];
	u8 reserved5[4];
	u8 exefssuperblockhash[0x20];
	u8 romfssuperblockhash[0x20];
} ctr_ncchheader;


typedef struct
{
	FILE* file;
	u8 key[2][16];
	u8 seed[16];
	u32 encrypted;
	u64 offset;
	u64 size;
	settings* usersettings;
	ctr_ncchheader header;
	ctr_aes_context aes;
	exefs_context exefs;
	romfs_context romfs;
	exheader_context exheader;
	int exefshashcheck;
	int romfshashcheck;
	int exheaderhashcheck;
	int logohashcheck;
	int headersigcheck;
	u64 extractsize;
	u32 extractflags;
} ncch_context;

void ncch_init(ncch_context* ctx);
void ncch_process(ncch_context* ctx, u32 actions);
void ncch_set_offset(ncch_context* ctx, u64 offset);
void ncch_set_size(ncch_context* ctx, u64 size);
void ncch_set_file(ncch_context* ctx, FILE* file);
void ncch_set_usersettings(ncch_context* ctx, settings* usersettings);
u64 ncch_get_exefs_offset(ncch_context* ctx);
u64 ncch_get_exefs_size(ncch_context* ctx);
u64 ncch_get_romfs_offset(ncch_context* ctx);
u64 ncch_get_romfs_size(ncch_context* ctx);
u64 ncch_get_exheader_offset(ncch_context* ctx);
u64 ncch_get_exheader_size(ncch_context* ctx);
u64 ncch_get_logo_offset(ncch_context* ctx);
u64 ncch_get_logo_size(ncch_context* ctx);
u64 ncch_get_plainrgn_offset(ncch_context* ctx);
u64 ncch_get_plainrgn_size(ncch_context* ctx);
void ncch_print(ncch_context* ctx);
int ncch_signature_verify(ncch_context* ctx, rsakey2048* key);
void ncch_verify(ncch_context* ctx, u32 flags);
void ncch_save(ncch_context* ctx, u32 type, u32 flags);
int ncch_extract_prepare(ncch_context* ctx, u32 type, u32 flags);
int ncch_extract_buffer(ncch_context* ctx, u8* buffer, u32 buffersize, u32* outsize, u8 nocrypto);
u64 ncch_get_mediaunit_size(ncch_context* ctx);
void ncch_get_counter(ncch_context* ctx, u8 counter[16], u8 type);
void ncch_determine_key(ncch_context* ctx, u32 actions);
#endif // _NCCH_H_
