#include "stdafx.h"
#include "PayloadQueue.h"
#include "PayloadContainer.h"

using namespace NetworkProxyLibrary;

void PayloadQueue::Add(PayloadContainer^ payload)
{
    _queueMutex.lock();
    if (_index >= _queueSize)
    {
        _queueFull = true;
        _index = 0;
    }
    _queue->InsertAt(_index, payload);
    _index++;
    _queueMutex.unlock();
}

PayloadContainer^ PayloadQueue::Get(String^ messageId)
{
    int lastSlotFull = _queueFull ? _queueSize : _index;
    PayloadContainer^ result = nullptr;

    for (int i = 0; i < lastSlotFull; i++)
    {
        auto message = _queue->GetAt(i);
        if (message->ResponseId == messageId)
        {
            result = message;
            break;
        }
    }

    return result;
}
