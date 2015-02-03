/*
 Copyright (c) 2010, The Barbarian Group
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Vector.h"
#include "cinder/Surface.h"

#include <windows.h>
#undef min
#undef max

#define LOWORD(_dw)     ((WORD)(((DWORD_PTR)(_dw)) & 0xffff))
#define HIWORD(_dw)     ((WORD)((((DWORD_PTR)(_dw)) >> 16) & 0xffff))
#define LODWORD(_qw)    ((DWORD)(_qw))
#define HIDWORD(_qw)    ((DWORD)(((_qw) >> 32) & 0xffffffff))

#define BREAK_ON_FAIL(value)          if( FAILED( value ) ) break;
#define BREAK_ON_NULL(value, result)  if( value == NULL ) { hr = result; break; }
#define BREAK_IF_FALSE(test, result)  if( !(test) ) { hr = result; break; }

namespace cinder { namespace msw {

/** Converts a Win32 HBITMAP to a cinder::Surface8u
	\note Currently always copies the alpha channel **/
Surface8uRef convertHBitmap( HBITMAP hbitmap );

//! Converts a UTF-8 string into a wide string (wstring). Note that wstring is not a good cross-platform choice and this is here for interop with Windows APIs.
std::wstring toWideString( const std::string &utf8String );
//! Converts a wide string to a UTF-8 string. Note that wstring is not a good cross-platform choice and this is here for interop with Windows APIs.
std::string toUtf8String( const std::wstring &wideString );

//! Converts a Win32 POINTFX fixed point point to a cinder::vec2
#if !defined ( CINDER_WINRT )
inline vec2 toVec2( const ::POINTFX &p )
{ return vec2( ( (p.x.value << 16) | p.x.fract ) / 65535.0f, ( (p.y.value << 16) | p.y.fract ) / 65535.0f ); }
#endif

//! Warps a critical section.
class CriticalSection {
private:
	CRITICAL_SECTION mCriticalSection;
public:
	CriticalSection()
	{
		InitializeCriticalSection( &mCriticalSection );
	}

	~CriticalSection()
	{
		assert( mCriticalSection.LockCount == -1 );
		DeleteCriticalSection( &mCriticalSection );
	}

	void Lock()
	{
		EnterCriticalSection( &mCriticalSection );
	}

	void Unlock()
	{
		assert( mCriticalSection.LockCount < -1 );
		LeaveCriticalSection( &mCriticalSection );
	}
};

//! Provides automatic locking and unlocking of a critical section.
class ScopedCriticalSection {
private:
	CriticalSection *mCriticalSectionPtr;
public:
	ScopedCriticalSection( CriticalSection& section )
	{
		mCriticalSectionPtr = &section;
		mCriticalSectionPtr->Lock();
	}
	~ScopedCriticalSection()
	{
		assert( mCriticalSectionPtr != nullptr );
		mCriticalSectionPtr->Unlock();
	}
};

} } // namespace cinder::msw
