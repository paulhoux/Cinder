
#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/MediaFoundation.h"

#include <string>
#include <Shlwapi.h>

namespace cinder {
namespace msw {

static const wchar_t *MF_WINDOW_CLASS_NAME = TEXT( "MFPlayer" );

static const UINT WM_APP_PLAYER_EVENT = WM_APP + 1; // TODO: come up with better name

LRESULT CALLBACK MFWndProc( HWND wnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	MFPlayer *impl = NULL;

	// If the message is WM_NCCREATE we need to hide 'this' in the window long.
	if( uMsg == WM_NCCREATE ) {
		impl = reinterpret_cast<MFPlayer*>( ( (LPCREATESTRUCT) lParam )->lpCreateParams );
		::SetWindowLongPtr( wnd, GWLP_USERDATA, (__int3264) (LONG_PTR) impl );
	}
	else
		impl = reinterpret_cast<MFPlayer*>( ::GetWindowLongPtr( wnd, GWLP_USERDATA ) );

	switch( uMsg ) {
		case WM_DESTROY:
			PostQuitMessage( 0 );
			break;
		case WM_APP_PLAYER_EVENT:
			if( impl )
				return impl->HandleEvent( wParam );
			break;
		default:
			return DefWindowProc( wnd, uMsg, wParam, lParam );
	}

	return 0;
}

MFPlayer::MFPlayer()
	: mRefCount( 1 ), mWnd( NULL )
{
	CreateWnd();
}

MFPlayer::~MFPlayer()
{
	Shutdown();
	DestroyWnd();
}

void MFPlayer::CreateWnd()
{
	const std::wstring unicodeTitle = L"";

	if( !mWnd ) {
		RegisterWindowClass();

		mWnd = ::CreateWindowEx( WS_EX_APPWINDOW,
								 MF_WINDOW_CLASS_NAME,
								 unicodeTitle.c_str(),
								 WS_OVERLAPPEDWINDOW,
								 CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
								 NULL,
								 NULL,
								 ::GetModuleHandle( NULL ),
								 reinterpret_cast<LPVOID>( this ) );
		if( !mWnd )
			return;

		::ShowWindow( mWnd, SW_SHOW );
		::SetWindowLongA( mWnd, GWL_EXSTYLE, ::GetWindowLongA( mWnd, GWL_EXSTYLE ) & ~WS_DLGFRAME & ~WS_CAPTION & ~WS_BORDER & WS_POPUP );
		::SetForegroundWindow( mWnd );
		::SetFocus( mWnd );
	}
}

void MFPlayer::DestroyWnd()
{
	if( mWnd )
		::DestroyWindow( mWnd );
}

void MFPlayer::Shutdown()
{
}

void MFPlayer::RegisterWindowClass()
{
	static bool sRegistered = false;

	if( sRegistered )
		return;

	WNDCLASS	wc;
	HMODULE instance = ::GetModuleHandle( NULL );
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MFWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = instance;
	wc.hIcon = ::LoadIcon( NULL, IDI_WINLOGO );
	wc.hCursor = ::LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH) ( BLACK_BRUSH );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = MF_WINDOW_CLASS_NAME;

	if( !::RegisterClass( &wc ) ) {
		DWORD err = ::GetLastError();
		return;
	}

	sRegistered = true;
}

// IUnknown methods

HRESULT MFPlayer::QueryInterface( REFIID riid, void** ppv )
{
	static const QITAB qit[] =
	{
		QITABENT( MFPlayer, IMFAsyncCallback ),
		{ 0 }
	};
	return QISearch( this, qit, riid, ppv );
}

ULONG MFPlayer::AddRef()
{
	return InterlockedIncrement( &mRefCount );
}

ULONG MFPlayer::Release()
{
	ULONG uCount = InterlockedDecrement( &mRefCount );
	if( uCount == 0 ) {
		delete this;
	}
	return uCount;
}

HRESULT MFPlayer::Invoke( IMFAsyncResult *pResult )
{
	return S_OK;
}

LRESULT MFPlayer::HandleEvent( WPARAM wParam )
{
	HRESULT hr = S_OK;
	IMFMediaEvent *pEvent = (IMFMediaEvent*) wParam;

	do {
		BREAK_ON_NULL( pEvent, E_POINTER );

		// Get the event type.
		MediaEventType meType = MEUnknown;
		hr = pEvent->GetType( &meType );
		BREAK_ON_FAIL( hr );

		// Get the event status. If the operation that triggered the event 
		// did not succeed, the status is a failure code.
		HRESULT hrStatus = S_OK;
		hr = pEvent->GetStatus( &hrStatus );
		BREAK_ON_FAIL( hr );

		// Check if the async operation succeeded.
		BREAK_ON_FAIL( hr = hrStatus );

		/*//
		switch( meType ) {
			case MESessionTopologyStatus:
				hr = OnTopologyStatus( pEvent );
				break;

			case MEEndOfPresentation:
				hr = OnPresentationEnded( pEvent );
				break;

			case MENewPresentation:
				hr = OnNewPresentation( pEvent );
				break;

			case MESessionTopologySet:
				IMFTopology * topology;
				GetEventObject<IMFTopology>( pEvent, &topology );
				WORD nodeCount;
				topology->GetNodeCount( &nodeCount );
				cout << "Topo set and we have " << nodeCount << " nodes" << endl;
				SafeRelease( &topology );
				break;

			case MESessionStarted:

				break;

			default:
				hr = OnSessionEvent( pEvent, meType );
				break;
		}
		//*/
	} while( false );

	SafeRelease( pEvent );

	return 0;
}

}
}
