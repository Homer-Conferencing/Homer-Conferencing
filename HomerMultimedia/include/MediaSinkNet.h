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
 * Purpose: network based media sink which supports RTP
 * Author:  Thomas Volkert
 * Since:   2009-12-27
 */

#ifndef _MULTIMEDIA_MEDIA_SINK_NET_
#define _MULTIMEDIA_MEDIA_SINK_NET_

#include <Header_Ffmpeg.h>
#include <NAPI.h>
#include <HBSocket.h>
#include <HBThread.h>
#include <MediaSinkMem.h>

#include <string>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSIN_DEBUG_PACKETS

//#define MSIN_DEBUG_TIMING

///////////////////////////////////////////////////////////////////////////////

class MediaSinkNet:
    public MediaSinkMem, public Thread
{

public:
    // general purpose constructor which uses NAPI library
    MediaSinkNet(string pTarget, Requirements *pTransportRequirements, enum MediaSinkType pType, bool pRtpActivated);
    // constructor to send media data via the same port of an existing already allocated socket object (can be used in conferences to support NAT traversal)
    MediaSinkNet(std::string pTargetHost, unsigned int pTargetPort, Socket* pLocalSocket, enum MediaSinkType pType, bool pRtpActivated);

    virtual ~MediaSinkNet();

    virtual void ProcessPacket(char* pPacketData, unsigned int pPacketSize, AVStream *pStream = NULL, bool pIsKeyFrame = false);

    /* network oriented ID */
    static std::string CreateId(std::string pHost, std::string pPort, enum TransportType pSocketTransportType = SOCKET_TRANSPORT_TYPE_INVALID, bool pRtpActivated = true);

    virtual void StopProcessing();

protected:
    virtual void WriteFragment(char* pData, unsigned int pSize, int64_t pFragmentNumber);

private:
    /* sender thread */
    virtual void* Run(void* pArgs = NULL);
    void StartSender();
    void StopSender();

    /* sending one single fragment of an (rtp) packet stream */
    virtual void SendPacket(char* pData, unsigned int pSize);

    void BasicInit(string pTargetHost, unsigned int pTargetPort);

    /* general transport */
    bool                mSenderNeeded;
    int                 mMaxNetworkPacketSize;
    bool                mBrokenPipe;
    bool                mStreamedTransport;
    char                *mStreamFragmentCopyBuffer;
    /* Berkeley sockets based transport */
    Socket              *mDataSocket;
    /* NAPI based transport */
    IConnection         *mNAPIDataSocket;
    bool                mNAPIUsed;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
