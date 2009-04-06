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

#include "PrecompiledHeader.h"
#include <cmath>
#include <assert.h>

#include "Common.h"
#include "VUmicro.h"
#include "Vif.h"
#include "VifDma.h"

VIFregisters *_vifRegs;
u32* _vifRow = NULL, *_vifCol = NULL;
u32* _vifMaskRegs = NULL;
vifStruct *_vif;

PCSX2_ALIGNED16(u32 g_vifRow0[4]);
PCSX2_ALIGNED16(u32 g_vifCol0[4]);
PCSX2_ALIGNED16(u32 g_vifRow1[4]);
PCSX2_ALIGNED16(u32 g_vifCol1[4]);

extern int g_vifCycles;
u16 vifqwc = 0;
bool mfifodmairq = FALSE;

enum UnpackOffset
{
	OFFSET_X = 0,
	OFFSET_Y = 1,
	OFFSET_Z = 2,
	OFFSET_W =3
};

#define spr0 ((DMACh*)&PS2MEM_HW[0xD000])

__forceinline static int _limit(int a, int max)
{
	return (a > max) ? max : a;
}
	
static __releaseinline void writeX(u32 &dest, u32 data)
{
	int n;
	
	if (_vifRegs->code & 0x10000000)
	{
		switch (_vif->cl)
		{
			case 0:
				n = (_vifRegs->mask) & 0x3;
				break;
			case 1:
				n = (_vifRegs->mask >> 8) & 0x3;
				break;
			case 2:
				n = (_vifRegs->mask >> 16) & 0x3;
				break;
			default:
				n = (_vifRegs->mask >> 24) & 0x3;
				break;
		}
	}
	else n = 0;

	switch (n)
	{
		case 0:
			if ((_vif->cmd & 0x6F) == 0x6f)
			{
				dest = data;
			}
			else if (_vifRegs->mode == 1)
			{
				dest = data + _vifRegs->r0;
			}
			else if (_vifRegs->mode == 2)
			{
				_vifRegs->r0 += data;
				dest = _vifRegs->r0;
			}
			else
			{
				dest = data;
			}
			break;
		case 1:
			dest = _vifRegs->r0;
			break;
		case 2:
			switch (_vif->cl)
			{
				case 0:
					dest = _vifRegs->c0;
					break;
				case 1:
					dest = _vifRegs->c1;
					break;
				case 2:
					dest = _vifRegs->c2;
					break;
				default:
					dest = _vifRegs->c3;
					break;
			}
			break;
	}
//	VIF_LOG("writeX %8.8x : Mode %d, r0 = %x, data %8.8x", *dest,_vifRegs->mode,_vifRegs->r0,data);
}

static __releaseinline void writeY(u32 &dest, u32 data)
{
	int n;
	
	if (_vifRegs->code & 0x10000000)
	{
		switch (_vif->cl)
		{
			case 0:
				n = (_vifRegs->mask >> 2) & 0x3;
				break;
			case 1:
				n = (_vifRegs->mask >> 10) & 0x3;
				break;
			case 2:
				n = (_vifRegs->mask >> 18) & 0x3;
				break;
			default:
				n = (_vifRegs->mask >> 26) & 0x3;
				break;
		}
	}
	else n = 0;

	switch (n)
	{
		case 0:
			if ((_vif->cmd & 0x6F) == 0x6f)
			{
				dest = data;
			}
			else if (_vifRegs->mode == 1)
			{
				dest = data + _vifRegs->r1;
			}
			else if (_vifRegs->mode == 2)
			{
				_vifRegs->r1 += data;
				dest = _vifRegs->r1;
			}
			else
			{
				dest = data;
			}
			break;
		case 1:
			dest = _vifRegs->r1;
			break;
		case 2:
			switch (_vif->cl)
			{
				case 0:
					dest = _vifRegs->c0;
					break;
				case 1:
					dest = _vifRegs->c1;
					break;
				case 2:
					dest = _vifRegs->c2;
					break;
				default:
					dest = _vifRegs->c3;
					break;
			}
			break;
	}
//	VIF_LOG("writeY %8.8x : Mode %d, r1 = %x, data %8.8x", *dest,_vifRegs->mode,_vifRegs->r1,data);
}

