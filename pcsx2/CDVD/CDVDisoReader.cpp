/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2009  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */
/*
 *  Original code from libcdvd by Hiryu & Sjeep (C) 2002
 *  Modified by Florin for PCSX2 emu
 *  Fixed CdRead by linuzappz
 */
#include "PrecompiledHeader.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "CDVDisoReader.h"

#ifndef MAX_PATH
#define MAX_PATH 255
#endif

bool loadFromISO=false;

char isoFileName[256];
char IsoCWD[256];
char CdDev[256];

_cdIso cdIso[8];
u8 *pbuffer;
int cdblocksize;
int cdblockofs;
int cdoffset;
int cdtype;
int cdblocks;

int psize;

int Zmode; // 1 Z - 2 bz2
int fmode;						// 0 - file / 1 - Zfile
char *Ztable;

int BlockDump;
isoFile *iso;

FILE *cdvdLog = NULL;

// This var is used to detect resume-style behavior of the Pcsx2 emulator,
// and skip prompting the user for a new CD when it's likely they want to run the existing one.
static char cdvdCurrentIso[MAX_PATH];

char *methods[] =
{
	".Z  - compress faster",
	".BZ - compress better",
	NULL
};

u8 cdbuffer[CD_FRAMESIZE_RAW * 10] = {0};

s32 msf_to_lba(u8 m, u8 s, u8 f)
{
	u32 lsn;
	lsn = f;
	lsn += (s - 2) * 75;
	lsn += m * 75 * 60;
	return lsn;
}

void lba_to_msf(s32 lba, u8* m, u8* s, u8* f)
{
	lba += 150;
	*m = lba / (60 * 75);
	*s = (lba / 75) % 60;
	*f = lba % 75;
}

#define btoi(b)	((b)/16*10 + (b)%16)	/* BCD to u_char */
#define itob(i)		((i)/10*16 + (i)%10)		/* u_char to BCD */


#ifdef PCSX2_DEBUG
void __Log(char *fmt, ...)
{
	va_list list;

	if (cdvdLog == NULL) return;

	va_start(list, fmt);
	vfprintf(cdvdLog, fmt, list);
	va_end(list);
}
#else
#define __Log 0&&
#endif


s32 ISOinit()
{
#ifdef PCSX2_DEBUG
	cdvdLog = fopen("logs/cdvdLog.txt", "w");
	if (cdvdLog == NULL)
	{
		cdvdLog = fopen("cdvdLog.txt", "w");
		if (cdvdLog == NULL)
		{
			Console::Error("Can't create cdvdLog.txt");
			return -1;
		}
	}
	setvbuf(cdvdLog, NULL,  _IONBF, 0);
	CDVD_LOG("CDVDinit\n");
#endif

	cdvdCurrentIso[0] = 0;
	memset(cdIso, 0, sizeof(cdIso));
	return 0;
}

void ISOshutdown()
{
	cdvdCurrentIso[0] = 0;
#ifdef CDVD_LOG
	if (cdvdLog != NULL) fclose(cdvdLog);
#endif
}

s32 ISOopen(const char* pTitle)
{
	//if (pTitle != NULL) strcpy(isoFileName, pTitle);

	iso = isoOpen(isoFileName);
	if (iso == NULL)
	{
		Console::Error("Error loading %s\n", params isoFileName);
		return -1;
	}

	if (iso->type == ISOTYPE_DVD)
		cdtype = CDVD_TYPE_PS2DVD;
	else if (iso->type == ISOTYPE_AUDIO)
		cdtype = CDVD_TYPE_CDDA;
	else
		cdtype = CDVD_TYPE_PS2CD;

	return 0;
}

void ISOclose()
{
	strcpy(cdvdCurrentIso, isoFileName);

	isoClose(iso);
}

