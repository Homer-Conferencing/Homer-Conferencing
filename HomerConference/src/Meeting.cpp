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

Meeting *sMeeting = NULL;

///////////////////////////////////////////////////////////////////////////////

struct ParticipantDescriptor
{
    std::string    User;
    std::string    Host;
    std::string    Port;
    enum TransportType Transport;
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
    nua_handle_t   *SipNuaHandleForPresenceSubscription;
    Socket         *VideoReceiveSocket;
    Socket         *AudioReceiveSocket;
    Socket         *VideoSendSocket;
    Socket         *AudioSendSocket;
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
	if (sMeeting == NULL)
		sMeeting = new Meeting();
    return *sMeeting;
}

///////////////////////////////////////////////////////////////////////////////

void Meeting::Init(string pLocalGatewayAddress, AddressesList pLocalAddresses, AddressesList pLocalAddressesNetmask, int pSipStartPort, TransportType pSipListenerTransport, bool pNatTraversalSupport, int pStunStartPort, int pVideoAudioStartPort, string pBroadcastIdentifier)
{
    SIP::Init(pLocalGatewayAddress, pLocalAddresses, pLocalAddressesNetmask, pSipStartPort, pSipListenerTransport, pNatTraversalSupport, pStunStartPort);

    // start value for port auto probing (default is 5000)
    SetVideoAudioStartPort(pVideoAudioStartPort);

    mBroadcastIdentifier = pBroadcastIdentifier;

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

    tParticipantDescriptor.User = GetUserName();
    tParticipantDescriptor.Host = pLocalGatewayAddress;
    tParticipantDescriptor.Port = toString(mSipHostPort);
    tParticipantDescriptor.SipNuaHandleForCalls = NULL;
    tParticipantDescriptor.SipNuaHandleForMsgs = NULL;
    tParticipantDescriptor.SipNuaHandleForOptions = NULL;
    tParticipantDescriptor.SipNuaHandleForPresenceSubscription = NULL;
    tParticipantDescriptor.CallState = CALLSTATE_STANDBY;
    tParticipantDescriptor.VideoReceiveSocket = NULL;
    tParticipantDescriptor.AudioReceiveSocket = NULL;
    tParticipantDescriptor.VideoSendSocket = NULL;
    tParticipantDescriptor.AudioSendSocket = NULL;

    mParticipants.push_back(tParticipantDescriptor);
    mMeetingInitiated = true;
}

void Meeting::Stop()
{
    if (!mMeetingInitiated)
    {
        LOG(LOG_WARN, "Session manager wasn't initiated yet");
        return;
    }

    //trigger ending of SIP main loop
    StopSipMainLoop();

    //wait for SIP main loop
    if (!StopThread(2000))
        LOG(LOG_INFO, "SIP-Listener thread terminated, pending packets skipped");
}

void Meeting::Deinit()
{
    SIP_stun::Deinit();
    SIP::Deinit();
}

