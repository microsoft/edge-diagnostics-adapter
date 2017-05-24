#pragma once

#include "stdafx.h"
#include <vector>
#include <mutex>
#include <collection.h>
#include <atomic>

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Data::Json;
using namespace Windows::Web::Http::Diagnostics;
using namespace Windows::Foundation::Collections;

namespace NetworkProxyLibrary
{

    enum IdTypes
    {
        RequestId = 0,
        FrameId = 1,
        LoaderId = 2
     };

    public enum class MessageTypes : int
    {
        RequestSent = 0,
        ResponseReceived = 1,
        RequestResponseCompleted = 2
    };

    ref class Message sealed
    {

    public: 
        Message(HttpDiagnosticProviderRequestSentEventArgs^ requestSentEventArgs)
        {    
            Init(requestSentEventArgs->ActivityId);
            _messageType = MessageTypes::RequestSent;
            _requestSentEventArgs = requestSentEventArgs;
        }
        Message(HttpDiagnosticProviderResponseReceivedEventArgs^ responseReceivedEventArgs)
        {          
            Init(responseReceivedEventArgs->ActivityId);
            _messageType = MessageTypes::ResponseReceived;
            _responseReceivedEventArgs = responseReceivedEventArgs;
        }
        Message(HttpDiagnosticProviderRequestResponseCompletedEventArgs^ requestResponseCompletedEventArgs) 
        {
            Init(requestResponseCompletedEventArgs->ActivityId);
            _messageType = MessageTypes::RequestResponseCompleted;
            _requestResponseCompletedEventArgs = requestResponseCompletedEventArgs;
        }        
        property MessageTypes MessageType { MessageTypes get() { return _messageType; } }
        property Guid MessageId { Guid get() { return _messageId; } }
        property int ProcessingRetries;
        property long long TimeStamp;
        property HttpDiagnosticProviderRequestSentEventArgs^ RequestSentEventArgs { HttpDiagnosticProviderRequestSentEventArgs^ get() { return _requestSentEventArgs; }}
        property HttpDiagnosticProviderResponseReceivedEventArgs^ ResponseReceivedEventArgs { HttpDiagnosticProviderResponseReceivedEventArgs^ get() { return _responseReceivedEventArgs; }}
        property HttpDiagnosticProviderRequestResponseCompletedEventArgs^ RequestResponseCompletedEventArgs { HttpDiagnosticProviderRequestResponseCompletedEventArgs^ get() { return _requestResponseCompletedEventArgs; }}

    private:
        void Init(Guid id)
        {
            _messageId = id;
            ProcessingRetries = 0; 
            Windows::Globalization::Calendar^ calendar = ref new Windows::Globalization::Calendar();
            calendar->SetToNow();
            auto dateTime = calendar->GetDateTime();            
            TimeStamp = dateTime.UniversalTime;
        }
        MessageTypes _messageType;
        Guid _messageId;
        HttpDiagnosticProviderRequestSentEventArgs^ _requestSentEventArgs;
        HttpDiagnosticProviderResponseReceivedEventArgs^ _responseReceivedEventArgs;
        HttpDiagnosticProviderRequestResponseCompletedEventArgs^ _requestResponseCompletedEventArgs;
    };

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
        JsonObject^ GenerateLoadingFinishedMessage(HttpDiagnosticProviderRequestResponseCompletedEventArgs^ data, JsonObject^ requestMessage);        
       
        event MessageProcessedEventHandler^ MessageProcessed;
        

    private:        
        ~MessageManager();
        unsigned int _processId;
        int _currentMessageCounter;     
        int _idCounters[3] = {1,1,1};
        Map<Guid, JsonObject^>^ _requestSentDictionary;
        Vector<Message^>^ _retryQueue;
        std::mutex _dictionaryMutex;
        std::mutex _retryMutex;

        void PostProcessMessage(JsonObject^ jsonObject);
        void ProcessMessage(Message^ message);
        String^ GetNextSequenceId(IdTypes counterType); 
        void ProcessRequestSentMessage(Message^ message);
        void ProcessResponseReceivedMessage(Message^ message);
        void ProcessRequestResponseCompletedMessage(Message^ message);
        JsonObject^ GetRequestMessage(Guid id);
        void AddMessageToQueueForRetry(Message^ message);
        void OnMapChanged(Windows::Foundation::Collections::IObservableMap<Platform::Guid, Windows::Data::Json::JsonObject ^> ^sender, Windows::Foundation::Collections::IMapChangedEventArgs<Platform::Guid> ^event);
    };

}


