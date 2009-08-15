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
#include "App.h"
#include "MainFrame.h"
#include "Utilities/Console.h"
#include "DebugTools/Debug.h"

#include <wx/file.h>
#include <wx/textfile.h>

// This code was 'borrowed' from wxWidgets built in console log class and then heavily
// modified to suite our needs.  I would have used some crafty subclassing instead except
// who ever wrote the code of wxWidgets had a peculiar love of the 'private' keyword,
// thus killing any possibility of subclassing in a useful manner.  (sigh)


DECLARE_EVENT_TYPE(wxEVT_LOG_Write, -1)
DECLARE_EVENT_TYPE(wxEVT_LOG_Newline, -1)
DECLARE_EVENT_TYPE(wxEVT_SetTitleText, -1)
DECLARE_EVENT_TYPE(wxEVT_SemaphoreWait, -1);

DEFINE_EVENT_TYPE(wxEVT_LOG_Write)
DEFINE_EVENT_TYPE(wxEVT_LOG_Newline)
DEFINE_EVENT_TYPE(wxEVT_SetTitleText)
DEFINE_EVENT_TYPE(wxEVT_DockConsole)
DEFINE_EVENT_TYPE(wxEVT_SemaphoreWait);

using Console::Colors;

// ----------------------------------------------------------------------------
sptr ConsoleTestThread::ExecuteTask()
{
	static int numtrack = 0;
	
	while( !m_done )
	{
		// Two lines, both formatted, and varied colors.  This makes for a fairly realistic
		// worst case scenario (without being entirely unrealistic).
		Console::WriteLn( wxsFormat( L"This is a threaded logging test. Something bad could happen... %d", ++numtrack ) );
		Console::Status( wxsFormat( L"Testing high stress loads %s", L"(multi-color)" ) );
		Sleep( 0 );
	}
	return 0;
}

// ----------------------------------------------------------------------------
// pass an uninitialized file object, the function will ask the user for the
// filename and try to open it, returns true on success (file was opened),
// false if file couldn't be opened/created and -1 if the file selection
// dialog was canceled
//
static bool OpenLogFile(wxFile& file, wxString& filename, wxWindow *parent)
{
    filename = wxSaveFileSelector(L"log", L"txt", L"log.txt", parent);
    if ( !filename ) return false; // canceled

    if( wxFile::Exists(filename) )
    {
        bool bAppend = false;
        wxString strMsg;
        strMsg.Printf(L"Append log to file '%s' (choosing [No] will overwrite it)?",
                      filename.c_str());

        switch ( wxMessageBox(strMsg, L"Question", wxICON_QUESTION | wxYES_NO | wxCANCEL) )
		{
			case wxYES:
				bAppend = true;
			break;

			case wxNO:
				bAppend = false;
			break;

			case wxCANCEL:
				return false;

			default:
				wxFAIL_MSG( L"invalid message box return value" );
        }

		return ( bAppend ) ?
			file.Open(filename, wxFile::write_append) :
            file.Create(filename, true /* overwrite */);
    }

	return file.Create(filename);
}

// ------------------------------------------------------------------------
// fontsize - size of the font specified in points.
//   (actual font used is the system-selected fixed-width font)
//
ConsoleLogFrame::ColorArray::ColorArray( int fontsize ) :
	m_table( 8 )
{
	Create( fontsize );
}

ConsoleLogFrame::ColorArray::~ColorArray()
{
	Cleanup();
}