void Meeting::SetVideoAudioStartPort(int pPort)
{
    if (pPort != mVideoAudioStartPort)
    {
        LOG(LOG_VERBOSE, "Setting start port for video/audio sockets to %d", pPort);
        mVideoAudioStartPort = pPort;
    }
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

string Meeting::GetOwnRoutingAddressForPeer(std::string pForeignHost)
{
    if(!mSipNatTraversalSupport)
        return GetHostAdr();

    if ((pForeignHost == "") ||
        (pForeignHost == GetHostAdr()))
    {
        return GetHostAdr();
    }

    if (IS_IPV6_ADDRESS(mLocalGatewayAddress))
    {//IPv6
        return GetHostAdr();
    }else
    {//IPv4 processing, based on RFC 5735
        // check for loopback address
        if (pForeignHost.substr(0, 3) == "127.")    /* 127.0.0.0.0 - 172.255.255.255  */
        {
            return "127.0.0.1";         // loopback address
        }

        // check for private address
        if ((pForeignHost.substr(0, 3) == "10.")      /* 10.0.0.0       - 10.255.255.255  */ ||
            (pForeignHost.substr(0, 4) == "172.")     /* 172.16.0.0     - 172.31.255.255  */ || //HINT: we check only for the first digit ;)
            (pForeignHost.substr(0, 8) == "192.168.") /* 192.168.0.0    - 192.168.255.255 */ )
        {
            return GetHostAdr();        // local address
        }

        // check for link local address
        if (pForeignHost.substr(0, 8) == "169.254.") /* 169.254.0.0.0  - 169.254.255.255  */
        {
            return GetHostAdr();        // local address
        }

        if (GetStunNatIp() != "")
            return GetStunNatIp();      // outmost NAT address
        else
            return GetHostAdr();        // local address
    }
}

void Meeting::SetLocalUserName(string pName)
{
    string tFilteredString = "";
    LOG(LOG_VERBOSE, "Called to set local name to %s", pName.c_str());
    for (int i = 0; i < (int)pName.length(); i++)
    {
        switch((unsigned char)pName[i])
        {
            case 'A'...'Z':
            case 'a'...'z':
            case ' ':
            case '_':
            case 196 /* AE */:
            case 214 /* OE */:
            case 220 /* UE */:
            case 218 /* ae */:
            case 246 /* oe */:
            case 252 /* ue */:
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

bool Meeting::OpenParticipantSession(string pUser, string pHost, string pPort, enum TransportType pTransport, string pIPLocalInterface)
{
    bool        tFound = false;
    ParticipantDescriptor tParticipantDescriptor;
    ParticipantList::iterator tIt;

    if(pPort == "")
        pPort = "5060";

    // lock
    mParticipantsMutex.lock();

    // is this user already involved in the conference?
    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        //LOG(LOG_VERBOSE, "CompareForOpen: \"%s\" with \"%s\" and state: %d", (pUser + "@" + pHost + ":" + pPort).c_str(), (tIt->User + "@" + tIt->Host + ":" + tIt->Port).c_str(), tIt->CallState);
        if (IsThisParticipant(pUser, pHost, pPort, pTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            LOG(LOG_VERBOSE, "...session found");
            tFound = true;
        }
    }

    if (!tFound)
    {
        LOG(LOG_VERBOSE, "Opening session to: %s", SipCreateId(pUser, pHost, pPort).c_str());
        string tIPFromBestLocalInterface = (pIPLocalInterface == "" ? GetLocalSource(pHost) : pIPLocalInterface);

        tParticipantDescriptor.User = pUser;
        tParticipantDescriptor.Host = pHost;
        tParticipantDescriptor.Port = pPort;
        tParticipantDescriptor.Transport = pTransport;
        tParticipantDescriptor.OwnIp = tIPFromBestLocalInterface;
        tParticipantDescriptor.OwnPort = (unsigned int)GetHostPort();
        tParticipantDescriptor.RemoteVideoHost = "0.0.0.0";
        tParticipantDescriptor.RemoteVideoPort = 0;
        tParticipantDescriptor.RemoteVideoCodec = "";
        tParticipantDescriptor.RemoteAudioHost = "0.0.0.0";
        tParticipantDescriptor.RemoteAudioPort = 0;
        tParticipantDescriptor.RemoteAudioCodec = "";
        tParticipantDescriptor.SipNuaHandleForCalls = NULL;
        tParticipantDescriptor.SipNuaHandleForMsgs = NULL;
        tParticipantDescriptor.SipNuaHandleForOptions = NULL;
        tParticipantDescriptor.SipNuaHandleForPresenceSubscription = NULL;
        tParticipantDescriptor.CallState = CALLSTATE_STANDBY;

        bool tUseBirectionalMediaSockets = false;
        #ifdef MEETING_ALLOW_BIRECTIONAL_MEDIA_SOCKETS
            tUseBirectionalMediaSockets = mSipNatTraversalSupport;
        #endif
        if (tUseBirectionalMediaSockets)
		{
            LOG(LOG_VERBOSE, "Using bidirectional media sockets to support NAT traversal");

			#if defined(WINDOWS) || defined(APPLE) || defined(BSD)
                // create video receiver port
                tParticipantDescriptor.VideoReceiveSocket = Socket::CreateServerSocket(IS_IPV6_ADDRESS(pHost) ? SOCKET_IPv6 : SOCKET_IPv4, GetSocketTypeFromMediaTransportType(GetVideoTransportType()), mVideoAudioStartPort, true, 2);
                if (tParticipantDescriptor.VideoReceiveSocket == NULL)
                    LOG(LOG_ERROR, "Invalid video receive socket");
                else
                    mVideoAudioStartPort = tParticipantDescriptor.VideoReceiveSocket->GetLocalPort() + 2;

                // create audio receiver port
                tParticipantDescriptor.AudioReceiveSocket = Socket::CreateServerSocket(IS_IPV6_ADDRESS(pHost) ? SOCKET_IPv6 : SOCKET_IPv4, GetSocketTypeFromMediaTransportType(GetAudioTransportType()), mVideoAudioStartPort, true, 2);
                if (tParticipantDescriptor.AudioReceiveSocket == NULL)
                    LOG(LOG_ERROR, "Invalid audio receive socket");
                else
                    mVideoAudioStartPort = tParticipantDescriptor.AudioReceiveSocket->GetLocalPort() + 2;

                // create video sender port
                //HINT: for Windows/BSD/OSX the client socket has to be created before the server socket when using both assigned to the same port, Linux doesn't care about the order
                tParticipantDescriptor.VideoSendSocket = Socket::CreateClientSocket(tParticipantDescriptor.VideoReceiveSocket->GetNetworkType(), tParticipantDescriptor.VideoReceiveSocket->GetTransportType(), tParticipantDescriptor.VideoReceiveSocket->GetLocalPort(), true, 0);
                if (tParticipantDescriptor.VideoSendSocket == NULL)
                    LOG(LOG_ERROR, "Invalid video send socket");
                // create audio sender port
                tParticipantDescriptor.AudioSendSocket = Socket::CreateClientSocket(tParticipantDescriptor.AudioReceiveSocket->GetNetworkType(), tParticipantDescriptor.AudioReceiveSocket->GetTransportType(), tParticipantDescriptor.AudioReceiveSocket->GetLocalPort(), true, 0);
                if (tParticipantDescriptor.AudioSendSocket == NULL)
                    LOG(LOG_ERROR, "Invalid audio send socket");
            #else
                // create video sender port
                tParticipantDescriptor.VideoSendSocket = Socket::CreateClientSocket(IS_IPV6_ADDRESS(pHost) ? SOCKET_IPv6 : SOCKET_IPv4, GetSocketTypeFromMediaTransportType(GetVideoTransportType()), mVideoAudioStartPort, true, 2);
                if (tParticipantDescriptor.VideoSendSocket == NULL)
                    LOG(LOG_ERROR, "Invalid video send socket");
                else
                    mVideoAudioStartPort = tParticipantDescriptor.VideoSendSocket->GetLocalPort() + 2;

                // create audio sender port
                tParticipantDescriptor.AudioSendSocket = Socket::CreateClientSocket(IS_IPV6_ADDRESS(pHost) ? SOCKET_IPv6 : SOCKET_IPv4, GetSocketTypeFromMediaTransportType(GetAudioTransportType()), mVideoAudioStartPort, true, 2);
                if (tParticipantDescriptor.AudioSendSocket == NULL)
                    LOG(LOG_ERROR, "Invalid audio send socket");
                else
                    mVideoAudioStartPort = tParticipantDescriptor.AudioSendSocket->GetLocalPort() + 2;

                // create video listener port
                //HINT: for Windows/BSD/OSX the client socket has to be created before the server socket when using both assigned to the same port, Linux doesn't care about the order
                tParticipantDescriptor.VideoReceiveSocket = Socket::CreateServerSocket(tParticipantDescriptor.VideoSendSocket->GetNetworkType(), tParticipantDescriptor.VideoSendSocket->GetTransportType(), tParticipantDescriptor.VideoSendSocket->GetLocalPort(), true, 0);
                if (tParticipantDescriptor.VideoReceiveSocket == NULL)
                    LOG(LOG_ERROR, "Invalid video receive socket");
                // create audio listener port
                tParticipantDescriptor.AudioReceiveSocket = Socket::CreateServerSocket(tParticipantDescriptor.AudioSendSocket->GetNetworkType(), tParticipantDescriptor.AudioSendSocket->GetTransportType(), tParticipantDescriptor.AudioSendSocket->GetLocalPort(), true, 0);
                if (tParticipantDescriptor.AudioReceiveSocket == NULL)
                    LOG(LOG_ERROR, "Invalid audio receive socket");
            #endif
		}else
		{
            LOG(LOG_VERBOSE, "Using only unidirectional media sockets without NAT traversal support");

            // create video sender port
			tParticipantDescriptor.VideoSendSocket = Socket::CreateClientSocket(IS_IPV6_ADDRESS(pHost) ? SOCKET_IPv6 : SOCKET_IPv4, GetSocketTypeFromMediaTransportType(GetVideoTransportType()), mVideoAudioStartPort, false, 2);
			if (tParticipantDescriptor.VideoSendSocket == NULL)
				LOG(LOG_ERROR, "Invalid video send socket");
            else
                mVideoAudioStartPort = tParticipantDescriptor.VideoSendSocket->GetLocalPort() + 2;

			// create audio sender port
			tParticipantDescriptor.AudioSendSocket = Socket::CreateClientSocket(IS_IPV6_ADDRESS(pHost) ? SOCKET_IPv6 : SOCKET_IPv4, GetSocketTypeFromMediaTransportType(GetAudioTransportType()), mVideoAudioStartPort, false, 2);
			if (tParticipantDescriptor.AudioSendSocket == NULL)
				LOG(LOG_ERROR, "Invalid audio send socket");
            else
                mVideoAudioStartPort = tParticipantDescriptor.AudioSendSocket->GetLocalPort() + 2;

			// create video listener port
			tParticipantDescriptor.VideoReceiveSocket = Socket::CreateServerSocket(SOCKET_IPv6, GetSocketTypeFromMediaTransportType(GetVideoTransportType()), mVideoAudioStartPort, false, 2);
			if (tParticipantDescriptor.VideoReceiveSocket == NULL)
				LOG(LOG_ERROR, "Invalid video receive socket");
			else
				mVideoAudioStartPort = tParticipantDescriptor.VideoReceiveSocket->GetLocalPort() + 2;

			// create audio listener port
			tParticipantDescriptor.AudioReceiveSocket = Socket::CreateServerSocket(SOCKET_IPv6, GetSocketTypeFromMediaTransportType(GetAudioTransportType()), mVideoAudioStartPort, false, 2);
			if (tParticipantDescriptor.AudioReceiveSocket == NULL)
				LOG(LOG_ERROR, "Invalid audio receive socket");
			else
				mVideoAudioStartPort = tParticipantDescriptor.AudioReceiveSocket->GetLocalPort() + 2;
		}

        mParticipants.push_back(tParticipantDescriptor);

    }else
        LOG(LOG_VERBOSE, "Participant session already exists, open request ignored");

    // unlock
    mParticipantsMutex.unlock();

    return !tFound;
}

bool Meeting::CloseParticipantSession(string pParticipant, enum TransportType pParticipantTransport)
{
    bool        tFound = false;
    ParticipantList::iterator tIt;

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            // hint: the media sources are deleted within video/audio-widget

            #if defined(WINDOWS) || defined(APPLE) || defined(BSD)
                // delete video/audio sockets
                delete (*tIt).VideoSendSocket;
                delete (*tIt).AudioSendSocket;
                delete (*tIt).VideoReceiveSocket;
                delete (*tIt).AudioReceiveSocket;
            #else
                // delete video/audio sockets
                delete (*tIt).VideoReceiveSocket;
                delete (*tIt).AudioReceiveSocket;
                delete (*tIt).VideoSendSocket;
                delete (*tIt).AudioSendSocket;
            #endif

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
            mOutgoingEvents.Fire((GeneralEvent*) tMEvent);
        }

        tResult = true;
    }else
        tResult = false;

    // unlock
    mParticipantsMutex.unlock();

    return tResult;
}

bool Meeting::SendMessage(string pParticipant, enum TransportType pParticipantTransport, string pMessage)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;
    string tDestinationAddress = "";

    // should we use broadcast scheme instead of unciast one?
    if (pParticipant == mBroadcastIdentifier)
        return SendBroadcastMessage(pMessage);

    LOG(LOG_VERBOSE, "Sending message to: %s[%s]", pParticipant.c_str(), Socket::TransportType2String(pParticipantTransport).c_str());

    LOG(LOG_VERBOSE, "Search matching database entry for SendMessage()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        //LOG(LOG_VERBOSE, "SendMessage() compares participant %s with %s,%s,%s", pParticipant.c_str(), tIt->User.c_str(), tIt->Host.c_str(), tIt->Port.c_str());
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            LOG(LOG_VERBOSE, "...found");
            tFound = true;
            tDestinationAddress = tIt->Host;
            tHandlePtr = &tIt->SipNuaHandleForMsgs;
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tFound)
    {
        MessageEvent *tMEvent = new MessageEvent();
        // is participant user of the registered SIP server then replace sender by our official server login
        if ((pParticipant.find(mSipRegisterServerAddress) != string::npos) && (GetServerRegistrationState()))
            tMEvent->Sender = "sip:" + GetServerConferenceId();
        else
            tMEvent->Sender = "sip:" + GetLocalConferenceId(tDestinationAddress);
        tMEvent->SenderName = GetLocalUserName();
        tMEvent->SenderComment = "";
        tMEvent->Receiver = "sip:" + pParticipant;
        tMEvent->Transport = pParticipantTransport;
        tMEvent->HandlePtr = tHandlePtr;
        tMEvent->Text = pMessage;
        mOutgoingEvents.Fire((GeneralEvent*) tMEvent);
    }

    return tFound;
}

bool Meeting::SendCall(string pParticipant, enum TransportType pParticipantTransport)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;
    string tDestinationAddress = "";

//TODO: support broadcast, similar to SendMessage()
    // lock
    mParticipantsMutex.lock();

    LOG(LOG_VERBOSE, "Search matching database entry for SendCall()");
    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            if(tIt->CallState == CALLSTATE_STANDBY)
            {
                LOG(LOG_VERBOSE, "...found");
                tFound = true;
                tDestinationAddress = tIt->Host;
                tHandlePtr = &tIt->SipNuaHandleForCalls;
                break;
            }else
            {
                LOG(LOG_WARN, "SendCall() detected inconsistency in state machine, ignoring call request in order to fix this");
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tFound)
    {
        CallEvent *tCEvent = new CallEvent();
        // is participant user of the registered SIP server then replace sender by our official server login
        if ((pParticipant.find(mSipRegisterServerAddress) != string::npos) && (GetServerRegistrationState()))
            tCEvent->Sender = "sip:" + GetServerConferenceId();
        else
            tCEvent->Sender = "sip:" + GetLocalConferenceId(tDestinationAddress);
        tCEvent->SenderName = GetLocalUserName();
        tCEvent->SenderComment = "";
        tCEvent->Receiver = "sip:" + pParticipant;
        tCEvent->Transport = pParticipantTransport;
        tCEvent->HandlePtr = tHandlePtr;
        mOutgoingEvents.Fire((GeneralEvent*) tCEvent);
    }

    return tFound;
}