s32 ISOreadSubQ(u32 lsn, cdvdSubQ* subq)
{
	// fake it
	u8 min, sec, frm;
	subq->ctrl		= 4;
	subq->mode		= 1;
	subq->trackNum	= itob(1);
	subq->trackIndex = itob(1);

	lba_to_msf(lsn, &min, &sec, &frm);
	subq->trackM	= itob(min);
	subq->trackS	= itob(sec);
	subq->trackF	= itob(frm);

	subq->pad		= 0;

	lba_to_msf(lsn + (2*75), &min, &sec, &frm);
	subq->discM		= itob(min);
	subq->discS		= itob(sec);
	subq->discF		= itob(frm);
	return 0;
}

s32 ISOgetTN(cdvdTN *Buffer)
{
	Buffer->strack = 1;
	Buffer->etrack = 1;

	return 0;
}

s32 ISOgetTD(u8 Track, cdvdTD *Buffer)
{
	if (Track == 0)
	{
		Buffer->lsn = iso->blocks;
	}
	else
	{
		Buffer->type = CDVD_MODE1_TRACK;
		Buffer->lsn = 0;
	}

	return 0;
}

static s32 layer1start = -1;
s32 ISOgetTOC(void* toc)
{
	u8 type = CDVDgetDiskType();
	u8* tocBuff = (u8*)toc;

	//__Log("CDVDgetTOC\n");

	if (type == CDVD_TYPE_DVDV || type == CDVD_TYPE_PS2DVD)
	{
		// get dvd structure format
		// scsi command 0x43
		memset(tocBuff, 0, 2048);

		if (layer1start != -2 && iso->blocks >= 0x300000)
		{
			int off = iso->blockofs;
			u8* tempbuffer;

			// dual sided
			tocBuff[ 0] = 0x24;
			tocBuff[ 1] = 0x02;
			tocBuff[ 2] = 0xF2;
			tocBuff[ 3] = 0x00;
			tocBuff[ 4] = 0x41;
			tocBuff[ 5] = 0x95;

			tocBuff[14] = 0x60; // dual sided, ptp

			tocBuff[16] = 0x00;
			tocBuff[17] = 0x03;
			tocBuff[18] = 0x00;
			tocBuff[19] = 0x00;

			// search for it
			if (layer1start == -1)
			{
				printf("CDVD: searching for layer1...");
				tempbuffer = (u8*)malloc(CD_FRAMESIZE_RAW * 10);
				for (layer1start = (iso->blocks / 2 - 0x10) & ~0xf; layer1start < 0x200010; layer1start += 16)
				{
					isoReadBlock(iso, tempbuffer, layer1start);
					// CD001
					if (tempbuffer[off+1] == 0x43 && tempbuffer[off+2] == 0x44 && tempbuffer[off+3] == 0x30 && tempbuffer[off+4] == 0x30 && tempbuffer[off+5] == 0x31)
						break;
				}
				free(tempbuffer);

				if (layer1start == 0x200010)
				{
					printf("Couldn't find second layer on dual layer... ignoring\n");
					// fake it
					tocBuff[ 0] = 0x04;
					tocBuff[ 1] = 0x02;
					tocBuff[ 2] = 0xF2;
					tocBuff[ 3] = 0x00;
					tocBuff[ 4] = 0x86;
					tocBuff[ 5] = 0x72;

					tocBuff[16] = 0x00;
					tocBuff[17] = 0x03;
					tocBuff[18] = 0x00;
					tocBuff[19] = 0x00;
					layer1start = -2;
					return 0;
				}

				printf("found at 0x%8.8x\n", layer1start);
				layer1start = layer1start + 0x30000 - 1;
			}

			tocBuff[20] = layer1start >> 24;
			tocBuff[21] = (layer1start >> 16) & 0xff;
			tocBuff[22] = (layer1start >> 8) & 0xff;
			tocBuff[23] = (layer1start >> 0) & 0xff;
		}
		else
		{
			// fake it
			tocBuff[ 0] = 0x04;
			tocBuff[ 1] = 0x02;
			tocBuff[ 2] = 0xF2;
			tocBuff[ 3] = 0x00;
			tocBuff[ 4] = 0x86;
			tocBuff[ 5] = 0x72;

			tocBuff[16] = 0x00;
			tocBuff[17] = 0x03;
			tocBuff[18] = 0x00;
			tocBuff[19] = 0x00;
		}
	}
	else if ((type == CDVD_TYPE_CDDA) || (type == CDVD_TYPE_PS2CDDA) ||
		(type == CDVD_TYPE_PS2CD) || (type == CDVD_TYPE_PSCDDA) || (type == CDVD_TYPE_PSCD))
	{
		// cd toc
		// (could be replaced by 1 command that reads the full toc)
		u8 min, sec, frm;
		s32 i, err;
		cdvdTN diskInfo;
		cdvdTD trackInfo;
		memset(tocBuff, 0, 1024);
		if (CDVDgetTN(&diskInfo) == -1)
		{
			diskInfo.etrack = 0;
			diskInfo.strack = 1;
		}
		if (CDVDgetTD(0, &trackInfo) == -1) trackInfo.lsn = 0;

		tocBuff[0] = 0x41;
		tocBuff[1] = 0x00;

		//Number of FirstTrack
		tocBuff[2] = 0xA0;
		tocBuff[7] = itob(diskInfo.strack);

		//Number of LastTrack
		tocBuff[12] = 0xA1;
		tocBuff[17] = itob(diskInfo.etrack);

		//DiskLength
		lba_to_msf(trackInfo.lsn, &min, &sec, &frm);
		tocBuff[22] = 0xA2;
		tocBuff[27] = itob(min);
		tocBuff[28] = itob(sec);

		for (i = diskInfo.strack; i <= diskInfo.etrack; i++)
		{
			err = CDVDgetTD(i, &trackInfo);
			lba_to_msf(trackInfo.lsn, &min, &sec, &frm);
			tocBuff[i*10+30] = trackInfo.type;
			tocBuff[i*10+32] = err == -1 ? 0 : itob(i);	  //number
			tocBuff[i*10+37] = itob(min);
			tocBuff[i*10+38] = itob(sec);
			tocBuff[i*10+39] = itob(frm);
		}
	}
	else
		return -1;

	return 0;
}

