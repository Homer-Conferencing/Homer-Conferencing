/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: session initiation protocol
 * Author:  Thomas Volkert
 * Since:   2009-04-14
 */

#ifndef _CONFERENCE_SIP_
#define _CONFERENCE_SIP_

#include <Header_SofiaSipForwDecl.h>
#include <HBThread.h>
#include <HBSocket.h>
#include <MeetingEvents.h>
#include <SIP_stun.h>
#include <PIDF.h>

#include <string>

using namespace Homer::Base;

namespace Homer { namespace Conference {

///////////////////////////////////////////////////////////////////////////////

// de/activate STUN
#define SIP_SUPORTING_STUN

#define SIP_OUTBOUND_OPTIONS        "outbound natify use-stun use-rport"

// de/activate NAT traversal mechanism: adaption of source address in case of NAT by using proprietary signaling which is stored within "phrase" string
//HINT: incompatible clients don't recognize this mechanism and react in a usual way
//#define SIP_NAT_PROPRIETARY_ADDRESS_ADAPTION

// de/activate strict assertions
//#define SIP_ASSERTS

#define USER_AGENT_SIGNATURE                            "homer-conferencing.com"
#define ORGANIZATION_SIGNATURE                          "homer-conferencing.com"

enum AvailabilityState{
	AVAILABILITY_STATE_OFFLINE = 0,
	AVAILABILITY_STATE_ONLINE = 1,
	AVAILABILITY_STATE_ONLINE_AUTO = 2
};

struct SipContext;

///////////////////////////////////////////////////////////////////////////////

class SIP:
    public SIP_stun, public PIDF, public Thread
{
public:
    SIP();

    virtual ~SIP();

    static std::string GetSofiaSipVersion();

    /* presence management */
    void SetAvailabilityState(enum AvailabilityState pState, std::string pStateText = "");
    void SetAvailabilityState(std::string pState);
    int GetAvailabilityState();
    std::string GetAvailabilityStateStr();

    /* server based user registration */
    bool RegisterAtServer();
    bool RegisterAtServer(std::string pUsername, std::string pPassword, std::string pServer, unsigned int pServerPort);
    void UnregisterAtServer();
    bool GetServerRegistrationState();
    std::string GetServerSoftwareId();

    /* NAT detection */
    virtual void SetStunServer(std::string pServer);

    /* identify participant session */
    static std::string SipCreateId(std::string pUser, std::string pHost, std::string pPort = "");
    static bool SplitParticipantName(string pParticipant, string &pUser, string &Host, string &pPort);
    bool IsThisParticipant(string pParticipant, enum TransportType pParticipantTransport, string pUser, string pHost, string pPort, enum TransportType pTransport); // supports only IP addresses

    /* SIP call back */
    void SipCallBack(int pEvent, int pStatus, char const *pPhrase, nua_t *pNua, nua_magic_t *pMagic, nua_handle_t *pNuaHandle, nua_hmagic_t *pHMagic, sip_t const *pSip, void* pTags);

private:
    /* main SIP event loop handling */
    virtual void* Run(void*);

    /* init. general application events for SIP requests/responses */
    void InitGeneralEvent_FromSipReceivedRequestEvent(const sip_to_t *pRemote, const sip_to_t *pLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, GeneralEvent *pEvent, std::string pEventName, std::string &pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    std::string InitGeneralEvent_FromSipReceivedResponseEvent(const sip_to_t *pRemote, const sip_to_t *pLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, GeneralEvent *pEvent, std::string pEventName, std::string &pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);

protected:
    void Init(int pStartPort = 5060, Homer::Base::TransportType pSipListenerTransport = SOCKET_UDP, bool pSipNatTraversalSupport = false, int pStunPort = 5070);
    void DeInit();

    void StopSipMainLoop();

    void PrintSipHeaderInfo(const sip_to_t *pRemote, const sip_to_t *pLocal, sip_t const *pSip);
    void printFromToSendingSipEvent(nua_handle_t *pNuaHandle, GeneralEvent *pEvent, std::string pEventName);
    void initParticipantTriplet(const sip_to_t *pRemote, sip_t const *pSip, std::string &pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport, std::string &pUser, std::string &pHost, std::string &pPort);

    /* identify participant session */
    bool IsThisParticipant(string pParticipantUser, string pParticipantHost, string pParticipantPort, enum TransportType pParticipantTransport, string pUser, string pHost, string pPort, enum TransportType pTransport);

    void SipReceivedError(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);

    void SipReceivedMessage(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipReceivedMessageResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, char const *pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        /* helpers for "message response */
        void SipReceivedMessageAccept(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        void SipReceivedMessageAcceptDelayed(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        void SipReceivedMessageUnavailable(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, char const *pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        /* */
    void SipReceivedCall(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, void* pTags, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipReceivedCallResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, void* pTags, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        /* helpers for "call response */
        void SipReceivedCallRinging(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        void SipReceivedCallAccept(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        void SipReceivedCallUnavailable(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        void SipReceivedCallDeny(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        void SipReceivedCallDenyNat(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport, std::string pOwnNatIp, unsigned int pOwnNatPort);
        /* */
    void SipReceivedCallCancel(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipReceivedCallHangup(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipReceivedCallHangupResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipReceivedCallTermination(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipReceivedCallStateChange(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, void* pTags, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipReceivedOptionsResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        /* helpers for "options response */
        void SipReceivedOptionsResponseAccept(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        void SipReceivedOptionsResponseUnavailable(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
        /* */
    void SipReceivedShutdownResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipReceivedRegisterResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipReceivedPublishResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipReceivedAuthenticationResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, std::string pSourceIp, unsigned int pSourcePort, enum TransportType pSourcePortTransport);
    void SipSendMessage(MessageEvent *pMEvent);
    void SipSendCall(CallEvent *pCEvent);
    void SipSendCallRinging(CallRingingEvent *pCREvent);
    void SipSendCallCancel(CallCancelEvent *pCCEvent);
    void SipSendCallAccept(CallAcceptEvent *pCAEvent);
    void SipSendCallDeny(CallDenyEvent *pCDEvent);
    void SipSendCallHangUp(CallHangUpEvent *pCHUEvent);
    void SipSendOptionsRequest(OptionsEvent *pOEvent);
    void SipProcessOutgoingEvents();

    /* auth support */
    string CreateAuthInfo(sip_t const *pSip);

    /* sip registrar support */
    bool SipLoginAtServer();
    void SipLogoutAtServer();

    EventManager        mOutgoingEvents; // from users point of view
    enum AvailabilityState mAvailabilityState;
    SipContext          *mSipContext;
    std::string         mSipHostAdr;
    nua_handle_t        *mSipRegisterHandle, *mSipPublishHandle;
    std::string         mSipRegisterServer;
    std::string         mSipRegisterServerPort;
    std::string         mSipRegisterServerSoftwareId;
    std::string         mSipRegisterUsername;
    std::string         mSipRegisterPassword;
    sip_payload_t       *mPresenceDesription;
    int                 mSipHostPort;
    enum TransportType  mSipHostPortTransport;
    bool                mSipListenerNeeded;
    bool                mSipStackOnline;
    bool                mSipNatTraversalSupport;
    int                 mSipRegisteredAtServer; //tri-state: 0 = unregistered, 1 = registered, -1 = registration failed
    int                 mSipPresencePublished; //tri-state: 0 = unpublished, 1 = published, -1 = publication failed
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

