#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "romfs.h"
#include "utils.h"

void romfs_init(romfs_context* ctx)
{
	memset(ctx, 0, sizeof(romfs_context));
	ivfc_init(&ctx->ivfc);
}

void romfs_set_file(romfs_context* ctx, FILE* file)
{
	ctx->file = file;
}

void romfs_set_offset(romfs_context* ctx, u64 offset)
{
	ctx->offset = offset;
}

void romfs_set_size(romfs_context* ctx, u64 size)
{
	ctx->size = size;
}

void romfs_set_usersettings(romfs_context* ctx, settings* usersettings)
{
	ctx->usersettings = usersettings;
}

void romfs_set_encrypted(romfs_context* ctx, u32 encrypted)
{
	ctx->encrypted = encrypted;
}

void romfs_set_key(romfs_context* ctx, u8 key[16])
{
	memcpy(ctx->key, key, 16);
	ctr_init_key(&ctx->aes, key);
}

void romfs_set_counter(romfs_context* ctx, u8 counter[16])
{
	memcpy(ctx->counter, counter, 16);
}

void romfs_fseek(romfs_context* ctx, u64 offset)
{
	u64 data_pos = offset - ctx->offset;
	fseeko64(ctx->file, offset, SEEK_SET);

	if (ctx->encrypted) {
		ctr_init_counter(&ctx->aes, ctx->counter);
		ctr_add_counter(&ctx->aes, (u32)(data_pos / 0x10));
	}
}

size_t romfs_fread(romfs_context* ctx, void* buffer, size_t size, size_t count)
{
	size_t read;
	if ((read = fread(buffer, size, count, ctx->file)) != count) {
		//printf("romfs_fread() fail\n");
		return read;
	}
	if (ctx->encrypted) {
		ctr_crypt_counter(&ctx->aes, buffer, buffer, size*read);
	}
	return read;
}

void romfs_process(romfs_context* ctx, u32 actions)
{
	u32 dirblockoffset = 0;
	u32 dirblocksize = 0;
	u32 fileblockoffset = 0;
	u32 fileblocksize = 0;

	ivfc_set_offset(&ctx->ivfc, ctx->offset);
	ivfc_set_size(&ctx->ivfc, ctx->size);
	ivfc_set_file(&ctx->ivfc, ctx->file);
	ivfc_set_usersettings(&ctx->ivfc, ctx->usersettings);
	ivfc_set_counter(&ctx->ivfc, ctx->counter);
	ivfc_set_key(&ctx->ivfc, ctx->key);
	ivfc_set_encrypted(&ctx->ivfc, ctx->encrypted);
	ivfc_process(&ctx->ivfc, actions);

	romfs_fseek(ctx, ctx->offset);
	romfs_fread(ctx, &ctx->header, 1, sizeof(romfs_header));

	if (getle32(ctx->header.magic) != MAGIC_IVFC)
	{
		fprintf(stdout, "Error, RomFS corrupted\n");
		return;
	}

	ctx->infoblockoffset = (u32) (ctx->offset + 0x1000);

	romfs_fseek(ctx, ctx->infoblockoffset);
	romfs_fread(ctx, &ctx->infoheader, 1, sizeof(romfs_infoheader));
	
	if (getle32(ctx->infoheader.headersize) != sizeof(romfs_infoheader))
	{
		fprintf(stderr, "Error, info header mismatch\n");
		return;
	}

	dirblockoffset = ctx->infoblockoffset + getle32(ctx->infoheader.section[1].offset);
	dirblocksize = getle32(ctx->infoheader.section[1].size);
	fileblockoffset = ctx->infoblockoffset + getle32(ctx->infoheader.section[3].offset);
	fileblocksize = getle32(ctx->infoheader.section[3].size);

	u32 hdrsize = getle32(ctx->infoheader.dataoffset);
	u8 *block = malloc(hdrsize);
	romfs_fseek(ctx, ctx->infoblockoffset);
	romfs_fread(ctx, block, hdrsize, 1);

	ctx->dirblock = malloc(dirblocksize);
	ctx->dirblocksize = dirblocksize;
	if(ctx->dirblock)
		memcpy(ctx->dirblock, block + getle32(ctx->infoheader.section[1].offset), dirblocksize);

	ctx->fileblock = malloc(fileblocksize);
	ctx->fileblocksize = fileblocksize;
	if (ctx->fileblock)
		memcpy(ctx->fileblock, block + getle32(ctx->infoheader.section[3].offset), fileblocksize);

	free(block);

	ctx->datablockoffset = ctx->infoblockoffset + getle32(ctx->infoheader.dataoffset);

	if (actions & InfoFlag)
		romfs_print(ctx);

	if (settings_get_romfs_dir_path(ctx->usersettings)->valid)
		ctx->extractdir = os_CopyConvertCharStr(settings_get_romfs_dir_path(ctx->usersettings)->pathname);
	else
		ctx->extractdir = NULL;

	romfs_visit_dir(ctx, 0, 0, actions, ctx->extractdir);
	free(ctx->extractdir);
}

