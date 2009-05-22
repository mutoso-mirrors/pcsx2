/* 
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "GSTextureCache.h"
#include "GSDevice9.h"

class GSTextureCache9 : public GSTextureCache
{
	class GSRenderTarget9 : public GSRenderTarget
	{
	public:
		explicit GSRenderTarget9(GSRenderer* r) : GSRenderTarget(r) {}

		void Update();
		void Read(const GSVector4i& r);
	};

	class GSDepthStencil9 : public GSDepthStencil
	{
	public:
		explicit GSDepthStencil9(GSRenderer* r) : GSDepthStencil(r) {}

		void Update();
	};

	class GSCachedTexture9 : public GSCachedTexture
	{
	public:
		explicit GSCachedTexture9(GSRenderer* r) : GSCachedTexture(r) {}

		bool Create();
		bool Create(GSRenderTarget* rt);
		bool Create(GSDepthStencil* ds);
	};

protected:
	GSRenderTarget* CreateRenderTarget() {return new GSRenderTarget9(m_renderer);}
	GSDepthStencil* CreateDepthStencil() {return new GSDepthStencil9(m_renderer);}
	GSCachedTexture* CreateTexture() {return new GSCachedTexture9(m_renderer);}

public:
	GSTextureCache9(GSRenderer* r);
};