bool Meeting::SendCallAcknowledge(string pParticipant, enum TransportType pParticipantTransport)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;
    string tDestinationAddress = "";

    LOG(LOG_VERBOSE, "Search matching database entry for SendCallAcknowledge()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if ((IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport)) && (tIt->CallState == CALLSTATE_RINGING))
        {
            tFound = true;
            tHandlePtr = &tIt->SipNuaHandleForCalls;
            tDestinationAddress = tIt->Host;
            LOG(LOG_VERBOSE, "...found");
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tFound)
    {
        CallRingingEvent *tCREvent = new CallRingingEvent();
        if ((pParticipant.find(mSipRegisterServerAddress) != string::npos) && (GetServerRegistrationState()))
            tCREvent->Sender = "sip:" + GetServerConferenceId();
        else
            tCREvent->Sender = "sip:" + GetLocalConferenceId(tDestinationAddress);
        tCREvent->SenderName = GetLocalUserName();
        tCREvent->SenderComment = "";
        tCREvent->Receiver = "sip:" + pParticipant;
        tCREvent->HandlePtr = tHandlePtr;
        mOutgoingEvents.Fire((GeneralEvent*) tCREvent);
    }

    return tFound;
}

bool Meeting::SendCallAccept(string pParticipant, enum TransportType pParticipantTransport)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;
    string tDestinationAddress = "";

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if ((IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport)) && (tIt->CallState == CALLSTATE_RINGING))
        {
            tFound = true;
            tHandlePtr = &tIt->SipNuaHandleForCalls;
            tDestinationAddress = tIt->Host;
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tFound)
    {
        CallAcceptEvent *tCAEvent = new CallAcceptEvent();
        if ((pParticipant.find(mSipRegisterServerAddress) != string::npos) && (GetServerRegistrationState()))
            tCAEvent->Sender = "sip:" + GetServerConferenceId();
        else
            tCAEvent->Sender = "sip:" + GetLocalConferenceId(tDestinationAddress);
        tCAEvent->SenderName = GetLocalUserName();
        tCAEvent->SenderComment = "";
        tCAEvent->Receiver = "sip:" + pParticipant;
        tCAEvent->HandlePtr = tHandlePtr;
        mOutgoingEvents.Fire((GeneralEvent*) tCAEvent);
    }

    return tFound;
}