int romfs_dirblock_read(romfs_context* ctx, u32 diroffset, u32 dirsize, void* buffer)
{
	if (!ctx->dirblock)
		return 0;

	if (diroffset+dirsize > ctx->dirblocksize)
		return 0;

	memcpy(buffer, ctx->dirblock + diroffset, dirsize);
	return 1;
}

int romfs_dirblock_readentry(romfs_context* ctx, u32 diroffset, romfs_direntry* entry)
{
	u32 size_without_name = sizeof(romfs_direntry) - ROMFS_MAXNAMESIZE;
	u32 namesize;


	if (!ctx->dirblock)
		return 0;

	if (!romfs_dirblock_read(ctx, diroffset, size_without_name, entry))
		return 0;
	
	namesize = getle32(entry->namesize);
	if (namesize > (ROMFS_MAXNAMESIZE-2))
		namesize = (ROMFS_MAXNAMESIZE-2);
	memset(entry->name + namesize, 0, 2);
	if (!romfs_dirblock_read(ctx, diroffset + size_without_name, namesize, entry->name))
		return 0;

	return 1;
}


int romfs_fileblock_read(romfs_context* ctx, u32 fileoffset, u32 filesize, void* buffer)
{
	if (!ctx->fileblock)
		return 0;

	if (fileoffset+filesize > ctx->fileblocksize)
		return 0;

	memcpy(buffer, ctx->fileblock + fileoffset, filesize);
	return 1;
}

int romfs_fileblock_readentry(romfs_context* ctx, u32 fileoffset, romfs_fileentry* entry)
{
	u32 size_without_name = sizeof(romfs_fileentry) - ROMFS_MAXNAMESIZE;
	u32 namesize;


	if (!ctx->fileblock)
		return 0;

	if (!romfs_fileblock_read(ctx, fileoffset, size_without_name, entry))
		return 0;
	
	namesize = getle32(entry->namesize);
	if (namesize > (ROMFS_MAXNAMESIZE-2))
		namesize = (ROMFS_MAXNAMESIZE-2);
	memset(entry->name + namesize, 0, 2);
	if (!romfs_fileblock_read(ctx, fileoffset + size_without_name, namesize, entry->name))
		return 0;

	return 1;
}



void romfs_visit_dir(romfs_context* ctx, u32 diroffset, u32 depth, u32 actions, const oschar_t* rootpath)
{
	u32 siblingoffset;
	u32 childoffset;
	u32 fileoffset;
	oschar_t* currentpath;
	romfs_direntry* entry = &ctx->direntry;


	if (!romfs_dirblock_readentry(ctx, diroffset, entry))
		return;


//	fprintf(stdout, "%08X %08X %08X %08X %08X ", 
//			getle32(entry->parentoffset), getle32(entry->siblingoffset), getle32(entry->childoffset), 
//			getle32(entry->fileoffset), getle32(entry->weirdoffset));
//	fwprintf(stdout, L"%ls\n", entry->name);


	if (rootpath && os_strlen(rootpath))
	{
		if (utf16_strlen((const utf16char_t*)entry->name) > 0)
			currentpath = os_AppendUTF16StrToPath(rootpath, (const utf16char_t*)entry->name);
		else // root dir, use the provided extract path instead of the empty root name.
			currentpath = os_CopyStr(rootpath);

		if (currentpath)
		{
			os_makedir(currentpath);
		}
		else
		{
			fputs("Error creating directory in root ", stderr);
			os_fputs(rootpath, stderr);
			fputs("\n", stderr);
			return;
		}
	}
	else
	{
		currentpath = os_CopyConvertUTF16Str((const utf16char_t*)entry->name);
		if (settings_get_list_romfs_files(ctx->usersettings))
		{
			u32 i;

			for(i=0; i<depth; i++)
				printf(" ");
			os_fputs(currentpath, stdout);
			fputs("\n", stdout);
		}
		free(currentpath);
		currentpath = NULL;
	}
	

	siblingoffset = getle32(entry->siblingoffset);
	childoffset = getle32(entry->childoffset);
	fileoffset = getle32(entry->fileoffset);

	if (fileoffset != (~0))
		romfs_visit_file(ctx, fileoffset, depth+1, actions, currentpath);

	if (childoffset != (~0))
		romfs_visit_dir(ctx, childoffset, depth+1, actions, currentpath);

	if (siblingoffset != (~0))
		romfs_visit_dir(ctx, siblingoffset, depth, actions, rootpath);

	free(currentpath);
}


