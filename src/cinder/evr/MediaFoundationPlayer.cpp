#include "cinder/evr/MediaFoundationPlayer.h"
#include "cinder/evr/MediaFoundationVideo.h"

#include "cinder/CinderAssert.h"
#include "cinder/Log.h"

#if defined(CINDER_MSW)

//#pragma comment(lib, "mf.lib")
//#pragma comment(lib, "mfplat.lib")
//#pragma comment(lib, "mfuuid.lib")
//#pragma comment(lib, "mfreadwrite.lib")
//#pragma comment(lib, "strmiids.lib")
//#pragma comment(lib, "shlwapi.lib")

namespace cinder {
	namespace msw {
		namespace video {

			//////////////////////////////////////////////////////////////////////////////////////////

			MediaFoundationPlayer::MediaFoundationPlayer( HRESULT &hr, HWND hwnd )
				: mRefCount( 0 ), mHwnd( hwnd ), mState( Closed ), mIsInitialized( false ), mCurrentVolume( 1 ), mWidth( 0 ), mHeight( 0 )
				, mMediaSessionPtr( NULL ), mSequencerSourcePtr( NULL ), mMediaSourcePtr( NULL ), mVideoDisplayControlPtr( NULL ), mAudioStreamVolumePtr( NULL ), mPresenterPtr( NULL )
				, mOpenEventHandle( NULL ), mCloseEventHandle( NULL )
			{
				hr = S_OK;

				mOpenEventHandle = CreateEvent( NULL, FALSE, FALSE, NULL );
				mCloseEventHandle = CreateEvent( NULL, FALSE, FALSE, NULL );

				do {
					hr = MFStartup( MF_VERSION );
					BREAK_ON_FAIL( hr );

					mIsInitialized = true;

					// Create custom EVR presenter.
					mPresenterPtr = new EVRCustomPresenter( hr );
					mPresenterPtr->AddRef();
					BREAK_ON_FAIL( hr );

					hr = mPresenterPtr->SetVideoWindow( mHwnd );
					BREAK_ON_FAIL( hr );
				} while( false );

				if( FAILED( hr ) )
					SafeRelease( mPresenterPtr );

				CI_LOG_V( "Created MediaFoundationPlayer." );
			}

			MediaFoundationPlayer::~MediaFoundationPlayer()
			{
				SafeRelease( mPresenterPtr );

				if( mIsInitialized )
					MFShutdown();

				mIsInitialized = false;

				CloseHandle( mCloseEventHandle );
				CloseHandle( mOpenEventHandle );

				CI_LOG_V( "Destroyed MediaFoundationPlayer." );
			}

			HRESULT MediaFoundationPlayer::OpenFile( LPCWSTR pszFileName )
			{
				HRESULT hr = S_OK;

				do {
					// Create the media session.
					hr = CreateSession();
					BREAK_ON_FAIL( hr );

					// Create the media source.
					hr = CreateMediaSource( pszFileName, &mMediaSourcePtr );
					BREAK_ON_FAIL( hr );

					// Create the presentation descriptor for the media source.
					ScopedComPtr<IMFPresentationDescriptor> pPD;
					hr = mMediaSourcePtr->CreatePresentationDescriptor( &pPD );
					BREAK_ON_FAIL( hr );

					hr = CreatePartialTopology( pPD );
					BREAK_ON_FAIL( hr );

					mState = OpenPending;
				} while( false );

				if( FAILED( hr ) )
					CloseSession();

				return hr;
			}

			HRESULT MediaFoundationPlayer::Close()
			{
				HRESULT hr = S_OK;

				do {
					hr = Stop();
					hr = CloseSession();
				} while( false );

				return hr;
			}

			//  Create a new instance of the media session.
			HRESULT MediaFoundationPlayer::CreateSession()
			{
				HRESULT hr = S_OK;

				do {
					// Close the old session.
					hr = CloseSession();
					BREAK_ON_FAIL( hr );

					CI_ASSERT( mState == Closed );

					// Create the media session.
					hr = MFCreateMediaSession( NULL, &mMediaSessionPtr );
					BREAK_ON_FAIL( hr );

					CI_LOG_V( "Starting media session..." );

					// Start pulling events from the media session.
					hr = mMediaSessionPtr->BeginGetEvent( this, NULL );
					BREAK_ON_FAIL( hr );

					CI_LOG_V( "Media session started." );

					mState = Ready;
				} while( false );

				return hr;
			}