static __releaseinline void writeZ(u32 &dest, u32 data)
{
	int n;
	
	if (_vifRegs->code & 0x10000000)
	{
		switch (_vif->cl)
		{
			case 0:
				n = (_vifRegs->mask >> 4) & 0x3;
				break;
			case 1:
				n = (_vifRegs->mask >> 12) & 0x3;
				break;
			case 2:
				n = (_vifRegs->mask >> 20) & 0x3;
				break;
			default:
				n = (_vifRegs->mask >> 28) & 0x3;
				break;
		}
	}
	else n = 0;

	switch (n)
	{
		case 0:
			if ((_vif->cmd & 0x6F) == 0x6f)
			{
				dest = data;
			}
			else if (_vifRegs->mode == 1)
			{
				dest = data + _vifRegs->r2;
			}
			else if (_vifRegs->mode == 2)
			{
				_vifRegs->r2 += data;
				dest = _vifRegs->r2;
			}
			else
			{
				dest = data;
			}
			break;
		case 1:
			dest = _vifRegs->r2;
			break;
		case 2:
			switch (_vif->cl)
			{
				case 0:
					dest = _vifRegs->c0;
					break;
				case 1:
					dest = _vifRegs->c1;
					break;
				case 2:
					dest = _vifRegs->c2;
					break;
				default:
					dest = _vifRegs->c3;
					break;
			}
			break;
	}
//	VIF_LOG("writeZ %8.8x : Mode %d, r2 = %x, data %8.8x", *dest,_vifRegs->mode,_vifRegs->r2,data);
}

static __releaseinline void writeW(u32 &dest, u32 data)
{
	int n;
	
	if (_vifRegs->code & 0x10000000)
	{
		switch (_vif->cl)
		{
			case 0:
				n = (_vifRegs->mask >> 6) & 0x3;
				break;
			case 1:
				n = (_vifRegs->mask >> 14) & 0x3;
				break;
			case 2:
				n = (_vifRegs->mask >> 22) & 0x3;
				break;
			default:
				n = (_vifRegs->mask >> 30) & 0x3;
				break;
		}
	}
	else n = 0;

	switch (n)
	{
		case 0:
			if ((_vif->cmd & 0x6F) == 0x6f)
			{
				dest = data;
			}
			else if (_vifRegs->mode == 1)
			{
				dest = data + _vifRegs->r3;
			}
			else if (_vifRegs->mode == 2)
			{
				_vifRegs->r3 += data;
				dest = _vifRegs->r3;
			}
			else
			{
				dest = data;
			}
			break;
		case 1:
			dest = _vifRegs->r3;
			break;
		case 2:
			switch (_vif->cl)
			{
				case 0:
					dest = _vifRegs->c0;
					break;
				case 1:
					dest = _vifRegs->c1;
					break;
				case 2:
					dest = _vifRegs->c2;
					break;
				default:
					dest = _vifRegs->c3;
					break;
			}
			break;
	}
//	VIF_LOG("writeW %8.8x : Mode %d, r3 = %x, data %8.8x", *dest,_vifRegs->mode,_vifRegs->r3,data);
}

template <class T>
static void _UNPACKpart(u32 offnum,  u32 &x, T y)
{
	if (_vifRegs->offset == offnum) 
	{
		switch (offnum)
		{
			case OFFSET_X:
				writeX(x,y);
				break;
			case OFFSET_Y: 
				writeY(x,y);
				break;
			case OFFSET_Z:
				writeZ(x,y);
				break;
			case OFFSET_W:
				writeW(x,y);
				break;
			default:
				break;
		}
		_vifRegs->offset++;
	}
}

template <class T>
static void _UNPACKpart(u32 offnum,  u32 &x, T y, int size)
{
	if(size == 0) return;

	if (_vifRegs->offset == offnum) 
	{
		switch (offnum)
		{
			case OFFSET_X:
				writeX(x,y);
				break;
			case OFFSET_Y: 
				writeY(x,y);
				break;
			case OFFSET_Z:
				writeZ(x,y);
				break;
			case OFFSET_W:
				writeW(x,y);
				break;
			default:
				break;
		}
		size--;
		_vifRegs->offset++;
	}
}

