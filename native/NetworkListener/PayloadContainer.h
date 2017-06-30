#pragma once

#include "stdafx.h"

using namespace Platform;

namespace NetworkProxyLibrary
{
    ref class PayloadContainer sealed
    {
    public:
        PayloadContainer(String^ responseId, String^ payload)
            : _responseId{ responseId }
            , _payload{ payload }
        {}

        property String^ Payload { String^ get() { return _payload; } };
        property String^ ResponseId { String^ get() { return _responseId; } };

    private:
        String^ _payload;
        String^ _responseId;
    };

}