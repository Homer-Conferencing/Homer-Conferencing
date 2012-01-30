/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of conference functions
 * Author:  Thomas Volkert
 * Since:   2008-11-25
 */

#include <Meeting.h>
#include <Logger.h>
#include <SDP.h>
#include <SIP.h>

#include <string>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <HBSocket.h>

namespace Homer { namespace Conference {

using namespace std;
using namespace Homer::Base;

Meeting sMeeting;

///////////////////////////////////////////////////////////////////////////////

struct ParticipantDescriptor
{
    std::string    User;
    std::string    Host;
    std::string    Port;
    std::string    Sdp;
    std::string    OwnIp; //necessary for NAT traversal: store the outmost NAT's IP, directed towards this participant
    unsigned int   OwnPort; //necessary for NAT traversal: store the outmost NAT's PORT, directed towards this participant
    std::string    RemoteVideoHost;
    unsigned int   RemoteVideoPort;
    std::string    RemoteVideoCodec;
    std::string    RemoteAudioHost;
    unsigned int   RemoteAudioPort;
    std::string    RemoteAudioCodec;
    nua_handle_t   *SipNuaHandleForCalls;
    nua_handle_t   *SipNuaHandleForMsgs;
    nua_handle_t   *SipNuaHandleForOptions;
    Socket         *VSocket;
    Socket         *ASocket;
    int            CallState;
};

///////////////////////////////////////////////////////////////////////////////
/////////////////////////// Meeting ///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Meeting::Meeting() :
    SDP(),
    SIP(),
    MeetingObservable()
{
    mMeetingInitiated = false;
}

Meeting::~Meeting()
{
}

Meeting& Meeting::GetInstance()
{
    return sMeeting;
}

///////////////////////////////////////////////////////////////////////////////

void Meeting::Init(string pSipHostAdr, LocalAddressesList pLocalAddresses, string pBroadcastAdr, int pSipStartPort, int pStunStartPort, int pVideoAudioStartPort)
{
    SIP::Init(pSipStartPort, pStunStartPort);

    // start value for port auto probing (default is 5000)
    mVideoAudioStartPort = pVideoAudioStartPort;

    //SetHostAdr("0.0.0.0");//pSipHostAdr);
    SetHostAdr(pSipHostAdr);
    mBroadcastAdr = pBroadcastAdr;
    mLocalAddresses = pLocalAddresses;

    ParticipantDescriptor tParticipantDescriptor;

    LOG(LOG_VERBOSE, "Setting up session manager..");

    //###################################################################
    //### start SIP main loop
    //###################################################################
    if (!StartThread())
    	LOG(LOG_ERROR, "Start of SIP main loop failed");

    // wait until SIP main loop is activated
    while(!mSipStackOnline)
    {
    	Thread::Suspend(100000);
    }
    LOG(LOG_VERBOSE, "..SIP listener loop");

    //###################################################################
    //### initiate internal database
    //###################################################################
    mOwnName = "user";
    mOwnMail = "user@host.com";

    tParticipantDescriptor.User = getUser();
    tParticipantDescriptor.Host = pSipHostAdr;
    tParticipantDescriptor.Port = toString(mSipHostPort);
    tParticipantDescriptor.SipNuaHandleForCalls = NULL;
    tParticipantDescriptor.SipNuaHandleForMsgs = NULL;
    tParticipantDescriptor.CallState = CALLSTATE_STANDBY;
    tParticipantDescriptor.VSocket = NULL;
    tParticipantDescriptor.ASocket = NULL;

    mParticipants.push_back(tParticipantDescriptor);
    mMeetingInitiated = true;
}

void Meeting::Stop()
{
    if (!mMeetingInitiated)
        LOG(LOG_ERROR, "Session manager wasn't initiated yet");

    //trigger ending of SIP main loop
    StopSipMainLoop();

    //wait for SIP main loop
    if (!StopThread(2000))
        LOG(LOG_INFO, "SIP-Listener thread terminated, pending packets skipped");
}

void Meeting::Deinit()
{
    CloseAllSessions();
    SIP_stun::Deinit();
    SIP::Deinit();
}

string Meeting::CallStateAsString(int pCallState)
{
    string tResult = "undefined";

    switch(pCallState)
    {
        case CALLSTATE_INVALID:
                tResult = "invalid";
                break;
        case CALLSTATE_STANDBY:
                tResult = "standby";
                break;
        case CALLSTATE_RINGING:
                tResult = "ringing";
                break;
        case CALLSTATE_RUNNING:
                tResult = "running";
                break;
    }
    return tResult;
}

string Meeting::getUser()
{
    string tResult = "user";
    char *tUser;

	tUser = getenv("USER");
    if (tUser != NULL)
    	tResult = string(tUser);
    else
    {
    	tUser = getenv("USERNAME");
        if (tUser != NULL)
        	tResult = string(tUser);
    }

    return tResult;
}

void Meeting::SetHostAdr(string pHost)
{
    LOG(LOG_VERBOSE, "Setting SIP host address to %s", pHost.c_str());
    mSipHostAdr = pHost;
}

string Meeting::GetHostAdr()
{
    return mSipHostAdr;
}

int Meeting::GetHostPort()
{
    return mSipHostPort;
}

void Meeting::SetLocalUserName(string pName)
{
    string tFilteredString = "";
    LOG(LOG_VERBOSE, "Called to set local name to %s", pName.c_str());
    for (int i = 0; i < (int)pName.length(); i++)
    {
        switch(pName[i])
        {
            case 'A'...'Z':
            case 'a'...'z':
            case ' ':
            case '_':
            case 'Ä':
            case 'Ö':
            case 'Ü':
            case 'ä':
            case 'ö':
            case 'ü':
                tFilteredString += pName[i];
                break;
            default:
                LOG(LOG_ERROR, "Filtered char in new meeting name: %c(%d)", pName[i], 0xFF & pName[i]);
                break;
        }
    }
    LOG(LOG_VERBOSE, "Setting local name to %s", tFilteredString.c_str());
    mOwnName = tFilteredString;
}

void Meeting::SetLocalUserMailAdr(string pMailAdr)
{
    LOG(LOG_VERBOSE, "Setting local mail address to %s", pMailAdr.c_str());
    mOwnMail = pMailAdr;
}

string Meeting::GetLocalUserName()
{
    return mOwnName;
}

string Meeting::GetLocalUserMailAdr()
{
    return mOwnMail;
}

string Meeting::GetLocalConferenceId()
{
    string tResult = "";

    tResult = SipCreateId(getUser(), GetHostAdr(), toString(GetHostPort()));

    LOG(LOG_VERBOSE, "Determined local conference ID with \"%s\"", tResult.c_str());

    return tResult;
}

string Meeting::GetServerConferenceId()
{
    string tResult = "";

    tResult = SipCreateId(mSipRegisterUsername, mSipRegisterServer, "");

    LOG(LOG_VERBOSE, "Determined server conference ID with \"%s\"", tResult.c_str());

    return tResult;
}

bool Meeting::OpenParticipantSession(string pUser, string pHost, string pPort, int pInitState)
{
    bool        tFound = false;
    ParticipantDescriptor tParticipantDescriptor;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is this user already involved in the conference?
    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        LOG(LOG_VERBOSE, "CompareForOpen: \"%s\" with \"%s\" and state: %d", (pUser + "@" + pHost + ":" + pPort).c_str(), (tIt->User + "@" + tIt->Host + ":" + tIt->Port).c_str(), tIt->CallState);
        if (IsThisParticipant(pUser, pHost, pPort, tIt->User, tIt->Host, tIt->Port))
        {
            LOG(LOG_VERBOSE, "...found");
            tFound = true;
        }
    }