template <class T>
void __fastcall UNPACK_S(u32 *dest, T *data, int size)
{
	_UNPACKpart(OFFSET_X, *dest++, *data, size);
	_UNPACKpart(OFFSET_Y, *dest++, *data, size);
	_UNPACKpart(OFFSET_Z, *dest++, *data, size);
	_UNPACKpart(OFFSET_W, *dest , *data, size);
	if (_vifRegs->offset == 4) _vifRegs->offset = 0;
}

template <class T>
void __fastcall UNPACK_V2(u32 *dest, T *data, int size)
{
	_UNPACKpart(OFFSET_X, *dest++, *data++, size);
	_UNPACKpart(OFFSET_Y, *dest++, *data--, size);
	_UNPACKpart(OFFSET_Z, *dest++, *data++);
	_UNPACKpart(OFFSET_W, *dest , *data);
	if (_vifRegs->offset == 4) _vifRegs->offset = 0;
}

template <class T>
void __fastcall UNPACK_V3(u32 *dest, T *data, int size)
{
	_UNPACKpart(OFFSET_X, *dest++, *data++, size);
	_UNPACKpart(OFFSET_Y, *dest++, *data++, size);
	_UNPACKpart(OFFSET_Z, *dest++, *data++, size);
	 //V3-# does some bizzare thing with alignment, every 6qw of data the W becomes 0 (strange console!)
	if((_vif->qwcalign % 24) == 0)     
		_UNPACKpart(OFFSET_W, *dest, 0);
	else 
		_UNPACKpart(OFFSET_W, *dest, *data);

	if (_vifRegs->offset == 4) _vifRegs->offset = 0;
}

template <class T>
void __fastcall UNPACK_V4(u32 *dest, T *data , int size)
{
	_UNPACKpart(OFFSET_X, *dest++, *data++, size);
	_UNPACKpart(OFFSET_Y, *dest++, *data++, size);
	_UNPACKpart(OFFSET_Z, *dest++, *data++, size);
	_UNPACKpart(OFFSET_W, *dest , *data, size);
	if (_vifRegs->offset == 4) _vifRegs->offset = 0;
}

void __fastcall UNPACK_V4_5(u32 *dest, u32 *data, int size)
{
	_UNPACKpart(OFFSET_X, *dest++,  ((*data & 0x001f) << 3), size);
	_UNPACKpart(OFFSET_Y, *dest++, ((*data & 0x03e0) >> 2), size);
	_UNPACKpart(OFFSET_Z, *dest++, ((*data & 0x7c00) >> 7), size);
	_UNPACKpart(OFFSET_W, *dest, ((*data & 0x8000) >> 8), size);
	if (_vifRegs->offset == 4) _vifRegs->offset = 0;
}

void __fastcall UNPACK_S_32(u32 *dest, u32 *data, int size) 
{
	UNPACK_S(dest, data, size); 
}

void __fastcall UNPACK_S_16s(u32 *dest, u32 *data, int size) 
{ 
	s16 *sdata = (s16*)data;  
	UNPACK_S(dest, sdata, size); 
}

void __fastcall UNPACK_S_16u(u32 *dest, u32 *data, int size) 
{
	u16 *sdata = (u16*)data; 
	UNPACK_S(dest, sdata, size);
}

void __fastcall UNPACK_S_8s(u32 *dest, u32 *data, int size)
{
	s8 *cdata = (s8*)data;
	UNPACK_S(dest, cdata, size);
}

void __fastcall UNPACK_S_8u(u32 *dest, u32 *data, int size)
{
	u8 *cdata = (u8*)data;
	UNPACK_S(dest, cdata, size);
}

void __fastcall UNPACK_V2_32(u32 *dest, u32 *data, int size)
{
	UNPACK_V2(dest, data, size);
}

void __fastcall UNPACK_V2_16s(u32 *dest, u32 *data, int size)
{
	s16 *sdata = (s16*)data;
	UNPACK_V2(dest, sdata, size);
}

void __fastcall UNPACK_V2_16u(u32 *dest, u32 *data, int size)
{
	u16 *sdata = (u16*)data;
	UNPACK_V2(dest, sdata, size);
}