			//  Close the media session. 
			HRESULT MediaFoundationPlayer::CloseSession()
			{
				//  The IMFMediaSession::Close method is asynchronous, but the 
				//  MediaFoundationPlayer::CloseSession method waits on the MESessionClosed event.
				//  
				//  MESessionClosed is guaranteed to be the last event that the 
				//  media session fires.

				HRESULT hr = S_OK;

				SafeRelease( mVideoDisplayControlPtr );
				SafeRelease( mAudioStreamVolumePtr );

				// First close the media session.
				if( mMediaSessionPtr ) {
					CI_LOG_V( "Closing media session..." );

					mState = Closing;
					hr = mMediaSessionPtr->Close();

					// Wait for the close operation to complete
					if( SUCCEEDED( hr ) ) {
						DWORD dwWaitResult = WaitForSingleObject( mCloseEventHandle, 5000 );
						if( dwWaitResult == WAIT_FAILED ) {
							assert( FALSE );
						}
						else if( dwWaitResult == WAIT_TIMEOUT ) {
							assert( FALSE );
						}
						// Now there will be no more events from this session.
					}
				}

				// Complete shutdown operations.
				if( SUCCEEDED( hr ) ) {
					// Shut down the media source. (Synchronous operation, no events.)
					if( mMediaSourcePtr )
						mMediaSourcePtr->Shutdown();

					// Shut down the media session. (Synchronous operation, no events.)
					if( mMediaSessionPtr ) {
						mMediaSessionPtr->Shutdown();
						CI_LOG_V( "Media session closed." );
					}
				}

				SafeRelease( mMediaSourcePtr );
				SafeRelease( mMediaSessionPtr );

				mState = Closed;

				return hr;
			}

			HRESULT MediaFoundationPlayer::CreatePartialTopology( IMFPresentationDescriptor *pPD )
			{
				HRESULT hr = S_OK;

				do {
					ScopedComPtr<IMFTopology> pTopology;
					hr = CreatePlaybackTopology( mMediaSourcePtr, pPD, mHwnd, &pTopology, mPresenterPtr );
					BREAK_ON_FAIL( hr );

					hr = SetMediaInfo( pPD );
					BREAK_ON_FAIL( hr );

					// Set the topology on the media session.
					hr = mMediaSessionPtr->SetTopology( 0, pTopology );
					BREAK_ON_FAIL( hr );

					// If SetTopology succeeds, the media session will queue an MESessionTopologySet event.

				} while( false );

				return hr;
			}

			HRESULT MediaFoundationPlayer::SetMediaInfo( IMFPresentationDescriptor *pPD )
			{
				mWidth = 0;
				mHeight = 0;

				assert( pPD != NULL );

				HRESULT hr = S_OK;

				DWORD count;
				pPD->GetStreamDescriptorCount( &count );

				for( DWORD i = 0; i < count; i++ ) {
					BOOL selected;

					ScopedComPtr<IMFStreamDescriptor> spStreamDesc;
					hr = pPD->GetStreamDescriptorByIndex( i, &selected, &spStreamDesc );
					BREAK_ON_FAIL( hr );

					if( selected ) {
						ScopedComPtr<IMFMediaTypeHandler> pHandler;
						hr = spStreamDesc->GetMediaTypeHandler( &pHandler );
						BREAK_ON_FAIL( hr );

						GUID guidMajorType = GUID_NULL;
						hr = pHandler->GetMajorType( &guidMajorType );
						BREAK_ON_FAIL( hr );

						if( MFMediaType_Video == guidMajorType ) {
							// Obtain width and height of the video.
							ScopedComPtr<IMFMediaType> pMediaType;
							hr = pHandler->GetCurrentMediaType( &pMediaType );
							BREAK_ON_FAIL( hr );

							hr = MFGetAttributeSize( pMediaType, MF_MT_FRAME_SIZE, &mWidth, &mHeight );
							BREAK_ON_FAIL( hr );
						}
					}
				}

				return hr;
			}

