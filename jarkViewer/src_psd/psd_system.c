// created: 2007-01-27
#include <stdio.h>
#include <stdlib.h>
#include "libpsd.h"
#include "psd_system.h"


void * psd_malloc(psd_int size)
{
	return malloc(size);
}

void * psd_realloc(void * block, psd_int size)
{
	return realloc(block, size);
}

void psd_free(void * block)
{
	free(block);
}

void psd_freeif(void * block)
{
	if (block != NULL)
		psd_free(block);
}

void * psd_fopen(psd_char * file_name)
{
	return (void *)fopen(file_name, "rb");
}

psd_int psd_fsize(void * file)
{
	psd_int offset, size;

	offset = ftell((FILE *)file);
	fseek((FILE *)file, 0, SEEK_END);
	size = ftell(file);
	fseek((FILE *)file, 0, SEEK_SET);
	fseek((FILE *)file, offset, SEEK_CUR);

	return size;
}

psd_int psd_fread(psd_uchar * buffer, psd_int count, void * file)
{
	return fread(buffer, 1, count, (FILE *)file);
}

psd_int psd_fseek(void * file, psd_int length)
{
	return fseek((FILE *)file, length, SEEK_CUR);
}

void psd_fclose(void * file)
{
	fclose((FILE *)file);
}