void __fastcall UNPACK_V2_8s(u32 *dest, u32 *data, int size)
{
	s8 *cdata = (s8*)data;
	UNPACK_V2(dest, cdata, size);
}

void __fastcall UNPACK_V2_8u(u32 *dest, u32 *data, int size)
{
	u8 *cdata = (u8*)data;
	UNPACK_V2(dest, cdata, size);
}

void __fastcall UNPACK_V3_32(u32 *dest, u32 *data, int size)
{
	UNPACK_V3(dest, data, size);
}

void __fastcall UNPACK_V3_16s(u32 *dest, u32 *data, int size)
{
	s16 *sdata = (s16*)data;
	UNPACK_V3(dest, sdata, size);
}

void __fastcall UNPACK_V3_16u(u32 *dest, u32 *data, int size)
{
	u16 *sdata = (u16*)data;
	UNPACK_V3(dest, sdata, size);
}

void __fastcall UNPACK_V3_8s(u32 *dest, u32 *data, int size)
{
	s8 *cdata = (s8*)data;
	UNPACK_V3(dest, cdata, size);
}

void __fastcall UNPACK_V3_8u(u32 *dest, u32 *data, int size)
{
	u8 *cdata = (u8*)data;
	UNPACK_V3(dest, cdata, size);
}

void __fastcall UNPACK_V4_32(u32 *dest, u32 *data , int size)
{
	UNPACK_V4(dest, data, size);
}

void __fastcall UNPACK_V4_16s(u32 *dest, u32 *data, int size)
{
	s16 *sdata = (s16*)data;
	UNPACK_V4(dest, sdata, size);
}

void __fastcall UNPACK_V4_16u(u32 *dest, u32 *data, int size)
{
	u16 *sdata = (u16*)data;
	UNPACK_V4(dest, sdata, size);
}

void __fastcall UNPACK_V4_8s(u32 *dest, u32 *data, int size)
{
	s8 *cdata = (s8*)data;
	UNPACK_V4(dest, cdata, size);
}

void __fastcall UNPACK_V4_8u(u32 *dest, u32 *data, int size)
{
	u8 *cdata = (u8*)data;
	UNPACK_V4(dest, cdata, size);
}

static __forceinline int mfifoVIF1rbTransfer()
{
	u32 maddr = psHu32(DMAC_RBOR);
	u32 ret, msize = psHu32(DMAC_RBOR) + psHu32(DMAC_RBSR) + 16;
	u16 mfifoqwc = std::min(vif1ch->qwc, vifqwc);
	u32 *src;

	/* Check if the transfer should wrap around the ring buffer */
	if ((vif1ch->madr + (mfifoqwc << 4)) > (msize))
	{
		int s1 = ((msize) - vif1ch->madr) >> 2;

		SPR_LOG("Split MFIFO");

		/* it does, so first copy 's1' bytes from 'addr' to 'data' */
		src = (u32*)PSM(vif1ch->madr);
		if (src == NULL) return -1;
		
		if (vif1.vifstalled == 1)
			ret = VIF1transfer(src + vif1.irqoffset, s1 - vif1.irqoffset, 0);
		else
			ret = VIF1transfer(src, s1, 0);
		
		if (ret == -2) return ret;

		/* and second copy 's2' bytes from 'maddr' to '&data[s1]' */
		vif1ch->madr = maddr;

		src = (u32*)PSM(maddr);
		if (src == NULL) return -1;
		ret = VIF1transfer(src, ((mfifoqwc << 2) - s1), 0);
	}
	else
	{
		SPR_LOG("Direct MFIFO");

		/* it doesn't, so just transfer 'qwc*4' words */
		src = (u32*)PSM(vif1ch->madr);
		if (src == NULL) return -1;
		
		if (vif1.vifstalled == 1)
			ret = VIF1transfer(src + vif1.irqoffset, mfifoqwc * 4 - vif1.irqoffset, 0);
		else
			ret = VIF1transfer(src, mfifoqwc << 2, 0);
		
		if (ret == -2) return ret;
	}

	return ret;
}