			HRESULT MediaFoundationPlayer::Play()
			{
				assert( mMediaSessionPtr != NULL );

				PROPVARIANT varStart;
				PropVariantInit( &varStart );

				HRESULT hr = mMediaSessionPtr->Start( &GUID_NULL, &varStart );
				if( SUCCEEDED( hr ) ) {
					// Note: Start is an asynchronous operation. However, we
					// can treat our state as being already started. If Start
					// fails later, we'll get an MESessionStarted event with
					// an error code, and we will update our state then.
					mState = Started;
				}

				PropVariantClear( &varStart );

				return hr;
			}

			HRESULT MediaFoundationPlayer::Pause()
			{
				if( mState != Started )
					return MF_E_INVALIDREQUEST;

				if( mMediaSessionPtr == NULL || mMediaSourcePtr == NULL )
					return E_UNEXPECTED;

				HRESULT hr = mMediaSessionPtr->Pause();
				if( SUCCEEDED( hr ) )
					mState = Paused;

				return hr;
			}

			HRESULT MediaFoundationPlayer::Stop()
			{
				if( mState != Started && mState != Paused )
					return MF_E_INVALIDREQUEST;

				if( mMediaSessionPtr == NULL || mMediaSourcePtr == NULL )
					return E_UNEXPECTED;

				HRESULT hr = mMediaSessionPtr->Stop();
				if( SUCCEEDED( hr ) )
					mState = Stopped;

				return hr;
			}

			HRESULT MediaFoundationPlayer::SkipToPosition( MFTIME seekTime )
			{
				HRESULT hr = S_OK;

				PROPVARIANT var;
				PropVariantInit( &var );

				// Get the rate control service.
				ScopedComPtr<IMFRateControl> pRateControl;
				hr = MFGetService( mMediaSessionPtr, MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS( &pRateControl ) );

				// Set the playback rate to zero without thinning.
				if( SUCCEEDED( hr ) ) {
					hr = pRateControl->SetRate( FALSE, 0.0F );
				}

				// Create the Media Session start position.
				if( seekTime == PRESENTATION_CURRENT_POSITION ) {
					var.vt = VT_EMPTY;
				}
				else {
					var.vt = VT_I8;
					var.hVal.QuadPart = seekTime;
				}

				// Start the Media Session.
				if( SUCCEEDED( hr ) ) {
					hr = mMediaSessionPtr->Start( NULL, &var );
				}

				// Clean up.
				PropVariantClear( &var );

				return hr;
			}

			HRESULT MediaFoundationPlayer::HandleEvent( UINT_PTR pEventPtr )
			{
				HRESULT hr = S_OK;

				do {
					ScopedComPtr<IMFMediaEvent> pEvent( (IMFMediaEvent*) pEventPtr );
					BREAK_ON_NULL( pEvent, E_POINTER );

					// Get the event type.
					MediaEventType eventType = MEUnknown;
					hr = pEvent->GetType( &eventType );
					BREAK_ON_FAIL( hr );

					// Get the event status. If the operation that triggered the event 
					// did not succeed, the status is a failure code.
					HRESULT hrStatus = S_OK;
					hr = pEvent->GetStatus( &hrStatus );
					BREAK_ON_FAIL( hr );

					// Check if the async operation succeeded.
					if( SUCCEEDED( hr ) && FAILED( hrStatus ) ) {
						hr = hrStatus;
					}
					//BREAK_ON_FAIL( hr );

					switch( eventType ) {
					case MESessionTopologySet:
						if( FAILED( hr ) ) {
							CI_LOG_V( "MESessionTopologySet failed: " << hrStatus );
							hr = Close();
						}
						else
							hr = HandleSessionTopologySetEvent( pEvent );
						break;
					case MESessionTopologyStatus:
						if( FAILED( hr ) ) {
							CI_LOG_V( "MESessionTopologyStatus failed: " << hrStatus );
						}
						else
							hr = HandleSessionTopologyStatusEvent( pEvent );
						break;
					case MEEndOfPresentation:
						if( FAILED( hr ) )
							CI_LOG_V( "MEEndOfPresentation failed: " << hrStatus );
						else
							hr = HandleEndOfPresentationEvent( pEvent );
						break;
					case MENewPresentation:
						if( FAILED( hr ) )
							CI_LOG_V( "MENewPresentation failed: " << hrStatus );
						else
							hr = HandleNewPresentationEvent( pEvent );
						break;
					case MESessionStarted:
						if( FAILED( hr ) )
							CI_LOG_V( "MESessionStarted failed: " << hrStatus );
						else
							mState = Started;
						break;
					case MESessionScrubSampleComplete:
						if( SUCCEEDED( hr ) )
							hr = HandleSessionEvent( pEvent, eventType );
						break;
					default:
						if( SUCCEEDED( hr ) )
							hr = HandleSessionEvent( pEvent, eventType );
						break;
					}
				} while( false );

				return hr;
			}