bool Meeting::SendCallCancel(string pParticipant, enum TransportType pParticipantTransport)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;
    string tDestinationAddress = "";

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if ((IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport)) && (tIt->CallState == CALLSTATE_RINGING))
        {
            tFound = true;
            tHandlePtr = &tIt->SipNuaHandleForCalls;
            tDestinationAddress = tIt->Host;
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tFound)
    {
        CallCancelEvent *tCCEvent = new CallCancelEvent();
        if ((pParticipant.find(mSipRegisterServerAddress) != string::npos) && (GetServerRegistrationState()))
            tCCEvent->Sender = "sip:" + GetServerConferenceId();
        else
            tCCEvent->Sender = "sip:" + GetLocalConferenceId(tDestinationAddress);
        tCCEvent->SenderName = GetLocalUserName();
        tCCEvent->SenderComment = "";
        tCCEvent->Receiver = "sip:" + pParticipant;
        tCCEvent->HandlePtr = tHandlePtr;
        mOutgoingEvents.Fire((GeneralEvent*) tCCEvent);
    }

    return tFound;
}

bool Meeting::SendCallDeny(string pParticipant, enum TransportType pParticipantTransport)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;
    string tDestinationAddress = "";

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if ((IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport)) && (tIt->CallState == CALLSTATE_RINGING))
        {
            tFound = true;
            tHandlePtr = &tIt->SipNuaHandleForCalls;
            tDestinationAddress = tIt->Host;
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tFound)
    {
        CallDenyEvent *tCDEvent = new CallDenyEvent();
        if ((pParticipant.find(mSipRegisterServerAddress) != string::npos) && (GetServerRegistrationState()))
            tCDEvent->Sender = "sip:" + GetServerConferenceId();
        else
            tCDEvent->Sender = "sip:" + GetLocalConferenceId(tDestinationAddress);
        tCDEvent->SenderName = GetLocalUserName();
        tCDEvent->SenderComment = "";
        tCDEvent->Receiver = "sip:" + pParticipant;
        tCDEvent->HandlePtr = tHandlePtr;
        mOutgoingEvents.Fire((GeneralEvent*) tCDEvent);
    }

    return tFound;
}

