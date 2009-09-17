/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2009  PCSX2 Dev Team
 * 
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */
 

#include "PrecompiledHeader.h"
#include "Threading.h"

#include <wx/datetime.h>
#include <wx/thread.h>
#include <wx/app.h>

#ifdef __LINUX__
#	include <signal.h>		// for pthread_kill, which is in pthread.h on w32-pthreads
#endif

using namespace Threading;

namespace Threading
{
	static const wxTimeSpan ts_msec_250( 0, 0, 0, 250 );

	static void _pt_callback_cleanup( void* handle )
	{
		((PersistentThread*)handle)->DoThreadCleanup();
	}

	PersistentThread::PersistentThread() :
		m_thread()
	,	m_sem_event()
	,	m_sem_finished()
	,	m_returncode( 0 )
	,	m_detached( false )
	,	m_running( false )
	{
	}

	// This destructor performs basic "last chance" cleanup, which is a blocking
	// join against non-detached threads.  Detached threads are unhandled.
	// Extending classes should always implement their own thread closure process.
	// This class must not be deleted from its own thread.  That would be like marrying
	// your sister, and then cheating on her with your daughter.
	PersistentThread::~PersistentThread() throw()
	{
		if( !m_running ) return;

		wxASSERT( !IsSelf() );		// not allowed from our own thread.

		if( !_InterlockedExchange( &m_detached, true ) )
		{
#if wxUSE_GUI
			m_sem_finished.WaitGui();
#else
			m_sem_finished.Wait();
#endif
			m_running = false;
		}
	}

	// This function should not be called from the owner thread.
	void PersistentThread::Start()
	{
		if( m_running ) return;
		m_sem_finished.Reset();
		if( pthread_create( &m_thread, NULL, _internal_callback, this ) != 0 )
			throw Exception::ThreadCreationError();

		m_running = true;
	}

	// This function should not be called from the owner thread.
	void PersistentThread::Detach()
	{
		if( !m_running ) return;
		if( _InterlockedExchange( &m_detached, true ) ) return;

		wxASSERT( !IsSelf() );		// not allowed from our own thread.
		pthread_detach( m_thread );
	}

	// Remarks:
	//   Provision of non-blocking Cancel() is probably academic, since destroying a PersistentThread
	//   object performs a blocking Cancel regardless of if you explicitly do a non-blocking Cancel()
	//   prior, since the ExecuteTask() method requires a valid object state.  If you really need
	//   fire-and-forget behavior on threads, use pthreads directly for now.
	//
	// This function should not be called from the owner thread.
	//
	// Parameters:
	//   isBlocking - indicates if the Cancel action should block for thread completion or not.
	//
	void PersistentThread::Cancel( bool isBlocking )
	{
	    if( !m_running ) return;

		if( _InterlockedExchange( &m_detached, true ) )
		{
			Console::Notice( "Threading Warning: Attempted to cancel detached thread; Ignoring..." );
			return;
		}

		wxASSERT( !IsSelf() );
		pthread_cancel( m_thread );

		if( isBlocking )
		{
#if wxUSE_GUI
			m_sem_finished.WaitGui();
#else
			m_sem_finished.Wait();
#endif
		}
		else
			pthread_detach( m_thread );

		m_running = false;
	}

	// Blocks execution of the calling thread until this thread completes its task.  The
	// caller should make sure to signal the thread to exit, or else blocking may deadlock the
	// calling thread.  Classes which extend PersistentThread should override this method
	// and signal any necessary thread exit variables prior to blocking.
	//
	// Returns the return code of the thread.
	// This method is roughly the equivalent of pthread_join().
	//
	sptr PersistentThread::Block()
	{
		if( _InterlockedExchange( &m_detached, true ) )
		{
			// already detached: if we're still running then its an invalid operation
			if( m_running )
				throw Exception::InvalidOperation( "Blocking on detached threads requires manual semaphore implementation." );

			return m_returncode;
		}
		else
		{
			DevAssert( !IsSelf(), "Thread deadlock detected; Block() should never be called by the owner thread." );

#if wxUSE_GUI
			m_sem_finished.WaitGui();
#else
			m_sem_finished.Wait();
#endif
			return m_returncode;
		}
	}

	bool PersistentThread::IsSelf() const
	{
		return pthread_self() == m_thread;
	}

	bool PersistentThread::IsRunning() const
	{
	    if (!m_running) return false;
	    
		if( !!m_detached )
			return !!m_running;
		else
			return ( ESRCH != pthread_kill( m_thread, 0 ) );
	}

	// Exceptions:
	//   InvalidOperation - thrown if the thread is still running or has never been started.
	//
	sptr PersistentThread::GetReturnCode() const
	{
		if( IsRunning() )
			throw Exception::InvalidOperation( "Thread.GetReturnCode : thread is still running." );

		return m_returncode;
	}