    if (!tFound)
    {
        LOG(LOG_VERBOSE, "Open session to: %s", SipCreateId(pUser, pHost, pPort).c_str());

        tParticipantDescriptor.User = pUser;
        tParticipantDescriptor.Host = pHost;
        tParticipantDescriptor.Port = pPort;
        tParticipantDescriptor.OwnIp = GetHostAdr();
        tParticipantDescriptor.OwnPort = (unsigned int)GetHostPort();
        tParticipantDescriptor.RemoteVideoHost = "0.0.0.0";
        tParticipantDescriptor.RemoteVideoPort = 0;
        tParticipantDescriptor.RemoteVideoCodec = "";
        tParticipantDescriptor.RemoteAudioHost = "0.0.0.0";
        tParticipantDescriptor.RemoteAudioPort = 0;
        tParticipantDescriptor.RemoteAudioCodec = "";
        tParticipantDescriptor.CallState = CALLSTATE_STANDBY;
        tParticipantDescriptor.VSocket = new Socket(mVideoAudioStartPort, GetSocketTypeFromMediaTransportType(GetVideoTransportType()), 2);
        if (tParticipantDescriptor.VSocket == NULL)
            LOG(LOG_ERROR, "Invalid video socket");
        tParticipantDescriptor.ASocket = new Socket(mVideoAudioStartPort, GetSocketTypeFromMediaTransportType(GetAudioTransportType()), 2);
        if (tParticipantDescriptor.ASocket == NULL)
            LOG(LOG_ERROR, "Invalid audio socket");

        mParticipants.push_back(tParticipantDescriptor);

        if (pInitState == CALLSTATE_RINGING)
            SendCall(SipCreateId(pUser, pHost, pPort));
    }else
        LOG(LOG_VERBOSE, "Participant session already exists, open request ignored");

