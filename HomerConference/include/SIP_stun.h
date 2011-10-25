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
 * Name:    SIP_stun.h
 * Purpose: session initiation protocol's stun support
 * Author:  Thomas Volkert
 * Since:   2009-06-20
 * Version: $Id$
 */

#ifndef _CONFERENCE_SIP_STUN_
#define _CONFERENCE_SIP_STUN_

#include <Header_SofiaSip.h>

#include <string>

namespace Homer { namespace Conference {

///////////////////////////////////////////////////////////////////////////////


struct StunContext
{
  stun_handle_t         *Handle;    /* handle for STUN based nat traversal */
  //stun_discovery_magic_t StunDiscoveryContext; /* context for STUN discovery process */
  su_socket_t           Socket;     /* socket for STUN nat traversal processes */
  su_root_t*            SipRoot;    /* root of current SIP stack */
};

///////////////////////////////////////////////////////////////////////////////

class SIP_stun
{
public:
    SIP_stun();

    virtual ~SIP_stun();

    virtual void SetStunServer(std::string pServer);
    std::string GetStunServer();
    std::string getStunNatIp();
    int getStunNatPort();
    std::string getStunNatType();

protected:
    void Init(su_root_t* pSipRoot);
    void Deinit();

    bool DetectNatViaStun(std::string &pFailureReason);

    StunContext         mStunContext;
    int                 mStunHostPort;
    std::string         mStunServer;
    std::string         mStunNatType;
    std::string         mStunOutmostAdr;
    int                 mStunOutmostPort;
    bool                mStunNatDetectionFinished;
    bool                mStunSupportActivated;

private:
    static void StunCallBack(stun_discovery_magic_t *pStunDiscoveryMagic, stun_handle_t *pStunHandle, stun_discovery_t *pStunDiscoveryHandle, stun_action_t pStunAction, stun_state_t pStunState);
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