	// invoked when canceling or exiting the thread.
	void PersistentThread::DoThreadCleanup()
	{
		wxASSERT( IsSelf() );	// only allowed from our own thread, thanks.
		_InterlockedExchange( &m_running, false );
		m_sem_finished.Post();
	}

	void* PersistentThread::_internal_callback( void* itsme )
	{
		jASSUME( itsme != NULL );
		PersistentThread& owner = *((PersistentThread*)itsme);

		pthread_cleanup_push( _pt_callback_cleanup, itsme );
		owner.m_returncode = owner.ExecuteTask();
		pthread_cleanup_pop( true );

		return (void*)owner.m_returncode;
	}

// --------------------------------------------------------------------------------------
//  BaseTaskThread Implementations
// --------------------------------------------------------------------------------------

	// Tells the thread to exit and then waits for thread termination.
	sptr BaseTaskThread::Block()
	{
		if( !IsRunning() ) return m_returncode;
		m_Done = true;
		m_sem_event.Post();
		return PersistentThread::Block();
	}

	// Initiates the new task.  This should be called after your own StartTask has
	// initialized internal variables / preparations for task execution.
	void BaseTaskThread::PostTask()
	{
		wxASSERT( !m_detached );

		ScopedLock locker( m_lock_TaskComplete );
		m_TaskPending = true;
		m_post_TaskComplete.Reset();
		m_sem_event.Post();
	}
	
	// Blocks current thread execution pending the completion of the parallel task.
	void BaseTaskThread::WaitForResult()
	{
		if( m_detached || !m_running ) return;
		if( m_TaskPending )
		#ifdef wxUSE_GUI
			m_post_TaskComplete.WaitGui();
		#else
			m_post_TaskComplete.Wait();
		#endif

		m_post_TaskComplete.Reset();
	}

	sptr BaseTaskThread::ExecuteTask()
	{
		while( !m_Done )
		{
			// Wait for a job -- or get a pthread_cancel.  I'm easy.
			m_sem_event.Wait();

			Task();
			m_lock_TaskComplete.Lock();
			m_TaskPending = false;
			m_post_TaskComplete.Post();
			m_lock_TaskComplete.Unlock();
		};

		return 0;
	}

// --------------------------------------------------------------------------------------
//  pthread Cond is an evil api that is not suited for Pcsx2 needs.
//  Let's not use it. (Air)
// --------------------------------------------------------------------------------------

#if 0
	WaitEvent::WaitEvent()
	{
		int err = 0;

		err = pthread_cond_init(&cond, NULL);
		err = pthread_mutex_init(&mutex, NULL);
	}

	WaitEvent::~WaitEvent()
	{
		pthread_cond_destroy( &cond );
		pthread_mutex_destroy( &mutex );
	}

	void WaitEvent::Set()
	{
		pthread_mutex_lock( &mutex );
		pthread_cond_signal( &cond );
		pthread_mutex_unlock( &mutex );
	}

	void WaitEvent::Wait()
	{
		pthread_mutex_lock( &mutex );
		pthread_cond_wait( &cond, &mutex );
		pthread_mutex_unlock( &mutex );
	}
#endif

// --------------------------------------------------------------------------------------
//  Semaphore Implementations
// --------------------------------------------------------------------------------------
	
	Semaphore::Semaphore()
	{
		sem_init( &sema, false, 0 );
	}

	Semaphore::~Semaphore()
	{
		sem_destroy( &sema );
	}

	void Semaphore::Reset()
	{
		sem_destroy( &sema );
		sem_init( &sema, false, 0 );
	}

	void Semaphore::Post()
	{
		sem_post( &sema );
	}

	// Valid on Win32 builds only!!  Attempts to use it on Linux will result in unresolved
	// external linker errors.
	void Semaphore::Post( int multiple )
	{
#if defined(_MSC_VER)
		sem_post_multiple( &sema, multiple );
#else
		// Only w32pthreads has the post_multiple, but it's easy enough to fake:
		while( multiple > 0 )
		{
			multiple--;
			sem_post( &sema );
		}
#endif
	}

#if wxUSE_GUI
	// This is a wxApp-safe implementation of Wait, which makes sure and executes the App's
	// pending messages *if* the Wait is performed on the Main/GUI thread.  If the Wait is
	// called from another thread, no message pumping is performed.
	void Semaphore::WaitGui()
	{
		if( !wxThread::IsMain() || (wxTheApp == NULL) )
			Wait();
		else
		{
			// In order to avoid deadlock we need to make sure we cut some time
			// to handle messages.
			
			do {
				wxTheApp->Yield();
			} while( !Wait( ts_msec_250 ) );
		}
	}

