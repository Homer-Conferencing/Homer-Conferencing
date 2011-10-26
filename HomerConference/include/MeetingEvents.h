/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This source is published in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License version 2
 * along with this program. Otherwise, you can write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 * Alternatively, you find an online version of the license text under
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *****************************************************************************/

/*
 * Purpose: Event management
 * Author:  Thomas Volkert
 * Since:   2008-12-09
 */

#ifndef _CONFERENCE_MEETING_EVENTS_
#define _CONFERENCE_MEETING_EVENTS_

#include <Header_SofiaSip.h>
#include <HBMutex.h>

#include <string>

using namespace std;
using namespace Homer::Base;

namespace Homer { namespace Conference {

#define INT_START_NAT_DETECTION                   1 // have to start NAT detection via STUN
#define GENERAL_ERROR                            10 // an error was received
#define MESSAGE                                 100 // a new instant message
#define MESSAGE_ACCEPT                          110 // message was accepted
#define MESSAGE_ACCEPT_DELAYED                  111 // message was accepted for delayed delivery (server buffers message until remote side logs in again)
#define MESSAGE_UNAVAILABLE                     120 // message response: service unavailable (e.g. no running sip client on remote side)
#define CALL                                    200 // a new call
#define CALL_RINGING                            210 // inform about local ringing caused by incoming call
#define CALL_CANCEL                             220 // abort the ringing
#define CALL_ACCEPT                             230 // accept message for incoming call
#define CALL_DENY                               240 // deny message for incoming call
#define CALL_HANGUP                             250 // hang up (call was successful before)
#define CALL_TERMINATION                        260 // termination (call was successful before)
#define CALL_MEDIA_UPDATE                       270 // new video/audio connection information for call
#define CALL_UNAVAILABLE                        280 // call response: service unavailable (e.g. no running sip client on remote side)
#define REGISTRATION                            300 // registration at server succeeded
#define REGISTRATION_FAILED                     310 // registration at server failed
#define PUBLICATION                             400 // presence publication succeeded
#define PUBLICATION_FAILED                      410 // presence publication failed
#define OPTIONS                                 500 // receive options from SIP server
#define OPTIONS_ACCEPT                          510 // options from SIP server available
#define OPTIONS_UNAVAILABLE                     520 // options from SIP server unavailable (server unavailable!)

///////////////////////////////////////////////////////////////////////////////

class GeneralEvent
{
public:
    GeneralEvent(int pType):
        mType(pType)
    {

    }

    virtual ~GeneralEvent()
    {

    }

    int getType()
    {
        return mType;
    }