static __forceinline int mfifoVIF1chain()
{
	int ret;

	/* Is QWC = 0? if so there is nothing to transfer */
	if (vif1ch->qwc == 0 && vif1.vifstalled == 0)
	{
		vif1.inprogress = 0;
		return 0;
	}

	if (vif1ch->madr >= psHu32(DMAC_RBOR) &&
	        vif1ch->madr <= (psHu32(DMAC_RBOR) + psHu32(DMAC_RBSR)))
	{
		u16 startqwc = vif1ch->qwc;
		ret = mfifoVIF1rbTransfer();
		vifqwc -= startqwc - vif1ch->qwc;
	}
	else
	{
		u32 *pMem = (u32*)dmaGetAddr(vif1ch->madr);
		SPR_LOG("Non-MFIFO Location");

		if (pMem == NULL) return -1;
		if (vif1.vifstalled == 1)
			ret = VIF1transfer(pMem + vif1.irqoffset, vif1ch->qwc * 4 - vif1.irqoffset, 0);
		else
			ret = VIF1transfer(pMem, vif1ch->qwc << 2, 0);
	}

	return ret;
}

void mfifoVIF1transfer(int qwc)
{
	u32 *ptag;
	int id;
	int ret, temp;

	g_vifCycles = 0;

	if (qwc > 0)
	{
		vifqwc += qwc;
		if (vif1.inprogress & 0x10)
		{
			if (vif1ch->madr >= psHu32(DMAC_RBOR) && vif1ch->madr <= (psHu32(DMAC_RBOR) + psHu32(DMAC_RBSR)))
				CPU_INT(10, min((int)vifqwc, (int)vif1ch->qwc) * BIAS);
			else
				CPU_INT(10, vif1ch->qwc * BIAS);

			vif1Regs->stat |= 0x10000000; // FQC=16
		}
		vif1.inprogress &= ~0x10;
		SPR_LOG("Added %x qw to mfifo, total now %x - Vif CHCR %x Stalled %x done %x", qwc, vifqwc, vif1ch->chcr, vif1.vifstalled, vif1.done);
		return;
	}

	mfifodmairq = FALSE; //Clear any previous TIE interrupt

	if (vif1ch->qwc == 0)
	{
		ptag = (u32*)dmaGetAddr(vif1ch->tadr);

		if (vif1ch->chcr & 0x40)
		{
			if (vif1.stallontag == 1)
				ret = VIF1transfer(ptag + (2 + vif1.irqoffset), 2 - vif1.irqoffset, 1);  //Transfer Tag on Stall
			else 
				ret = VIF1transfer(ptag + 2, 2, 1);  //Transfer Tag
			
			if (ret == -2)
			{
				VIF_LOG("MFIFO Stallon tag");
				vif1.stallontag	= 1;
				return;        //IRQ set by VIFTransfer
			}
		}

		id = (ptag[0] >> 28) & 0x7;
		vif1ch->qwc  = (ptag[0] & 0xffff);
		vif1ch->madr = ptag[1];

		vif1ch->chcr = (vif1ch->chcr & 0xFFFF) | ((*ptag) & 0xFFFF0000);

		SPR_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, madr=%lx, tadr=%lx mfifo qwc = %x spr0 madr = %x",
		        ptag[1], ptag[0], vif1ch->qwc, id, vif1ch->madr, vif1ch->tadr, vifqwc, spr0->madr);
		vifqwc--;

		switch (id)
		{
			case 0: // Refe - Transfer Packet According to ADDR field
				vif1ch->tadr = psHu32(DMAC_RBOR) + ((vif1ch->tadr + 16) & psHu32(DMAC_RBSR));
				vif1.done = 1;										//End Transfer
				break;

			case 1: // CNT - Transfer QWC following the tag.
				vif1ch->madr = psHu32(DMAC_RBOR) + ((vif1ch->tadr + 16) & psHu32(DMAC_RBSR));						//Set MADR to QW after Tag
				vif1ch->tadr = psHu32(DMAC_RBOR) + ((vif1ch->madr + (vif1ch->qwc << 4)) & psHu32(DMAC_RBSR));			//Set TADR to QW following the data
				vif1.done = 0;
				break;

			case 2: // Next - Transfer QWC following tag. TADR = ADDR
				temp = vif1ch->madr;								//Temporarily Store ADDR
				vif1ch->madr = psHu32(DMAC_RBOR) + ((vif1ch->tadr + 16) & psHu32(DMAC_RBSR)); 					  //Set MADR to QW following the tag
				vif1ch->tadr = temp;								//Copy temporarily stored ADDR to Tag
				if ((temp & psHu32(DMAC_RBSR)) != psHu32(DMAC_RBOR)) Console::WriteLn("Next tag = %x outside ring %x size %x", params temp, psHu32(DMAC_RBOR), psHu32(DMAC_RBSR));
				vif1.done = 0;
				break;

			case 3: // Ref - Transfer QWC from ADDR field
			case 4: // Refs - Transfer QWC from ADDR field (Stall Control)
				vif1ch->tadr = psHu32(DMAC_RBOR) + ((vif1ch->tadr + 16) & psHu32(DMAC_RBSR));							//Set TADR to next tag
				vif1.done = 0;
				break;

			case 7: // End - Transfer QWC following the tag
				vif1ch->madr = psHu32(DMAC_RBOR) + ((vif1ch->tadr + 16) & psHu32(DMAC_RBSR));		//Set MADR to data following the tag
				vif1ch->tadr = psHu32(DMAC_RBOR) + ((vif1ch->madr + (vif1ch->qwc << 4)) & psHu32(DMAC_RBSR));			//Set TADR to QW following the data
				vif1.done = 1;										//End Transfer
				break;
		}

		if ((vif1ch->chcr & 0x80) && (ptag[0] >> 31))
		{
			VIF_LOG("dmaIrq Set");
			vif1.done = 1;
			mfifodmairq = TRUE; //Let the handler know we have prematurely ended MFIFO
		}
	}
	
	vif1.inprogress |= 1;
	
	SPR_LOG("mfifoVIF1transfer end %x madr %x, tadr %x vifqwc %x", vif1ch->chcr, vif1ch->madr, vif1ch->tadr, vifqwc);
}

