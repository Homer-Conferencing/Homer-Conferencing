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
 * Purpose: Implementation for session initiation protocol
 *          implemented as base class (some inits are done within MEETING class)
 * Author:  Thomas Volkert
 * Since:   2009-04-14
 */

#include <string>
#include <stdlib.h>
#include <sstream>
#include <assert.h>

#include <HBSocket.h>
#include <Header_SofiaSip.h>
#include <Meeting.h>
#include <ProcessStatisticService.h>
#include <SIP.h>
#include <SIP_stun.h>
#include <PIDF.h>
#include <Logger.h>

namespace Homer { namespace Conference {

using namespace std;
using namespace Homer::Base;
using namespace Homer::Monitor;

#define                 SIP_STATE_OKAY                      200
#define                 SIP_STATE_OKAY_DELAYED_DELIVERY     202
#define                 SIP_STATE_METHODE_NOT_ALLOWED       405
#define                 SIP_STATE_PROXY_AUTH_REQUIRED       407
#define                 SIP_STATE_REQUEST_TIMEOUT           408
#define                 SIP_STATE_BAD_EVENT                 489

struct SipContext
{
  su_home_t             Home;           /* memory home */
  su_root_t             *Root;          /* root object */
  nua_t                 *Nua;           /* NUA stack object */
};

///////////////////////////////////////////////////////////////////////////////

SIP::SIP():
    SIP_stun(), PIDF()
{
    mSipContext = new SipContext;
    mAvailabilityState = AVAILABILITY_STATE_YES;

    mSipStackOnline = false;
    mSipListenerNeeded = false;
    mSipRegisteredAtServer = false;
    mSipRegisterServer = "";
    mSipRegisterServerSoftwareId = "";
    mSipRegisterUsername = "";
    mSipRegisterPassword = "";
    mSipPublishHandle = NULL;
    mSipRegisterHandle = NULL;

    // set to localhost, will be corrected within meeting.C
    mSipHostAdr = "127.0.0.1";
}

SIP::~SIP()
{
    delete mSipContext;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////// main loop ///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
string SIP::GetSofiaSipVersion()
{
    return SOFIA_SIP_VERSION;
}

void SIP::Init(int pStartPort, int pStunPort)
{
    // default port is 5060, auto-probing within run()
    mSipHostPort = pStartPort;
    mStunHostPort = pStunPort;
}

void SIP::DeInit()
{
    UnregisterAtServer();
}

void SIP::SetStunServer(string pServer)
{
    SIP_stun::SetStunServer(pServer);

    // ### create internal event for starting NAT detection in SIP thread context
    InternalNatDetectionEvent *tINDEvent = new InternalNatDetectionEvent();
    tINDEvent->Failed = false;
    tINDEvent->FailureReason = "";
    OutgoingEvents.Fire((GeneralEvent*) tINDEvent);
}

string SIP::SipCreateId(string pUser, string pHost, string pPort)
{
    // add brackets for IPv6 hosts
    if ((pHost.find(":") != string::npos) && (pHost.find("[") == string::npos))
        pHost = "[" + pHost + "]";

    // ignore port "5060" because not necessary
    // HINT: SIP servers would be confused by this suffix -> they ignore incoming messages/calls with ":5060" in SIP address
    if ((pPort != "") && (pPort != "5060"))
        return (pUser + "@" + pHost + ":" + pPort);
    else
        return (pUser + "@" + pHost);
}

bool SIP::SplitParticipantName(string pParticipant, string &pUser, string &pHost, string &pPort)
{
    size_t tPos;

//    LOGEX(SIP, LOG_VERBOSE, "Participant: %s", pParticipant.c_str());

    // separate user part
    tPos = pParticipant.find('@');
    if (tPos != string::npos)
    {
        pUser = pParticipant.substr(0, tPos);
        pParticipant = pParticipant.substr(tPos + 1, pParticipant.size() - tPos);
    }else{
        pUser = "";
        LOGEX(SIP, LOG_WARN, "Couldn't find user part in %s", pParticipant.c_str());
    }

    // IPv6?
    tPos = pParticipant.find('[');
    if (tPos != string::npos)
    {
        size_t tPos_HostEnd = pParticipant.find(']');
        // separate host part
        pHost = pParticipant.substr(tPos + 1, tPos_HostEnd - tPos);
        // separate port part
        pPort = pParticipant.substr(tPos_HostEnd + 1 - pParticipant.size() - tPos_HostEnd);
    }else
    {
        tPos = pParticipant.find(':');
        if (tPos != string::npos)
        {
            // separate host part
            pHost = pParticipant.substr(0, tPos);
            // separate port part
            pPort = pParticipant.substr(tPos + 1, pParticipant.size() - tPos);

        }else{
            // set host by remaining participant string
            pHost = pParticipant;
            // set default port
            pPort = "5060";
        }
    }

//    LOGEX(SIP, LOG_VERBOSE, "User: %s", pUser.c_str());
//    LOGEX(SIP, LOG_VERBOSE, "Host: %s", pHost.c_str());
//    LOGEX(SIP, LOG_VERBOSE, "Port: %s", pPort.c_str());

    return true;
}

bool SIP::IsThisParticipant(string pParticipant, string pUser, string pHost, string pPort)
{
    bool tResult = false;

    string tUser, tHost, tPort;
    if (!SplitParticipantName(pParticipant, tUser, tHost, tPort))
    {
        LOGEX(SIP, LOG_ERROR, "Could not split participant name into its parts");
        return false;
    }
    tResult = (((tUser == pUser) || (tHost != mSipRegisterServer)) && (tHost == pHost) && (tPort == pPort));

    LOGEX(SIP, LOG_VERBOSE, "Comparing: %s - %s, %s - %s, %s - %s  ==> %s", tUser.c_str(), pUser.c_str(), tHost.c_str(), pHost.c_str(), tPort.c_str(), pPort.c_str(), tResult ? "MATCH" : "different");

    return tResult;
}

void GlobalSipCallBack(nua_event_t pEvent, int pStatus, char const *pPhrase, nua_t *pNua, nua_magic_t *pMagic, nua_handle_t *pNuaHandle, nua_hmagic_t *pHMagic, sip_t const *pSip, tagi_t pTags[])
{
    SIP* tSIP = (SIP*)pMagic;

    tSIP->SipCallBack((int)pEvent, pStatus, pPhrase, pNua, pMagic, pNuaHandle, pHMagic, pSip, (void*)pTags);
}

void* SIP::Run(void*)
{
    string tOwnAddress;

    SVC_PROCESS_STATISTIC.AssignThreadName("SIP-MainLoop");

    LOG(LOG_VERBOSE, "Setting up environment variables");
    if (LOGGER.GetLogLevel() == LOG_VERBOSE)
    {
        putenv((char*)"SOFIA_DEBUG=9"); // Default debugging level
        putenv((char*)"NUA_DEBUG=9"); //User Agent engine (nua)
        putenv((char*)"SOA_DEBUG_SDP=9"); // Offer/Answer engine (soa)
        putenv((char*)"NEA_DEBUG=9"); // Event engine (nea)
        putenv((char*)"IPTSEC_DEBUG=9"); //HTTP/SIP authentication module
        putenv((char*)"NTA_DEBUG=9"); //Transaction engine
        putenv((char*)"TPORT_DEBUG=9"); //Transport events
        putenv((char*)"SU_DEBUG=9"); //su module
        putenv((char*)"TPORT_LOG=1"); //Transport logging
    }else
    {
        putenv((char*)"SOFIA_DEBUG=0"); // Default debugging level
        putenv((char*)"NUA_DEBUG=0"); //User Agent engine (nua)
        putenv((char*)"SOA_DEBUG_SDP=0"); // Offer/Answer engine (soa)
        putenv((char*)"NEA_DEBUG=0"); // Event engine (nea)
        putenv((char*)"IPTSEC_DEBUG=0"); //HTTP/SIP authentication module
        putenv((char*)"NTA_DEBUG=0"); //Transaction engine
        putenv((char*)"TPORT_DEBUG=0"); //Transport events
        putenv((char*)"SU_DEBUG=0"); //su module
    }

    LOG(LOG_VERBOSE, "Setting up stack..");

    // initialize system utilities
    LOG(LOG_VERBOSE, "..init");
    su_init();

    // initialize memory handling
    LOG(LOG_VERBOSE, "..home init");
    su_home_init(&mSipContext->Home);

    // initialize root object
    LOG(LOG_VERBOSE, "..SU root create");
    mSipContext->Root = su_root_create(&mSipContext);

    if (mSipContext->Root != NULL)
    {
        // create NUA stack
        // auto probe SIP port (default: 5060 to 5064)
        for(int i = 0; i < 5; i++)
        {
            LOG(LOG_VERBOSE, "..NUA create");

            // add brackets for IPv6 address
            if (mSipHostAdr.find(":") != string::npos)
	            tOwnAddress = "sip:[" + mSipHostAdr + "]:" + toString(mSipHostPort) + ";transport=udp";
			else
	            tOwnAddress = "sip:" + mSipHostAdr + ":" + toString(mSipHostPort) + ";transport=udp";

            // NAT traversal: use keepalive packets with interval of 10 seconds
            //                otherwise a NAT box won't maintain the state about the NAT forwarding
            mSipContext->Nua = nua_create(mSipContext->Root, GlobalSipCallBack, this, NUTAG_URL(URL_STRING_MAKE(tOwnAddress.c_str())), TPTAG_KEEPALIVE(10000), NUTAG_OUTBOUND("natify use-stun"), TPTAG_REUSE(true), TAG_NULL()); //NUTAG_MEDIA_ENABLE(0)
            if (mSipContext->Nua != NULL)
                break;
            else
                LOG(LOG_INFO, "Another agent is already listening at %s<%d>:udp. Probing alternatives...", mSipHostAdr.c_str(), mSipHostPort);
            mSipHostPort++;
        }

        if (mSipContext->Nua != NULL)
        {
            LOG(LOG_INFO, "Listener assigned to %s", tOwnAddress.c_str());

             // set necessary parameters
            LOG(LOG_VERBOSE, "..set_params");
            nua_set_params(mSipContext->Nua, NUTAG_AUTOACK(MEETING_AUTOACK_CALLS), NUTAG_URL(URL_STRING_MAKE(tOwnAddress.c_str())), SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), SIPTAG_ORGANIZATION_STR(ORGANIZATION_SIGNATURE), NUTAG_OUTBOUND("natify outbound use-rport use-stun"), NTATAG_USER_VIA(1), TAG_NULL());

            //###################################################################
            //### STUN support
            //###################################################################
            #ifdef SIP_SUPORTING_STUN
                SIP_stun::Init(mSipContext->Root);

                // open NAT support via STUN
                LOG(LOG_VERBOSE, "..STUN based NAT detection");
            #endif

            LOG(LOG_VERBOSE, "..main loop");
            mSipListenerNeeded = true;
            mSipStackOnline = true;

            // we use one main loop for both incoming and outgoing events
            // because we arent allowed to use several threads here (limited by the SIP library concept)
            while (mSipListenerNeeded)
            {
                // OUTGOING events
                SipProcessOutgoingEvents();

                // INCOMING events: one step of main loop for processing of SIP messages, timeout is set to 100 ms
                su_root_step(mSipContext->Root, 100);
            }

            LOG(LOG_VERBOSE, "Closing stack..");

            LOG(LOG_VERBOSE, "..destroy registration handle");
            nua_handle_destroy(mSipRegisterHandle);

            // shutdown NUA stack
            LOG(LOG_VERBOSE, "..shutdown NUA stack");
            nua_shutdown(mSipContext->Nua);

            // wait for shutdown of NUA stack
            while (mSipStackOnline)
            {
                LOG(LOG_VERBOSE, "Wait for Shutdown-Step");

                // one step of main loop for processing of messages, timeout is set to 100 ms
                su_root_step(mSipContext->Root, 100);
            }

            // destroy NUA stack
            LOG(LOG_VERBOSE, "..NUA destroy");
            nua_destroy(mSipContext->Nua);

        }else
        {
            mSipStackOnline = true; //to continue startup despite the failure
            mSipListenerNeeded = true;
            LOG(LOG_ERROR, "Unable to start SIP stack. The selected host address is wrong or other SIP clients prevent startup");
        }

        // deinit root object
        LOG(LOG_VERBOSE, "..SU root destroy");
        su_root_destroy(mSipContext->Root);
        mSipContext->Root = NULL;

    }else
    {
        LOG(LOG_VERBOSE, "No ROOT object");
        LOG(LOG_VERBOSE, "Closing stack..");
    }

    // deinitialize memory handling
    LOG(LOG_VERBOSE, "..home deinit");
    su_home_deinit(&mSipContext->Home);

    // deinitialize system utilities
    LOG(LOG_VERBOSE, "..deinit");
    su_deinit();

    return NULL;
}

void SIP::StopSipMainLoop()
{
    mSipListenerNeeded = false;
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////// SERVER SUPPORT ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//HINT: http://sourceforge.net/tracker/index.php?func=detail&aid=2412241&group_id=143636&atid=756076
//      ==> "The Ekiga.net checks during registration that both the Via and Contact headers contain a public IP address.
//           The registration fails with 606 if the Via header contains a NATted address from the private address space."
bool SIP::SipLoginAtServer()
{
    sip_contact_t *tContact;
    sip_from_t *tFrom;
    sip_to_t *tTo;

    LOG(LOG_VERBOSE, "Going to register at sip server..");

    if (GetServerRegistrationState())
    {
        LOG(LOG_WARN, "Still registered at the server, no additional registration possible");
        return false;
    }

    LOG(LOG_VERBOSE, "..FROM header: sip:%s", SipCreateId(mSipRegisterUsername, mSipRegisterServer).c_str());
    tFrom = sip_to_make(&mSipContext->Home, ("sip:" + SipCreateId(mSipRegisterUsername, mSipRegisterServer)).c_str());
    if (tFrom == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"from\" handle for function \"SipLoginAtServer\" and user id \"%s\"", ("sip:" + SipCreateId(mSipRegisterUsername, mSipRegisterServer)).c_str());
        return false;
    }

    LOG(LOG_VERBOSE, "..TO header: sip:%s", SipCreateId(mSipRegisterUsername, mSipRegisterServer).c_str());
    tTo = sip_to_make(&mSipContext->Home, ("sip:" + SipCreateId(mSipRegisterUsername, mSipRegisterServer)).c_str());
    if (tTo == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"to\" handle for function \"SipLoginAtServer\" and user id \"%s\"", ("sip:" + SipCreateId(mSipRegisterUsername, mSipRegisterServer)).c_str());
        return false;
    }