    // unlock
    mParticipantsMutex.unlock();

    return !tFound;
}

bool Meeting::CloseParticipantSession(string pParticipant)
{
    bool        tFound = false;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
        {
            // hint: the media sources are deleted within video/audio-widget

            // delete video/audio sockets
            delete (*tIt).VSocket;
            delete (*tIt).ASocket;

            // remove element from participants list
            tIt = mParticipants.erase(tIt);

            tFound = true;
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    // was the session found?
    if (!tFound)
    {
        LOG(LOG_ERROR, "Data inconsistency in session management detected");
        return false;
    }else
    {
        LOG(LOG_VERBOSE, "Closed session with: %s", pParticipant.c_str());
        return true;
    }
}

int Meeting::CountParticipantSessions()
{
    int tResult = 0;

    // lock
    mParticipantsMutex.lock();

    tResult = mParticipants.size();

    // unlock
    mParticipantsMutex.unlock();

    return tResult;
}

void Meeting::CloseAllSessions()
{
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    for (tIt = ++mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        // there were no audio/video-sources for the local user allocated
        if ((tIt->Host != GetHostAdr()) || (tIt->Port != toString(GetHostPort())))
        {
            LOG(LOG_VERBOSE, "Close session with: %s", SipCreateId(tIt->User, tIt->Host, tIt->Port).c_str());

            // hint: the media sources are deleted within video/audio-widget
        }else
            LOG(LOG_VERBOSE, "Close loopback session with: %s", SipCreateId(tIt->User, tIt->Host, tIt->Port).c_str());

        // delete video/audio sockets
        delete (*tIt).VSocket;
        delete (*tIt).ASocket;

        // remove element from participants list
        tIt = mParticipants.erase(tIt);
        LOG(LOG_VERBOSE, "...closed");
    }

    // unlock
    mParticipantsMutex.unlock();
}

bool Meeting::SendBroadcastMessage(string pMessage)
{
    bool tResult = false;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "Sending broadcast message");

    // lock
    mParticipantsMutex.lock();

    if (mParticipants.size() > 1)
    {
        for (tIt = ++mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            MessageEvent *tMEvent = new MessageEvent();
            tMEvent->Sender = "sip:" + GetLocalConferenceId();
            tMEvent->SenderName = GetLocalUserName();
            tMEvent->SenderComment = "Broadcast";
            tMEvent->Receiver = "sip:" + SipCreateId(tIt->User, tIt->Host, tIt->Port);
            tMEvent->HandlePtr = &tIt->SipNuaHandleForMsgs;
            tMEvent->Text = pMessage;
            OutgoingEvents.Fire((GeneralEvent*) tMEvent);
        }

        tResult = true;
    }else
        tResult = false;

    // unlock
    mParticipantsMutex.unlock();

    return tResult;
}

bool Meeting::SendMessage(string pParticipant, string pMessage)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;

    // should we use broadcast scheme instead of unciast one?
    if (pParticipant == mBroadcastAdr)
        return SendBroadcastMessage(pMessage);

    LOG(LOG_VERBOSE, "Sending message to: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for SendMessage()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
            {
                LOG(LOG_VERBOSE, "...found");
                tFound = true;
                tHandlePtr = &tIt->SipNuaHandleForMsgs;
                break;
            }
        }