void ConsoleLogFrame::ColorArray::Create( int fontsize )
{
	wxFont fixed( fontsize, wxMODERN, wxNORMAL, wxNORMAL );
	wxFont fixedB( fontsize, wxMODERN, wxNORMAL, wxBOLD );

	// Standard R, G, B format:
	new (&m_table[Color_Black])		wxTextAttr( wxColor(   0,   0,   0 ), wxNullColour, fixed );
	new (&m_table[Color_Red])		wxTextAttr( wxColor( 128,   0,   0 ), wxNullColour, fixedB );
	new (&m_table[Color_Green])		wxTextAttr( wxColor(   0, 128,   0 ), wxNullColour, fixed );
	new (&m_table[Color_Blue])		wxTextAttr( wxColor(   0,   0, 128 ), wxNullColour, fixed );
	new (&m_table[Color_Yellow])	wxTextAttr( wxColor( 160, 160,   0 ), wxNullColour, fixedB );
	new (&m_table[Color_Cyan])		wxTextAttr( wxColor(   0, 140, 140 ), wxNullColour, fixed );
	new (&m_table[Color_Magenta])	wxTextAttr( wxColor( 160,   0, 160 ), wxNullColour, fixed );
	new (&m_table[Color_White])		wxTextAttr( wxColor( 128, 128, 128 ), wxNullColour, fixed );
}

void ConsoleLogFrame::ColorArray::Cleanup()
{
	// The contents of m_table were created with placement new, and must be
	// disposed of manually:
	
	for( int i=0; i<8; ++i )
		m_table[i].~wxTextAttr();
}

// fixme - not implemented yet.
void ConsoleLogFrame::ColorArray::SetFont( const wxFont& font )
{
	//for( int i=0; i<8; ++i )
	//	m_table[i].SetFont( font );
}

void ConsoleLogFrame::ColorArray::SetFont( int fontsize )
{
	Cleanup();
	Create( fontsize );
}

static const Console::Colors DefaultConsoleColor = Color_White;

enum MenuIDs_t
{
	MenuID_FontSize_Small = 0x10,
	MenuID_FontSize_Normal,
	MenuID_FontSize_Large,
	MenuID_FontSize_Huge,
};

// ------------------------------------------------------------------------
ConsoleLogFrame::ConsoleLogFrame( MainEmuFrame *parent, const wxString& title, const AppConfig::ConsoleLogOptions& options ) :
	wxFrame(parent, wxID_ANY, title)