    string tOwnIp = MEETING.GetHostAdr();
    if (mStunOutmostAdr != "")
        tOwnIp = mStunOutmostAdr;

    LOG(LOG_VERBOSE, "..CONTACT header: sip:%s", SipCreateId(mSipRegisterUsername, tOwnIp, toString(MEETING.GetHostPort())).c_str());
    tContact = sip_contact_make(&mSipContext->Home, ("sip:" + SipCreateId(mSipRegisterUsername, tOwnIp, toString(MEETING.GetHostPort()))).c_str());

    // create operation handle
    mSipRegisterHandle = nua_handle(mSipContext->Nua, &mSipContext->Home, SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), TAG_IF(tContact, SIPTAG_CONTACT(tContact)), SIPTAG_TO(tTo), SIPTAG_FROM(tFrom), TAG_END());

    if (mSipRegisterHandle == NULL)
    {
        LOG(LOG_ERROR, "Can not create operation handle");
        return false;
    }

    // send first registration request, unauthenticated
    nua_register(mSipRegisterHandle, NUTAG_M_FEATURES("expires=180"), TAG_END());

    return true;
}

void SIP::SipLogoutAtServer()
{
    if (!GetServerRegistrationState())
    {
        LOG(LOG_WARN, "Not registered at SIP server at the moment, no logout possible");
        return;
    }

    if((mSipRegisterHandle != NULL))
    {
        LOG(LOG_VERBOSE, "Unregister from SIP server");
        nua_unregister(mSipRegisterHandle, SIPTAG_CONTACT_STR("*" /* unreg. all */), TAG_END());
        nua_handle_destroy(mSipRegisterHandle);
        mSipRegisterHandle = NULL;
    }
    mSipRegisteredAtServer = false;
}

bool SIP::RegisterAtServer(string pUsername, string pPassword, string pServer)
{
    bool tResult = false;

    if ((pUsername == "") || (pPassword == "") || (pServer == ""))
    {
        LOG(LOG_ERROR, "Too few parameters to register at SIP server, check given user name and password");
        return false;
    }

    if ((mSipRegisterServer != pServer) || (mSipRegisterUsername != pUsername) || (mSipRegisterPassword != pPassword))
    {
        if (GetServerRegistrationState())
            UnregisterAtServer();

        LOG(LOG_VERBOSE, "Register at SIP server %s with login %s:%s", pServer.c_str(), pUsername.c_str(), pPassword.c_str());

        mSipRegisterServer = pServer;
        mSipRegisterUsername = pUsername;
        mSipRegisterPassword = pPassword;

        // if STUN is activated then wait until NAT information is found
        if (mStunSupportActivated)
        {
            bool tFirst = false;

            while(!mStunNatDetectionFinished)
            {
                if(!tFirst)
                {
                    tFirst = true;
                    LOG(LOG_INFO, "Waiting for STUN based NAT detection (this could take some seconds)");
                }
            }
            LOG(LOG_INFO, "NAT information found");
        }

        // login at SIP registrar server
        tResult = SipLoginAtServer();
    }

    return tResult;
}

bool SIP::RegisterAtServer()
{
    bool tResult = false;

    if ((mSipRegisterUsername == "") || (mSipRegisterPassword == "") || (mSipRegisterServer == ""))
    {
        LOG(LOG_ERROR, "Too few parameters to register at SIP server, check given user name and password");
        return false;
    }

    if (GetServerRegistrationState())
        UnregisterAtServer();

    LOG(LOG_VERBOSE, "Re-registering at SIP server");

    // if STUN is activated then wait until NAT information is found
    if (mStunSupportActivated)
    {
        bool tFirst = false;

        while(!mStunNatDetectionFinished)
        {
            if(!tFirst)
            {
                tFirst = true;
                LOG(LOG_INFO, "Waiting for STUN based NAT detection (this could take some seconds)");
            }
        }
        LOG(LOG_INFO, "NAT information found");
    }

    // login at SIP registrar server
    tResult = SipLoginAtServer();

    return tResult;
}

void SIP::UnregisterAtServer()
{
    SipLogoutAtServer();
}

bool SIP::GetServerRegistrationState()
{
    return (mSipRegisteredAtServer == true);
}

string SIP::GetServerSoftwareId()
{
    return mSipRegisterServerSoftwareId;
}

void SIP::SipReceivedRegisterResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    RegistrationEvent *tREvent;
    RegistrationFailedEvent *tRFEvent;

    /*
    status  response status code (if the request is retried,
            status is 100, the sip->sip_status->st_status
            contain the real status code from the response
            message, e.g., 302, 401, or 407)
    */
    switch(pStatus)
    {
        case 401: // needs auth.
            if (mSipRegisterHandle != NULL)
            {
                string tAuthInfo = "Digest:\"" + mSipRegisterServer + "\":" + mSipRegisterUsername + ":" + mSipRegisterPassword;

                LOG(LOG_VERBOSE, "Authentication information for registration: %s", tAuthInfo.c_str());

                // set auth. information
                nua_authenticate(mSipRegisterHandle, NUTAG_AUTH(tAuthInfo.c_str()), TAG_END());
            }
            break;

        case 100: // successful based on cache
            LOG(LOG_VERBOSE, "Registration update at SIP server based on cache succeeded");
            break;

        case SIP_STATE_OKAY: // successful
            LOG(LOG_VERBOSE, "Registration at SIP server succeeded");
            mSipRegisteredAtServer = true;

            tREvent = new RegistrationEvent();
            InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tREvent, "Registration", pSourceIp, pSourcePort);
            MEETING.notifyObservers(tREvent);
            break;

        default: // failed or something else happened
            LOG(LOG_ERROR, "Registration at SIP server failed");
            mSipRegisteredAtServer = -1;

            tRFEvent = new RegistrationFailedEvent();
            InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tRFEvent, "RegistrationFailed", pSourceIp, pSourcePort);
            MEETING.notifyObservers(tRFEvent);

            nua_handle_destroy(mSipRegisterHandle);
            mSipRegisterHandle = NULL;
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////// PRESENCE MANAGEMENT //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SIP::setAvailabilityState(enum AvailabilityState pState, string pStateText)
{
    int tResult = false;

    string tStateNote;

    LOG(LOG_VERBOSE, "Setting new availability-state: %d", pState);

    mAvailabilityState = pState;

    if ((mSipRegisteredAtServer <= 0) || (mAvailabilityState == pState))
        return;

    // publish presence state
    switch(mAvailabilityState)
    {
        case AVAILABILITY_STATE_NO:
            tStateNote = "dnd";
            break;
        case AVAILABILITY_STATE_YES_AUTO:
        case AVAILABILITY_STATE_YES:
            tStateNote = "online";
            break;
        default:
            LOG(LOG_ERROR, "We should never reach this point but we did");
            return;
    }
    if (pStateText != "")
        tStateNote += " - " + pStateText;
    mPresenceDesription = CreatePresenceInPidf(&mSipContext->Home, mSipRegisterUsername, mSipRegisterServer, tStateNote, true);

    mSipPresencePublished = false;

    sip_contact_t *tContact;
    sip_from_t *tFrom;
    sip_to_t *tTo;

    LOG(LOG_VERBOSE, "..FROM header: sip:%s", SipCreateId(mSipRegisterUsername, mSipRegisterServer).c_str());
    tFrom = sip_to_make(&mSipContext->Home, ("sip:" + SipCreateId(mSipRegisterUsername, mSipRegisterServer)).c_str());
    if (tFrom == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"from\" handle for function \"setAvailabilityState\" and user id \"%s\"", ("sip:" + SipCreateId(mSipRegisterUsername, mSipRegisterServer)).c_str());
        return;
    }

    LOG(LOG_VERBOSE, "..TO header: sip:%s", SipCreateId(mSipRegisterUsername, mSipRegisterServer).c_str());
    tTo = sip_to_make(&mSipContext->Home, ("sip:" + SipCreateId(mSipRegisterUsername, mSipRegisterServer)).c_str());
    if (tTo == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"to\" handle for function \"setAvailabilityState\" and user id \"%s\"", ("sip:" + SipCreateId(mSipRegisterUsername, mSipRegisterServer)).c_str());
        return;
    }

    string tOwnIp = MEETING.GetHostAdr();
    if (mStunOutmostAdr != "")
        tOwnIp = mStunOutmostAdr;

    LOG(LOG_VERBOSE, "..CONTACT header: sip:%s", SipCreateId(mSipRegisterUsername, tOwnIp, toString(MEETING.GetHostPort())).c_str());
    tContact = sip_contact_make(&mSipContext->Home, ("sip:" + SipCreateId(mSipRegisterUsername, tOwnIp, toString(MEETING.GetHostPort()))).c_str());

    // create operation handle
    mSipPublishHandle = nua_handle(mSipContext->Nua, &mSipContext->Home, SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), TAG_IF(tContact, SIPTAG_CONTACT(tContact)), SIPTAG_TO(tTo), SIPTAG_FROM(tFrom), TAG_END());

    if (mSipPublishHandle == NULL)
    {
        LOG(LOG_ERROR, "Can not create operation handle");
        return;
    }

    nua_publish(mSipPublishHandle, SIPTAG_PAYLOAD(mPresenceDesription), TAG_IF(mPresenceDesription, SIPTAG_CONTENT_TYPE_STR(GetMimeFormatPidf().c_str())), TAG_END());
}

int SIP::getAvailabilityState()
{
    return mAvailabilityState;
}

void SIP::setAvailabilityState(string pState)
{
    LOG(LOG_VERBOSE, "Setting new availability-state \"%s\"", pState.c_str());

    if (pState == "Unavailable")
        setAvailabilityState(AVAILABILITY_STATE_NO);
    if (pState == "Available")
        setAvailabilityState(AVAILABILITY_STATE_YES);
    if (pState == "Available (auto)")
        setAvailabilityState(AVAILABILITY_STATE_YES_AUTO);
}

string SIP::getAvailabilityStateStr()
{
    switch(mAvailabilityState)
    {
        case AVAILABILITY_STATE_NO:
            return "Unavailable";
            break;
        case AVAILABILITY_STATE_YES:
            return "Available";
            break;
        case AVAILABILITY_STATE_YES_AUTO:
            return "Available (auto)";
            break;
        default:
            return "Available";
            break;
    }
}