void vifMFIFOInterrupt()
{
	g_vifCycles = 0;
	
	if (vif1.inprogress == 1) mfifoVIF1chain();

	if (vif1.irq && vif1.tag.size == 0)
	{
		vif1Regs->stat |= VIF1_STAT_INT;
		hwIntcIrq(INTC_VIF1);
		--vif1.irq;
		if (vif1Regs->stat & (VIF1_STAT_VSS | VIF1_STAT_VIS | VIF1_STAT_VFS))
		{
			vif1Regs->stat &= ~0x1F000000; // FQC=0
			vif1ch->chcr &= ~0x100;
			return;
		}
	}
	
	if (vif1.done != 1 || vif1.inprogress & 1)
	{
		if (vifqwc <= 0)
		{
			//Console::WriteLn("Empty 1");
			vif1.inprogress |= 0x10;
			vif1Regs->stat &= ~0x1F000000; // FQC=0
			hwDmacIrq(14);
			return;
		}
		if (!(vif1.inprogress & 0x1)) mfifoVIF1transfer(0);

		if (vif1ch->madr >= psHu32(DMAC_RBOR) && vif1ch->madr <= (psHu32(DMAC_RBOR) + psHu32(DMAC_RBSR)))
			CPU_INT(10, min((int)vifqwc, (int)vif1ch->qwc) * BIAS);
		else
			CPU_INT(10, vif1ch->qwc * BIAS);

		return;
	}
	else if (vifqwc <= 0)
	{
		//Console::WriteLn("Empty 2");
		vif1.inprogress |= 0x10;
		vif1Regs->stat &= ~0x1F000000; // FQC=0
		hwDmacIrq(14);
	}

	//On a TIE break we do not clear the MFIFO (Art of Fighting)
	//If we dont clear it on MFIFO end, Tekken Tag breaks, understandably (Refraction)
	if (!mfifodmairq)  vifqwc = 0;

	vif1.done = 1;
	g_vifCycles = 0;
	vif1ch->chcr &= ~0x100;
	hwDmacIrq(DMAC_VIF1);
	VIF_LOG("vif mfifo dma end");

	vif1Regs->stat &= ~0x1F000000; // FQC=0

}
