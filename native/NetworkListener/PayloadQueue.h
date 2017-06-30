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
            , _head(0)
            , _tail(0)
            , _queue(ref new Vector<PayloadContainer^>())
        {};

        void Add(PayloadContainer^ x);

    private:
        int _queueSize;
        int _head;
        int _tail;
        Vector<PayloadContainer^>^ _queue;
        std::mutex _queueMutex;
    };
}