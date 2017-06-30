#pragma once

#include "stdafx.h"
#include <vector>
#include <mutex>
#include <collection.h>
#include <atomic>
#include "Message.h"
#include "PayloadContainer.h"
#include "PayloadQueue.h"

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Data::Json;
using namespace Windows::Web::Http::Diagnostics;
using namespace Windows::Foundation::Collections;

namespace NetworkProxyLibrary
{
    ref class MessageManager;
    delegate void MessageProcessedEventHandler(MessageManager^sender, JsonObject^ message);

    ref class MessageManager sealed
    {
    public:
	    MessageManager(unsigned int processId);	
        void SendToProcess(Message^ message);
        
        JsonObject^ GenerateRequestWilBeSentMessage(HttpDiagnosticProviderRequestSentEventArgs ^data, String^ postPayload = nullptr);
        JsonObject^ GenerateResponseReceivedMessage(HttpDiagnosticProviderResponseReceivedEventArgs^ data);
        JsonObject^ GenerateDataReceivedMessage(JsonObject^ responseReceivedMessage, double contentLenght);
        JsonObject^ GenerateLoadingFinishedMessage(JsonObject^ dataReceivedMessage);
       
        event MessageProcessedEventHandler^ MessageProcessed;
        

    private: 
        static const int MessageProcessingRetries = 1;
        static const int MaxResponsePayloadsStored = 500;

        ~MessageManager();
        unsigned int _processId;
        int _currentMessageCounter;     
        int _idCounters[3] = {1,1,1};
        Map<Guid, JsonObject^>^ _requestSentDictionary;
        PayloadQueue^ _responsePayloadQueue;

        Vector<Message^>^ _retryQueue;
        std::mutex _dictionaryMutex;
        std::mutex _retryMutex;

        void PostProcessMessage(JsonObject^ jsonObject);
        void ProcessMessage(Message^ message);
        String^ GetNextSequenceId(IdTypes counterType); 
        void ProcessRequestSentMessage(Message^ message);
        void ProcessResponseReceivedMessage(Message^ message);        
        JsonObject^ GetRequestMessage(Guid id);
        void DeleteRequestMessage(Guid id);
        void AddMessageToQueueForRetry(Message^ message);
        void OnRequestInsertedToMap(Guid id);
    };

}


