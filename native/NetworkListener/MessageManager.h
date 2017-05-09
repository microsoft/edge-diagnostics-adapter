#pragma once

#include "stdafx.h"
#include <vector>
#include <collection.h>

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Data::Json;
using namespace Windows::Web::Http::Diagnostics;


enum IdTypes
{
    RequestId = 0,
    FrameId = 1,
    LoaderId = 2
 };

class MessageManager
{
public:
	MessageManager(unsigned int processId);
	~MessageManager();

    JsonObject^ GenerateRequestWilBeSentMessage(HttpDiagnosticProviderRequestSentEventArgs ^data, String^ postPayload = nullptr);
    JsonObject^ GenerateResponseReceivedMessage(HttpDiagnosticProviderResponseReceivedEventArgs^ data);

private:
    unsigned int _processId;
    int _currentMessageCounter;     
    int _idCounters[3] = {1,1,1};
    Map<Guid, JsonObject^>^ _requestSentDictionary;

    String^ GetNextSequenceId(IdTypes counterType);
};