void romfs_visit_file(romfs_context* ctx, u32 fileoffset, u32 depth, u32 actions, const oschar_t* rootpath)
{
	u32 siblingoffset = 0;
	oschar_t* currentpath = NULL;
	romfs_fileentry* entry = &ctx->fileentry;


	if (!romfs_fileblock_readentry(ctx, fileoffset, entry))
		return;


//	fprintf(stdout, "%08X %08X %016llX %016llX %08X ", 
//		getle32(entry->parentdiroffset), getle32(entry->siblingoffset), ctx->datablockoffset+getle64(entry->dataoffset),
//			getle64(entry->datasize), getle32(entry->unknown));
//	fwprintf(stdout, L"%ls\n", entry->name);

	if (rootpath && os_strlen(rootpath))
	{
		currentpath = os_AppendUTF16StrToPath(rootpath, (const utf16char_t*)entry->name);
		if (currentpath)
		{
			fputs("Saving ", stdout);
			os_fputs(currentpath, stdout);
			fputs("...\n", stdout);
			romfs_extract_datafile(ctx, getle64(entry->dataoffset), getle64(entry->datasize), currentpath);
		}
		else
		{
			fputs("Error creating file in root ", stderr);
			os_fputs(rootpath, stderr);
			fputs("\n", stderr);
			return;
		}
	}
	else
	{
		currentpath = os_CopyConvertUTF16Str((const utf16char_t*)entry->name);
		if (settings_get_list_romfs_files(ctx->usersettings))
		{
			u32 i;

			for(i=0; i<depth; i++)
				printf(" ");
			os_fputs(currentpath, stdout);
			fputs("\n", stdout);
		}
		free(currentpath);
		currentpath = NULL;
	}

	siblingoffset = getle32(entry->siblingoffset);

	if (siblingoffset != (~0))
		romfs_visit_file(ctx, siblingoffset, depth, actions, rootpath);

	free(currentpath);
}

void romfs_extract_datafile(romfs_context* ctx, u64 offset, u64 size, const oschar_t* path)
{
	FILE* outfile = 0;
	u32 max;
	u8 buffer[4096];


	if (path == NULL || os_strlen(path) == 0)
		goto clean;

	offset += ctx->datablockoffset;

	romfs_fseek(ctx, offset);
	outfile = os_fopen(path, OS_MODE_WRITE);
	if (outfile == NULL)
	{
		fprintf(stderr, "Error opening file for writing\n");
		goto clean;
	}

	while(size)
	{
		max = sizeof(buffer);
		if (max > size)
			max = (u32) size;

		if (max != romfs_fread(ctx, buffer, 1, max))
		{
			fprintf(stderr, "Error reading file\n");
			goto clean;
		}

		if (max != fwrite(buffer, 1, max, outfile))
		{
			fprintf(stderr, "Error writing file\n");
			goto clean;
		}

		size -= max;
	}
clean:
	if (outfile)
		fclose(outfile);
}


void romfs_print(romfs_context* ctx)
{
	u32 i;

	fprintf(stdout, "\nRomFS:\n");

	fprintf(stdout, "Header size:            0x%08X\n", getle32(ctx->infoheader.headersize));
	for(i=0; i<4; i++)
	{
		fprintf(stdout, "Section %d offset:       0x%08"PRIX64"\n", i, ctx->offset + 0x1000 + getle32(ctx->infoheader.section[i].offset));
		fprintf(stdout, "Section %d size:         0x%08X\n", i, getle32(ctx->infoheader.section[i].size));
	}

	fprintf(stdout, "Data offset:            0x%08"PRIX64"\n", ctx->offset + 0x1000 + getle32(ctx->infoheader.dataoffset));
}