        if (tFound)
        {
            MessageEvent *tMEvent = new MessageEvent();
            // is participant user of the registered SIP server then replace sender by our official server login
            if ((pParticipant.find(mSipRegisterServer) != string::npos) && (GetServerRegistrationState()))
                tMEvent->Sender = "sip:" + mSipRegisterUsername + "@" + mSipRegisterServer;
            else
                tMEvent->Sender = "sip:" + GetLocalConferenceId();
            tMEvent->SenderName = GetLocalUserName();
            tMEvent->SenderComment = "";
            tMEvent->Receiver = "sip:" + pParticipant;
            tMEvent->HandlePtr = tHandlePtr;
            tMEvent->Text = pMessage;
            OutgoingEvents.Fire((GeneralEvent*) tMEvent);
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SendCall(string pParticipant)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;
//TODO: wie bei nachrichten hier broadcasts unterstützen
    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for SendCall()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if ((IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port)) && (tIt->CallState == CALLSTATE_STANDBY))
            {
                LOG(LOG_VERBOSE, "...found");
                tFound = true;
                tHandlePtr = &tIt->SipNuaHandleForCalls;
                break;
            }
        }

        if (tFound)
        {
            CallEvent *tCEvent = new CallEvent();
            // is participant user of the registered SIP server then replace sender by our official server login
            if ((pParticipant.find(mSipRegisterServer) != string::npos) && (GetServerRegistrationState()))
                tCEvent->Sender = "sip:" + mSipRegisterUsername + "@" + mSipRegisterServer;
            else
                tCEvent->Sender = "sip:" + GetLocalConferenceId();
            tCEvent->SenderName = GetLocalUserName();
            tCEvent->SenderComment = "";
            tCEvent->Receiver = "sip:" + pParticipant;
            tCEvent->HandlePtr = tHandlePtr;
            OutgoingEvents.Fire((GeneralEvent*) tCEvent);
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SendCallAcknowledge(string pParticipant)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for SendCallAcknowledge()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if ((IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port)) && (tIt->CallState == CALLSTATE_RINGING))
            {
                tFound = true;
                tHandlePtr = &tIt->SipNuaHandleForCalls;
                LOG(LOG_VERBOSE, "...found");
                break;
            }
        }

        if (tFound)
        {
            CallRingingEvent *tCREvent = new CallRingingEvent();
            tCREvent->Sender = "sip:" + GetLocalConferenceId();
            tCREvent->SenderName = GetLocalUserName();
            tCREvent->SenderComment = "";
            tCREvent->Receiver = "sip:" + pParticipant;
            tCREvent->HandlePtr = tHandlePtr;
            OutgoingEvents.Fire((GeneralEvent*) tCREvent);
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SendCallAccept(string pParticipant)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if ((IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port)) && (tIt->CallState == CALLSTATE_RINGING))
            {
                tFound = true;
                tHandlePtr = &tIt->SipNuaHandleForCalls;
                break;
            }
        }

        if (tFound)
        {
            CallAcceptEvent *tCAEvent = new CallAcceptEvent();
            tCAEvent->Sender = "sip:" + GetLocalConferenceId();
            tCAEvent->SenderName = GetLocalUserName();
            tCAEvent->SenderComment = "";
            tCAEvent->Receiver = "sip:" + pParticipant;
            tCAEvent->HandlePtr = tHandlePtr;
            OutgoingEvents.Fire((GeneralEvent*) tCAEvent);
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SendCallCancel(string pParticipant)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if ((IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port)) && (tIt->CallState == CALLSTATE_RINGING))
            {
                tFound = true;
                tHandlePtr = &tIt->SipNuaHandleForCalls;
                break;
            }
        }

        if (tFound)
        {
            CallCancelEvent *tCCEvent = new CallCancelEvent();
            tCCEvent->Sender = "sip:" + GetLocalConferenceId();
            tCCEvent->SenderName = GetLocalUserName();
            tCCEvent->SenderComment = "";
            tCCEvent->Receiver = "sip:" + pParticipant;
            tCCEvent->HandlePtr = tHandlePtr;
            OutgoingEvents.Fire((GeneralEvent*) tCCEvent);
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SendCallDeny(string pParticipant)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if ((IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port)) && (tIt->CallState == CALLSTATE_RINGING))
            {
                tFound = true;
                tHandlePtr = &tIt->SipNuaHandleForCalls;
                break;
            }
        }

        if (tFound)
        {
            CallDenyEvent *tCDEvent = new CallDenyEvent();
            tCDEvent->Sender = "sip:" + GetLocalConferenceId();
            tCDEvent->SenderName = GetLocalUserName();
            tCDEvent->SenderComment = "";
            tCDEvent->Receiver = "sip:" + pParticipant;
            tCDEvent->HandlePtr = tHandlePtr;
            OutgoingEvents.Fire((GeneralEvent*) tCDEvent);
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SendHangUp(string pParticipant)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for SendHangUp()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if ((IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port)) && (tIt->CallState == CALLSTATE_RUNNING))
            {
                tFound = true;
                tIt->CallState = CALLSTATE_STANDBY;
                tHandlePtr = &tIt->SipNuaHandleForCalls;
                //LOG(LOG_VERBOSE, "...found");
                break;
            }
        }

        if (tFound)
        {
            CallHangUpEvent *tCHUEvent = new CallHangUpEvent();
            tCHUEvent->Sender = "sip:" + GetLocalConferenceId();
            tCHUEvent->SenderName = GetLocalUserName();
            tCHUEvent->SenderComment = "";
            tCHUEvent->Receiver = "sip:" + pParticipant;
            tCHUEvent->HandlePtr = tHandlePtr;
            OutgoingEvents.Fire((GeneralEvent*) tCHUEvent);
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SendProbe(std::string pParticipant)
{
    LOG(LOG_VERBOSE, "Probing: %s", pParticipant.c_str());

    OptionsEvent *tOEvent = new OptionsEvent();

    // is participant user of the registered SIP server then acknowledge directly
    if ((pParticipant.find(mSipRegisterServer) != string::npos) && (GetServerRegistrationState()))
    {
        LOG(LOG_VERBOSE, "Probing of %s skipped and participant reported as available because he belongs to the registered SIP server", pParticipant.c_str());
        OptionsAcceptEvent *tOAEvent = new OptionsAcceptEvent();

        tOAEvent->Sender = pParticipant;
        tOAEvent->Receiver = GetLocalConferenceId();
        notifyObservers(tOAEvent);
        return true;
    }

    // probe P2P SIP participants
    tOEvent->Sender = "sip:" + GetLocalConferenceId();
    tOEvent->SenderName = GetLocalUserName();
    tOEvent->SenderComment = "";
    tOEvent->Receiver = "sip:" + pParticipant;
    tOEvent->HandlePtr = NULL; // done within SIP class
    OutgoingEvents.Fire((GeneralEvent*) tOEvent);

    return true;
}

const char* Meeting::GetSdpData(std::string pParticipant)
{
    const char *tResult = "";
    ParticipantList::iterator tIt;
    int tLocalAudioPort;
    int tLocalVideoPort;


    LOG(LOG_VERBOSE, "GetSdp for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for GetSdpData()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
            {
                LOG(LOG_VERBOSE, "...found");
                if (tIt->VSocket == NULL)
                {
                    LOG(LOG_ERROR, "Found video socket reference is NULL");
                    return tResult;
                }
                if (tIt->ASocket == NULL)
                {
                    LOG(LOG_ERROR, "Found audio socket reference is NULL");
                    return tResult;
                }
                // ####################### get ports #############################
                tLocalVideoPort = tIt->VSocket->GetLocalPort();
                tLocalAudioPort = tIt->ASocket->GetLocalPort();

                // ##################### create SDP string #######################
                // set sdp string
                tIt->Sdp = CreateSdpData(tLocalAudioPort, tLocalVideoPort);

                tResult = tIt->Sdp.c_str();
                LOG(LOG_VERBOSE, "VPort: %d\n APort: %d\n SDP: %s\n", tLocalVideoPort, tLocalAudioPort, tResult);
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tResult;
}

bool Meeting::SearchParticipantAndSetState(string pParticipant, int pState)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndSetState()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
            {
                tIt->CallState = pState;
                tFound = true;
                LOG(LOG_VERBOSE, "...found");
                break;
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SearchParticipantAndSetOwnContactAddress(string pParticipant, string pOwnNatIp, unsigned int pOwnNatPort)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndSetOwnContactAddress()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
            {
                tIt->OwnIp = pOwnNatIp;
                tIt->OwnPort = pOwnNatPort;
                tFound = true;
                LOG(LOG_VERBOSE, "...found");
                LOG(LOG_VERBOSE, "...set own contact address to: %s:%u", pOwnNatIp.c_str(), pOwnNatPort);
                break;
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SearchParticipantAndSetNuaHandleForMsgs(string pParticipant, nua_handle_t *pNuaHandle)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndSetNuaHandleForMsgs()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
            {
                tIt->SipNuaHandleForMsgs = pNuaHandle;
                tFound = true;
                LOG(LOG_VERBOSE, "...found");
                break;
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SearchParticipantAndSetNuaHandleForCalls(string pParticipant, nua_handle_t *pNuaHandle)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndSetNuaHandleForCalls()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
            {
                tIt->SipNuaHandleForCalls = pNuaHandle;
                tFound = true;
                LOG(LOG_VERBOSE, "...found");
                break;
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

nua_handle_t** Meeting::SearchParticipantAndGetNuaHandleForCalls(string pParticipant)
{
    nua_handle_t** tResult = NULL;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndGetNuaHandleForCalls()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
            {
                tResult = &tIt->SipNuaHandleForCalls;
                LOG(LOG_VERBOSE, "...found");
                break;
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tResult;
}

bool Meeting::SearchParticipantByNuaHandleOrName(string &pUser, string &pHost, string &pPort, nua_handle_t *pNuaHandle)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            LOG(LOG_VERBOSE, "CompareForSearchByHandle: candidate \"%s\"", SipCreateId(tIt->User, tIt->Host, tIt->Port).c_str());
            if ((tIt->SipNuaHandleForCalls == pNuaHandle) || (tIt->SipNuaHandleForMsgs == pNuaHandle) || ((pUser == tIt->User) && (pHost == tIt->Host) && (pPort == tIt->Port)))
            {
                pUser = tIt->User;
                pHost = tIt->Host;
                pPort = tIt->Port;
                tFound = true;
                LOG(LOG_VERBOSE, "...found");
                break;
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SearchParticipantAndSetRemoteMediaInformation(std::string pParticipant, std::string pVideoHost, unsigned int pVideoPort, std::string pVideoCodec, std::string pAudioHost, unsigned int pAudioPort, std::string pAudioCodec)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndSetRemoteMediaInformation()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
            {
                tIt->RemoteVideoHost = pVideoHost;
                tIt->RemoteVideoPort = pVideoPort;
                tIt->RemoteVideoCodec = pVideoCodec;
                tIt->RemoteAudioHost = pAudioHost;
                tIt->RemoteAudioPort = pAudioPort;
                tIt->RemoteAudioCodec = pAudioCodec;
                tFound = true;
                LOG(LOG_VERBOSE, "...found");
                LOG(LOG_VERBOSE, "...set remote video information to: %s:%u with codec %s", pVideoHost.c_str(), pVideoPort, pVideoCodec.c_str());
                LOG(LOG_VERBOSE, "...set remote audio information to: %s:%u with codec %s", pAudioHost.c_str(), pAudioPort, pAudioCodec.c_str());
                break;
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::IsLocalAddress(string pHost, string pPort)
{
    bool        tFound = false;
    string      tLocalPort = toString(mSipHostPort);
    LocalAddressesList::iterator tIt;

    for (tIt = mLocalAddresses.begin(); tIt != mLocalAddresses.end(); tIt++)
    {
        LOG(LOG_VERBOSE, "CompareForLocalUser: \"%s\" with \"%s\"", (pHost + ":" + pPort).c_str(), ((*tIt) + ":" + tLocalPort).c_str());
        if ((pHost == (*tIt)) && (pPort == tLocalPort))
        {
            tFound = true;
            LOG(LOG_VERBOSE, "...found");
        }
    }

    return tFound;
}

Socket* Meeting::GetAudioSocket(string pParticipant)
{
    Socket *tResult = NULL;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "GetAudioSocket for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    if (pParticipant == mBroadcastAdr)
        tResult = mParticipants.begin()->ASocket;
    else
    {
        // is the recipient already involved in the conference?
        if (mParticipants.size() > 1)
        {
            LOG(LOG_VERBOSE, "Search matching database entry for GetAudioSocket()");
            for (tIt = mParticipants.begin()++; tIt != mParticipants.end(); tIt++)
            {
                if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
                {
                    tResult = tIt->ASocket;
                    LOG(LOG_VERBOSE, "...found");
                }
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tResult == NULL)
        LOG(LOG_WARN, "Resulting audio socket is NULL");

    return tResult;
}

Socket* Meeting::GetVideoSocket(string pParticipant)
{
    Socket *tResult = NULL;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "GetVideoSocket for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    if (pParticipant == mBroadcastAdr)
        tResult = mParticipants.begin()->VSocket;
    else
    {
        // is the recipient already involved in the conference?
        if (mParticipants.size() > 1)
        {
            LOG(LOG_VERBOSE, "Search matching database entry for GetAudioSocket()");
            for (tIt = mParticipants.begin()++; tIt != mParticipants.end(); tIt++)
            {
                if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
                {
                    tResult = tIt->VSocket;
                    LOG(LOG_VERBOSE, "...found");
                }
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tResult == NULL)
        LOG(LOG_WARN, "Resulting video socket is NULL");

    return tResult;
}

int Meeting::GetCallState(string pParticipant)
{
    int tResult = CALLSTATE_INVALID;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "getCallState for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    if (pParticipant == mBroadcastAdr)
        tResult = CALLSTATE_INVALID;
    else
    {
        // is the recipient already involved in the conference?
        if (mParticipants.size() > 1)
        {
            LOG(LOG_VERBOSE, "Search matching database entry for GetCallState()");
            for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
            {
                if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
                {
                    tResult = tIt->CallState;
                    LOG(LOG_VERBOSE, "...found");
                }
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tResult;
}

bool Meeting::GetSessionInfo(string pParticipant, struct SessionInfo *pInfo)
{
    bool tResult = false;
    ParticipantList::iterator tIt;

    //LOG(LOG_VERBOSE, "GetSessionInfo for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    if (pParticipant == mBroadcastAdr)
    {
        tResult = true;
        pInfo->User = "Broadcast";
        pInfo->Host = "multiple";
        pInfo->Port = "5060";
        pInfo->OwnIp = "multiple";
        pInfo->OwnPort = "5060";
        pInfo->RemoteVideoHost = "multiple";
        pInfo->RemoteVideoPort = "0";
        pInfo->RemoteVideoCodec = "";
        pInfo->RemoteAudioHost = "multiple";
        pInfo->RemoteAudioPort = "0";
        pInfo->RemoteAudioCodec = "";
        pInfo->LocalVideoPort = "0";
        pInfo->LocalAudioPort = "0";
        pInfo->CallState = "multiple";
    }else
    {
        // is the recipient already involved in the conference?
        if (mParticipants.size() > 1)
        {
            LOG(LOG_VERBOSE, "Search matching database entry for GetSessionInfo()");
            for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
            {
                if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
                {
                    tResult = true;
                    pInfo->User = tIt->User;
                    pInfo->Host = tIt->Host;
                    pInfo->Port = tIt->Port;
                    pInfo->OwnIp = tIt->OwnIp;
                    pInfo->OwnPort = toString(tIt->OwnPort);
                    pInfo->RemoteVideoHost = tIt->RemoteVideoHost;
                    pInfo->RemoteVideoPort = toString(tIt->RemoteVideoPort);
                    pInfo->RemoteVideoCodec = tIt->RemoteVideoCodec;
                    pInfo->RemoteAudioHost = tIt->RemoteAudioHost;
                    pInfo->RemoteAudioPort = toString(tIt->RemoteAudioPort);
                    pInfo->RemoteAudioCodec = tIt->RemoteAudioCodec;
                    pInfo->LocalVideoPort = toString(tIt->VSocket->GetLocalPort());
                    pInfo->LocalAudioPort = toString(tIt->ASocket->GetLocalPort());
                    pInfo->CallState = CallStateAsString(tIt->CallState);
                    //LOG(LOG_VERBOSE, "...found");
                }
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tResult;
}

void Meeting::GetOwnContactAddress(std::string pParticipant, std::string &pIp, unsigned int &pPort)
{
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "getOwnContactAddress for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    // is the recipient already involved in the conference?
    if (mParticipants.size() > 1)
    {
        LOG(LOG_VERBOSE, "Search matching database entry for GetOwnContactAddress()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, tIt->User, tIt->Host, tIt->Port))
            {
                pIp = tIt->OwnIp;
                pPort = tIt->OwnPort;
                LOG(LOG_VERBOSE, "...found");
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