,	m_conf( options )
,	m_TextCtrl( *new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE | wxHSCROLL | wxTE_READONLY | wxTE_RICH2 ) )
,	m_ColorTable( options.FontSize )
,	m_curcolor( DefaultConsoleColor )
,	m_msgcounter( 0 )
,	m_threadlogger( EnableThreadedLoggingTest ? new ConsoleTestThread() : NULL )
{
	m_TextCtrl.SetBackgroundColour( wxColor( 230, 235, 242 ) );

    // create Log menu (contains most options)
	wxMenuBar *pMenuBar = new wxMenuBar();
	wxMenu& menuLog = *new wxMenu();
	menuLog.Append(wxID_SAVE,  _("&Save..."),	_("Save log contents to file"));
	menuLog.Append(wxID_CLEAR, _("C&lear"),		_("Clear the log window contents"));
	menuLog.AppendSeparator();
	menuLog.Append(wxID_CLOSE, _("&Close"),		_("Close this log window; contents are preserved"));

	// create Appearance menu and submenus

	wxMenu& menuFontSizes = *new wxMenu();
	menuFontSizes.Append( MenuID_FontSize_Small,	_("Small"),	_("Fits a lot of log in a microcosmically small area."),
		wxITEM_RADIO )->Check( options.FontSize == 7 );
	menuFontSizes.Append( MenuID_FontSize_Normal,	_("Normal"),_("It's what I use (the programmer guy)."),
		wxITEM_RADIO )->Check( options.FontSize == 8 );
	menuFontSizes.Append( MenuID_FontSize_Large,	_("Large"),	_("Its nice and readable."),
		wxITEM_RADIO )->Check( options.FontSize == 10 );
	menuFontSizes.Append( MenuID_FontSize_Huge,		_("Huge"),	_("In case you have a really high res display."),
		wxITEM_RADIO )->Check( options.FontSize == 12 );

	wxMenu& menuAppear = *new wxMenu();
	menuAppear.Append( wxID_ANY, _("Always on Top"),
		_("When checked the log window will be visible over other foreground windows."), wxITEM_CHECK );
	menuAppear.Append( wxID_ANY, _("Font Size"), &menuFontSizes ); 

	pMenuBar->Append(&menuLog,		_("&Log"));
	pMenuBar->Append(&menuAppear,	_("&Appearance"));
	SetMenuBar(pMenuBar);

	// status bar for menu prompts
	CreateStatusBar();
	ClearColor();

	SetSize( wxRect( options.DisplayPosition, options.DisplaySize ) );
	Show( options.Visible );

	// Bind Events:

	Connect( wxID_OPEN,  wxEVT_COMMAND_MENU_SELECTED, wxMenuEventHandler(ConsoleLogFrame::OnOpen)  );
	Connect( wxID_CLOSE, wxEVT_COMMAND_MENU_SELECTED, wxMenuEventHandler(ConsoleLogFrame::OnClose) );
	Connect( wxID_SAVE,  wxEVT_COMMAND_MENU_SELECTED, wxMenuEventHandler(ConsoleLogFrame::OnSave)  );
	Connect( wxID_CLEAR, wxEVT_COMMAND_MENU_SELECTED, wxMenuEventHandler(ConsoleLogFrame::OnClear) );

	Connect( MenuID_FontSize_Small, MenuID_FontSize_Huge, wxEVT_COMMAND_MENU_SELECTED, wxMenuEventHandler( ConsoleLogFrame::OnFontSize ) );

	Connect( wxEVT_CLOSE_WINDOW,	wxCloseEventHandler(ConsoleLogFrame::OnCloseWindow) );
	Connect( wxEVT_MOVE,			wxMoveEventHandler(ConsoleLogFrame::OnMoveAround) );
	Connect( wxEVT_SIZE,			wxSizeEventHandler(ConsoleLogFrame::OnResize) );

	Connect( wxEVT_LOG_Write,		wxCommandEventHandler(ConsoleLogFrame::OnWrite) );
	Connect( wxEVT_LOG_Newline,		wxCommandEventHandler(ConsoleLogFrame::OnNewline) );
	Connect( wxEVT_SetTitleText,	wxCommandEventHandler(ConsoleLogFrame::OnSetTitle) );
	Connect( wxEVT_DockConsole,		wxCommandEventHandler(ConsoleLogFrame::OnDockedMove) );
	Connect( wxEVT_SemaphoreWait,	wxCommandEventHandler(ConsoleLogFrame::OnSemaphoreWait) );
	
	if( m_threadlogger != NULL )
		m_threadlogger->Start();
}

ConsoleLogFrame::~ConsoleLogFrame()
{
	safe_delete( m_threadlogger );
}

void ConsoleLogFrame::SetColor( Colors color )
{
	if( color != m_curcolor )
		m_TextCtrl.SetDefaultStyle( m_ColorTable[m_curcolor=color] );
}

void ConsoleLogFrame::ClearColor()
{
	if( DefaultConsoleColor != m_curcolor )
		m_TextCtrl.SetDefaultStyle( m_ColorTable[m_curcolor=DefaultConsoleColor] );
}

void ConsoleLogFrame::Write( const wxString& text )
{
	// remove selection (WriteText is in fact ReplaceSelection)
	// TODO : Optimize this to only replaceslection if some selection
	//   messages have been recieved since the last write.

#ifdef __WXMSW__
	wxTextPos nLen = m_TextCtrl.GetLastPosition();
	m_TextCtrl.SetSelection(nLen, nLen);
#endif

	m_TextCtrl.AppendText( text );
	
	// cap at 256k for now...
	// fixme - 256k runs well on win32 but appears to be very sluggish on linux.  Might
	// need platform dependent defaults here. - air
	if( m_TextCtrl.GetLastPosition() > 0x40000 )
	{
		m_TextCtrl.Remove( 0, 0x10000 );
	}
}