void SIP::SipReceivedPublishResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    PublicationEvent *tPEvent;
    PublicationFailedEvent *tPFEvent;

    /*
    status  response status code (if the request is retried,
            status is 100, the sip->sip_status->st_status
            contain the real status code from the response
            message, e.g., 302, 401, or 407)
    */
    switch(pStatus)
    {
        case 401: // needs auth.
        case SIP_STATE_PROXY_AUTH_REQUIRED:
            if (mSipPublishHandle != NULL)
            {
                string tAuthInfo = "Digest:\"" + mSipRegisterServer + "\":" + mSipRegisterUsername + ":" + mSipRegisterPassword;

                LOG(LOG_VERBOSE, "Authentication information for publication: %s", tAuthInfo.c_str());

                // set auth. information
                nua_authenticate(mSipPublishHandle, NUTAG_AUTH(tAuthInfo.c_str()), TAG_END());

                // send second registration request, authenticated now
                //snua_publish(mSipPublishhandle, SIPTAG_PAYLOAD(mPresenceDesription), TAG_IF(mPresenceDesription, SIPTAG_CONTENT_TYPE_STR(GetMimeFormatPidf().c_str())), TAG_END());
            }
            break;

        case 100: // successful based on cache
            LOG(LOG_VERBOSE, "Publication update at SIP server based on cache succeeded");
            break;

        case SIP_STATE_OKAY: // successful
            LOG(LOG_VERBOSE, "Presence publication at SIP server succeeded");
            mSipPresencePublished = true;

            tPEvent = new PublicationEvent();
            InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tPEvent, "Publication", pSourceIp, pSourcePort);
            MEETING.notifyObservers(tPEvent);

            break;

        case SIP_STATE_METHODE_NOT_ALLOWED:
            LOG(LOG_ERROR, "Method is not allowed");
            mSipPresencePublished = -1;

            tPFEvent = new PublicationFailedEvent();
            InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tPFEvent, "PublicationFailed", pSourceIp, pSourcePort);
            MEETING.notifyObservers(tPFEvent);

            //nua_handle_destroy(mSipPublishHandle);
            break;

        case SIP_STATE_REQUEST_TIMEOUT:
            LOG(LOG_ERROR, "Request timeout");
            mSipPresencePublished = -1;

            tPFEvent = new PublicationFailedEvent();
            InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tPFEvent, "PublicationFailed", pSourceIp, pSourcePort);
            MEETING.notifyObservers(tPFEvent);

            //nua_handle_destroy(mSipPublishHandle);
            break;

        case SIP_STATE_BAD_EVENT:
            LOG(LOG_ERROR, "Bad event received as answer for publish request");
            mSipPresencePublished = -1;

            tPFEvent = new PublicationFailedEvent();
            InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tPFEvent, "PublicationFailed", pSourceIp, pSourcePort);
            MEETING.notifyObservers(tPFEvent);

            //nua_handle_destroy(mSipPublishHandle);
            break;

        default: // failed or something else happened
            LOG(LOG_ERROR, "Presence publication at SIP server failed because of some unknown reasen");
            mSipPresencePublished = -1;

            tPFEvent = new PublicationFailedEvent();
            InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tPFEvent, "PublicationFailed", pSourceIp, pSourcePort);
            MEETING.notifyObservers(tPFEvent);

            //nua_handle_destroy(mSipPublishHandle);
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////// CENTRAL CALLBACK /////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SIP::SipCallBack(int pEvent, int pStatus, char const *pPhrase, nua_t *pNua, nua_magic_t *pMagic, nua_handle_t *pNuaHandle, nua_hmagic_t *pHMagic, sip_t const *pSip, void* pTags)
{
    string tSourceIp;
    unsigned int tSourcePort = 0;
    SocketAddressDescriptor *tAddressDescriptor = NULL;
    msg_t *tCurrentMessage = nua_current_request(pNua);
    char const* tEventName = nua_event_name((nua_event_t)pEvent);
    const sip_to_t *tRemote = nua_handle_remote(pNuaHandle);
    const sip_to_t *tLocal = nua_handle_local(pNuaHandle);

    LOGEX(SIP, LOG_INFO, "############# SIP-new INCOMING event with ID <%d> and name \"%s\" occurred ###########", pEvent, tEventName);
    if (tCurrentMessage != NULL)
    {
        tAddressDescriptor = (SocketAddressDescriptor *)msg_addrinfo(tCurrentMessage)->ai_addr;
        if (!Socket::GetAddrFromDescriptor(tAddressDescriptor, tSourceIp, tSourcePort))
            LOGEX(SIP, LOG_ERROR, "Could not determine SIP apcket's source");
        else
            LOGEX(SIP, LOG_INFO, "SIP-Network source: [%s]:%u", tSourceIp.c_str(), tSourcePort);
    }
    LOGEX(SIP, LOG_INFO, "Handle: 0x%lx", (unsigned long)pNuaHandle);
    LOGEX(SIP, LOG_INFO, "Lib-Status: \"%s\"(%d)", pPhrase, pStatus);
    PrintSipHeaderInfo(tRemote, tLocal, pSip);
    switch (pEvent)
    {
            /*################################################################
                Error indication.

                Will be sent when an internal error happened or an error
                occurred while responding a request.

                Parameters:
                    status  SIP status code or NUA status code (>= 900)
                            describing the problem
                    phrase  a short textual description of status code
                    nh      NULL or operation handle associated with the call
                    hmagic  NULL or operation magic associated with the call
                    sip     NULL
                    tags    empty or error specific information
             */
            case nua_i_error:
                SipReceivedError(tRemote, tLocal, pNuaHandle, pStatus, pPhrase, pSip, tSourceIp, tSourcePort);
                break;
            /*################################################################
                Incoming call INVITE.

                Parameters:
                    status  statuscode of response sent automatically by stack
                    phrase  a short textual description of status code
                    nh      operation handle associated with this call (maybe
                            created for this call)
                    hmagic  application context associated with this call
                            (maybe NULL if call handle was created for this call)
                    sip     incoming INVITE request
                    tags    SOATAG_ACTIVE_AUDIO()
                            SOATAG_ACTIVE_VIDEO()

                Responding to INVITE with nua_respond()
             */
            case nua_i_invite:
                SipReceivedCall(tRemote, tLocal, pNuaHandle, pSip, pTags, tSourceIp, tSourcePort);
                break;
            /*################################################################
                Incoming INVITE has been cancelled.

                Parameters:
                    status  status code of response to CANCEL sent
                            automatically by stack
                    phrase  a short textual description of status code
                    nh      operation handle associated with the call
                    hmagic  application context associated with the call
                    sip     incoming CANCEL request
                    tags    empty
            */
            case nua_i_cancel:
                SipReceivedCallCancel(tRemote, tLocal, pNuaHandle, pSip, tSourceIp, tSourcePort);
                break;
            /*################################################################
                Final response to INVITE has been ACKed.

                Note:
                    This event is only sent after 2XX response.

                Parameters:
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     incoming ACK request
                        tags    empty
             */
            case nua_i_ack:
                LOGEX(SIP, LOG_INFO, "No further processing for this event");
                break;
            /*################################################################
                Outgoing call has been forked.

                This is sent when an INVITE request is answered with multiple
                2XX series responses.

                Parameters:
                        status  response status code
                        phrase  a short textual description of status code
                        nh      operation handle associated with the original
                                call
                        hmagic  operation magic associated with the original
                                call
                        sip     preliminary or 2XX response to INVITE
                        tags    NUTAG_HANDLE() of the new forked call
            */
            case nua_i_fork:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                A call has been activated.

                This event will be sent after a succesful response to the
                initial INVITE has been received and the media has been
                activated.

                Parameters:
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     NULL
                        tags    SOATAG_ACTIVE_AUDIO()
                                SOATAG_ACTIVE_VIDEO()
                                SOATAG_ACTIVE_IMAGE()
                                SOATAG_ACTIVE_CHAT()
             */
            case nua_i_active:
                LOGEX(SIP, LOG_INFO, "No further processing for this event");
                break;
            /*################################################################
                A call has been terminated.

                This event will be sent after a call has been terminated. A
                call is terminated, when 1) an error response (300..599) is
                sent to an incoming initial INVITE 2) a reliable response
                (200..299 or reliable preliminary response) to an incoming
                initial INVITE is not acknowledged with ACK or PRACK 3) BYE
                is received or sent

                Parameters:
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     NULL
                        tags    empty
             */
            case nua_i_terminated:
                SipReceivedCallTermination(tRemote, tLocal, pNuaHandle, pSip, tSourceIp, tSourcePort);
                break;
            /*################################################################
                Call state has changed.

                This event will be sent whenever the call state changes.

                In addition to basic changes of session status indicated with
                enum nua_callstate, the RFC 3264 SDP Offer/Answer negotiation
                status is also included. The tags NUTAG_OFFER_RECV() or NUTAG_
                ANSWER_RECV() indicate whether the remote SDP that was received
                was considered as an offer or an answer. Tags NUTAG_OFFER_SENT()
                or NUTAG_ANSWER_SENT() indicate whether the local SDP which was
                sent was considered as an offer or answer.

                If the soa SDP negotiation is enabled (by default or with NUTAG_
                MEDIA_ENABLE(1)), the received remote SDP is included in tags
                SOATAG_REMOTE_SDP() and SOATAG_REMOTE_SDP_STR(). The SDP
                negotiation result from soa is included in the tags SOATAG_
                LOCAL_SDP() and SOATAG_LOCAL_SDP_STR().

                SOATAG_ACTIVE_AUDIO() and SOATAG_ACTIVE_VIDEO() are informational
                tags used to indicate what is the status of audio or video.

                Note that nua_i_state also covers the information relayed in call
                establisment (nua_i_active) and termination (nua_i_terminated)
                events.

                Parameters:
                        status  protocol status code
                                (always present)
                        phrase  short description of status code
                                (always present)
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     NULL
                        tags    NUTAG_CALLSTATE()
                                SOATAG_LOCAL_SDP()
                                SOATAG_LOCAL_SDP_STR()
                                NUTAG_OFFER_SENT()
                                NUTAG_ANSWER_SENT()
                                SOATAG_REMOTE_SDP()
                                SOATAG_REMOTE_SDP_STR()
                                NUTAG_OFFER_RECV()
                                NUTAG_ANSWER_RECV()
                                SOATAG_ACTIVE_AUDIO()
                                SOATAG_ACTIVE_VIDEO()
                                SOATAG_ACTIVE_IMAGE()
                                SOATAG_ACTIVE_CHAT()
            */
            case nua_i_state:
                SipReceivedCallStateChange(tRemote, tLocal, pNuaHandle, pSip, pTags, tSourceIp, tSourcePort);
                break;
            /*################################################################
                Status from outbound processing.

                Parameters:
                        status  SIP status code or NUA status code (>= 900)
                                describing the outbound state
                        phrase  a short textual description of status code
                        nh      operation handle associated with the outbound
                                engine
                        hmagic  application context associated with the handle
                        sip     NULL or response message to an keepalive message
                                or registration probe (error code and message
                                are in status an phrase parameters)
                        tags    empty
            */
            case nua_i_outbound:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Incoming BYE call hangup.

                Parameters:
                        status  statuscode of response sent automatically by
                                stack
                        phrase  a short textual description of status code
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     pointer to BYE request
                        tags    empty
             */
            case nua_i_bye:
                SipReceivedCallHangup(tRemote, tLocal, pNuaHandle, pSip, tSourceIp, tSourcePort);
                break;
            /*################################################################
                Incoming OPTIONS.

                The user-agent should respond to an OPTIONS request with the
                same statuscode as it would respond to an INVITE request.

                Stack responds automatically to OPTIONS request unless OPTIONS
                is included in the set of application methods, set by NUTAG_
                APPL_METHOD().

                The OPTIONS request does not create a dialog. Currently the
                processing of incoming OPTIONS creates a new handle for each
                incoming request which is not assiciated with an existing
                dialog. If the handle nh is not bound, you should probably
                destroy it after responding to the OPTIONS request.

                Parameters:
                        status  status code of response sent automatically by
                                stack
                        phrase  a short textual description of status code
                        nh      operation handle associated with the OPTIONS
                                request
                        hmagic  application context associated with the call
                                (NULL if outside session)
                        sip     incoming OPTIONS request
                        tags    empty
             */
            case nua_i_options :
                LOGEX(SIP, LOG_INFO, "No further processing of this event, SIP stack will automatically answer the request");
                break;
            /*################################################################
                Incoming REFER call transfer.

                The tag list will contain tag NUTAG_REFER_EVENT() with the
                Event header constructed from the REFER request. It will also
                contain the SIPTAG_REFERRED_BY() tag with the Referred-By
                header containing the identity of the party sending the REFER.
                The Referred-By structure contained in the tag is constructed
                from the From header if the Referred-By header was not present
                in the REFER request.

                The application can let the nua to send NOTIFYs from the call
                it initiates with nua_invite() if it includes in the nua_invite()
                arguments both the NUTAG_NOTIFY_REFER() with the handle with
                which nua_i_refer was received and the NUTAG_REFER_EVENT()
                from nua_i_refer event tags.

                Parameters:
                        status  status code of response sent automatically by
                                stack
                        phrase  a short textual description of status code
                        nh      operation handle associated with the incoming
                                request
                        hmagic  application context associated with the handle
                                (NULL if outside of an already established
                                session)
                        sip     incoming REFER request
                        tags    NUTAG_REFER_EVENT()
                                SIPTAG_REFERRED_BY()
            */
            case nua_i_refer:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Incoming PUBLISH.

                In order to receive nua_i_publish events, the application must
                enable both the PUBLISH method with NUTAG_ALLOW() tag and the
                acceptable SIP events with nua_set_params() tag NUTAG_ALLOW_
                EVENTS().

                The nua_response() call responding to a PUBLISH request must
                have NUTAG_WITH() (or NUTAG_WITH_THIS()/NUTAG_WITH_SAVED())
                tag. Note that a successful response to PUBLISH MUST include
                Expires and SIP-ETag headers.

                The PUBLISH request does not create a dialog. Currently the
                processing of incoming PUBLISH creates a new handle for each
                incoming request which is not assiciated with an existing
                dialog. If the handle nh is not bound, you should probably
                destroy it after responding to the PUBLISH request.

                Parameters:
                        status  status code of response sent automatically by
                                stack
                        phrase  a short textual description of status code
                        nh      operation handle associated with the incoming
                                request
                        hmagic  application context associated with the call
                                (usually NULL)
                        sip     incoming PUBLISH request
                        tags    empty
            */
            case nua_i_publish:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Incoming PRACK.

                PRACK request is used to acknowledge reliable preliminary
                responses and it is usually sent automatically by the nua stack.

                Parameters:
                        status  status code of response sent automatically by
                                stack
                        phrase  a short textual description of status code
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     incoming PRACK request
                        tags    empty
             */
            case nua_i_prack:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Incoming session INFO.

                Parameters:
                        status  statuscode of response sent automatically by
                                stack
                        phrase  a short textual description of status code
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     incoming INFO request
                        tags    empty             */
            case nua_i_info:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Incoming session UPDATE.

                Parameters:
                        status  statuscode of response sent automatically by
                                stack
                        phrase  a short textual description of status code
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     incoming UPDATE request
                        tags    empty
            */
            case nua_i_update:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Incoming MESSAGE.

                The MESSAGE request does not create a dialog. If the incoming
                MESSAGE request is not assiciated with an existing dialog the
                stack creates a new handle for it. If the handle nh is not
                bound, you should probably destroy it after responding to the
                MESSAGE request.

                Parameters:
                        status  status code of response sent automatically by
                                stack
                        phrase  a short textual description of status code
                        nh      operation handle associated with the message
                        hmagic  application context associated with the handle
                                (maybe NULL if outside session)
                        sip     incoming MESSAGE request
                        tags    empty
            */
            case nua_i_message:
                SipReceivedMessage(tRemote, tLocal, pNuaHandle, pSip, tSourceIp, tSourcePort);
                break;
            /*################################################################
                Incoming chat MESSAGE.

                Parameters:
                        nh      operation handle associated with the message
                        hmagic  operation magic associated with the handle
                        sip     incoming chat message
                        tags    empty
            */
            case nua_i_chat:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Incoming SUBSCRIBE.

                SUBSCRIBE request is used to query SIP event state or establish
                a SIP event subscription.

                Parameters:
                        status  status code of response sent automatically by
                                stack
                        phrase  response phrase sent automatically by stack
                        nh      operation handle associated with the incoming
                                request
                        hmagic  application context associated with the handle
                                (NULL when handle is created by the stack)
                        sip     SUBSCRIBE request headers
                        tags    NUTAG_SUBSTATE()

                Initial SUBSCRIBE requests are dropped with 489 Bad Event
                response, unless the application has explicitly included the
                Event in the list of allowed events with nua_set_params() tag
                NUTAG_ALLOW_EVENTS() (or SIPTAG_ALLOW_EVENTS() or SIPTAG_ALLOW_
                EVENTS_STR()).

                If the event has been allowed the application can decide whether
                to accept the SUBSCRIBE request or reject it. The nua_response()
                call responding to a SUBSCRIBE request must have NUTAG_WITH()
                (or NUTAG_WITH_THIS()/NUTAG_WITH_SAVED()) tag.

                If the application accepts the SUBSCRIBE request, it must
                immediately send an initial NOTIFY establishing the dialog. This
                is because the response to the SUBSCRIBE request may be lost by
                an intermediate proxy because it had forked the SUBSCRIBE request.

                SUBSCRIBE requests modifying (usually refreshing or terminating)
                an existing event subscription are accepted by default and a
                200 OK response along with a copy of previously sent NOTIFY is
                sent automatically to the subscriber.

                By default, only event subscriptions accepted are those created
                implicitly by REFER request. See nua_i_refer how the application
                must handle the REFER requests.

                Subscription Lifetime and Terminating Subscriptions

                Accepting the SUBSCRIBE request creates a dialog with a notifier
                dialog usage on the handle. The dialog usage is active, until
                the subscriber terminates the subscription, it times out or the
                application terminates the usage with nua_notify() call containing
                the tag NUTAG_SUBSTATE(nua_substate_terminated) or Subscription-
                State header with state "terminated" and/or expiration time 0.

                When the subscriber terminates the subscription, the application
                is notified of an termination by a nua_i_subscribe event with
                NUTAG_SUBSTATE(nua_substate_terminated) tag. When the subscription
                times out, nua automatically initiates a NOTIFY transaction. When
                it is terminated, the application is sent a nua_r_notify event
                with NUTAG_SUBSTATE(nua_substate_terminated) tag.
            */
            case nua_i_subscribe:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Incoming subscription to be authorized.

                This event is launched by nua_notifier() to inform application
                of the current state of the subscriber. The subscriber state
                is included in the NUTAG_SUBSTATE() tag. If the state is nua_
                substate_pending or nua_substate_embryonic, application should
                to authorize the subscriber with nua_authorize().

                Parameters:
                        nh      operation handle associated with the notifier
                        hmagic  operation magic
                        status  statuscode of response sent automatically by
                                stack
                        sip     incoming SUBSCRIBE request
                        tags    NEATAG_SUB()
                                NUTAG_SUBSTATE()
            */
            case nua_i_subscription:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Incoming event NOTIFY.

                Parameters:
                        status  statuscode of response sent automatically by
                                stack
                        phrase  a short textual description of status code
                        nh      operation handle associated with the
                                subscription
                        hmagic  application context associated with the handle
                        sip     incoming NOTIFY request
                        tags    NUTAG_SUBSTATE() indicating the subscription
                                state
            */
            case nua_i_notify:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Incoming, unknown method.

                The extension request does not create a dialog. If the incoming
                request was not assiciated with an existing dialog the stack
                creates a new handle for it. If the handle nh is not bound,
                you should probably destroy it after responding to the request.

                Parameters:
                        status  status code of response sent automatically by
                                stack
                        phrase  a short textual description of status code
                        nh      operation handle associated with the method
                        hmagic  application context associated with the handle
                                (maybe NULL if outside session)
                        sip     headers in incoming request (see also nua_
                                current_request())
                        tags    NUTAG_METHOD()

                The extension method name is in sip->sip_request->rq_method_name,
                too.

                Note:
                    If the status is < 200, it is up to application to respond
                    to the request with nua_respond(). If the handle is
                    destroyed, the stack returns a 500 Internal Server Error
                    response to any unresponded request.
            */
            case nua_i_method:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Offer-answer error indication.

                This may be sent after an SOA operation has failed while
                processing incoming or outgoing call.

                Parameters:
                        status  SIP status code or NUA status code (>= 900)
                                describing the problem
                        phrase  a short textual description of status code
                        nh      operation handle associated with the call
                        hmagic  operation magic associated with this handle
                                (maybe NULL if call handle was created for
                                this call)
                        sip     NULL
                        tags    empty
            */
            case nua_i_media_error:
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
            /*################################################################
                Answer to nua_set_params() or nua_get_hparams().

                Parameters:
                        status  200 when successful, error code otherwise
                        phrase  a short textual description of status code
                        nh      NULL when responding to nua_set_params(),
                                operation handle when responding to
                                nua_set_hparams()
                        hmagic  NULL when responding to nua_set_params(),
                                application contact associated with the
                                operation handle when responding to
                                nua_set_hparams()
                        sip     NULL
                        tags    None
            */
            case nua_r_set_params:
                LOGEX(SIP, LOG_INFO, "No further processing for this event");
                break;
            /*################################################################
                nua_r_get_params        Answer to nua_get_params() or
                                        nua_get_hparams().

                Parameters:
                        status  200 when succesful, error code otherwise
                        phrase  a short textual description of status code
                        nh      NULL when responding to nua_get_params(),
                                operation handle when responding to
                                nua_get_hparams()
                        hmagic  NULL when responding to nua_get_params(),
                                application contact associated with the
                                operation handle when responding to
                                nua_get_hparams()
                        sip     NULL
                        tags    NUTAG_APPL_METHOD()
                                NUTAG_AUTH_CACHE()
                                NUTAG_AUTOACK()
                                NUTAG_AUTOALERT()
                                NUTAG_AUTOANSWER()
                                NUTAG_CALLEE_CAPS()
                                NUTAG_DETECT_NETWORK_UPDATES()
                                NUTAG_EARLY_ANSWER()
                                NUTAG_EARLY_MEDIA()
                                NUTAG_ENABLEINVITE()
                                NUTAG_ENABLEMESSAGE()
                                NUTAG_ENABLEMESSENGER()
                                NUTAG_INITIAL_ROUTE()
                                NUTAG_INITIAL_ROUTE_STR()
                                NUTAG_INSTANCE()
                                NUTAG_INVITE_TIMER()
                                NUTAG_KEEPALIVE()
                                NUTAG_KEEPALIVE_STREAM()
                                NUTAG_MAX_SUBSCRIPTIONS()
                                NUTAG_MEDIA_ENABLE()
                                NUTAG_MEDIA_FEATURES()
                                NUTAG_MIN_SE()
                                NUTAG_M_DISPLAY()
                                NUTAG_M_FEATURES()
                                NUTAG_M_PARAMS()
                                NUTAG_M_USERNAME()
                                NUTAG_ONLY183_100REL()
                                NUTAG_OUTBOUND()
                                NUTAG_PATH_ENABLE()
                                NUTAG_REFER_EXPIRES()
                                NUTAG_REFER_WITH_ID()
                                NUTAG_REFRESH_WITHOUT_SDP()
                                NUTAG_REGISTRAR()
                                NUTAG_RETRY_COUNT()
                                NUTAG_SERVICE_ROUTE_ENABLE()
                                NUTAG_SESSION_REFRESHER()
                                NUTAG_SESSION_TIMER()
                                NUTAG_SMIME_ENABLE()
                                NUTAG_SMIME_KEY_ENCRYPTION()
                                NUTAG_SMIME_MESSAGE_DIGEST()
                                NUTAG_SMIME_MESSAGE_ENCRYPTION()
                                NUTAG_SMIME_OPT()
                                NUTAG_SMIME_PROTECTION_MODE()
                                NUTAG_SMIME_SIGNATURE()
                                NUTAG_SOA_NAME()
                                NUTAG_SUBSTATE()
                                NUTAG_SUB_EXPIRES()
                                NUTAG_UPDATE_REFRESH()
                                NUTAG_USER_AGENT()
                                SIPTAG_ALLOW()
                                SIPTAG_ALLOW_STR()
                                SIPTAG_ALLOW_EVENTS()
                                SIPTAG_ALLOW_EVENTS_STR()
                                SIPTAG_FROM()
                                SIPTAG_FROM_STR()
                                SIPTAG_ORGANIZATION()
                                SIPTAG_ORGANIZATION_STR()
                                SIPTAG_SUPPORTED()
                                SIPTAG_SUPPORTED_STR()
                                SIPTAG_USER_AGENT()
                                SIPTAG_USER_AGENT_STR()
            */
            /*################################################################
                Answer to nua_shutdown().

                Status codes

                    * 100 shutdown started
                    * 101 shutdown in progress (sent when shutdown has been progressed)
                    * 200 shutdown was successful
                    * 500 shutdown timeout after 30 sec

                Parameters:
                        status  shutdown status code
                        nh      NULL
                        hmagic  NULL
                        sip     NULL
                        tags    empty
            */
            case nua_r_shutdown:
                SipReceivedShutdownResponse(tRemote, tLocal, pNuaHandle, pStatus, pSip, tSourceIp, tSourcePort);
                break;
            /*################################################################
                nua_r_notifier  Answer to nua_notifier().

                Parameters:
                        nh      operation handle associated with the call
                        hmagic  operation magic associated with the call
                        sip     NULL
                        tags    SIPTAG_EVENT()
                                SIPTAG_CONTENT_TYPE()
            //################################################################
                nua_r_terminate         Answer to nua_terminate().

                Parameters:
                        nh      operation handle associated with the notifier
                        hmagic  operation magic associated with the notifier
                        sip     NULL
                        tags    empty
            //################################################################
                nua_r_authorize         Answer to nua_authorize().
            //################################################################
                nua_r_register  Answer to outgoing REGISTER.

                The REGISTER may be sent explicitly by nua_register() or
                implicitly by NUA state machines.

                When REGISTER request has been restarted the status may be 100
                even while the real response status returned is different.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the registration
                        hmagic  application context associated with the
                                registration
                        sip     response message to REGISTER request or NULL
                                upon an error (status code is in status and
                                descriptive message in phrase parameters)
                        tags    empty
            */
            case nua_r_register:
                SipReceivedRegisterResponse(tRemote, tLocal, pNuaHandle, pStatus, pPhrase, pSip, tSourceIp, tSourcePort);
                break;
            /*################################################################
                nua_r_unregister        Answer to outgoing un-REGISTER.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the registration
                        hmagic  application context associated with the
                                registration
                        sip     response message to REGISTER request or NULL
                                upon an error (status code is in status and
                                descriptive message in phrase parameters)
                        tags    empty
            */
            /*################################################################
                Answer to outgoing INVITE.
            */
            case nua_r_invite:
                SipReceivedCallResponse(tRemote, tLocal, pNuaHandle, pStatus, pPhrase, pSip, pTags, tSourceIp, tSourcePort);
                break;
            /*################################################################
                nua_r_cancel    Answer to outgoing CANCEL.

                The CANCEL may be sent explicitly by nua_cancel() or implicitly
                by NUA state machine.

                Parameters:
                        status  response status code
                        phrase  a short textual description of status code
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     response to CANCEL request or NULL upon an
                                error (status code is in status and descriptive
                                message in phrase parameters)
                        tags    empty
            //################################################################
                nua_r_bye       Answer to outgoing BYE.

                The BYE may be sent explicitly by nua_bye() or implicitly by
                NUA state machine.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     response to BYE request or NULL upon an error
                                (status code is in status and descriptive
                                message in phrase parameters)
                        tags    empty
            */
            case nua_r_bye:
                SipReceivedCallHangupResponse(tRemote, tLocal, pNuaHandle, pStatus, pSip, tSourceIp, tSourcePort);
                break;
            /*################################################################
                nua_r_options   Answer to outgoing OPTIONS.

                Parameters:
                        status  response status code (if the request is retried
                                the status is 100 and the sip->sip_status->
                                st_status contain the real status code from the
                                response message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the incoming
                                OPTIONS request
                        hmagic  application context associated with the handle
                        sip     response to OPTIONS request or NULL upon an
                                error (status code is in status and descriptive
                                message in phrase parameters)
                        tags    empty
            */
            case nua_r_options:
                SipReceivedOptionsResponse(tRemote, tLocal, pNuaHandle, pStatus, pSip, tSourceIp, tSourcePort);
                break;
            /*################################################################
                nua_r_refer     Answer to outgoing REFER.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the REFER
                                request
                        hmagic  application context associated with the handle
                        sip     response to REFER request or NULL upon an error
                                (status code is in status and descriptive
                                message in phrase parameters)
                        tags    NUTAG_REFER_EVENT()
                                NUTAG_SUBSTATE()
            */
            /*################################################################
                nua_r_publish   Answer to outgoing PUBLISH.

                The PUBLISH request may be sent explicitly by nua_publish() or
                implicitly by NUA state machine.

                Parameters:
                        status  status code of PUBLISH request (if the request
                                is retried, status is 100, the sip->sip_status->
                                st_status contain the real status code from
                                the response message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the publication
                        hmagic  application context associated with the handle
                        sip     response to PUBLISH request or NULL upon an
                                error (status code is in status and descriptive
                                message in phrase parameters)
                        tags    empty
            */
            case nua_r_publish:
                SipReceivedPublishResponse(tRemote, tLocal, pNuaHandle, pStatus, pPhrase, pSip, tSourceIp, tSourcePort);
                break;
            /*################################################################
                nua_r_unpublish         Answer to outgoing un-PUBLISH.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the publication
                        hmagic  application context associated with the handle
                        sip     response to PUBLISH request or NULL upon an
                                error (status code is in status and descriptive
                                message in phrase parameters)
                        tags    empty
            //################################################################
                nua_r_info      Answer to outgoing INFO.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     response to INFO or NULL upon an error (status
                                code is in status and descriptive message in
                                phrase parameters)
                        tags    empty
            //################################################################
                nua_r_prack     Answer to outgoing PRACK.

                PRACK request is used to acknowledge reliable preliminary
                responses and it is usually sent automatically by the nua stack.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     response to PRACK or NULL upon an error (status
                                code is in status and descriptive message in
                                phrase parameters)
                        tags    empty
            //################################################################
                nua_r_update    Answer to outgoing UPDATE.

                The UPDATE may be sent explicitly by nua_update() or implicitly
                by NUA state machine.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the call
                        hmagic  application context associated with the call
                        sip     response to UPDATE request or NULL upon an
                                error (status code is in status and descriptive
                                message in phrase parameters)
                        tags    empty
            //################################################################
                nua_r_message   Answer to outgoing MESSAGE.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the message
                        hmagic  application context associated with the handle
                        sip     response to MESSAGE request or NULL upon an
                                error (status code is in status and descriptive
                                message in phrase parameters)
                        tags    empty
            */
            case nua_r_message:
                SipReceivedMessageResponse(tRemote, tLocal, pNuaHandle, pStatus, pSip, tSourceIp, tSourcePort);
                break;

            /*################################################################
                nua_r_chat      Answer to outgoing chat message.

                Parameters:
                        nh      operation handle associated with the notifier
                        hmagic  operation magic associated with the notifier
                        sip     response to MESSAGE request or NULL upon an
                                error (error code and message are in status
                                and phrase parameters)
                        tags    empty
            //################################################################
                nua_r_subscribe         Answer to outgoing SUBSCRIBE.

                The SUBSCRIBE request may have been sent explicitly by
                nua_subscribe() or implicitly by NUA state machine.

                Parameters:
                        status  response status code (if the request is
                                retried, status is 100, the sip->sip_status->
                                st_status contain the real status code from
                                the response message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the subscription
                        hmagic  application context associated with the handle
                        sip     response to SUBSCRIBE request or NULL upon an
                                error (status code is in status and descriptive
                                message in phrase parameters)
                        tags    NUTAG_SUBSTATE()
            //################################################################
                nua_r_unsubscribe       Answer to outgoing un-SUBSCRIBE.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the subscription
                        hmagic  application context associated with the handle
                        sip     response to SUBSCRIBE request or NULL upon an
                                error (status code is in status and descriptive
                                message in phrase parameters)
                        tags    NUTAG_SUBSTATE()
            //################################################################
                nua_r_notify    Answer to outgoing NOTIFY.

                The NOTIFY may be sent explicitly by nua_notify() or implicitly
                by NUA state machine. Implicit NOTIFY is sent when an established
                dialog is refreshed by client or it is terminated (either by
                client or because of a timeout).

                The current subscription state is included in NUTAG_SUBSTATE()
                tag. The nua_substate_terminated indicates that the subscription
                is terminated, the notifier usage has been removed and when
                there was no other usages of the dialog the dialog state is
                also removed.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                message, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the subscription
                        hmagic  application context associated with the handle
                        sip     response to NOTIFY request or NULL upon an
                                error (status code is in status and descriptive
                                message in phrase parameters)
                        tags    NUTAG_SUBSTATE() indicating subscription state
                                SIPTAG_EVENT() indicating subscription event
            //################################################################
                nua_r_method    Answer to unknown outgoing method.

                Parameters:
                        status  response status code (if the request is retried,
                                status is 100, the sip->sip_status->st_status
                                contain the real status code from the response
                                method, e.g., 302, 401, or 407)
                        phrase  a short textual description of status code
                        nh      operation handle associated with the method
                        hmagic  application context associated with the handle
                        sip     response to the extension request or NULL upon
                                an error (status code is in status and
                                descriptive method in phrase parameters)
                        tags    empty
            //################################################################
                nua_r_authenticate      Answer to nua_authenticate().

                Under normal operation, this event is never sent but rather
                the unauthenticated operation is completed. However, if there
                is no operation to authentication or if there is an
                authentication error the nua_r_authenticate event is sent to
                the application with the status code as follows:

                    * 202 No operation to restart:
                      The authenticator associated with the handle was updated,
                      but there was no operation to retry with the new
                      credentials.
                    * 900 Cannot add credentials:
                      There was internal problem updating authenticator.
                    * 904 No matching challenge:
                      There was no challenge matching with the credentials
                      provided by nua_authenticate(), e.g., their realm did
                      not match with the one received with the challenge.

                Parameters:
                        status  status code from authentication
                        phrase  a short textual description of status code
                        nh      operation handle authenticated
                        hmagic  application context associated with the handle
                        sip     NULL
                        tags    empty
            */
            case nua_r_authenticate:
                SipReceivedAuthenticationResponse(tRemote, tLocal, pNuaHandle, pStatus, tSourceIp, tSourcePort);
                break;

            /*################################################################
                nua_i_network_changed   Local IP(v6) address has changed.
            //################################################################
                nua_i_register  Incoming REGISTER.
            */
            default:
                LOGEX(SIP, LOG_INFO, "Event ignored");
                LOGEX(SIP, LOG_INFO, "FUNCTION NOT IMPLEMENTED YET");
                break;
    }
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////// HELPER /////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SIP::PrintSipHeaderInfo(const sip_to_t *pRemote, const sip_to_t *pLocal, sip_t const *pSip)
{
    if (pRemote)
        LOG(LOG_INFO, "From: %s %s %s@%s:%s %s %s",
                                        pRemote->a_display,
                                        pRemote->a_url->url_user,
                                        pRemote->a_url->url_password,
                                        pRemote->a_url->url_host,
                                        pRemote->a_url->url_port,
                                        pRemote->a_url->url_path,
                                        pRemote->a_comment);
    if (pLocal)
        LOG(LOG_INFO, "To: %s %s %s@%s:%s %s %s",
                                        pLocal->a_display,
                                        pLocal->a_url->url_user,
                                        pLocal->a_url->url_password,
                                        pLocal->a_url->url_host,
                                        pLocal->a_url->url_port,
                                        pLocal->a_url->url_path,
                                        pLocal->a_comment);
    if (pSip)
    {
        if (pSip->sip_via)
            LOG(LOG_INFO, "Via: %s:%s", pSip->sip_via->v_host, pSip->sip_via->v_port);
        if (pSip->sip_route)
            LOG(LOG_INFO, "Route: %s@[%s]:%s", pSip->sip_route->r_url->url_user, pSip->sip_route->r_url->url_host, pSip->sip_route->r_url->url_port);
        if (pSip->sip_record_route)
            LOG(LOG_INFO, "Record route: %s@[%s]:%s", pSip->sip_record_route->r_url->url_user, pSip->sip_record_route->r_url->url_host, pSip->sip_record_route->r_url->url_port);
        if (pSip->sip_request)
        {
            LOG(LOG_INFO, "Request of type \"%s\"(ID %d)", pSip->sip_request->rq_method_name, (int)pSip->sip_request->rq_method);
            LOG(LOG_INFO, "Request protocol version: %s", pSip->sip_request->rq_version);
        }
        if (pSip->sip_status)
            LOG(LOG_INFO, "Status: \"%s\"(%d)", pSip->sip_status->st_phrase, pSip->sip_status->st_status);
        if (pSip->sip_server)
        {
            LOG(LOG_INFO, "Remote server: \"%s\"", pSip->sip_server->g_string);
            if (pSip->sip_server->g_string != NULL)
                mSipRegisterServerSoftwareId = toString(pSip->sip_server->g_string);
        }
        if (pSip->sip_user_agent)
            LOG(LOG_INFO, "Remote client: %s", pSip->sip_user_agent->g_string);
        if (pSip->sip_error)
            LOG(LOG_ERROR, "Erroneous header \"%s\" detected", pSip->sip_error->er_name);
        if (pSip->sip_subscription_state)
        {
            LOG(LOG_INFO, "SuscriptionState: %s", pSip->sip_subscription_state->ss_substate);
            LOG(LOG_INFO, "SuscriptionParams: %s", pSip->sip_subscription_state->ss_params);
            LOG(LOG_INFO, "SuscriptionTermReason: %s", pSip->sip_subscription_state->ss_reason);
            LOG(LOG_INFO, "SuscriptionLifetime: %s", pSip->sip_subscription_state->ss_expires);
        }
        if (pSip->sip_www_authenticate)
        {
            LOG(LOG_INFO, "ServerAuthScheme: %s", pSip->sip_www_authenticate->au_scheme);
            LOG(LOG_INFO, "ServerAuthParams: %s", pSip->sip_www_authenticate->au_params);
        }
        if (pSip->sip_proxy_authenticate)
        {
            LOG(LOG_INFO, "ProxyAuthScheme: %s", pSip->sip_proxy_authenticate->au_scheme);
            LOG(LOG_INFO, "ProxyAuthParams: %s", pSip->sip_proxy_authenticate->au_params);
        }
        if(pSip->sip_authentication_info)
            LOG(LOG_INFO, "AuthInfo: %s", pSip->sip_authentication_info->ai_params);
    }
}

void SIP::printFromToSendingSipEvent(nua_handle_t *pNuaHandle, GeneralEvent *pEvent, string pEventName)
{
    LOG(LOG_INFO, "Sending%sFrom: %s", pEventName.c_str(), pEvent->Sender.c_str());
    LOG(LOG_INFO, "Sending%sTo: %s", pEventName.c_str(), pEvent->Receiver.c_str());
    LOG(LOG_INFO, "Sending%sHandle: 0x%lx", pEventName.c_str(), (unsigned long)pNuaHandle);
}

void SIP::initParticipantTriplet(const sip_to_t *pRemote, sip_t const *pSip, string &pSourceIp, unsigned int pSourcePort, string &pUser, string &pHost, string &pPort)
{
    pUser = pRemote->a_url->url_user;
    pHost = string(pRemote->a_url->url_host);

    /*
     * use source IP from network layer if:
     *              * source IP is valid
     *              * source from SIP header is not a DNS name (means the SIP header is not from a SIP server (PBX box))
     */

    if ((pSourceIp.size()) && (pSourceIp != "0.0.0.0") && (pSourceIp != "::") && (!IsLetter(&pHost[0])))
    {
        pHost = pSourceIp;
    }else
    {
        pSourceIp = pHost;
    }

    if (pRemote->a_url->url_port != NULL)
        pPort = string(pRemote->a_url->url_port);
    else
        pPort = "5060";
    pSourcePort = atoi(pPort.c_str());
}

// function is called for a new incoming call/message request
void SIP::InitGeneralEvent_FromSipReceivedRequestEvent(const sip_to_t *pRemote, const sip_to_t *pLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, GeneralEvent *pEvent, string pEventName, string &pSourceIp, unsigned int pSourcePort)
{
    string      tUser = "", tHost = "", tPort = "";
    string      tParticipant;
    bool        tFound = false;

    initParticipantTriplet(pRemote, pSip, pSourceIp, pSourcePort, tUser, tHost, tPort);

    tParticipant = SipCreateId(tUser, tHost, tPort);

    if (!MEETING.OpenParticipantSession(tUser, tHost, tPort, CALLSTATE_STANDBY))
        LOG(LOG_INFO, "%s-User \"%s\" already known to system", pEventName.c_str(), tParticipant.c_str());

    LOG(LOG_VERBOSE, "Participant is %s", tParticipant.c_str());

    if ( ((pEventName == "Message") && (tFound = MEETING.SearchParticipantAndSetNuaHandleForMsgs(tParticipant, pNuaHandle))) ||
         ((pEventName == "Call") && (tFound = MEETING.SearchParticipantAndSetNuaHandleForCalls(tParticipant, pNuaHandle))) ||
         ((pEventName == "Error")))
    {
        pEvent->Sender = tParticipant;

        pEvent->SenderName = toString(pRemote->a_display);

        pEvent->SenderComment = toString(pRemote->a_url->url_password);

        pEvent->Receiver = SipCreateId(toString(pLocal->a_url->url_user), toString(pLocal->a_url->url_host));
        if (toString(pLocal->a_url->url_port).size())
            pEvent->Receiver += ":" + toString(pLocal->a_url->url_port);
        else
            pEvent->Receiver += ":5060";

        if ((pSip) && (pSip->sip_user_agent))
            pEvent->SenderApplication = toString(pSip->sip_user_agent->g_string);

        pEvent->IsIncomingEvent = true;
    }

    LOG(LOG_INFO, "%s-NewHandle: 0x%lx", pEventName.c_str(), (unsigned long)pNuaHandle);
    LOG(LOG_INFO, "%s-EventSender: \"%s\" %s", pEventName.c_str(), pEvent->SenderName.c_str(), pEvent->Sender.c_str());
    if (pEvent->SenderComment.size())
        LOG(LOG_INFO, "%s-SenderComment: %s", pEventName.c_str(), pEvent->SenderComment.c_str());
    LOG(LOG_INFO, "%s-EventReceiver: %s", pEventName.c_str(), pEvent->Receiver.c_str());
    LOG(LOG_INFO, "%s-EventSendingApp: %s", pEventName.c_str(), pEvent->SenderApplication.c_str());

    #ifdef SIP_ASSERTS
        assert(tFound == true);
    #endif
}

// returns source IP of the response
string SIP::InitGeneralEvent_FromSipReceivedResponseEvent(const sip_to_t *pRemote, const sip_to_t *pLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, GeneralEvent *pEvent, string pEventName, string &pSourceIp, unsigned int pSourcePort)
{
    string      tUser = "", tHost = "", tPort = "";
    bool        tFound = false;

    initParticipantTriplet(pRemote, pSip, pSourceIp, pSourcePort, tUser, tHost, tPort);

    // should there be an already existing session to this participant? -> if not simply set tFound to "true"
    if ((pEventName != "Registration") && (pEventName != "RegistrationFailed") && (pEventName != "Publication") && (pEventName != "PublicationFailed") && (pEventName != "OptionsAccept") && (pEventName != "OptionsUnavailable"))
        tFound = MEETING.SearchParticipantByNuaHandleOrName(tUser, tHost, tPort, pNuaHandle);
    else
        tFound = true;

    if (tFound)
    {
        pEvent->Sender = SipCreateId(tUser, tHost, tPort);

        pEvent->SenderName = toString(pRemote->a_display);

        pEvent->SenderComment = toString(pRemote->a_url->url_password);

        pEvent->Receiver = SipCreateId(toString(pLocal->a_url->url_user), toString(pLocal->a_url->url_host));
        if (pLocal->a_url->url_port != NULL)
            pEvent->Receiver += ":" + toString(pLocal->a_url->url_port);
        else
            pEvent->Receiver += ":5060";

        if (pSip)
        {
            if (pSip->sip_user_agent)
                pEvent->SenderApplication = toString(pSip->sip_user_agent->g_string);
            else
                if (pSip->sip_server)
                    pEvent->SenderApplication = toString(pSip->sip_server->g_string);
        }

        pEvent->IsIncomingEvent = true;
    }

    LOG(LOG_INFO, "%s-EventSender: \"%s\" %s", pEventName.c_str(), pEvent->SenderName.c_str(), pEvent->Sender.c_str());
    if (pEvent->SenderComment.size())
        LOG(LOG_INFO, "%s-SenderComment: %s", pEventName.c_str(), pEvent->SenderComment.c_str());
    LOG(LOG_INFO, "%s-EventReceiver: %s", pEventName.c_str(), pEvent->Receiver.c_str());
    LOG(LOG_INFO, "%s-EventSendingApp: %s", pEventName.c_str(), pEvent->SenderApplication.c_str());

    #ifdef SIP_ASSERTS
        assert(tFound == true);
    #endif

    return tHost;
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////// RECEIVING ////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SIP::SipReceivedError(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    ErrorEvent *tEEvent = new ErrorEvent();

    InitGeneralEvent_FromSipReceivedRequestEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tEEvent, "Error", pSourceIp, pSourcePort);

    nua_handle_destroy(pNuaHandle);

    LOG(LOG_ERROR, "Received an error from \"%s\", state is %d, description is \"%s\"", tEEvent->Sender.c_str(), pStatus, pPhrase);

    tEEvent->StatusCode = pStatus;
    tEEvent->Description = pPhrase;

    MEETING.notifyObservers(tEEvent);
}

void SIP::SipReceivedAuthenticationResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, std::string pSourceIp, unsigned int pSourcePort)
{
    switch(pStatus)
    {
        case 202:
            LOG(LOG_ERROR, "There was no operation to restart, authentication cache was updated");
            break;
        case 900:
            LOG(LOG_ERROR, "Unable to add authentication credentials");
            break;
        case 904:
            LOG(LOG_ERROR, "No matching challenge, the provided realm did not match with the received one");
            break;
        default:
            LOG(LOG_ERROR, "Unsupported status code");
            break;
    }

}

void SIP::SipReceivedMessage(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    MessageEvent *tMEvent = new MessageEvent();

    InitGeneralEvent_FromSipReceivedRequestEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tMEvent, "Message", pSourceIp, pSourcePort);

    nua_handle_destroy(pNuaHandle);

    LOG(LOG_INFO, "MessageType: %s", pSip->sip_content_type->c_type);
    if (pSip->sip_payload)
    {
        LOG(LOG_INFO, "MessageText-length: %d", pSip->sip_payload->pl_len);
        LOG(LOG_INFO, "MessageText: \"%s\"", pSip->sip_payload->pl_data);
        LOG(LOG_INFO, "MessageId: %s", pSip->sip_call_id->i_id);

        // additionally we have to store the message text
        tMEvent->Text = pSip->sip_payload->pl_data;

        MEETING.notifyObservers(tMEvent);
    }else
        LOG(LOG_ERROR, "Message: no message text");
}

