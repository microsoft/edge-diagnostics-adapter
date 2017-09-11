#pragma once

#include "stdafx.h"

using namespace Platform;
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

}
