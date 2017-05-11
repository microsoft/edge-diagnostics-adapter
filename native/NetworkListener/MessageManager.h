#pragma once

#include "stdafx.h"
#include <vector>
#include <mutex>
#include <collection.h>

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
        property HttpDiagnosticProviderRequestSentEventArgs^ RequestSentEventArgs { HttpDiagnosticProviderRequestSentEventArgs^ get() { return _requestSentEventArgs; }}
        property HttpDiagnosticProviderResponseReceivedEventArgs^ ResponseReceivedEventArgs { HttpDiagnosticProviderResponseReceivedEventArgs^ get() { return _responseReceivedEventArgs; }}
        property HttpDiagnosticProviderRequestResponseCompletedEventArgs^ RequestResponseCompletedEventArgs { HttpDiagnosticProviderRequestResponseCompletedEventArgs^ get() { return _requestResponseCompletedEventArgs; }}

    private:
        void Init(Guid id)
        {
            _messageId = id;
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
        void PostProcessMessage(JsonObject^ jsonObject, Guid messageId, MessageTypes messageType);
        event MessageProcessedEventHandler^ MessageProcessed;

    private:        
        ~MessageManager();
        unsigned int _processId;
        int _currentMessageCounter;     
        int _idCounters[3] = {1,1,1};
        Map<Guid, JsonObject^>^ _requestSentDictionary;
        Vector<Message^>^ _httpMessages;                        
        std::mutex _vectorMutex;

        String^ GetNextSequenceId(IdTypes counterType);
        void ProcessNextMessage(); 
        void ProcessRequestSentMessage(Message^ message);                      
    };

}


