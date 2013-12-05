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
 * Purpose: session initiation protocol's stun support
 * Since:   2009-06-20
 */

#ifndef _CONFERENCE_SIP_STUN_
#define _CONFERENCE_SIP_STUN_

#include <Header_SofiaSipForwDecl.h>

#include <string>

namespace Homer { namespace Conference {

///////////////////////////////////////////////////////////////////////////////
struct StunContext;

class SIP_stun
{
public:
    SIP_stun();

    virtual ~SIP_stun();

    virtual void SetStunServer(std::string pServer);
    std::string GetStunServer();
    std::string GetStunNatIp();
    int GetStunNatPort();
    std::string GetStunNatType();

    void StunCallBack(stun_discovery_t *pStunDiscoveryHandle, int pStunAction, int pStunState);

protected:
    void Init(su_root_t* pSipRoot);
    void Deinit();

    bool DetectNatViaStun(std::string &pFailureReason);

    StunContext*        mStunContext;
    int                 mStunHostPort;
    std::string         mStunServer;
    std::string         mStunNatType;
    std::string         mStunOutmostAdr;
    int                 mStunOutmostPort;
    bool                mStunNatDetectionFinished;
    bool                mStunSupportActivated;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