    static string getNameFromType(int pType)
    {
        switch(pType)
        {
            case INT_START_NAT_DETECTION:
                return "Internal signaling for NAT detection in SIP thread context";
            case OPTIONS:
                return "Options of SIP server";
            case OPTIONS_ACCEPT:
                return "Options from SIP server delivered";
            case OPTIONS_UNAVAILABLE:
                return "Options from SIP server unavailable";
            case GENERAL_ERROR:
                return "General Error";
            case MESSAGE:
                return "Message";
            case MESSAGE_ACCEPT:
                return "Message accept";
            case MESSAGE_ACCEPT_DELAYED:
                return "Message accept delayed";
            case MESSAGE_UNAVAILABLE:
                return "Message unavailable";
            case CALL:
                return "Call request";
            case CALL_RINGING:
                return "Call ringing";
            case CALL_CANCEL:
                return "Call cancel";
            case CALL_ACCEPT:
                return "Call accept";
            case CALL_DENY:
                return "Call deny";
            case CALL_HANGUP:
                return "Call hangup";
            case CALL_TERMINATION:
                return "Call termination";
            case CALL_MEDIA_UPDATE:
                return "Call media update";
            case CALL_UNAVAILABLE:
                return "Call unavailable";
            case REGISTRATION:
                return "Registration succeeded";
            case REGISTRATION_FAILED:
                return "Registration failed";
            case PUBLICATION:
                return "Publication succeeded";
            case PUBLICATION_FAILED:
                return "Publication failed";
            default:
                "User defined event";
        }
        return "";
    }

public:
    string Receiver;
    string Sender;
    string SenderName;
    string SenderComment;
    string SenderApplication;
    bool   IsIncomingEvent; // incoming in meanings of "received from outside"

private:
    friend class SIP;
    friend class Meeting;
    nua_handle_t **HandlePtr;

private:
    int mType;
};

///////////////////////////////////////////////////////////////////////////////

template <typename DerivedClass, int pType>
class TEvent:
    public GeneralEvent
{
public:
    TEvent():GeneralEvent(pType) { }
    virtual ~TEvent() { }

    static int type() { return pType; }
};

//////////////////////////////////////////////////////////////////////////////

class InternalNatDetectionEvent:
    public TEvent<InternalNatDetectionEvent, INT_START_NAT_DETECTION>
{
public:
    bool Failed;
    string FailureReason;
};

class OptionsEvent:
    public TEvent<OptionsEvent, OPTIONS>
{
public:
};

class OptionsAcceptEvent:
    public TEvent<OptionsAcceptEvent, OPTIONS_ACCEPT>
{
public:
};

class OptionsUnavailableEvent:
    public TEvent<OptionsUnavailableEvent, OPTIONS_UNAVAILABLE>
{
public:
};

class ErrorEvent:
    public TEvent<ErrorEvent, GENERAL_ERROR>
{
public:
    int StatusCode;
    string Description;
};

class MessageEvent:
    public TEvent<MessageEvent, MESSAGE>
{
public:
    string Text;
};

class MessageAcceptEvent:
    public TEvent<MessageAcceptEvent, MESSAGE_ACCEPT>
{
};

class MessageAcceptDelayedEvent:
    public TEvent<MessageAcceptDelayedEvent, MESSAGE_ACCEPT_DELAYED>
{
};

class MessageUnavailableEvent:
    public TEvent<MessageUnavailableEvent, MESSAGE_UNAVAILABLE>
{
};

class CallEvent:
    public TEvent<CallEvent, CALL>
{
public:
    bool AutoAnswering;
};

class CallRingingEvent:
    public TEvent<CallRingingEvent, CALL_RINGING>
{
};

class CallCancelEvent:
    public TEvent<CallCancelEvent, CALL_CANCEL>
{
};

class CallAcceptEvent:
    public TEvent<CallAcceptEvent, CALL_ACCEPT>
{
};

class CallDenyEvent:
    public TEvent<CallDenyEvent, CALL_DENY>
{
};

class CallHangUpEvent:
    public TEvent<CallHangUpEvent, CALL_HANGUP>
{
};

class CallTerminationEvent:
    public TEvent<CallTerminationEvent, CALL_TERMINATION>
{
};

class CallMediaUpdateEvent:
    public TEvent<CallMediaUpdateEvent, CALL_MEDIA_UPDATE>
{
public:
    string RemoteAudioAddress;
    unsigned int RemoteAudioPort;
    string RemoteAudioCodec;
    string RemoteVideoAddress;
    unsigned int RemoteVideoPort;
    string RemoteVideoCodec;
};

class CallUnavailableEvent:
    public TEvent<CallUnavailableEvent, CALL_UNAVAILABLE>
{
};

class RegistrationEvent:
    public TEvent<RegistrationEvent, REGISTRATION>
{
};

class RegistrationFailedEvent:
    public TEvent<RegistrationFailedEvent, REGISTRATION_FAILED>
{
};

class PublicationEvent:
    public TEvent<PublicationEvent, PUBLICATION>
{
};

class PublicationFailedEvent:
    public TEvent<PublicationFailedEvent, PUBLICATION_FAILED>
{
};

///////////////////////////////////////////////////////////////////////////////

class MeetingObserver
{
public:
    MeetingObserver() { }

    virtual ~MeetingObserver() { }

    /* handling function for Meeting events */
    virtual void handleMeetingEvent(GeneralEvent *pEvent) = 0;
};

//////////////////////////////////////////////////////////////////////////////

class MeetingObservable
{
public:
    MeetingObservable();

    virtual ~MeetingObservable();

    virtual void notifyObservers(GeneralEvent *pEvent);
    virtual void AddObserver(MeetingObserver *pObserver);
    virtual void DeleteObserver(MeetingObserver *pObserver);

private:
    MeetingObserver     *mMeetingObserver;
};

//////////////////////////////////////////////////////////////////////////////

#define QUEUE_LENGTH    64
class EventManager
{
public:
    EventManager();

    virtual ~EventManager();

    bool Fire(GeneralEvent* pEvent); // false if queue is full
    GeneralEvent* Scan();

private:
    Mutex               mMutex;
    GeneralEvent        *mEvents[QUEUE_LENGTH];
    int                 mRemainingEvents;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