	bool Semaphore::WaitGui( const wxTimeSpan& timeout )
	{
		if( !wxThread::IsMain() || (wxTheApp == NULL) )
		{
			return Wait( timeout );
		}
		else
		{
			wxTimeSpan countdown( (timeout) );

			// In order to avoid deadlock we need to make sure we cut some time
			// to handle messages.

			do {
				wxTheApp->Yield();
				if( Wait( ts_msec_250 ) ) break;
				countdown -= ts_msec_250;
			} while( countdown.GetMilliseconds() > 0 );

			return countdown.GetMilliseconds() > 0;
		}
	}
#endif

	void Semaphore::Wait()
	{
		sem_wait( &sema );
	}

	bool Semaphore::Wait( const wxTimeSpan& timeout )
	{
		wxDateTime megafail( wxDateTime::UNow() + timeout );
		const timespec fail = { megafail.GetTicks(), megafail.GetMillisecond() * 1000000 };
		return sem_timedwait( &sema, &fail ) != -1;
	}

	// Performs an uncancellable wait on a semaphore; restoring the thread's previous cancel state
	// after the wait has completed.  Useful for situations where the semaphore itself is stored on
	// the stack and passed to another thread via GUI message or such, avoiding complications where
	// the thread might be canceled and the stack value becomes invalid.
	//
	// Performance note: this function has quite a bit more overhead compared to Semaphore::Wait(), so
	// consider manually specifying the thread as uncancellable and using Wait() instead if you need
	// to do a lot of no-cancel waits in a tight loop worker thread, for example.
	void Semaphore::WaitNoCancel()
	{
		int oldstate;
		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldstate );
		Wait();
		pthread_setcancelstate( oldstate, NULL );
	}

	int Semaphore::Count()
	{
		int retval;
		sem_getvalue( &sema, &retval );
		return retval;
	}

// --------------------------------------------------------------------------------------
//  MutexLock Implementations
// --------------------------------------------------------------------------------------

	MutexLock::MutexLock()
	{
		int err = 0;
		err = pthread_mutex_init( &mutex, NULL );
	}

	MutexLock::MutexLock( bool isRecursive )
	{
		if( isRecursive )
		{
			pthread_mutexattr_t mutexAttribute;
			int status = pthread_mutexattr_init( &mutexAttribute );
			if (status != 0) { /* ... */ }
			status = pthread_mutexattr_settype( &mutexAttribute, PTHREAD_MUTEX_RECURSIVE);
			if (status != 0) { /* ... */}

			int err = 0;
			err = pthread_mutex_init( &mutex, &mutexAttribute );
		}
		else
		{
			int err = 0;
			err = pthread_mutex_init( &mutex, NULL );
		}
	}

	MutexLock::~MutexLock()
	{
		pthread_mutex_destroy( &mutex );
	}

	void MutexLock::Lock()
	{
		pthread_mutex_lock( &mutex );
	}

	void MutexLock::Unlock()
	{
		pthread_mutex_unlock( &mutex );
	}

// --------------------------------------------------------------------------------------
//  InterlockedExchanges / AtomicExchanges (PCSX2's Helper versions)
// --------------------------------------------------------------------------------------
// define some overloads for InterlockedExchanges for commonly used types, like u32 and s32.

	__forceinline void AtomicExchange( volatile u32& Target, u32 value )
	{
		_InterlockedExchange( (volatile long*)&Target, value );
	}

	__forceinline void AtomicExchangeAdd( volatile u32& Target, u32 value )
	{
		_InterlockedExchangeAdd( (volatile long*)&Target, value );
	}

	__forceinline void AtomicIncrement( volatile u32& Target )
	{
		_InterlockedExchangeAdd( (volatile long*)&Target, 1 );
	}

	__forceinline void AtomicDecrement( volatile u32& Target )
	{
		_InterlockedExchangeAdd( (volatile long*)&Target, -1 );
	}

	__forceinline void AtomicExchange( volatile s32& Target, s32 value )
	{
		_InterlockedExchange( (volatile long*)&Target, value );
	}

	__forceinline void AtomicExchangeAdd( s32& Target, u32 value )
	{
		_InterlockedExchangeAdd( (volatile long*)&Target, value );
	}

	__forceinline void AtomicIncrement( volatile s32& Target )
	{
		_InterlockedExchangeAdd( (volatile long*)&Target, 1 );
	}

	__forceinline void AtomicDecrement( volatile s32& Target )
	{
		_InterlockedExchangeAdd( (volatile long*)&Target, -1 );
	}

	__forceinline void _AtomicExchangePointer( const void ** target, const void* value )
	{
		_InterlockedExchange( (volatile long*)target, (long)value );
	}

	__forceinline void _AtomicCompareExchangePointer( const void ** target, const void* value, const void* comparand )
	{
		_InterlockedCompareExchange( (volatile long*)target, (long)value, (long)comparand );
	}
}
