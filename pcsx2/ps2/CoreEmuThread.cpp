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
#include "System.h"
#include "SaveState.h"
#include "ElfHeader.h"
#include "Plugins.h"
#include "CoreEmuThread.h"

#include "R5900.h"
#include "R3000A.h"
#include "VUmicro.h"

sptr CoreEmuThread::ExecuteTask()
{
	while( !m_Done && (m_ExecMode != ExecMode_Running) )
	{
		m_ResumeEvent.Wait();
	}

	try
	{
		cpuReset();
		SysClearExecutionCache();
		OpenPlugins();

		if( StateRecovery::HasState() )
		{
			// no need to boot bios or detect CDs when loading savestates.
			// [TODO] : It might be useful to detect game SLUS/CRC and compare it against
			// the savestate info, and issue a warning to the user since, chances are, they
			// don't really want to run a game with the wrong ISO loaded into the emu.
			StateRecovery::Recover();
		}
		else
		{
			ScopedLock lock( m_lock_elf_file );
			if( !m_elf_file.IsEmpty() )
			{
				// Skip Bios Hack -- Runs the PS2 BIOS stub, and then manually loads the ELF
				// executable data, and injects the cpuRegs.pc with the address of the
				// execution start point.
				//
				// This hack is necessary for non-CD ELF files, and is optional for game CDs
				// (though not recommended for games because of rare ill side effects).

				cpuExecuteBios();
				loadElfFile( m_elf_file );
			}
		}
	}
	catch( Exception::BaseException& ex )
	{
		Msgbox::Alert( ex.DisplayMessage() );
	}

	StateCheck();

	return 0;
}

void CoreEmuThread::StateCheck()
{
	{
		ScopedLock locker( m_lock_ExecMode );
		
		switch( m_ExecMode )
		{
			case ExecMode_Idle:
				// threads should never have an idle execution state set while the
				// thread is in any way active or alive.
				DevAssert( false, "Invalid execution state detected." );
			break;

			// These are not the case statements you're looking for.  Move along.
			case ExecMode_Running: break;
			case ExecMode_Suspended: break;
			
			case ExecMode_Suspending:
				m_ExecMode = ExecMode_Suspended;
				m_SuspendEvent.Post();
			break;
		}
	}
	
	while( (m_ExecMode == ExecMode_Suspended) && !m_Done )
	{
		m_ResumeEvent.Wait();
	}
}

void CoreEmuThread::Start()
{
	if( IsRunning() ) return;
	
	m_running			= false;
	m_ExecMode			= ExecMode_Idle;
	m_Done				= false;
	m_resetProfilers	= false;
	m_resetRecompilers	= false;
	m_elf_file			= wxEmptyString;

	m_ResumeEvent.Reset();
	m_SuspendEvent.Reset();
	PersistentThread::Start();

	pthread_detach( m_thread );
}

void CoreEmuThread::Reset()
{
	Cancel();
	StateRecovery::Clear();
}

// Resumes the core execution state, or does nothing is the core is already running.  If
// settings were changed, resets will be performed as needed and emulation state resumed from
// memory savestates.
void CoreEmuThread::Resume()
{
	Start();

	{
		ScopedLock locker( m_lock_ExecMode );

		if( m_ExecMode == ExecMode_Running )
			return;

		if( m_ExecMode == ExecMode_Suspending )
		{
			// if there are resets to be done, then we need to make sure and wait for the
			// emuThread to enter a fully suspended state before continuing...

			if( m_resetRecompilers || m_resetProfilers )
			{
				locker.Unlock();		// no deadlocks please, thanks. :)
				m_SuspendEvent.Wait();
			}
			else
			{
				m_ExecMode = ExecMode_Running;
				return;
			}
		}

		DevAssert( (m_ExecMode == ExecMode_Suspended) || (m_ExecMode == ExecMode_Idle),
			"EmuCoreThread is not in a suspended or idle state?  wtf!" );
	}

	if( m_resetRecompilers || m_resetProfilers )
	{
		SysClearExecutionCache();
		m_resetRecompilers = false;
		m_resetProfilers = false;
	}
	
	m_ExecMode = ExecMode_Running;
	m_ResumeEvent.Post();
}

// Pauses the emulation state at the next PS2 vsync, and returns control to the calling
// thread; or does nothing if the core is already suspended.  Calling this thread from the
// Core thread will result in deadlock.
//
// Parameters:
//   isNonblocking - if set to true then the function will not block for emulation suspension.
//      Defaults to false if parameter is not specified.  Performing non-blocking suspension
//      is mostly useful for starting certain non-Emu related gui activities (improves gui
//      responsiveness).
//
void CoreEmuThread::Suspend( bool isBlocking )
{
	{
		ScopedLock locker( m_lock_ExecMode );

		if( (m_ExecMode == ExecMode_Suspended) || (m_ExecMode == ExecMode_Idle) )
			return;
			
		if( m_ExecMode == ExecMode_Running )
			m_ExecMode = ExecMode_Suspending;
		
		DevAssert( m_ExecMode == ExecMode_Suspending, "ExecMode should be nothing other than Suspended..." );
	}

	m_SuspendEvent.Wait();
}

// Applies a full suite of new settings, which will automatically facilitate the necessary
// resets of the core and components (including plugins, if needed).  The scope of resetting
// is determined by comparing the current settings against the new settings.
void CoreEmuThread::ApplySettings( const Pcsx2Config& src )
{
	m_resetRecompilers = ( src.Cpu != EmuConfig.Cpu ) || ( src.Gamefixes != EmuConfig.Gamefixes ) || ( src.Speedhacks != EmuConfig.Speedhacks );
	m_resetProfilers = (src.Profiler != EmuConfig.Profiler );
	EmuConfig = src;
}
