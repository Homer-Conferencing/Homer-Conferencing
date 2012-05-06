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
#if !defined(APPLE) && !defined(BSD) // no STUN for APPLE & BSD environment: TODO: fix the occurring hangs and reactivate STUN support for APPLE / BSD
#define SIP_SUPORTING_STUN
#endif

#define SIP_OUTBOUND_OPTIONS        "outbound natify use-stun use-rport"

// de/activate NAT traversal mechanism: adaption of source address in case of NAT by using proprietary signaling which is stored within "phrase" string
//HINT: incompatible clients don't recognize this mechanism and react in a usual way
//#define SIP_NAT_PROPRIETARY_ADDRESS_ADAPTION

// de/activate strict assertions
//#define SIP_ASSERTS

#define USER_AGENT_SIGNATURE                            "homer-conferencing.com"
#define ORGANIZATION_SIGNATURE                          "homer-conferencing.com"

#define CALL_REQUEST_RETRIES                            1
#define MESSAGE_REQUEST_RETRIES                         1
#define OPTIONS_REQUEST_RETRIES                         0

#define CALL_REQUEST_TIMEOUT                            3 //seconds


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
    void setAvailabilityState(enum AvailabilityState pState, std::string pStateText = "");
    void setAvailabilityState(std::string pState);
    int getAvailabilityState();
    std::string getAvailabilityStateStr();
    /* server based user registration */
    bool RegisterAtServer();
    bool RegisterAtServer(std::string pUsername, std::string pPassword, std::string pServer = "sip2sip.info");
    void UnregisterAtServer();
    bool GetServerRegistrationState();
    std::string GetServerSoftwareId();
    /* NAT detection */
    virtual void SetStunServer(std::string pServer);
    /* general */
    static std::string SipCreateId(std::string pUser, std::string pHost, std::string pPort = "");
    static bool SplitParticipantName(string pParticipant, string &pUser, string &Host, string &pPort);
    bool IsThisParticipant(string pParticipant, string pUser, string pHost, string pPort);
    bool IsThisParticipant(string pParticipantUser, string pParticipantHost, string pParticipantPort, string pUser, string pHost, string pPort);

    /* SIP call back */
    void SipCallBack(int pEvent, int pStatus, char const *pPhrase, nua_t *pNua, nua_magic_t *pMagic, nua_handle_t *pNuaHandle, nua_hmagic_t *pHMagic, sip_t const *pSip, void* pTags);
private:
    /* main SIP event loop handling */
    virtual void* Run(void*);

    /* init. general application events for SIP requests/responses */
    void InitGeneralEvent_FromSipReceivedRequestEvent(const sip_to_t *pRemote, const sip_to_t *pLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, GeneralEvent *pEvent, std::string pEventName, std::string &pSourceIp, unsigned int pSourcePort);
    std::string InitGeneralEvent_FromSipReceivedResponseEvent(const sip_to_t *pRemote, const sip_to_t *pLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, GeneralEvent *pEvent, std::string pEventName, std::string &pSourceIp, unsigned int pSourcePort);

protected:
    void Init(int pStartPort = 5060, Homer::Base::TransportType pSipListenerTransport = SOCKET_UDP, bool pSipNatTraversalSupport = false, int pStunPort = 5070);
    void DeInit();

    void StopSipMainLoop();

    void PrintSipHeaderInfo(const sip_to_t *pRemote, const sip_to_t *pLocal, sip_t const *pSip);
    void printFromToSendingSipEvent(nua_handle_t *pNuaHandle, GeneralEvent *pEvent, std::string pEventName);
    void initParticipantTriplet(const sip_to_t *pRemote, sip_t const *pSip, std::string &pSourceIp, unsigned int pSourcePort, std::string &pUser, std::string &pHost, std::string &pPort);

    void SipReceivedError(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);

    void SipReceivedMessage(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
    void SipReceivedMessageResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, char const *pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        /* helpers for "message response */
        void SipReceivedMessageAccept(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        void SipReceivedMessageAcceptDelayed(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        void SipReceivedMessageUnavailable(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, char const *pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        /* */
    void SipReceivedCall(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, void* pTags, std::string pSourceIp, unsigned int pSourcePort);
    void SipReceivedCallResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, void* pTags, std::string pSourceIp, unsigned int pSourcePort);
        /* helpers for "call response */
        void SipReceivedCallRinging(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        void SipReceivedCallAccept(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        void SipReceivedCallUnavailable(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        void SipReceivedCallDeny(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        void SipReceivedCallDenyNat(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort, std::string pOwnNatIp, unsigned int pOwnNatPort);
        /* */
    void SipReceivedCallCancel(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
    void SipReceivedCallHangup(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
    void SipReceivedCallHangupResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
    void SipReceivedCallTermination(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
    void SipReceivedCallStateChange(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, void* pTags, std::string pSourceIp, unsigned int pSourcePort);
    void SipReceivedOptionsResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        /* helpers for "options response */
        void SipReceivedOptionsResponseAccept(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        void SipReceivedOptionsResponseUnavailable(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
        /* */
    void SipReceivedShutdownResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort);
    void SipReceivedRegisterResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort);
    void SipReceivedPublishResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort);
    void SipReceivedAuthenticationResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, std::string pSourceIp, unsigned int pSourcePort);
    void SipSendMessage(MessageEvent *pMEvent);
    void SipSendCall(CallEvent *pCEvent);
    void SipSendCallRinging(CallRingingEvent *pCREvent);
    void SipSendCallCancel(CallCancelEvent *pCCEvent);
    void SipSendCallAccept(CallAcceptEvent *pCAEvent);
    void SipSendCallDeny(CallDenyEvent *pCDEvent);
    void SipSendCallHangUp(CallHangUpEvent *pCHUEvent);
    void SipSendOptionsRequest(OptionsEvent *pOEvent);
    void SipProcessOutgoingEvents();

    /* sip registrar support */
    bool SipLoginAtServer();
    void SipLogoutAtServer();

    EventManager        mOutgoingEvents; // from users point of view
    enum AvailabilityState mAvailabilityState;
    SipContext          *mSipContext;
    std::string         mSipHostAdr;
    nua_handle_t        *mSipRegisterHandle, *mSipPublishHandle;
    std::string         mSipRegisterServer;
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

