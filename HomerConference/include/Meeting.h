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
 * Purpose: conference management
 * Author:  Thomas Volkert
 * Since:   2008-11-25
 */

#ifndef _CONFERENCE_MEETING_
#define _CONFERENCE_MEETING_

#include <Header_SofiaSipForwDecl.h>
#include <SDP.h>
#include <SIP.h>
#include <MeetingEvents.h>
#include <HBMutex.h>
#include <HBSocket.h>

#include <string>
#include <list>

using namespace Homer::Base;

namespace Homer { namespace Conference {

///////////////////////////////////////////////////////////////////////////////

// birectional sockets for video/audio transmission: local media socket is used both for sending stream to peer and for receiving stream from peer
//HINT: this is need for easier NAT traversal because it allows the assumption sending and receiving port number at remote side are the same and can be derived from SDP data
#define MEETING_ALLOW_BIRECTIONAL_MEDIA_SOCKETS

///////////////////////////////////////////////////////////////////////////////

#define MEETING Meeting::GetInstance()

// configuration
#define MEETING_AUTOACK_CALLS                   false

#define CALLSTATE_INVALID                       -1
#define CALLSTATE_STANDBY                       0
#define CALLSTATE_RINGING                       1
#define CALLSTATE_RUNNING                       2

struct SessionInfo
{
    std::string    User;
    std::string    Host;
    std::string    Port;
    std::string    OwnIp; //necessary for NAT traversal: store the outmost NAT's IP, directed towards this participant
    std::string    OwnPort; //necessary for NAT traversal: store the outmost NAT's PORT, directed towards this participant
    std::string    RemoteVideoHost;
    std::string    RemoteVideoPort;
    std::string    RemoteVideoCodec;
    std::string    RemoteAudioHost;
    std::string    RemoteAudioPort;
    std::string    RemoteAudioCodec;
    std::string    LocalVideoPort;
    std::string    LocalAudioPort;
    std::string    CallState;
};

struct ParticipantDescriptor;
typedef std::list<ParticipantDescriptor>  ParticipantList;
typedef std::list<std::string>            LocalAddressesList;

///////////////////////////////////////////////////////////////////////////////

class Meeting:
    public SDP, public SIP, public MeetingObservable
{
public:
    Meeting();

    virtual ~Meeting();

    static Meeting& GetInstance();

    void Init(std::string pSipHostAdr, LocalAddressesList pLocalAddresses, bool pNatTraversalSupport, std::string pBroadcastAdr = "Global messages", int pSipStartPort = 5060, Homer::Base::TransportType pSipListenerTransport = SOCKET_UDP, int pStunStartPort = 5070, int pVideoAudioStartPort = 5000);
    void SetVideoAudioStartPort(int pPort);
    void Stop();
    void Deinit();

    /* local interface/contact data */
    std::string GetHostAdr();
    int GetHostPort();
    std::string getUser();
    void SetLocalUserName(std::string pName);
    std::string GetLocalUserName();
    void SetLocalUserMailAdr(std::string pMailAdr);
    std::string GetLocalUserMailAdr();
    std::string GetLocalConferenceId();

    /* server interface/contact data */
    std::string GetServerConferenceId();

    /* local I/O interfaces and state */
    bool IsLocalAddress(std::string pHost, std::string pPort);
    Socket* GetAudioReceiveSocket(std::string pParticipant);
    Socket* GetVideoReceiveSocket(std::string pParticipant);
    Socket* GetAudioSendSocket(std::string pParticipant);
    Socket* GetVideoSendSocket(std::string pParticipant);
    int GetCallState(std::string pParticipant);
    bool GetSessionInfo(std::string pParticipant, struct SessionInfo *pInfo);
    void GetOwnContactAddress(std::string pParticipant, std::string &pIp, unsigned int &pPort);

    /* session management */
    bool OpenParticipantSession(std::string pUser, std::string pHost, std::string pPort);
    bool CloseParticipantSession(std::string pParticipant);
    int CountParticipantSessions();

    /* outgoing events */
    bool SendMessage(std::string pParticipant, std::string pMessage);
    bool SendBroadcastMessage(std::string pMessage);
    bool SendCall(std::string pParticipant);
    bool SendCallAcknowledge(std::string pParticipant);
    bool SendCallCancel(std::string pParticipant);
    bool SendCallAccept(std::string pParticipant);
    bool SendCallDeny(std::string pParticipant);
    bool SendHangUp(std::string pParticipant);
    bool SendProbe(std::string pParticipant);

private:
    friend class SIP;

    void SetHostAdr(std::string pHost); // no one should be allowed to change the local address from the outside
    std::string GetOwnRoutingAddressForPeer(std::string pForeignHost);

    bool SearchParticipantAndSetState(std::string pParticipant, int pState);
    bool SearchParticipantAndSetOwnContactAddress(std::string pParticipant, std::string pOwnNatIp, unsigned int pOwnNatPort);
    bool SearchParticipantAndSetNuaHandleForMsgs(std::string pParticipant, nua_handle_t *pNuaHandle);
    bool SearchParticipantAndSetNuaHandleForCalls(std::string pParticipant, nua_handle_t *pNuaHandle);
    bool SearchParticipantAndSetRemoteMediaInformation(std::string pParticipant, std::string pVideoHost, unsigned int pVideoPort, std::string pVideoCodec, std::string pAudioHost, unsigned int pAudioPort, std::string pAudioCodec);
    nua_handle_t ** SearchParticipantAndGetNuaHandleForCalls(string pParticipant);
    bool SearchParticipantByNuaHandleOrName(string &pUser, string &pHost, string &pPort, nua_handle_t *pNuaHandle);

    const char* GetSdpData(std::string pParticipant);
    std::string CallStateAsString(int pCallState);

    Mutex               mParticipantsMutex;
    ParticipantList     mParticipants;
    LocalAddressesList  mLocalAddresses;
    std::string         mOwnName;
    std::string         mOwnMail;
    std::string         mBroadcastAdr;
    bool                mMeetingInitiated;
    int                 mVideoAudioStartPort;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