// Implementation note:  Calls SetColor and Write( text ).  Override those virtuals
// and this one will magically follow suite. :)
void ConsoleLogFrame::Write( Colors color, const wxString& text )
{
	SetColor( color );
	Write( text );
}

void ConsoleLogFrame::Newline()
{
	Write( L"\n" );
}

void ConsoleLogFrame::DoClose()
{
    // instead of closing just hide the window to be able to Show() it later
    Show(false);
	if( wxFrame* main = wxGetApp().GetMainWindow() )
		wxStaticCast( main, MainEmuFrame )->OnLogBoxHidden();
}

void ConsoleLogFrame::DockedMove()
{
	SetPosition( m_conf.DisplayPosition );
}

// =================================================================================
//  Section : Event Handlers
//    * Misc Window Events (Move, Resize,etc)
//    * Menu Events
//    * Logging Events
// =================================================================================

// Special event recieved from a window we're docked against.
void ConsoleLogFrame::OnDockedMove( wxCommandEvent& event )
{
	DockedMove();
}

void ConsoleLogFrame::OnMoveAround( wxMoveEvent& evt )
{
	// Docking check!  If the window position is within some amount
	// of the main window, enable docking.

	if( wxFrame* main = wxGetApp().GetMainWindow() )
	{
		wxPoint topright( main->GetRect().GetTopRight() );
		wxRect snapzone( topright - wxSize( 8,8 ), wxSize( 16,16 ) );

		m_conf.AutoDock = snapzone.Contains( GetPosition() );
		//Console::WriteLn( "DockCheck: %d", params g_Conf->ConLogBox.AutoDock );
		if( m_conf.AutoDock )
		{
			SetPosition( topright + wxSize( 1,0 ) );
			m_conf.AutoDock = true;
		}
	}
	m_conf.DisplayPosition = GetPosition();
	evt.Skip();
}

void ConsoleLogFrame::OnResize( wxSizeEvent& evt )
{
	m_conf.DisplaySize = GetSize();
	evt.Skip();
}

void ConsoleLogFrame::OnCloseWindow(wxCloseEvent& event)
{
	if( event.CanVeto() )
		DoClose();
	else
	{
		safe_delete( m_threadlogger );
		event.Skip();
	}
}

void ConsoleLogFrame::OnOpen(wxMenuEvent& WXUNUSED(event))
{
	Show(true);
}

void ConsoleLogFrame::OnClose( wxMenuEvent& event )
{
	DoClose();
}

void ConsoleLogFrame::OnSave(wxMenuEvent& WXUNUSED(event))
{
    wxString filename;
    wxFile file;
    bool rc = OpenLogFile( file, filename, this );
    if ( !rc )
    {
        // canceled
        return;
    }

    // retrieve text and save it
    // -------------------------
    int nLines = m_TextCtrl.GetNumberOfLines();
    for ( int nLine = 0; nLine < nLines; nLine++ )
    {
		if( !file.Write(m_TextCtrl.GetLineText(nLine) + wxTextFile::GetEOL()) )
		{
			wxLogError( L"Can't save log contents to file." );
			return;
		}
	}

	wxLogStatus(this, L"Log saved to the file '%s'.", filename.c_str());
}

void ConsoleLogFrame::OnClear(wxMenuEvent& WXUNUSED(event))
{
    m_TextCtrl.Clear();
}

void ConsoleLogFrame::OnFontSize(wxMenuEvent& evt )
{
	int ptsize = 8;
	switch( evt.GetId() )
	{
		case MenuID_FontSize_Small:		ptsize = 7; break;
		case MenuID_FontSize_Normal:	ptsize = 8; break;
		case MenuID_FontSize_Large:		ptsize = 10; break;
		case MenuID_FontSize_Huge:		ptsize = 12; break;
	}
	
	if( ptsize == m_conf.FontSize ) return;
	
	m_conf.FontSize = ptsize;
	m_ColorTable.SetFont( ptsize );
	m_TextCtrl.SetDefaultStyle( m_ColorTable[m_curcolor] );

	// TODO: Process the attributes of each character and upgrade the font size,
	// while still retaining color and bold settings...  (might be slow but then
	// it hardly matters being a once-in-a-bluemoon action).

}