			HRESULT MediaFoundationPlayer::HandleSessionTopologySetEvent( IMFMediaEvent *pEvent )
			{
#if _DEBUG
				ScopedComPtr<IMFTopology> pTopology;
				GetEventObject<IMFTopology>( pEvent, &pTopology );

				if( !pTopology )
					return E_POINTER;

				WORD nodeCount;
				pTopology->GetNodeCount( &nodeCount );

				CI_LOG_V( "Topo set and we have " << nodeCount << " nodes" );
#endif

				return S_OK;
			}

			HRESULT MediaFoundationPlayer::HandleSessionTopologyStatusEvent( IMFMediaEvent *pEvent )
			{
				UINT32 status;

				HRESULT hr = pEvent->GetUINT32( MF_EVENT_TOPOLOGY_STATUS, &status );
				if( SUCCEEDED( hr ) && ( status == MF_TOPOSTATUS_READY ) ) {
					SafeRelease( mVideoDisplayControlPtr );

					hr = Play();

					//if( !mIsPlaying )
					//	hr = Pause();
				}

				return hr;
			}

			HRESULT MediaFoundationPlayer::HandleEndOfPresentationEvent( IMFMediaEvent *pEvent )
			{
				HRESULT hr = S_OK;

				mMediaSessionPtr->Pause();
				mState = Paused;

				// Seek to the beginning.
				PROPVARIANT varStart;
				PropVariantInit( &varStart );
				varStart.vt = VT_I8;
				varStart.hVal.QuadPart = 0;

				hr = mMediaSessionPtr->Start( &GUID_NULL, &varStart );

				//if( !mLoop )
				//mMediaSessionPtr->Pause();
				//else
				mState = Started;

				PropVariantClear( &varStart );

				// The session puts itself into the stopped state automatically.

				return hr;
			}

			HRESULT MediaFoundationPlayer::HandleNewPresentationEvent( IMFMediaEvent *pEvent )
			{
				HRESULT hr = S_OK;

				do {
					// Get the presentation descriptor from the event.
					ScopedComPtr<IMFPresentationDescriptor> pPD;
					hr = GetEventObject( pEvent, &pPD );
					BREAK_ON_FAIL( hr );

					// Create a partial topology.
					hr = CreatePartialTopology( pPD );
					BREAK_ON_FAIL( hr );

					mState = OpenPending;
				} while( false );

				return hr;
			}

			HRESULT MediaFoundationPlayer::HandleSessionEvent( IMFMediaEvent *pEvent, MediaEventType eventType )
			{
				return S_OK;
			}

			HRESULT MediaFoundationPlayer::QueryInterface( REFIID riid, void** ppv )
			{
				AddRef();

				static const QITAB qit[] = {
					QITABENT( MediaFoundationPlayer, IMFAsyncCallback ),
					{ 0 }
				};
				return QISearch( this, qit, riid, ppv );
			}

			ULONG MediaFoundationPlayer::AddRef()
			{
				//CI_LOG_V( "MediaFoundationPlayer::AddRef():" << mRefCount + 1 );
				return InterlockedIncrement( &mRefCount );
			}