void SIP::SipReceivedMessageAccept(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    MessageAcceptEvent *tMAEvent = new MessageAcceptEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tMAEvent, "MessageAccept", pSourceIp, pSourcePort);

    nua_handle_destroy(pNuaHandle);

    MEETING.notifyObservers(tMAEvent);
}

void SIP::SipReceivedMessageAcceptDelayed(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    MessageAcceptDelayedEvent *tMADEvent = new MessageAcceptDelayedEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tMADEvent, "MessageAcceptDelayed", pSourceIp, pSourcePort);

    nua_handle_destroy(pNuaHandle);

    MEETING.notifyObservers(tMADEvent);
}

void SIP::SipReceivedMessageUnavailable(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    MessageUnavailableEvent *tMUEvent = new MessageUnavailableEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tMUEvent, "MessageUnavailable", pSourceIp, pSourcePort);

    nua_handle_destroy(pNuaHandle);

    MEETING.notifyObservers(tMUEvent);
}

void SIP::SipReceivedMessageResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    switch(pStatus)
    {
        case SIP_STATE_OKAY: // the other side accepted the message
            SipReceivedMessageAccept(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
        case SIP_STATE_OKAY_DELAYED_DELIVERY: // message delivery is delay but okay
            SipReceivedMessageAcceptDelayed(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
        case SIP_STATE_PROXY_AUTH_REQUIRED:
            if ((pNuaHandle != NULL) && (GetServerRegistrationState()))
            {
                string tAuthInfo = "Digest:\"" + mSipRegisterServer + "\":" + mSipRegisterUsername + ":" + mSipRegisterPassword;

                LOG(LOG_VERBOSE, "Authentication information for message: %s", tAuthInfo.c_str());

                // set auth. information
                nua_authenticate(pNuaHandle, NUTAG_AUTH(tAuthInfo.c_str()), TAG_END());
            }else
                SipReceivedMessageUnavailable(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
        case 408 ... 599: // user/service unavailable
            //    408 = Timeout
            //    415 = Unsupported media type (linphone)
            //    503 = Service unavailable, e.g. no transport (tried to talk with a SIP server?)
            SipReceivedMessageUnavailable(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
        default:
            LOG(LOG_ERROR, "Unsupported status code");
            break;
    }
}

void SIP::SipReceivedCall(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, void* pTags, string pSourceIp, unsigned int pSourcePort)
{
    int tCallState = nua_callstate_init;
    //char const *tLocalSdp = NULL;
    char const *tRemoteSdp = NULL;
    sip_to_t *to;
    sip_from_t *from;
    tagi_t* tTags = (tagi_t*)pTags;

    tl_gets(tTags,
            NUTAG_CALLSTATE_REF(tCallState),
            //SOATAG_LOCAL_SDP_STR_REF(tLocalSdp),
            SOATAG_REMOTE_SDP_STR_REF(tRemoteSdp),
            TAG_END());

    LOG(LOG_INFO, "CallCallState: %d", tCallState);
    //LOG(LOG_INFO, "CallLocalSdp: %s", tLocalSdp);
    LOG(LOG_INFO, "CallRemoteSdp: %s", tRemoteSdp);
    LOG(LOG_INFO, "CallId: %s", pSip->sip_call_id->i_id);

    CallEvent *tCEvent = new CallEvent ();

    InitGeneralEvent_FromSipReceivedRequestEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tCEvent, "Call", pSourceIp, pSourcePort);

    #ifdef SIP_NAT_SOURCE_ADDRESS_ADAPTION
        // only for IPv4
        if (pSourceIp.find(":") == string::npos)
        {
            // NAT traversal: is network layer based IP address equal to the SIP based IP address?
            if (pSourceIp.compare(pSipRemote->a_url->url_host) != 0)
            {// no, NAT detected!
                delete tCEvent;
                LOG(LOG_INFO, "NAT at remote side detected, trying to set traversal address: %s:%d", pSourceIp.c_str(), pSourcePort);
                char* tAnswer = (char*)malloc(strlen("Decline, NAT:255.255.255.255:65535") + 1);
                sprintf(tAnswer, "Decline, NAT:%s:%u", pSourceIp.c_str(), pSourcePort);
                nua_respond(pNuaHandle, 603, tAnswer, SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), TAG_END());
                nua_handle_destroy (pNuaHandle);
                return;
            }
        }
    #endif

    // NAT traversal: if we were reached via a foreign IP then we are behind a NAT box
    //                followed by this we have to fake our own IP and PORT to the ones from the outer interface from the NAT gateway
    //                this fake address will be used within the SIP response
    if (pSipLocal->a_url->url_port != NULL)
        MEETING.SearchParticipantAndSetOwnContactAddress(tCEvent->Sender, string(pSipLocal->a_url->url_host), (unsigned int)atoi(pSipLocal->a_url->url_port));
    else
        MEETING.SearchParticipantAndSetOwnContactAddress(tCEvent->Sender, string(pSipLocal->a_url->url_host), 5060);

    switch(mAvailabilityState)
    {
        case AVAILABILITY_STATE_NO: // automatically deny this call

                    printFromToSendingSipEvent(pNuaHandle, tCEvent, "CallAutoDeny");
                    nua_respond(pNuaHandle, 603, "Decline", SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), TAG_END());
                    nua_handle_destroy (pNuaHandle);
                    delete tCEvent;
                    break;
        case AVAILABILITY_STATE_YES: // ask user if we should accept this call
                    if (MEETING.SearchParticipantAndSetState(tCEvent->Sender, CALLSTATE_RINGING))
                    {
                        tCEvent->AutoAnswering = false;

                        MEETING.notifyObservers(tCEvent);
                    }
                    break;
        case AVAILABILITY_STATE_YES_AUTO: // automatically accept this call
                    if (MEETING.SearchParticipantAndSetState(tCEvent->Sender, CALLSTATE_RINGING))
                    {
                        tCEvent->AutoAnswering = true;

                        MEETING.notifyObservers(tCEvent);
                    }
                    MEETING.SendCallAccept(tCEvent->Sender);
                    break;
        default:
                    LOG(LOG_ERROR, "AvailabilityState unknown");
                    delete tCEvent;
                    break;
    }
}

