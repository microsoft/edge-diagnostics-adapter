#pragma once

#include "stdafx.h"
#include <atomic>
#include <mutex>
#include <collection.h>
#include "PayloadContainer.h"

using namespace Platform;
using namespace Platform::Collections;

namespace NetworkProxyLibrary
{
    ref class PayloadQueue sealed
    {
    public:
        PayloadQueue(int queueSize)
            : _queueSize(queueSize)
            , _index(0)
            , _queueFull(false)
            , _queue(ref new Vector<PayloadContainer^>())
        {};

        void Add(PayloadContainer^ x);
        PayloadContainer^ Get(String^ messageId);

    private:
        int _queueSize;
        int _index;
        bool _queueFull;
        Vector<PayloadContainer^>^ _queue;
        std::mutex _queueMutex;
    };
}