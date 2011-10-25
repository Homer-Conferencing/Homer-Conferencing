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
 * Name:    SIP_stun.cpp
 * Purpose: Implementation for session initiation protocol's stun support
 * Author:  Thomas Volkert
 * Since:   2009-06-20
 * Version: $Id: SIP_stun.cpp,v 1.12 2011/08/03 20:10:40 chaos Exp $
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

SIP_stun::SIP_stun()
{
    mStunServer = "";
    mStunNatType = "";
    mStunOutmostAdr = "";
    mStunOutmostPort = -1;
    mStunSupportActivated = false;
    mStunNatDetectionFinished = false;
    mStunContext.Handle = NULL;
    mStunContext.SipRoot = NULL;
    mStunContext.Socket = (su_socket_t)-1;
}

SIP_stun::~SIP_stun()
{
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////// main stun functions ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SIP_stun::Init(su_root_t* pSipRoot)
{
    mStunContext.SipRoot = pSipRoot;

    // create STUN socket
    mStunContext.Socket = su_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if ((int)mStunContext.Socket == -1)
    {
        LOG(LOG_ERROR, "Error during su_socket() because of \"%s\"", strerror(errno));
        return;
    }

    su_sockaddr_t tSocketAddrDesc;
    socklen_t tSocketAddrDescLen = sizeof(tSocketAddrDesc);

    memset(&tSocketAddrDesc, 0, sizeof(su_sockaddr_t));
    tSocketAddrDesc.su_port = htons((uint16_t)mStunHostPort);
    tSocketAddrDesc.su_family = AF_INET;

    while (bind(mStunContext.Socket, (struct sockaddr *) &tSocketAddrDesc, tSocketAddrDescLen) < 0)
    {
        mStunHostPort++;
        tSocketAddrDesc.su_port = htons((uint16_t)mStunHostPort);
    }

    LOG(LOG_INFO, "STUN-Client assigned to 0.0.0.0:%d", mStunHostPort);
}

void SIP_stun::Deinit()
{
    if (mStunSupportActivated)
    {
        // destroy STUN handle
//        if (mStunContext.Handle != NULL)
//            stun_handle_destroy(mStunContext.Handle);

        // close STUN socket
        if ((int)mStunContext.Socket != -1)
            su_close(mStunContext.Socket);

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
    return mStunServer;
}

string SIP_stun::getStunNatIp()
{
    if ((!mStunSupportActivated) || (!mStunNatDetectionFinished))
        return "";
    else
        return mStunOutmostAdr;
}

int SIP_stun::getStunNatPort()
{
    if ((!mStunSupportActivated) || (!mStunNatDetectionFinished))
        return 0;
    else
        return mStunOutmostPort;
}

string SIP_stun::getStunNatType()
{
    if ((!mStunSupportActivated) || (!mStunNatDetectionFinished))
        return "";
    else
        return mStunNatType;
}

bool SIP_stun::DetectNatViaStun(string &pFailureReason)
{
    if (mStunServer == "")
        return false;

    // destroy STUN handle
    if (mStunContext.Handle != NULL)
        stun_handle_destroy(mStunContext.Handle);

    // init STUN handle
    mStunContext.Handle = stun_handle_init(mStunContext.SipRoot, STUNTAG_SERVER(mStunServer.c_str()), TAG_NULL());
    if (mStunContext.Handle == NULL)
    {
        LOG(LOG_ERROR, "Error during first contact with STUN server because of \"%s\"", strerror(errno));
        pFailureReason = string(strerror(errno));
        return false;
    }

    // perform shared secret request/response processing
    /*
    if (stun_obtain_shared_secret(mStunContext.Handle, StunCallBack, NULL, STUNTAG_SOCKET(mStunContext.Socket), STUNTAG_REGISTER_EVENTS(1), TAG_NULL()) < 0)
    {
        LOG(LOG_ERROR, "Error during STUN shared secret request/response because of \"%s\"", strerror(errno));
        stun_handle_destroy(mStunContext.Handle);
        su_close(mStunContext.Socket);
        return false;
    }

    // perform a STUN binding discovery (rfc 3489/3489bis) process
    if (stun_bind(mStunContext.Handle, StunCallBack, (stun_discovery_magic_t*)this, STUNTAG_SOCKET(mStunContext.Socket), STUNTAG_REGISTER_EVENTS(1), TAG_NULL()) < 0)
    {
        //LOG(LOG_ERROR, "Error during STUN binding discovery because of \"%s\"", strerror(errno));
        stun_handle_destroy(mStunContext.Handle);
        su_close(mStunContext.Socket);
        return false;
    }
    */

    // initiates STUN discovery process to find out NAT characteristics
    if (stun_test_nattype(mStunContext.Handle, StunCallBack, (stun_discovery_magic_t*)this, STUNTAG_SOCKET(mStunContext.Socket), STUNTAG_REGISTER_EVENTS(1), TAG_NULL()) < 0)
    {
        LOG(LOG_ERROR, "Error during STUN nat discovery because of \"%s\"", strerror(errno));
        pFailureReason = string(strerror(errno));
        stun_handle_destroy(mStunContext.Handle);
        mStunContext.Handle = NULL;
        return false;
    }

    mStunSupportActivated = true;
    LOG(LOG_VERBOSE, "STUN support activated");

    return true;
}

void SIP_stun::StunCallBack(stun_discovery_magic_t *pStunDiscoveryMagic, stun_handle_t *pStunHandle, stun_discovery_t *pStunDiscoveryHandle, stun_action_t pStunAction, stun_state_t pStunState)
{
    SIP_stun *tSIP_stun = (SIP_stun*)pStunDiscoveryMagic;
    su_sockaddr_t tSocketAddrDesc;
    socklen_t tSocketAddrDescLen = sizeof(tSocketAddrDesc);

    memset(&tSocketAddrDesc, 0, sizeof(su_sockaddr_t));

    LOGEX(SIP_stun, LOG_INFO, "############# STUN-new state <%d> for action <%d> occurred ###########", pStunState, pStunAction);

    // nat detection finished?
    if ((pStunAction == stun_action_test_nattype) && (pStunState == stun_discovery_done))
    {
        tSIP_stun->mStunNatType = string(stun_nattype_str(pStunDiscoveryHandle));
        stun_discovery_get_address(pStunDiscoveryHandle, (struct sockaddr *) &tSocketAddrDesc, &tSocketAddrDescLen);
        tSIP_stun->mStunOutmostAdr = string(inet_ntoa(tSocketAddrDesc.su_sin.sin_addr));
        tSIP_stun->mStunOutmostPort = (int)ntohs(tSocketAddrDesc.su_sin.sin_port);

        LOGEX(SIP_stun, LOG_INFO, "STUN nat detection finished");
        LOGEX(SIP_stun, LOG_INFO, "     ..nat type: \"%s\"", tSIP_stun->mStunNatType.c_str());
        LOGEX(SIP_stun, LOG_INFO, "     ..nat outmost IP: %s", tSIP_stun->mStunOutmostAdr.c_str());
        LOGEX(SIP_stun, LOG_INFO, "     ..nat outmost port: %d", tSIP_stun->mStunOutmostPort);
    }
    tSIP_stun->mStunNatDetectionFinished = true;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