bool Meeting::SendHangUp(string pParticipant, enum TransportType pParticipantTransport)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;
    string tDestinationAddress = "";

    LOG(LOG_VERBOSE, "Search matching database entry for SendHangUp()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if ((IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport)) && (tIt->CallState == CALLSTATE_RUNNING))
        {
            tFound = true;
            tIt->CallState = CALLSTATE_STANDBY;
            tHandlePtr = &tIt->SipNuaHandleForCalls;
            tDestinationAddress = tIt->Host;
            //LOG(LOG_VERBOSE, "...found");
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tFound)
    {
        CallHangUpEvent *tCHUEvent = new CallHangUpEvent();
        if ((pParticipant.find(mSipRegisterServerAddress) != string::npos) && (GetServerRegistrationState()))
            tCHUEvent->Sender = "sip:" + GetServerConferenceId();
        else
            tCHUEvent->Sender = "sip:" + GetLocalConferenceId(tDestinationAddress);
        tCHUEvent->SenderName = GetLocalUserName();
        tCHUEvent->SenderComment = "";
        tCHUEvent->Receiver = "sip:" + pParticipant;
        tCHUEvent->HandlePtr = tHandlePtr;
        mOutgoingEvents.Fire((GeneralEvent*) tCHUEvent);
    }

    return tFound;
}