// ----------------------------------------------------------------------------
//  Logging Events (typically recieved from Console class interfaces)
// ----------------------------------------------------------------------------

void ConsoleLogFrame::OnWrite( wxCommandEvent& event )
{
	Write( (Colors)event.GetExtraLong(), event.GetString() );
	DoMessage();
}

void ConsoleLogFrame::OnNewline( wxCommandEvent& event )
{
	Newline();
	DoMessage();
}

void ConsoleLogFrame::OnSetTitle( wxCommandEvent& event )
{
	SetTitle( event.GetString() );
}

void ConsoleLogFrame::OnSemaphoreWait( wxCommandEvent& event )
{
	m_semaphore.Post();
}

// ------------------------------------------------------------------------
// Deadlock protection: High volume logs will over-tax our message pump and cause the
// GUI to become inaccessible.  The cool solution would be a threaded log window, but wx
// is entirely un-safe for that kind of threading.  So instead I use a message counter
// that stalls non-GUI threads when they attempt to over-tax an already burdened log.
// If too many messages get queued up, non-gui threads are stalled to allow the gui to
// catch up.
void ConsoleLogFrame::CountMessage()
{
	long result = _InterlockedIncrement( &m_msgcounter );

	if( result > 0x20 )		// 0x20 -- arbitrary value that seems to work well (tested on P4 and C2D)
	{
		if( !wxThread::IsMain() )
		{
			// Append an event that'll post up our semaphore.  It'll get run "in
			// order" which means when it posts all queued messages will have been
			// processed.

			wxCommandEvent evt( wxEVT_SemaphoreWait );
			GetEventHandler()->AddPendingEvent( evt );
			m_semaphore.Wait();
		}
	}
}