void SIP::SipReceivedCallStateChange(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, void* pTags, string pSourceIp, unsigned int pSourcePort)
{
    int tCallState = nua_callstate_init;
    //char const *tLocalSdp = NULL;
    char const *tRemoteSdp = NULL;

    CallMediaUpdateEvent *tCMUEvent = new CallMediaUpdateEvent();
    tCMUEvent->RemoteAudioPort = 0;
    tCMUEvent->RemoteAudioAddress = "";
    tCMUEvent->RemoteAudioCodec = "";
    tCMUEvent->RemoteVideoPort = 0;
    tCMUEvent->RemoteVideoAddress = "";
    tCMUEvent->RemoteVideoCodec = "";

    string tSourceIpStr = InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tCMUEvent, "CallStateChange", pSourceIp, pSourcePort);

    // are there brackets from Sofia-SIP's format of IPv6 addresses? => remove them to deliver correct IPV6 addresses upwards to the user application
    size_t tPos = 0;
    if ((tPos = pSourceIp.find("[")) != string::npos)
        pSourceIp = pSourceIp.substr(1, pSourceIp.length() - 2);

    tagi_t* tTags = (tagi_t*)pTags;
    tl_gets(tTags,
            NUTAG_CALLSTATE_REF(tCallState),
            //SOATAG_LOCAL_SDP_STR_REF(tLocalSdp),
            SOATAG_REMOTE_SDP_STR_REF(tRemoteSdp),
            TAG_END());

    LOG(LOG_INFO, "CallStateChangeCallState: %d", tCallState);
    //LOG(LOG_INFO, "CallStateChangeLocalSdp: %s", tLocalSdp);
    LOG(LOG_INFO, "CallStateChangeRemoteSdp: %s", tRemoteSdp);

    if (tRemoteSdp != NULL)
    {
        sdp_parser_t  *tSdpParsed;
        char const*   tParserError;

        /*
         * flags for sdp_parse:
         * ---------------------
         *    sdp_f_strict              Accept only conforming SDP.
         *    sdp_f_anynet              Accept any network type.
         *    sdp_f_realloc             Reallocate message.
         *    sdp_f_all_rtpmaps         Include well-known rtpmaps in message, too.
         *    sdp_f_print_prefix        Print buffer already contains a valid prefix.
         *    sdp_f_mode_0000           Connection line with INADDR_ANY is considered equal to sendonly.
         *    sdp_f_insane              Don't run sanity check.
         *    sdp_f_c_missing           Don't require c= for each media line.
         *    sdp_f_config              Parse SDP config files.
         *    sdp_f_mode_manual         Do not generate or parse SDP mode.
         *    sdp_f_mode_always         Always generate media-level mode attributes.
         *
         */

        tSdpParsed = sdp_parse(&mSipContext->Home, tRemoteSdp, strlen(tRemoteSdp), sdp_f_insane);
        if ((tParserError = sdp_parsing_error(tSdpParsed)) != NULL)
            LOG(LOG_INFO, "Error parsing remote SDP with result: %s", tParserError);
        else
        {
            sdp_session_t *tSdpSession;

            tSdpSession = sdp_session(tSdpParsed);
            if (tSdpSession == NULL)
                LOG(LOG_INFO, "Error finding SDP session data from parsed SDP data");
            else
            {
                bool tFoundAudioVideo = false;
                sdp_connection_t *tSdpConnection;
                sdp_media_t *tMedia = tSdpSession->sdp_media;

                if (tSdpSession->sdp_origin)
                {
                    LOG(LOG_INFO, "CallStateChange-RemoteOriginator: %s@%s", tSdpSession->sdp_origin->o_username, tSdpSession->sdp_origin->o_address->c_address);
                    LOG(LOG_INFO, "CallStateChange-RemoteConnection: %s", tSdpSession->sdp_connection->c_address);
                }

                while (tMedia != NULL)
                {
                    LOG(LOG_INFO, "CallStateChange-Media type: %s", tMedia->m_type_name);
                    LOG(LOG_INFO, "CallStateChange-Media port: %lu", tMedia->m_port);
                    LOG(LOG_INFO, "CallStateChange-Media port count: %lu", tMedia->m_number_of_ports);
                    LOG(LOG_INFO, "CallStateChange-Media information: %s", tMedia->m_information);
                    LOG(LOG_INFO, "CallStateChange-Transport type: %s", tMedia->m_proto_name);
                    if (tMedia->m_rtpmaps)
                    {
                        LOG(LOG_INFO, "CallStateChange-RtpMap codec: %s", tMedia->m_rtpmaps->rm_encoding);
                        LOG(LOG_INFO, "CallStateChange-RtpMap rate: %ld", tMedia->m_rtpmaps->rm_rate);
                        LOG(LOG_INFO, "CallStateChange-RtpMap params: %s", tMedia->m_rtpmaps->rm_params);
                        LOG(LOG_INFO, "CallStateChange-RtpMap fmtp: %s", tMedia->m_rtpmaps->rm_fmtp);
                        LOG(LOG_INFO, "CallStateChange-RtpMap known entry: %d", tMedia->m_rtpmaps->rm_predef);
                        LOG(LOG_INFO, "CallStateChange-RtpMap payload: %d", tMedia->m_rtpmaps->rm_pt);
                    }
                    tSdpConnection = sdp_media_connections(tMedia);
                    if (tSdpConnection == NULL)
                        LOG(LOG_ERROR, "Error when searching SDP media connections data from SDP session media data");
                    else
                    {
                        switch (tMedia->m_type)
                        {
                            case sdp_media_audio:
                                tCMUEvent->RemoteAudioAddress = tSdpConnection->c_address;
                                tCMUEvent->RemoteAudioPort = tMedia->m_port;
                                if (tMedia->m_rtpmaps)
                                    tCMUEvent->RemoteAudioCodec = string(tMedia->m_proto_name) + "(" + string(tMedia->m_rtpmaps->rm_encoding) + ")";
                                else
                                    tCMUEvent->RemoteAudioCodec = "incompatibly transported. Local transport is " + string(tMedia->m_proto_name);
                                tFoundAudioVideo = true;
                                if (tMedia->m_number_of_ports)
                                    LOG(LOG_INFO, "Remote audio sink for \"%s\" is now at: %s:%u with: %ld ports", tCMUEvent->Sender.c_str(), tCMUEvent->RemoteAudioAddress.c_str(), tCMUEvent->RemoteAudioPort, tMedia->m_number_of_ports);
                                else
                                    LOG(LOG_INFO, "Remote audio sink for \"%s\" is now at: %s:%u", tCMUEvent->Sender.c_str(), tCMUEvent->RemoteAudioAddress.c_str(), tCMUEvent->RemoteAudioPort);
                                break;
                            case sdp_media_video:
                                tCMUEvent->RemoteVideoAddress = tSdpConnection->c_address;
                                tCMUEvent->RemoteVideoPort = tMedia->m_port;
                                if (tMedia->m_rtpmaps)
                                    tCMUEvent->RemoteVideoCodec = string(tMedia->m_proto_name) + "(" + string(tMedia->m_rtpmaps->rm_encoding) + ")";
                                else
                                    tCMUEvent->RemoteVideoCodec = "incompatibly transported. Local transport is " + string(tMedia->m_proto_name);
                                tFoundAudioVideo = true;
                                if (tMedia->m_number_of_ports)
                                    LOG(LOG_INFO, "Remote video sink for \"%s\" is now at: %s:%u with: %ld ports", tCMUEvent->Sender.c_str(), tCMUEvent->RemoteVideoAddress.c_str(), tCMUEvent->RemoteVideoPort, tMedia->m_number_of_ports);
                                else
                                    LOG(LOG_INFO, "Remote video sink for \"%s\" is now at: %s:%u", tCMUEvent->Sender.c_str(), tCMUEvent->RemoteVideoAddress.c_str(), tCMUEvent->RemoteVideoPort);
                                break;
                        }
                    }
                    tMedia = tMedia->m_next;
                }
                if (tFoundAudioVideo)
                {
                    LOG(LOG_VERBOSE, "Audio codec: %s", tCMUEvent->RemoteAudioCodec.c_str());
                    LOG(LOG_VERBOSE, "Video codec: %s", tCMUEvent->RemoteVideoCodec.c_str());
                    MEETING.SearchParticipantAndSetRemoteMediaInformation(tCMUEvent->Sender, tCMUEvent->RemoteVideoAddress, tCMUEvent->RemoteVideoPort, tCMUEvent->RemoteVideoCodec, tCMUEvent->RemoteAudioAddress, tCMUEvent->RemoteAudioPort, tCMUEvent->RemoteAudioCodec);
                    MEETING.notifyObservers(tCMUEvent);
                }
            }
        }
    }
}

