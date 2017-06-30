#include "stdafx.h"
#include "PayloadQueue.h"
#include "PayloadContainer.h"

using namespace NetworkProxyLibrary;

void PayloadQueue::Add(PayloadContainer^ payload)
{
    _queueMutex.lock();
    _queue->InsertAt(_head, payload);
    _head++;
    _queueMutex.unlock();
}