			ULONG MediaFoundationPlayer::Release()
			{
				assert( mRefCount > 0 );
				//CI_LOG_V( "MediaFoundationPlayer::Release():" << mRefCount - 1 );

				ULONG uCount = InterlockedDecrement( &mRefCount );
				if( uCount == 0 ) {
					mRefCount = DESTRUCTOR_REF_COUNT;
					delete this;
				}
				return uCount;
			}

			HRESULT MediaFoundationPlayer::Invoke( IMFAsyncResult *pResult )
			{
				HRESULT hr = S_OK;

				do {
					// Sometimes Invoke is called but mMediaSessionPtr is closed.
					BREAK_ON_NULL( mMediaSessionPtr, E_POINTER );

					// Get the event from the event queue.
					ScopedComPtr<IMFMediaEvent> pEvent;
					hr = mMediaSessionPtr->EndGetEvent( pResult, &pEvent );
					BREAK_ON_FAIL( hr );

					// Get the event type. 
					MediaEventType eventType = MEUnknown;
					hr = pEvent->GetType( &eventType );
					BREAK_ON_FAIL( hr );

					if( eventType == MESessionClosed ) {
						// The session was closed. 
						// The application is waiting on the mCloseEventHandle.
						SetEvent( mCloseEventHandle );
					}
					else {
						// For all other events, get the next event in the queue.
						hr = mMediaSessionPtr->BeginGetEvent( this, NULL );
						BREAK_ON_FAIL( hr );
					}

					// Check the application state. 

					// If a call to IMFMediaSession::Close is pending, it means the 
					// application is waiting on the mCloseEventHandle and
					// the application's message loop is blocked. 

					// Otherwise, post a private window message to the application. 

					if( mState != Closing ) {
						// Leave a reference count on the event.
						pEvent.get()->AddRef();

						PostMessage( mHwnd, WM_PLAYER_EVENT, (WPARAM) pEvent.get(), (LPARAM) eventType );
					}
				} while( false );

				return hr;
			}

			//  Create a media source from a URL.
			HRESULT MediaFoundationPlayer::CreateMediaSource( LPCWSTR pUrl, IMFMediaSource **ppSource )
			{
				HRESULT hr = S_OK;

				do {
					// Create the source resolver.
					ScopedComPtr<IMFSourceResolver> pSourceResolver;
					hr = MFCreateSourceResolver( &pSourceResolver );
					BREAK_ON_FAIL( hr );

					// Use the source resolver to create the media source.

					// Note: For simplicity this sample uses the synchronous method to create 
					// the media source. However, creating a media source can take a noticeable
					// amount of time, especially for a network source. For a more responsive 
					// UI, use the asynchronous BeginCreateObjectFromURL method.

					const DWORD dwFlags = MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE;
					MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
					ScopedComPtr<IUnknown> pSource;
					hr = pSourceResolver->CreateObjectFromURL( pUrl, dwFlags, NULL, &objectType, &pSource );
					BREAK_ON_FAIL( hr );

					// Get the IMFMediaSource interface from the media source.
					hr = pSource.QueryInterface( __uuidof( **ppSource ), (void**) ppSource );
				} while( false );

				return hr;
			}

			//  Create a playback topology from a media source.
			HRESULT MediaFoundationPlayer::CreatePlaybackTopology( IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, HWND hVideoWnd, IMFTopology **ppTopology, IMFVideoPresenter *pVideoPresenter )
			{
				HRESULT hr = S_OK;

				do {
					// Create a new topology.
					ScopedComPtr<IMFTopology> pTopology;
					hr = MFCreateTopology( &pTopology );
					BREAK_ON_FAIL( hr );

					// Get the number of streams in the media source.
					DWORD dwSourceStreams = 0;
					hr = pPD->GetStreamDescriptorCount( &dwSourceStreams );
					BREAK_ON_FAIL( hr );

					// For each stream, create the topology nodes and add them to the topology.
					for( DWORD i = 0; i < dwSourceStreams; i++ ) {
						hr = AddBranchToPartialTopology( pTopology, pSource, pPD, i, hVideoWnd, pVideoPresenter );
						BREAK_ON_FAIL( hr );
					}
					BREAK_ON_FAIL( hr );

					// Return the IMFTopology pointer to the caller.
					*ppTopology = pTopology;
					( *ppTopology )->AddRef();
				} while( false );

				return hr;
			}

