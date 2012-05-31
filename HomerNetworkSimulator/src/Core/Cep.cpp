/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This Peer is published in the hope that it will be useful, but
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
 * Purpose: Implementation of CEP
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#include <Core/Cep.h>
#include <Core/Node.h>

#include <GAPI.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Cep::Cep(Node *pNode, enum TransportType pTransportType, unsigned int pLocalPort)
{
    mLocalPort = pLocalPort;
    mPeerPort = 0;
    mPeerNode = "";
    mNode = pNode;
    mTransportType = pTransportType;
}

Cep::Cep(Node *pNode, enum TransportType pTransportType, string pTarget, unsigned int pTargetPort)
{
    static int sLastClientPort = 1023;

    mNode = pNode;
    mLocalPort = ++sLastClientPort;

    mPeerNode = pTarget;
    mPeerPort = pTargetPort;
    mTransportType = pTransportType;
}

Cep::~Cep()
{

}

///////////////////////////////////////////////////////////////////////////////

bool Cep::SetQoS(const QoSSettings &pQoSSettings)
{
    LOG(LOG_VERBOSE, "Desired QoS: %u KB/s min. data rate, %u ms max. delay, features: 0x%hX", pQoSSettings.DataRate, pQoSSettings.Delay, pQoSSettings.Features);

    //TODO
//    mQoSSettings = pQoSSettings;
//
//    if (IsQoSSupported())
//    {
//        setqos(mSocketHandle, pQoSSettings.DataRate, pQoSSettings.Delay, pQoSSettings.Features);
//    }else
//        LOG(LOG_WARN, "QoS support deactivated, settings will be ignored");

    return true;
}

bool Cep::Send(std::string pTargetNode, unsigned int pTargetPort, void *pBuffer, ssize_t pBufferSize)
{
    LOG(LOG_VERBOSE, "Sending %d bytes to %s:%u", (int)pBufferSize, mPeerNode.c_str(), mPeerPort);

    //TODO: add FIFO here
    return true;
}

bool Cep::Receive(std::string &pPeerNode, unsigned int &pPeerPort, void *pBuffer, ssize_t &pBufferSize)
{
    //TODO: add FIFO here
    LOG(LOG_VERBOSE, "Received %d bytes from %s:%u", (int)pBufferSize, mPeerNode.c_str(), mPeerPort);

    return true;
}

enum TransportType Cep::GetTransportType()
{
    return mTransportType;
}

unsigned int Cep::GetLocalPort()
{
    return mLocalPort;
}

string Cep::GetLocalNode()
{
    return mNode->GetName();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
