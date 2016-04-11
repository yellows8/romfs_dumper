#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

Result dump_file(char *outpath, char *inpath, size_t filesize)
{
	Result ret=0;
	FILE *foutput, *finput;

	u8 *tmpbuf;
	size_t pos, chunksize = 0x400000;
	size_t transfersize;

	printf("Dumping file...\n");

	finput = fopen(inpath, "rb");
	foutput = fopen(outpath, "wb");

	if(finput==NULL)
	{
		printf("Failed to open the input file: %s\n", inpath);
		ret = -1;
	}

	if(foutput==NULL)
	{
		printf("Failed to open the output file: %s\n", outpath);
		ret = -1;
	}

	tmpbuf = malloc(chunksize);
	if(tmpbuf==NULL)
	{
		printf("Failed to allocate tmpbuf.\n");
		ret = -2;
	}
	else
	{
		memset(tmpbuf, 0, chunksize);
	}

	if (R_FAILED(ret))
	{
		if(finput)fclose(finput);
		if(foutput)fclose(foutput);
		return ret;
	}

	for(pos=0; pos<filesize; pos+= chunksize)
	{
		if(filesize-pos < chunksize)chunksize = filesize-pos;

		transfersize = fread(tmpbuf, 1, chunksize, finput);
		if(transfersize != chunksize)
		{
			printf("Data read fail, only 0x%08x of 0x%08x bytes were transferred.\n", transfersize, chunksize);
			ret = -3;
			break;
		}

		transfersize = fwrite(tmpbuf, 1, chunksize, foutput);
		if(transfersize != chunksize)
		{
			printf("Data write fail, only 0x%08x of 0x%08x bytes were transferred.\n", transfersize, chunksize);
			ret = -3;
			break;
		}

		printf("Chunk transfer finished, transferred 0x%08x of 0x%08x bytes.\n", pos+chunksize, filesize);
	}

	fclose(finput);
	fclose(foutput);
	free(tmpbuf);

	return ret;
}

Result dump_filesystem(char *base_outpath, char *base_inpath)
{
	Result ret=0;
	DIR *dirp;
	struct dirent *direntry;
	struct stat entstats;
	int isfile=0;

	char inpath[NAME_MAX];
	char outpath[NAME_MAX];

	mkdir(base_outpath, 0777);

	dirp = opendir(base_inpath);
	if(dirp==NULL)
	{
		printf("Opendir failed with: %s\n", base_inpath);
		return -1;
	}

	printf("Dumping files for: %s\n", base_inpath);
	while((direntry = readdir(dirp)))
	{
		if(strcmp(direntry->d_name, ".")==0 || strcmp(direntry->d_name, "..")==0)continue;

		memset(inpath, 0, sizeof(inpath));
		memset(outpath, 0, sizeof(outpath));

		snprintf(inpath, sizeof(inpath)-1, "%s%s", base_inpath, direntry->d_name);
		snprintf(outpath, sizeof(outpath)-1, "%s%s", base_outpath, direntry->d_name);

		printf("%s ", direntry->d_name);

		if(stat(inpath, &entstats)==-1)
		{
			printf("stat() failed.\n");
			ret = -1;
			break;
		}

		isfile = 0;
		if ((entstats.st_mode & S_IFMT) == S_IFREG)isfile = 1;

		printf("isfile=%d ", isfile);
		if(isfile)
		{
			printf("size=0x%08x", (unsigned int)entstats.st_size);
		}
		printf("\n");

		if(isfile)
		{
			ret = dump_file(outpath, inpath, entstats.st_size);
			if (R_FAILED(ret))
			{
				printf("dump_file() returned 0x%08x.\n", (unsigned int)ret);
				break;
			}
		}
	}

	closedir(dirp);

	return ret;
}

Result dump_romfs(u32 contentindex)
{
	Result ret=0;
	Handle romFS_file=0;
	u32 procid=0;
	FS_ProgramInfo programinfo;

	char str[256];

	memset(str, 0, sizeof(str));

	ret = svcGetProcessId(&procid, CUR_PROCESS_HANDLE);
	if (R_FAILED(ret))return ret;

	ret = FSUSER_GetProgramLaunchInfo(&programinfo, procid);
	if (R_FAILED(ret))return ret;

	snprintf(str, sizeof(str)-1, "output_programid_%016llx_x%02x/", (unsigned long long)programinfo.programId, (unsigned int)contentindex);

	u32 lowpath[0x14>>2];
	memset(lowpath, 0, sizeof(lowpath));
	lowpath[1] = contentindex;

	FS_Archive arch = { ARCHIVE_SAVEDATA_AND_CONTENT, { PATH_BINARY, sizeof(programinfo), &programinfo }, 0 };
	FS_Path path = { PATH_BINARY, sizeof(lowpath), lowpath };

	ret = FSUSER_OpenFileDirectly(&romFS_file, arch, path, FS_OPEN_READ, 0);
	if (R_SUCCEEDED(ret))ret = romfsInitFromFile(romFS_file, 0);

	if (R_SUCCEEDED(ret))ret = dump_filesystem(str, "romfs:/");

	return ret;
}

int main(int argc, char **argv)
{
	Result ret=0;

	gfxInitDefault();

	consoleInit(GFX_TOP, NULL);

	ret = dump_romfs(2);
	printf("dump_romfs() returned 0x%08x.\n", (unsigned int)ret);

	printf("Press START to exit.\n");

	while (aptMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kDown = hidKeysDown();

		if (kDown & KEY_START) break; // break in order to return to hbmenu

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();

		//Wait for VBlank
		gspWaitForVBlank();
	}

	gfxExit();
	return 0;
}