			//  Create an activation object for a renderer, based on the stream media type.
			HRESULT MediaFoundationPlayer::CreateMediaSinkActivate( IMFStreamDescriptor *pSourceSD, HWND hVideoWindow, IMFActivate **ppActivate, IMFVideoPresenter *pVideoPresenter, IMFMediaSink **ppMediaSink )
			{
				HRESULT hr = S_OK;

				do {
					// Get the media type handler for the stream.
					ScopedComPtr<IMFMediaTypeHandler> pHandler;
					hr = pSourceSD->GetMediaTypeHandler( &pHandler );
					BREAK_ON_FAIL( hr );

					// Get the major media type.
					GUID guidMajorType;
					hr = pHandler->GetMajorType( &guidMajorType );
					BREAK_ON_FAIL( hr );

					// Create an IMFActivate object for the renderer, based on the media type.
					if( MFMediaType_Audio == guidMajorType ) {
						// Create the audio renderer.
						ScopedComPtr<IMFActivate> pActivate;
						hr = MFCreateAudioRendererActivate( &pActivate );
						BREAK_ON_FAIL( hr );

						// Return IMFActivate pointer to caller.
						*ppActivate = pActivate;
						( *ppActivate )->AddRef();
					}
					else if( MFMediaType_Video == guidMajorType ) {
						// Create the video renderer.
						ScopedComPtr<IMFMediaSink> pSink;
						hr = MFCreateVideoRenderer( __uuidof( IMFMediaSink ), (void**) &pSink );
						BREAK_ON_FAIL( hr );

						ScopedComPtr<IMFVideoRenderer> pVideoRenderer;
						hr = pSink.QueryInterface( __uuidof( IMFVideoRenderer ), (void**) &pVideoRenderer );
						BREAK_ON_FAIL( hr );

						hr = pVideoRenderer->InitializeRenderer( NULL, pVideoPresenter );
						BREAK_ON_FAIL( hr );

						// Return IMFMediaSink pointer to caller.
						*ppMediaSink = pSink;
						( *ppMediaSink )->AddRef();
					}
					else {
						// Unknown stream type.
						hr = E_FAIL;
						// Optionally, you could deselect this stream instead of failing.
					}
				} while( false );

				return hr;
			}

			// Add a source node to a topology.
			HRESULT MediaFoundationPlayer::AddSourceNode( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFStreamDescriptor *pSD, IMFTopologyNode **ppNode )
			{
				HRESULT hr = S_OK;

				do {
					// Create the node.
					ScopedComPtr<IMFTopologyNode> pNode;
					hr = MFCreateTopologyNode( MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode );
					BREAK_ON_FAIL( hr );

					// Set the attributes.
					hr = pNode->SetUnknown( MF_TOPONODE_SOURCE, pSource );
					BREAK_ON_FAIL( hr );

					hr = pNode->SetUnknown( MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD );
					BREAK_ON_FAIL( hr );

					hr = pNode->SetUnknown( MF_TOPONODE_STREAM_DESCRIPTOR, pSD );
					BREAK_ON_FAIL( hr );

					// Add the node to the topology.
					hr = pTopology->AddNode( pNode );
					BREAK_ON_FAIL( hr );

					// Return the pointer to the caller.
					*ppNode = pNode;
					( *ppNode )->AddRef();
				} while( false );

				return hr;
			}