void SIP::SipReceivedCallRinging(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    CallRingingEvent *tCREvent = new CallRingingEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tCREvent, "CallRinging", pSourceIp, pSourcePort);

    MEETING.notifyObservers(tCREvent);
}

void SIP::SipReceivedCallAccept(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    if (!MEETING_AUTOACK_CALLS)
    {
        nua_ack(pNuaHandle, TAG_END());
    }

    CallAcceptEvent *tCAEvent = new CallAcceptEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tCAEvent, "CallAccept", pSourceIp, pSourcePort);

    if (MEETING.SearchParticipantAndSetState(tCAEvent->Sender, CALLSTATE_RUNNING))
        MEETING.notifyObservers(tCAEvent);
}

void SIP::SipReceivedCallDeny(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    bool tFound = false;
    CallDenyEvent *tCDEvent = new CallDenyEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tCDEvent, "CallDeny", pSourceIp, pSourcePort);

    tFound = MEETING.SearchParticipantAndSetState(tCDEvent->Sender, CALLSTATE_STANDBY);

    nua_handle_destroy(pNuaHandle);

    if (tFound)
        MEETING.notifyObservers(tCDEvent);
}

void SIP::SipReceivedCallDenyNat(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort, string pOwnNatIp, unsigned int pOwnNatPort)
{
    bool tFound = false;
    CallDenyEvent *tCDEvent = new CallDenyEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tCDEvent, "CallDenyNat", pSourceIp, pSourcePort);

    tFound = MEETING.SearchParticipantAndSetState(tCDEvent->Sender, CALLSTATE_STANDBY);

    if (!tFound)
        return;

    MEETING.SearchParticipantAndSetOwnContactAddress(tCDEvent->Sender, pOwnNatIp, pOwnNatPort);

    nua_handle_destroy(pNuaHandle);

    // no signaling to the GUI because we will send another call request

    //#################################################################################
    //### send a new call request with patched from header for correct NAT traversal
    //#################################################################################
    CallEvent *tCEvent = new CallEvent();

    tCEvent->Sender = "sip:" + SipCreateId(MEETING.getUser(), pOwnNatIp, toString(MEETING.GetHostPort()));
    tCEvent->SenderName = MEETING.GetLocalUserName();
    tCEvent->SenderComment = "";
    tCEvent->Receiver = "sip:" + tCDEvent->Sender;
    tCEvent->HandlePtr = MEETING.SearchParticipantAndGetNuaHandleForCalls(tCDEvent->Sender);
    delete tCDEvent;

    LOG(LOG_INFO, "............. SIP-new OUTGOING RECALL event occurred ...........");
    SipSendCall(tCEvent);
}

void SIP::SipReceivedCallUnavailable(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    bool tFound = false;
    CallUnavailableEvent *tCUEvent = new CallUnavailableEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tCUEvent, "CallUnavilable", pSourceIp, pSourcePort);

    tFound = MEETING.SearchParticipantAndSetState(tCUEvent->Sender, CALLSTATE_STANDBY);

    nua_handle_destroy(pNuaHandle);

    if (tFound)
        MEETING.notifyObservers(tCUEvent);
}

void SIP::SipReceivedCallCancel(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    bool tFound = false;
    CallCancelEvent *tCCEvent = new CallCancelEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tCCEvent, "CallCancel", pSourceIp, pSourcePort);

    tFound = MEETING.SearchParticipantAndSetState(tCCEvent->Sender, CALLSTATE_STANDBY);

    nua_handle_destroy(pNuaHandle);

    if (tFound)
        MEETING.notifyObservers(tCCEvent);
}

void SIP::SipReceivedCallHangup(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    bool tFound = false;
    CallHangUpEvent *tCHUEvent = new CallHangUpEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tCHUEvent, "CallHangup", pSourceIp, pSourcePort);

    tFound = MEETING.SearchParticipantAndSetState(tCHUEvent->Sender, CALLSTATE_STANDBY);

    nua_handle_destroy(pNuaHandle);

    if (tFound)
        MEETING.notifyObservers(tCHUEvent);
}

void SIP::SipReceivedCallHangupResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    LOG(LOG_INFO, "Received call hangup response");
}

void SIP::SipReceivedCallTermination(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    bool tFound = false;
    CallTerminationEvent *tCTEvent = new CallTerminationEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tCTEvent, "CallTermination", pSourceIp, pSourcePort);

    tFound = MEETING.SearchParticipantAndSetState(tCTEvent->Sender, CALLSTATE_STANDBY);

    nua_handle_destroy(pNuaHandle);

    if (tFound)
        MEETING.notifyObservers(tCTEvent);
}

void SIP::SipReceivedOptionsResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort)
{
    switch(pStatus)
    {
        case SIP_STATE_OKAY: // the other side accepted the message
            SipReceivedOptionsResponseAccept(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
        case SIP_STATE_PROXY_AUTH_REQUIRED:
            if ((pNuaHandle != NULL) && (GetServerRegistrationState()))
            {
                string tAuthInfo = "Digest:\"" + mSipRegisterServer + "\":" + mSipRegisterUsername + ":" + mSipRegisterPassword;

                LOG(LOG_VERBOSE, "Authentication information for message: %s", tAuthInfo.c_str());

                // set auth. information
                nua_authenticate(pNuaHandle, NUTAG_AUTH(tAuthInfo.c_str()), TAG_END());
            }else
                SipReceivedOptionsResponseUnavailable(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
        case 404:
        case 406:
        case 408 ... 599: // user/service unavailable
            //    404 = Not found (AVM FRITZ!Box Fon WLAN 7270 v3 (UI) 74.04.80 TAL (Dec  4 2009))
            //    406 = Method not acceptable
            //    408 = Timeout
            //    415 = Unsupported media type (linphone)
            //    503 = Service unavailable, e.g. no transport (tried to talk with a SIP server?)
            SipReceivedOptionsResponseUnavailable(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
        default:
            LOG(LOG_WARN, "Unsupported status code %d, will interpret it as \"service unavailable\"", pStatus);
            SipReceivedOptionsResponseUnavailable(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
    }
}

void SIP::SipReceivedOptionsResponseAccept(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort)
{
    OptionsAcceptEvent *tOAEvent = new OptionsAcceptEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tOAEvent, "OptionsAccept", pSourceIp, pSourcePort);

    nua_handle_destroy(pNuaHandle);

    MEETING.notifyObservers(tOAEvent);
}

void SIP::SipReceivedOptionsResponseUnavailable(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, sip_t const *pSip, std::string pSourceIp, unsigned int pSourcePort)
{
    OptionsUnavailableEvent *tOUAEvent = new OptionsUnavailableEvent();

    InitGeneralEvent_FromSipReceivedResponseEvent(pSipRemote, pSipLocal, pNuaHandle, pSip, tOUAEvent, "OptionsUnavailable", pSourceIp, pSourcePort);

    nua_handle_destroy(pNuaHandle);

    MEETING.notifyObservers(tOUAEvent);
}

void SIP::SipReceivedShutdownResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, sip_t const *pSip, string pSourceIp, unsigned int pSourcePort)
{
    if ((pStatus == SIP_STATE_OKAY /* okay */) || (pStatus == 500 /* timeout */))
        mSipStackOnline = false;
}

void SIP::SipReceivedCallResponse(const sip_to_t *pSipRemote, const sip_to_t *pSipLocal, nua_handle_t *pNuaHandle, int pStatus, const char* pPhrase, sip_t const *pSip, void* pTags, string pSourceIp, unsigned int pSourcePort)
{
    size_t tNatIpPos;
    string tPhrase = string(pPhrase);

    int tCallState = nua_callstate_init;
    //char const *tLocalSdp = NULL;
    char const *tRemoteSdp = NULL;

    tagi_t* tTags = (tagi_t*)pTags;
    tl_gets(tTags,
            NUTAG_CALLSTATE_REF(tCallState),
            //SOATAG_LOCAL_SDP_STR_REF(tLocalSdp),
            SOATAG_REMOTE_SDP_STR_REF(tRemoteSdp),
            TAG_END());

    LOG(LOG_INFO, "CallResponseCallState: %d", tCallState);
    //LOG(LOG_INFO, "CallResponseLocalSdp: %s", tLocalSdp);
    LOG(LOG_INFO, "CallResponseRemoteSdp: %s", tRemoteSdp);

    switch(pStatus)
    {
        case SIP_STATE_OKAY: // the other side accepted the call
            SipReceivedCallAccept(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
        case 180: // ringing on the other side
            SipReceivedCallRinging(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
        case SIP_STATE_PROXY_AUTH_REQUIRED:
            if ((pNuaHandle != NULL) && (GetServerRegistrationState()))
            {
                string tAuthInfo = "Digest:\"" + mSipRegisterServer + "\":" + mSipRegisterUsername + ":" + mSipRegisterPassword;

                LOG(LOG_VERBOSE, "Authentication information for message: %s", tAuthInfo.c_str());

                // set auth. information
                nua_authenticate(pNuaHandle, NUTAG_AUTH(tAuthInfo.c_str()), TAG_END());
            }else
                SipReceivedCallUnavailable(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);

            break;
        case 408 ... 599: // user/service unavailable
            //    408 = Timeout
            //    415 = Unsupported media type (linphone)
            //    503 = Service unavailable
            SipReceivedCallUnavailable(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            break;
        case 600 ... 699: // we got a deny answer
            //     603 = Deny
            #ifdef SIP_NAT_SOURCE_ADDRESS_ADAPTION
                // NAT traversal: was NAT detected by remote side?
                if ((tNatIpPos = tPhrase.find("NAT:")) != string::npos)
                {
                    string tOwnNatIp = tPhrase.substr(tNatIpPos + 4, tPhrase.size() - (tNatIpPos + 4));
                    unsigned int tOwnNatPort = 5060;
                    size_t tNatPortPos;
                    if ((tNatPortPos = tOwnNatIp.find(":")) != string::npos)
                    {
                        tOwnNatPort = (unsigned int)atoi(tOwnNatIp.substr(tNatPortPos + 1, tOwnNatIp.size() - (tNatPortPos + 1)).c_str());
                        tOwnNatIp.erase(tNatPortPos, tOwnNatIp.size() - tNatPortPos );
                    }
                    SipReceivedCallDenyNat(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort, tOwnNatIp, tOwnNatPort);
                }else
                {
                    SipReceivedCallDeny(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
                }
            #else
                SipReceivedCallDeny(pSipRemote, pSipLocal, pNuaHandle, pSip, pSourceIp, pSourcePort);
            #endif
            break;
        default:
            LOG(LOG_ERROR, "Unsupported status code");
            break;
    }
}


///////////////////////////////////////////////////////////////////////////////
//////////////////////// SENDING //////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SIP::SipSendMessage(MessageEvent *pMEvent)
{
    nua_handle_t *tHandle;
    sip_to_t *to;
    sip_from_t *from;

    to = sip_to_make(&mSipContext->Home, pMEvent->Receiver.c_str());
    if (to == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"to\" handle for function \"SendMessage\" and receiver \"%s\"", pMEvent->Receiver.c_str());
        return;
    }

    from = sip_to_make(&mSipContext->Home, pMEvent->Sender.c_str());
    if (from == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"from\" handle for function \"SendMessage\" and sender \"%s\"", pMEvent->Sender.c_str());
        return;
    }

    from->a_display = pMEvent->SenderName.c_str();
    if (pMEvent->SenderComment.size())
        from->a_url->url_password = pMEvent->SenderComment.c_str();

    // create operation handle
    tHandle = nua_handle(mSipContext->Nua, NULL, SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), SIPTAG_TO(to), SIPTAG_FROM(from), TAG_END());

    if (tHandle == NULL)
    {
        LOG(LOG_INFO, "Can not create operation handle");
        return;
    }

    // set the created handle within ParticipantDescriptor
    *pMEvent->HandlePtr = tHandle;

    printFromToSendingSipEvent(tHandle, pMEvent, "Message");
    LOG(LOG_INFO, "MessageText: %s", pMEvent->Text.c_str());

    // is the receiver located behind the registered SIP server?
    if ((GetServerRegistrationState()) && (pMEvent->Receiver.find(mSipRegisterServer) != string::npos))
    {
        string tAuthInfo = "Digest:\"" + mSipRegisterServer + "\":" + mSipRegisterUsername + ":" + mSipRegisterPassword;

        LOG(LOG_VERBOSE, "Authentication information for registration: %s", tAuthInfo.c_str());

        // set auth. information
        nua_message(tHandle, NUTAG_RETRY_COUNT(MESSAGE_REQUEST_RETRIES), SIPTAG_CONTENT_TYPE_STR("text/plain"), SIPTAG_PAYLOAD_STR(pMEvent->Text.c_str()), NUTAG_AUTH(tAuthInfo.c_str()), TAG_END());
    }else
        nua_message(tHandle, NUTAG_RETRY_COUNT(MESSAGE_REQUEST_RETRIES), SIPTAG_CONTENT_TYPE_STR("text/plain"), SIPTAG_PAYLOAD_STR(pMEvent->Text.c_str()), TAG_END());

    // don't destroy handle because we are still waiting for the nua_r_message
    //nua_handle_destroy (tHandle);
}

void SIP::SipSendCall(CallEvent *pCEvent)
{
    nua_handle_t *tHandle;
    sip_from_t *tFrom;
    sip_to_t *tTo;
    const char *tSdp;
    sip_contact_t *tContact;
    string tOwnContactIp;
    unsigned int tOwnContactPort;

    tTo = sip_to_make(&mSipContext->Home, pCEvent->Receiver.c_str());
    if (tTo == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"to\" handle for function \"SipSendCall\" and receiver \"%s\"", pCEvent->Receiver.c_str());
        return;
    }

    tFrom = sip_to_make(&mSipContext->Home, pCEvent->Sender.c_str());
    if (tFrom == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"from\" handle for function \"SipSendCall\" and sender \"%s\"", pCEvent->Sender.c_str());
        return;
    }

    // get participant from "receiver" string by removing "sip:"
    string tParticipant = pCEvent->Receiver.substr(4);

    // create CONTACT header, structure of this header is defined in "20.10 Contact" of RFC 3261
    // NAT traversal: explicit CONTACT header necessary
    //                otherwise acknowledge and session activation from the answering host will be lost because of routing problems
    // hint: SIP usually uses the sender's CONTACT header for routing responses
    MEETING.GetOwnContactAddress(tParticipant, tOwnContactIp, tOwnContactPort);
    LOG(LOG_VERBOSE, "Contact header: %s", ("sip:" + tOwnContactIp + ":" + toString(tOwnContactPort)).c_str());
    tContact = sip_contact_make(&mSipContext->Home, ("sip:" + tOwnContactIp + ":" + toString(tOwnContactPort)).c_str());

    tFrom->a_display = pCEvent->SenderName.c_str();
    if (pCEvent->SenderComment.size())
        tFrom->a_url->url_password = pCEvent->SenderComment.c_str();

    // create operation handle
    tHandle = nua_handle(mSipContext->Nua, NULL, SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), SOATAG_ADDRESS(tOwnContactIp.c_str()), TAG_IF(tContact, SIPTAG_CONTACT(tContact)), SIPTAG_TO(tTo), SIPTAG_FROM(tFrom), TAG_END());

    if (tHandle == NULL)
    {
        LOG(LOG_ERROR, "Can not create operation handle");
        return;
    }

    // set the created handle within ParticipantDescriptor
    *pCEvent->HandlePtr = tHandle;

    // initialize SDP protocol parameters
    // set SDP string
    tSdp = MEETING.GetSdpData(tParticipant);

    printFromToSendingSipEvent(tHandle, pCEvent, "Call");
    LOG(LOG_INFO, "CallSdp: %s", tSdp);

    nua_invite(tHandle, NUTAG_RETRY_COUNT(CALL_REQUEST_RETRIES), TAG_IF(tSdp, SOATAG_USER_SDP_STR(tSdp)), TAG_END());
    // no "nua_handle_destroy" here because we need this handle as long as the call is running !

    MEETING.SearchParticipantAndSetState(tParticipant, CALLSTATE_RINGING);

    CallRingingEvent *tCREvent = new CallRingingEvent();

    tCREvent->Sender = pCEvent->Receiver;
    tCREvent->Receiver = pCEvent->Sender;
    tCREvent->IsIncomingEvent = false;

    MEETING.notifyObservers(tCREvent);
}

