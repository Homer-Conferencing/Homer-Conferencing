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
 * Purpose: Implementation for session initiation protocol's stun support
 * Since:   2009-06-20
 */

#include <string>
#include <sstream>
#include <assert.h>

#include <Header_SofiaSip.h>
#include <SIP_stun.h>
#include <HBSocket.h>
#include <Logger.h>

namespace Homer { namespace Conference {

using namespace std;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

struct StunContext
{
  stun_handle_t         *Handle;    /* handle for STUN based nat traversal */
  //stun_discovery_magic_t StunDiscoveryContext; /* context for STUN discovery process */
  su_socket_t           Socket;     /* socket for STUN nat traversal processes */
  su_root_t*            SipRoot;    /* root of current SIP stack */
};

// wrapper to avoid dependency of the header from the sofia-sip headers
void GlobalStunCallBack(stun_discovery_magic_t *pStunDiscoveryMagic, stun_handle_t *pStunHandle, stun_discovery_t *pStunDiscoveryHandle, stun_action_t pStunAction, stun_state_t pStunState)
{
	LOGEX(SIP_stun, LOG_VERBOSE, "Called GlobalStunCallBack()");
    SIP_stun *tSIP_stun = (SIP_stun*)pStunDiscoveryMagic;

    tSIP_stun->StunCallBack(pStunDiscoveryHandle, pStunAction, pStunState);
}

///////////////////////////////////////////////////////////////////////////////

SIP_stun::SIP_stun()
{
    mStunContext = new StunContext;
    mStunServer = "";
    mStunNatType = "";
    mStunOutmostAdr = "";
    mStunOutmostPort = -1;
    mStunSupportActivated = false;
    mStunNatDetectionFinished = false;
    mStunContext->Handle = NULL;
    mStunContext->SipRoot = NULL;
    mStunContext->Socket = -1;
}

SIP_stun::~SIP_stun()
{
    delete mStunContext;
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////// main stun functions ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SIP_stun::Init(su_root_t* pSipRoot)
{
	LOG(LOG_VERBOSE, "Initializing STUN support");

    mStunContext->SipRoot = pSipRoot;

    // create STUN socket
    LOG(LOG_VERBOSE, "  ..creating STUN socket");
    Socket *tSocket = Socket::CreateClientSocket(SOCKET_IPv4, SOCKET_UDP, mStunHostPort, false, 1, mStunHostPort + 10);
    if (tSocket != NULL)
    {
    	// get port number after auto-probing available ports
    	mStunHostPort = tSocket->GetLocalPort();

    	// store the handle for the created socket
    	mStunContext->Socket = tSocket->GetHandle();
    }else
    {
    	LOG(LOG_ERROR, "Invalid STUN client socket");
    	mStunContext->Socket = -1;
    }

    LOG(LOG_INFO, "STUN-Client assigned to 0.0.0.0:%d", mStunHostPort);
}

void SIP_stun::Deinit()
{
    if (mStunSupportActivated)
    {
        // destroy STUN handle
//        if (mStunContext->Handle != NULL)
//            stun_handle_destroy(mStunContext->Handle);

        // close STUN socket
        if ((int)mStunContext->Socket != -1)
            su_close(mStunContext->Socket);

        mStunSupportActivated = false;
    }
}

void SIP_stun::SetStunServer(string pServer)
{
    if (mStunServer != pServer)
    {
        LOG(LOG_VERBOSE, "Setting STUN server to: %s", pServer.c_str());
        mStunServer = pServer;
        mStunSupportActivated = false,
        mStunNatDetectionFinished = false;
    }
}

string SIP_stun::GetStunServer()
{
	string tResult = "";

	tResult = mStunServer;

	return tResult;
}

string SIP_stun::GetStunNatIp()
{
	string tResult = "";

	if ((!mStunSupportActivated) || (!mStunNatDetectionFinished) || (mStunOutmostAdr == "0.0.0.0"))
        tResult = "";
    else
        tResult = mStunOutmostAdr;

	return tResult;
}

int SIP_stun::GetStunNatPort()
{
    if ((!mStunSupportActivated) || (!mStunNatDetectionFinished))
        return 0;
    else
        return mStunOutmostPort;
}

string SIP_stun::GetStunNatType()
{
	string tResult = "";

	if ((!mStunSupportActivated) || (!mStunNatDetectionFinished))
		tResult = "";
    else
    	tResult = mStunNatType;

	return tResult;
}

bool SIP_stun::DetectNatViaStun(string &pFailureReason)
{
	LOG(LOG_VERBOSE, "Trying to detect NAT via STUN now..");

    if (mStunServer == "")
    {
    	LOG(LOG_WARN, "No STUN server defined");
    	return false;
    }

    // destroy STUN handle
    if (mStunContext->Handle != NULL)
    {
    	LOG(LOG_WARN, "Reseting STUN handle");
    	stun_handle_destroy(mStunContext->Handle);
    }

    // init STUN handle
    mStunContext->Handle = stun_handle_init(mStunContext->SipRoot, STUNTAG_SERVER(mStunServer.c_str()), TAG_END());
    if (mStunContext->Handle == NULL)
    {
        LOG(LOG_ERROR, "Error during first contact with STUN server because of \"%s\"", strerror(errno));
        pFailureReason = string(strerror(errno));
        return false;
    }

    // perform shared secret request/response processing
/*    if (stun_obtain_shared_secret(mStunContext->Handle, GlobalStunCallBack, (stun_discovery_magic_t*)this, STUNTAG_SOCKET(mStunContext->Socket), STUNTAG_REGISTER_EVENTS(1), TAG_END()) < 0)
    {
        LOG(LOG_ERROR, "Error during STUN shared secret request/response because of \"%s\"", strerror(errno));
        stun_handle_destroy(mStunContext->Handle);
        su_close(mStunContext->Socket);
        return false;
    }
*/
    // perform a STUN binding discovery (rfc 3489/3489bis) process
    if (stun_bind(mStunContext->Handle, GlobalStunCallBack, (stun_discovery_magic_t*)this, STUNTAG_SOCKET(mStunContext->Socket), STUNTAG_REGISTER_EVENTS(1), TAG_END()) < 0)
    {
        LOG(LOG_ERROR, "Error during STUN binding discovery because of \"%s\"", strerror(errno));
        pFailureReason = string(strerror(errno));
        stun_handle_destroy(mStunContext->Handle);
        mStunContext->Handle = NULL;
        return false;
    }

    // initiates STUN discovery process to find out NAT characteristics
    if (stun_test_nattype(mStunContext->Handle, GlobalStunCallBack, (stun_discovery_magic_t*)this, STUNTAG_SOCKET(mStunContext->Socket), STUNTAG_REGISTER_EVENTS(1), TAG_END()) < 0)
    {
        LOG(LOG_ERROR, "Error during STUN nat discovery because of \"%s\"", strerror(errno));
        pFailureReason = string(strerror(errno));
        stun_handle_destroy(mStunContext->Handle);
        mStunContext->Handle = NULL;
        return false;
    }

    mStunSupportActivated = true;
    LOG(LOG_VERBOSE, "STUN support activated");

    return true;
}

void SIP_stun::StunCallBack(stun_discovery_t *pStunDiscoveryHandle, int pStunAction, int pStunState)
{
    su_sockaddr_t tSocketAddrDesc;
    socklen_t tSocketAddrDescLen = sizeof(tSocketAddrDesc);

    memset(&tSocketAddrDesc, 0, sizeof(su_sockaddr_t));

    LOGEX(SIP_stun, LOG_INFO, "############# STUN-new state <%d> for action <%d> occurred ###########", pStunState, pStunAction);

    // nat detection finished?
    if (pStunState == stun_discovery_done)
    {
        mStunNatType = string(stun_nattype_str(pStunDiscoveryHandle));
        stun_discovery_get_address(pStunDiscoveryHandle, (struct sockaddr *) &tSocketAddrDesc, &tSocketAddrDescLen);
        mStunOutmostAdr = string(inet_ntoa(tSocketAddrDesc.su_sin.sin_addr));
        mStunOutmostPort = (int)ntohs(tSocketAddrDesc.su_sin.sin_port);

        LOGEX(SIP_stun, LOG_INFO, "STUN nat detection finished");
        LOGEX(SIP_stun, LOG_INFO, "     ..nat type: \"%s\"", mStunNatType.c_str());
        LOGEX(SIP_stun, LOG_INFO, "     ..nat outmost IP: %s", mStunOutmostAdr.c_str());
        LOGEX(SIP_stun, LOG_INFO, "     ..nat outmost port: %d", mStunOutmostPort);
    }
    mStunNatDetectionFinished = true;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