			HRESULT MediaFoundationPlayer::AddOutputNode( IMFTopology *pTopology, IMFStreamSink *pStreamSink, IMFTopologyNode **ppNode )
			{
				HRESULT hr = S_OK;

				do {
					// Create the node.
					ScopedComPtr<IMFTopologyNode> pNode;
					hr = MFCreateTopologyNode( MF_TOPOLOGY_OUTPUT_NODE, &pNode );
					BREAK_ON_FAIL( hr );

					// Set the object pointer.
					hr = pNode->SetObject( pStreamSink );
					BREAK_ON_FAIL( hr );

					// Add the node to the topology.
					hr = pTopology->AddNode( pNode );
					BREAK_ON_FAIL( hr );

					hr = pNode->SetUINT32( MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, TRUE );
					BREAK_ON_FAIL( hr );

					// Return the pointer to the caller.
					*ppNode = pNode;
					( *ppNode )->AddRef();
				} while( false );

				return hr;
			}

			// Add an output node to a topology.
			HRESULT MediaFoundationPlayer::AddOutputNode( IMFTopology *pTopology, IMFActivate *pActivate, DWORD dwId, IMFTopologyNode **ppNode )
			{
				HRESULT hr = S_OK;

				do {
					ScopedComPtr<IMFTopologyNode> pNode;

					// Create the node.
					hr = MFCreateTopologyNode( MF_TOPOLOGY_OUTPUT_NODE, &pNode );
					BREAK_ON_FAIL( hr );

					// Set the object pointer.
					hr = pNode->SetObject( pActivate );
					BREAK_ON_FAIL( hr );

					// Set the stream sink ID attribute.
					hr = pNode->SetUINT32( MF_TOPONODE_STREAMID, dwId );
					BREAK_ON_FAIL( hr );

					hr = pNode->SetUINT32( MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE );
					BREAK_ON_FAIL( hr );

					// Add the node to the topology.
					hr = pTopology->AddNode( pNode );
					BREAK_ON_FAIL( hr );

					// Return the pointer to the caller.
					*ppNode = pNode;
					( *ppNode )->AddRef();
				} while( false );

				return hr;
			}

			//  Add a topology branch for one stream.
			//
			//  For each stream, this function does the following:
			//
			//    1. Creates a source node associated with the stream. 
			//    2. Creates an output node for the renderer. 
			//    3. Connects the two nodes.
			//
			//  The media session will add any decoders that are needed.

			HRESULT MediaFoundationPlayer::AddBranchToPartialTopology( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, DWORD iStream, HWND hVideoWnd, IMFVideoPresenter *pVideoPresenter )
			{
				HRESULT hr = S_OK;

				do {
					BREAK_ON_NULL( pPD, E_POINTER );

					BOOL fSelected = FALSE;

					ScopedComPtr<IMFStreamDescriptor> pSD;
					hr = pPD->GetStreamDescriptorByIndex( iStream, &fSelected, &pSD );
					BREAK_ON_FAIL( hr );

					if( fSelected ) {
						// Create the media sink activation object.
						ScopedComPtr<IMFActivate> pSinkActivate;
						ScopedComPtr<IMFMediaSink> pMediaSink;
						hr = CreateMediaSinkActivate( pSD, hVideoWnd, &pSinkActivate, pVideoPresenter, &pMediaSink );
						BREAK_ON_FAIL( hr );

						// Add a source node for this stream.
						ScopedComPtr<IMFTopologyNode> pSourceNode;
						hr = AddSourceNode( pTopology, pSource, pPD, pSD, &pSourceNode );
						BREAK_ON_FAIL( hr );

						// Create the output node for the renderer.
						ScopedComPtr<IMFTopologyNode> pOutputNode;
						if( pSinkActivate ) {
							hr = AddOutputNode( pTopology, pSinkActivate, 0, &pOutputNode );
						}
						else if( pMediaSink ) {
							IMFStreamSink* pStreamSink = NULL;
							DWORD streamCount;

							pMediaSink->GetStreamSinkCount( &streamCount );
							pMediaSink->GetStreamSinkByIndex( 0, &pStreamSink );

							hr = AddOutputNode( pTopology, pStreamSink, &pOutputNode );
						}
						BREAK_ON_FAIL( hr );

						// Connect the source node to the output node.
						hr = pSourceNode->ConnectOutput( 0, pOutputNode, 0 );
						BREAK_ON_FAIL( hr );
					}
				} while( false );

				return hr;
			}

		} // namespace video
	} // namespace msw
} // namespace cinder

#endif // CINDER_MSW