void SIP::SipSendCallAccept(CallAcceptEvent *pCAEvent)
{
    const char *tSdp;
    sip_contact_t *tContact;
    string tOwnContactIp;
    unsigned int tOwnContactPort;

    // set the temporary handle to the handle from corresponding ParticipantDescriptor
    nua_handle_t *tHandle = *pCAEvent->HandlePtr;

    // initialize SDP protocol parameters
    // remove "sip:"
    pCAEvent->Receiver.erase(0, 4);

    // get SDP string
    tSdp = MEETING.GetSdpData(pCAEvent->Receiver);

    printFromToSendingSipEvent(tHandle, pCAEvent, "CallAccept");
    LOG(LOG_INFO, "CallAcceptSdp: %s", tSdp);

    // create CONTACT header, structure of this header is defined in "20.10 Contact" of RFC 3261
    // NAT traversal: explicit CONTACT header necessary
    //                otherwise acknowledge and session activation from the requesting host will be lost because of routing problems
    // hint: SIP usually uses the sender's CONTACT header for routing responses to the sender
    MEETING.GetOwnContactAddress(pCAEvent->Receiver, tOwnContactIp, tOwnContactPort);
    LOG(LOG_VERBOSE, "Contact header: %s", ("sip:" + tOwnContactIp + ":" + toString(tOwnContactPort)).c_str());
    tContact = sip_contact_make(&mSipContext->Home, ("sip:" + tOwnContactIp + ":" + toString(tOwnContactPort)).c_str());

    nua_respond(tHandle, SIP_STATE_OKAY, "OK", SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), SOATAG_ADDRESS(tOwnContactIp.c_str()), TAG_IF(tContact, SIPTAG_CONTACT(tContact)), TAG_IF(tSdp, SOATAG_USER_SDP_STR(tSdp)), TAG_END());

    //###############  FEEDBACK  ##########################
    CallAcceptEvent *tCAEvent = new CallAcceptEvent();

    tCAEvent->Sender = pCAEvent->Receiver;
    tCAEvent->Receiver = pCAEvent->Sender;
    tCAEvent->IsIncomingEvent = false;

    if (MEETING.SearchParticipantAndSetState(pCAEvent->Receiver, CALLSTATE_RUNNING))
        MEETING.notifyObservers(tCAEvent);
}

void SIP::SipSendCallRinging(CallRingingEvent *pCREvent)
{
    // set the temporary handle to the handle from corresponding ParticipantDescriptor
    nua_handle_t *tHandle = *pCREvent->HandlePtr;

    printFromToSendingSipEvent(tHandle, pCREvent, "CallRinging");
    nua_respond(tHandle, 180, "Ringing", SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), TAG_END());

    //###############  FEEDBACK  ##########################
    CallRingingEvent *tCREvent = new CallRingingEvent();

    tCREvent->Sender = pCREvent->Receiver;
    tCREvent->Receiver = pCREvent->Sender;
    tCREvent->IsIncomingEvent = false;

    // no state change for this participant !

    MEETING.notifyObservers(tCREvent);
}

void SIP::SipSendCallCancel(CallCancelEvent *pCCEvent)
{
    // set the temporary handle to the handle from corresponding ParticipantDescriptor
    nua_handle_t *tHandle = *pCCEvent->HandlePtr;

    printFromToSendingSipEvent(tHandle, pCCEvent, "CallCancel");
    nua_cancel(tHandle, SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), TAG_END());
    nua_handle_destroy (tHandle);

    //###############  FEEDBACK  ##########################
    CallCancelEvent *tCCEvent = new CallCancelEvent();

    tCCEvent->Sender = pCCEvent->Receiver;
    tCCEvent->Receiver = pCCEvent->Sender;
    tCCEvent->IsIncomingEvent = false;

    // remove "sip:"
    pCCEvent->Receiver.erase(0, 4);

    if (MEETING.SearchParticipantAndSetState(pCCEvent->Receiver, CALLSTATE_STANDBY))
        MEETING.notifyObservers(tCCEvent);
}

void SIP::SipSendCallDeny(CallDenyEvent *pCDEvent)
{
    // set the temporary handle to the handle from corresponding ParticipantDescriptor
    nua_handle_t *tHandle = *pCDEvent->HandlePtr;

    printFromToSendingSipEvent(tHandle, pCDEvent, "CallDeny");
    nua_respond(tHandle, 603, "Decline", SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), TAG_END());
    nua_handle_destroy (tHandle);

    //###############  FEEDBACK  ##########################
    CallDenyEvent *tCDEvent = new CallDenyEvent();

    tCDEvent->Sender = pCDEvent->Receiver;
    tCDEvent->Receiver = pCDEvent->Sender;
    tCDEvent->IsIncomingEvent = false;

    // remove "sip:"
    pCDEvent->Receiver.erase(0, 4);
    if (MEETING.SearchParticipantAndSetState(pCDEvent->Receiver, CALLSTATE_STANDBY))
        MEETING.notifyObservers(tCDEvent);
}

void SIP::SipSendCallHangUp(CallHangUpEvent *pCHUEvent)
{
    sip_to_t *to;

    to = sip_to_make(&mSipContext->Home, pCHUEvent->Receiver.c_str());
    if (to == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"to\" handle for function \"SipSendCallHangUp\" and sender \"%s\"", pCHUEvent->Receiver.c_str());
        return;
    }

    // set the temporary handle to the handle from corresponding ParticipantDescriptor
    nua_handle_t *tHandle = *pCHUEvent->HandlePtr;

    printFromToSendingSipEvent(tHandle, pCHUEvent, "CallHangUp");
    nua_bye(tHandle, SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), /*SIPTAG_TO(to),*/ TAG_END());
    nua_handle_destroy (tHandle);

    //###############  FEEDBACK  ##########################
    CallHangUpEvent *tCHUEvent = new CallHangUpEvent();

    tCHUEvent->Sender = pCHUEvent->Receiver;
    tCHUEvent->Receiver = pCHUEvent->Sender;
    tCHUEvent->IsIncomingEvent = false;

    // remove "sip:"
    pCHUEvent->Receiver.erase(0, 4);
    if (MEETING.SearchParticipantAndSetState(pCHUEvent->Receiver, CALLSTATE_STANDBY))
        MEETING.notifyObservers(tCHUEvent);
}

void SIP::SipSendOptionsRequest(OptionsEvent *pOEvent)
{
    nua_handle_t *tHandle;
    sip_to_t *to;
    sip_from_t *from;

    to = sip_to_make(&mSipContext->Home, pOEvent->Receiver.c_str());
    if (to == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"to\" handle for function \"SipSendOptionsRequest\" and receiver \"%s\"", pOEvent->Receiver.c_str());
        return;
    }

    from = sip_to_make(&mSipContext->Home, pOEvent->Sender.c_str());
    if (from == NULL)
    {
        LOG(LOG_ERROR, "Can not create \"from\" handle for function \"SipSendOptionsRequest\" and sender \"%s\"", pOEvent->Sender.c_str());
        return;
    }

    from->a_display = pOEvent->SenderName.c_str();
    if (pOEvent->SenderComment.size())
        from->a_url->url_password = pOEvent->SenderComment.c_str();

    // create operation handle
    tHandle = nua_handle(mSipContext->Nua, NULL, SIPTAG_USER_AGENT_STR(USER_AGENT_SIGNATURE), SIPTAG_TO(to), /*SIPTAG_FROM(from),*/ TAG_END());

    if (tHandle == NULL)
    {
        LOG(LOG_INFO, "Can not create operation handle");
        return;
    }

    // set the created handle within ParticipantDescriptor
    //*pOEvent->HandlePtr = tHandle;

    printFromToSendingSipEvent(tHandle, pOEvent, "Options");
    nua_options(tHandle, NUTAG_RETRY_COUNT(OPTIONS_REQUEST_RETRIES), TAG_END());

    // don't destroy handle because we are still waiting for the nua_r_message
    //nua_handle_destroy (tHandle);
}

void SIP::SipProcessOutgoingEvents()
{
    GeneralEvent *tEvent;

    // is there a waiting event?
    while ((tEvent = OutgoingEvents.Scan()) != NULL)
    {
        LOG(LOG_INFO, "............. SIP-new OUTGOING event with ID <%d> and name \"%s\" occurred ...........", tEvent->getType(), GeneralEvent::getNameFromType(tEvent->getType()).c_str());
        if (tEvent->getType() == MessageEvent::type())
        {
            SipSendMessage((MessageEvent*) tEvent);
            return;
        }
        if (tEvent->getType() == CallEvent::type())
        {
            SipSendCall((CallEvent*) tEvent);
            return;
        }
        if (tEvent->getType() == CallRingingEvent::type())
        {
            SipSendCallRinging((CallRingingEvent*) tEvent);
            return;
        }
        if (tEvent->getType() == CallCancelEvent::type())
        {
            SipSendCallCancel((CallCancelEvent*) tEvent);
            return;
        }
        if (tEvent->getType() == CallAcceptEvent::type())
        {
            SipSendCallAccept((CallAcceptEvent*) tEvent);
            return;
        }
        if (tEvent->getType() == CallDenyEvent::type())
        {
            SipSendCallDeny((CallDenyEvent*) tEvent);
            return;
        }
        if (tEvent->getType() == CallHangUpEvent::type())
        {
            SipSendCallHangUp((CallHangUpEvent*) tEvent);
            return;
        }
        if (tEvent->getType() == OptionsEvent::type())
        {
            SipSendOptionsRequest((OptionsEvent*) tEvent);
            return;
        }
        if (tEvent->getType() == InternalNatDetectionEvent::type())
        {
            //###############  NAT detection with FEEDBACK  ##########################
            InternalNatDetectionEvent *tINDEvent = new InternalNatDetectionEvent();

            tINDEvent->Failed = !DetectNatViaStun(tINDEvent->FailureReason);

            MEETING.notifyObservers(tINDEvent);

            return;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