// Thread Safety note: This function expects to be called from the Main GUI thread
// only.  If called from a thread other than Main, it will generate an assertion failure.
// 
void ConsoleLogFrame::DoMessage()
{
	wxASSERT_MSG( wxThread::IsMain(), L"DoMessage must only be called from the main gui thread!" );

	int cur = _InterlockedDecrement( &m_msgcounter );
	
	// We need to freeze the control if there are more than 2 pending messages,
	// otherwise the redraw of the console will prevent it from ever being able to
	// catch up with the rate the queue is being filled, and the whole app could
	// deadlock. >_<

	if( m_TextCtrl.IsFrozen() )
	{
		if( cur < 1 )
			m_TextCtrl.Thaw();
	}
	else if( cur >= 3 )
	{
		m_TextCtrl.Freeze();
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
// 
namespace Console
{
	// thread-local console color storage.
	__threadlocal Colors th_CurrentColor = DefaultConsoleColor;

	void __fastcall SetTitle( const wxString& title )
	{
		wxCommandEvent evt( wxEVT_SetTitleText );
		evt.SetString( title );
		wxGetApp().ProgramLog_PostEvent( evt );
	}

	void __fastcall SetColor( Colors color )
	{
		th_CurrentColor = color;
	}

	void ClearColor()
	{
		th_CurrentColor = DefaultConsoleColor;
	}

	bool Newline()
	{
		if( emuLog != NULL )
			fputs( "\n", emuLog );

		wxCommandEvent evt( wxEVT_LOG_Newline );
		wxGetApp().ProgramLog_PostEvent( evt );
		wxGetApp().ProgramLog_CountMsg();

		return false;
	}

	bool __fastcall Write( const char* fmt )
	{
		if( emuLog != NULL )
			fputs( fmt, emuLog );

		wxCommandEvent evt( wxEVT_LOG_Write );
		evt.SetString( wxString::FromAscii( fmt ) );
		evt.SetExtraLong( th_CurrentColor );
		wxGetApp().ProgramLog_PostEvent( evt );
		wxGetApp().ProgramLog_CountMsg();

		return false;
	}

	bool __fastcall Write( const wxString& fmt )
	{
		if( emuLog != NULL )
			fputs( fmt.ToAscii().data(), emuLog );

		wxCommandEvent evt( wxEVT_LOG_Write );
		evt.SetString( fmt );
		evt.SetExtraLong( th_CurrentColor );
		wxGetApp().ProgramLog_PostEvent( evt );
		wxGetApp().ProgramLog_CountMsg();

		return false;
	}
	
	bool __fastcall WriteLn( const char* fmt )
	{
		// Implementation note: I've duplicated Write+Newline behavior here to avoid polluting
		// the message pump with lots of erroneous messages (Newlines can be bound into Write message).
		
		if( emuLog != NULL )
		{
			fputs( fmt, emuLog );
			fputs( "\n", emuLog );
		}

		wxCommandEvent evt( wxEVT_LOG_Write );
		evt.SetString( wxString::FromAscii( fmt ) + L"\n" );
		evt.SetExtraLong( th_CurrentColor );
		wxGetApp().ProgramLog_PostEvent( evt );
		wxGetApp().ProgramLog_CountMsg();

		return false;
	}

	bool __fastcall WriteLn( const wxString& fmt )
	{
		// Implementation note: I've duplicated Write+Newline behavior here to avoid polluting
		// the message pump with lots of erroneous messages (Newlines can be bound into Write message).

		if( emuLog != NULL )
		{
			fputs( fmt.ToAscii().data(), emuLog );
			fputs( "\n", emuLog );
		}

		wxCommandEvent evt( wxEVT_LOG_Write );
		evt.SetString( fmt + L"\n" );
		evt.SetExtraLong( th_CurrentColor );
		wxGetApp().ProgramLog_PostEvent( evt );
		wxGetApp().ProgramLog_CountMsg();

		return false;
	}
}

#define wxEVT_BOX_ALERT		78

using namespace Threading;

DEFINE_EVENT_TYPE( pxEVT_MSGBOX );

namespace Msgbox
{
	struct InstanceData
	{
		Semaphore	WaitForMe;
		int			result;
		
		InstanceData() :
			WaitForMe(), result( 0 )
		{
		}
	};

	// parameters:
	//   flags - messagebox type flags, such as wxOK, wxCANCEL, etc.
	//
	static int ThreadedMessageBox( int flags, const wxString& text )
	{
		// must pass the message to the main gui thread, and then stall this thread, to avoid
		// threaded chaos where our thread keeps running while the popup is awaiting input.

		InstanceData instdat;
		wxCommandEvent tevt( pxEVT_MSGBOX );
		tevt.SetString( text );
		tevt.SetClientData( &instdat );
		tevt.SetExtraLong( flags );
		wxGetApp().AddPendingEvent( tevt );
		instdat.WaitForMe.WaitNoCancel();		// Important! disable cancellation since we're using local stack vars.
		return instdat.result;
	}
	
	void OnEvent( wxCommandEvent& evt )
	{
		// Must be called from the GUI thread ONLY.
		wxASSERT( wxThread::IsMain() );

		int result = Alert( evt.GetString() );
		InstanceData* instdat = (InstanceData*)evt.GetClientData();
		instdat->result = result;
		instdat->WaitForMe.Post();
	}

	bool Alert( const wxString& text )
	{
		if( wxThread::IsMain() )
			wxMessageBox( text, L"Pcsx2 Message", wxOK, wxGetApp().GetTopWindow() );
		else
			ThreadedMessageBox( wxOK, text );
		return false;
	}

	bool OkCancel( const wxString& text )
	{
		if( wxThread::IsMain() )
		{
			return wxOK == wxMessageBox( text, L"Pcsx2 Message", wxOK | wxCANCEL, wxGetApp().GetTopWindow() );
		}
		else
		{
			return wxOK == ThreadedMessageBox( wxOK | wxCANCEL, text );
		}
	}
}