bool Meeting::SendAvailabilityProbe(std::string pUser, std::string pHost, std::string pPort, enum TransportType pParticipantTransport)
{
    bool        tFound = false;
    nua_handle_t **tHandlePtr = NULL;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "Probing: %s@%s:%s[%s]", pUser.c_str(), pHost.c_str(), pPort.c_str(), Socket::TransportType2String(pParticipantTransport).c_str());

    // is participant user of the registered SIP server then acknowledge directly
    if ((pHost == mSipRegisterServerAddress) && (pPort == mSipRegisterServerPort) && (GetServerRegistrationState()))
    {
        LOG(LOG_VERBOSE, "OPTIONS based probing of %s skipped", pHost.c_str());

        OpenParticipantSession(pUser, pHost, pPort, pParticipantTransport);

        LOG(LOG_VERBOSE, "Search matching database entry for SendAvailabilityProbe()");

        // lock
        mParticipantsMutex.lock();

        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            LOG(LOG_WARN, "Found %s@%s:%s, transport=%d", tIt->User.c_str(), tIt->Host.c_str(), tIt->Port.c_str(), (int)tIt->Transport);
            if ((pUser == tIt->User) && (pHost == tIt->Host))
            {
                LOG(LOG_VERBOSE, "...found");
                tFound = true;
                tHandlePtr = &tIt->SipNuaHandleForPresenceSubscription;
                break;
            }
        }

        // unlock
        mParticipantsMutex.unlock();

        if (tFound)
        {
            SubscriptionPresenceEvent *tSPEvent = new SubscriptionPresenceEvent();
            // is participant user of the registered SIP server then replace sender by our official server login
            tSPEvent->Sender = "sip:" + GetServerConferenceId();
            tSPEvent->SenderName = GetLocalUserName();
            tSPEvent->SenderComment = "";
            tSPEvent->Receiver = "sip:" + pUser + "@" + pHost;
            tSPEvent->Transport = pParticipantTransport;
            tSPEvent->HandlePtr = tHandlePtr;
            mOutgoingEvents.Fire((GeneralEvent*) tSPEvent);
        }else
            LOG(LOG_ERROR, "SendAvailabilityProbe() couldn't find the participant in the internal database");
    }else
    {
		// probe P2P SIP participants
        OptionsEvent *tOEvent = new OptionsEvent();
		tOEvent->Sender = "sip:" + GetLocalConferenceId(pHost);
		tOEvent->SenderName = GetLocalUserName();
		tOEvent->SenderComment = "";
		tOEvent->Receiver = "sip:" + SipCreateId("", pHost, pPort);
		tOEvent->HandlePtr = NULL; // done within SIP class
		tOEvent->Transport = pParticipantTransport;
		mOutgoingEvents.Fire((GeneralEvent*) tOEvent);
    }

    return true;
}

