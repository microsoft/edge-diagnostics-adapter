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
    int _head = _queueFull ? _index : 0;

    for (int i = _head; i < _index; i++)
    {
        auto message = _queue->GetAt(i);
        if (message->ResponseId == messageId)
        {
            return message;
        }
    }
}
