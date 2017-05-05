#pragma once

#include <vector>

using namespace Platform;
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

    JsonObject^ GenerateRequestWilBeSendMessage(HttpDiagnosticProviderRequestSentEventArgs ^data);

private:
    unsigned int _processId;
    int _currentMessageCounter;     
    int _idCounters[3] = {1,1,1};

    String^ GetNextSequenceId(IdTypes counterType);
};