const char* Meeting::GetSdpData(std::string pParticipant, enum TransportType pParticipantTransport)
{
    const char *tResult = "";
    ParticipantList::iterator tIt;
    int tLocalAudioPort;
    int tLocalVideoPort;


    LOG(LOG_VERBOSE, "GetSdp for: %s", pParticipant.c_str());

    LOG(LOG_VERBOSE, "Search matching database entry for GetSdpData()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            LOG(LOG_VERBOSE, "...found");
            if (tIt->VideoReceiveSocket == NULL)
            {
                LOG(LOG_ERROR, "Found video socket reference is NULL");
                return tResult;
            }
            if (tIt->AudioReceiveSocket == NULL)
            {
                LOG(LOG_ERROR, "Found audio socket reference is NULL");
                return tResult;
            }
            // ####################### get ports #############################
            tLocalVideoPort = tIt->VideoReceiveSocket->GetLocalPort();
            tLocalAudioPort = tIt->AudioReceiveSocket->GetLocalPort();

            // ##################### create SDP string #######################
            // set sdp string
            tIt->Sdp = CreateSdpData(tLocalAudioPort, tLocalVideoPort);

            tResult = tIt->Sdp.c_str();
            LOG(LOG_VERBOSE, "VPort: %d\n APort: %d\n SDP: %s\n", tLocalVideoPort, tLocalAudioPort, tResult);
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tResult;
}

bool Meeting::SearchParticipantAndSetState(string pParticipant, enum TransportType pParticipantTransport, int pState)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndSetState()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            tIt->CallState = pState;
            tFound = true;
            LOG(LOG_VERBOSE, "...found");
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

//HINT: following function is used in case a call was received and the SIP destination address differs from the IP address of the network layer
bool Meeting::SearchParticipantAndSetOwnContactAddress(string pParticipant, enum TransportType pParticipantTransport, string pOwnNatIp, unsigned int pOwnNatPort)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndSetOwnContactAddress()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            tIt->OwnIp = pOwnNatIp;
            tIt->OwnPort = pOwnNatPort;
            tFound = true;
            LOG(LOG_VERBOSE, "...found");
            LOG(LOG_VERBOSE, "...set own contact address to: %s:%u", pOwnNatIp.c_str(), pOwnNatPort);
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SearchParticipantAndSetNuaHandleForMsgs(string pParticipant, enum TransportType pParticipantTransport, nua_handle_t *pNuaHandle)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndSetNuaHandleForMsgs()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            tIt->SipNuaHandleForMsgs = pNuaHandle;
            tFound = true;
            LOG(LOG_VERBOSE, "...found");
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SearchParticipantAndSetNuaHandleForCalls(string pParticipant, enum TransportType pParticipantTransport, nua_handle_t *pNuaHandle)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndSetNuaHandleForCalls()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            tIt->SipNuaHandleForCalls = pNuaHandle;
            tFound = true;
            LOG(LOG_VERBOSE, "...found");
            break;
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

nua_handle_t** Meeting::SearchParticipantAndGetNuaHandleForCalls(string pParticipant, enum TransportType pParticipantTransport)
{
    nua_handle_t** tResult = NULL;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndGetNuaHandleForCalls()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            tResult = &tIt->SipNuaHandleForCalls;
            LOG(LOG_VERBOSE, "...found");
            break;
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

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::SearchParticipantAndSetRemoteMediaInformation(std::string pParticipant, enum TransportType pParticipantTransport, std::string pVideoHost, unsigned int pVideoPort, std::string pVideoCodec, std::string pAudioHost, unsigned int pAudioPort, std::string pAudioCodec)
{
    bool tFound = false;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "Search matching database entry for SearchParticipantAndSetRemoteMediaInformation()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
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

    // unlock
    mParticipantsMutex.unlock();

    return tFound;
}

bool Meeting::IsLocalAddress(string pHost, string pPort, enum TransportType pTransport)
{
    bool        tFound = false;
    string      tLocalPort = toString(mSipHostPort);
    AddressesList::iterator tIt;

    if(pPort == "")
        pPort = "5060";

    for (tIt = mLocalAddresses.begin(); tIt != mLocalAddresses.end(); tIt++)
    {
        LOG(LOG_VERBOSE, "CompareForLocalUser: \"%s\" with \"%s\"", (pHost + ":" + pPort).c_str(), ((*tIt) + ":" + tLocalPort).c_str());
        if ((pHost == (*tIt)) && (pPort == tLocalPort) && ((pTransport == mSipHostPortTransport) || (mSipHostPortTransport == SOCKET_TRANSPORT_AUTO)))
        {
            tFound = true;
            LOG(LOG_VERBOSE, "...found");
        }
    }

    return tFound;
}

Socket* Meeting::GetAudioReceiveSocket(string pParticipant, enum TransportType pParticipantTransport)
{
    Socket *tResult = NULL;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "GetAudioSocket for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    if (pParticipant == mBroadcastIdentifier)
        tResult = mParticipants.begin()->AudioReceiveSocket;
    else
    {
        LOG(LOG_VERBOSE, "Search matching database entry for GetAudioReceiveSocket()");
        for (tIt = mParticipants.begin()++; tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
            {
                tResult = tIt->AudioReceiveSocket;
                LOG(LOG_VERBOSE, "...found");
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tResult == NULL)
        LOG(LOG_WARN, "Resulting audio receive socket is NULL");

    return tResult;
}

Socket* Meeting::GetVideoReceiveSocket(string pParticipant, enum TransportType pParticipantTransport)
{
    Socket *tResult = NULL;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "GetVideoSocket for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    if (pParticipant == mBroadcastIdentifier)
        tResult = mParticipants.begin()->VideoReceiveSocket;
    else
    {
        LOG(LOG_VERBOSE, "Search matching database entry for GetVideoReceiveSocket()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
            {
                tResult = tIt->VideoReceiveSocket;
                LOG(LOG_VERBOSE, "...found");
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tResult == NULL)
        LOG(LOG_WARN, "Resulting video receive socket is NULL");

    return tResult;
}

Socket* Meeting::GetAudioSendSocket(string pParticipant, enum TransportType pParticipantTransport)
{
    Socket *tResult = NULL;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "GetAudioSocket for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    if (pParticipant == mBroadcastIdentifier)
        tResult = mParticipants.begin()->AudioSendSocket;
    else
    {
        LOG(LOG_VERBOSE, "Search matching database entry for GetAudioSendSocket()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
            {
                tResult = tIt->AudioSendSocket;
                LOG(LOG_VERBOSE, "...found");
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tResult == NULL)
        LOG(LOG_WARN, "Resulting audio send socket is NULL");

    return tResult;
}

Socket* Meeting::GetVideoSendSocket(string pParticipant, enum TransportType pParticipantTransport)
{
    Socket *tResult = NULL;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "GetVideoSocket for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    if (pParticipant == mBroadcastIdentifier)
        tResult = mParticipants.begin()->VideoSendSocket;
    else
    {
        LOG(LOG_VERBOSE, "Search matching database entry for GetVideoSendSocket()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
            {
                tResult = tIt->VideoSendSocket;
                LOG(LOG_VERBOSE, "...found");
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    if (tResult == NULL)
        LOG(LOG_WARN, "Resulting video send socket is NULL");

    return tResult;
}

int Meeting::GetCallState(string pParticipant, enum TransportType pParticipantTransport)
{
    int tResult = CALLSTATE_INVALID;
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "getCallState for: %s", pParticipant.c_str());

    // lock
    mParticipantsMutex.lock();

    if (pParticipant == mBroadcastIdentifier)
        tResult = CALLSTATE_INVALID;
    else
    {
        LOG(LOG_VERBOSE, "Search matching database entry for GetCallState()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
            {
                tResult = tIt->CallState;
                LOG(LOG_VERBOSE, "...found");
            }
        }
    }

    // unlock
    mParticipantsMutex.unlock();

    return tResult;
}

bool Meeting::GetSessionInfo(string pParticipant, enum TransportType pParticipantTransport, struct SessionInfo *pInfo)
{
    bool tResult = false;
    ParticipantList::iterator tIt;

    //LOG(LOG_VERBOSE, "GetSessionInfo for: %s", pParticipant.c_str());

    if (pParticipant == mBroadcastIdentifier)
    {
        tResult = true;
        pInfo->User = "Broadcast";
        pInfo->Host = "multiple";
        pInfo->Port = "5060";
        pInfo->Transport = "*";
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
        // lock
        mParticipantsMutex.lock();

        //LOG(LOG_VERBOSE, "Search matching database entry for GetSessionInfo()");
        for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
        {
            if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
            {
                tResult = true;
                pInfo->User = tIt->User;
                pInfo->Host = tIt->Host;
                pInfo->Port = tIt->Port;
                pInfo->Transport = Socket::TransportType2String(tIt->Transport);
                pInfo->OwnIp = tIt->OwnIp;
                pInfo->OwnPort = toString(tIt->OwnPort);
                pInfo->RemoteVideoHost = tIt->RemoteVideoHost;
                pInfo->RemoteVideoPort = toString(tIt->RemoteVideoPort);
                pInfo->RemoteVideoCodec = tIt->RemoteVideoCodec;
                pInfo->RemoteAudioHost = tIt->RemoteAudioHost;
                pInfo->RemoteAudioPort = toString(tIt->RemoteAudioPort);
                pInfo->RemoteAudioCodec = tIt->RemoteAudioCodec;
                pInfo->LocalVideoPort = toString(tIt->VideoReceiveSocket->GetLocalPort());
                pInfo->LocalAudioPort = toString(tIt->AudioReceiveSocket->GetLocalPort());
                pInfo->CallState = CallStateAsString(tIt->CallState);
                //LOG(LOG_VERBOSE, "...found");
            }
        }

        // unlock
        mParticipantsMutex.unlock();
    }

    return tResult;
}

void Meeting::GetOwnContactAddress(std::string pParticipant, enum TransportType pParticipantTransport, std::string &pIp, unsigned int &pPort)
{
    ParticipantList::iterator tIt;

    LOG(LOG_VERBOSE, "getOwnContactAddress for: %s", pParticipant.c_str());

    LOG(LOG_VERBOSE, "Search matching database entry for GetOwnContactAddress()");

    // lock
    mParticipantsMutex.lock();

    for (tIt = mParticipants.begin(); tIt != mParticipants.end(); tIt++)
    {
        if (IsThisParticipant(pParticipant, pParticipantTransport, tIt->User, tIt->Host, tIt->Port, tIt->Transport))
        {
            pIp = tIt->OwnIp;
            pPort = tIt->OwnPort;
            LOG(LOG_VERBOSE, "...found");
        }
    }

    // unlock
    mParticipantsMutex.unlock();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
