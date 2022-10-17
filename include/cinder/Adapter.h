/*
 Copyright (c) 2022, The Cinder Project, All rights reserved.

 This code is intended for use with the Cinder C++ library: http://libcinder.org

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

#include <string>

namespace cinder {

typedef std::shared_ptr<class Adapter> AdapterRef;

class CI_API Adapter {
  public:
    Adapter()
        : mDeviceId( 0 )
        , mVendorId( 0 )
        , mName( "" )
        , mNameDirty( true )
    {
    }
    virtual ~Adapter() {}

    //! Returns the adapter's name or an empty string if unavailable.
    virtual std::string getName() const { return mName; }
    //! Returns the adapter's device id used by the operating system.
    unsigned int getDeviceId() const { return mDeviceId; }
    //! Returns the adapter's vendor id used by the operating system.
    unsigned int getVendorId() const { return mVendorId; }

    bool operator==( const Adapter &other ) const { return mDeviceId == other.mDeviceId && mVendorId == other.mVendorId; }
    bool operator!=( const Adapter &other ) const { return !( *this == other ); }

  protected:
    unsigned int        mDeviceId;
    unsigned int        mVendorId;
    mutable std::string mName;
    mutable bool        mNameDirty;
};

} // namespace cinder