s32 ISOreadSector(u8* tempbuffer, u32 lsn, int mode)
{
	int _lsn = lsn;

	if (_lsn < 0)
		lsn = iso->blocks + _lsn;

	if(lsn > iso->blocks)
		return -1;

	if(mode == CDVD_MODE_2352)
	{
		isoReadBlock(iso, tempbuffer, lsn);
		return 0;
	}

	isoReadBlock(iso, cdbuffer, lsn);

	pbuffer = cdbuffer;
	switch (mode)
	{
	case CDVD_MODE_2352:
		psize = 2352;
		break;
	case CDVD_MODE_2340:
		pbuffer += 12;
		psize = 2340;
		break;
	case CDVD_MODE_2328:
		pbuffer += 24;
		psize = 2328;
		break;
	case CDVD_MODE_2048:
		pbuffer += 24;
		psize = 2048;
		break;
	}

	memcpy_fast(tempbuffer,pbuffer,psize);

	return 0;
}

s32 ISOreadTrack(u32 lsn, int mode)
{
	int _lsn = lsn;

	if (_lsn < 0)
		lsn = iso->blocks + _lsn;

	if(lsn > iso->blocks)
		return -1;

	isoReadBlock(iso, cdbuffer, lsn);

	pbuffer = cdbuffer;
	switch (mode)
	{
	case CDVD_MODE_2352:
		psize = 2352;
		break;
	case CDVD_MODE_2340:
		pbuffer += 12;
		psize = 2340;
		break;
	case CDVD_MODE_2328:
		pbuffer += 24;
		psize = 2328;
		break;
	case CDVD_MODE_2048:
		pbuffer += 24;
		psize = 2048;
		break;
	}

	return 0;
}

s32 ISOgetBuffer(u8* buffer)
{
	memcpy_fast(buffer,pbuffer,psize);
	return 0;
}

s32 ISOgetDiskType()
{
	return cdtype;
}

s32 ISOgetTrayStatus()
{
	return CDVD_TRAY_CLOSE;
}

s32 ISOctrlTrayOpen()
{
	return 0;
}
s32 ISOctrlTrayClose()
{
	return 0;
}

