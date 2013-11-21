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
 * Purpose: ffmpeg based network video source
 * Author:  Thomas Volkert
 * Since:   2008-12-16
 */

#ifndef _MULTIMEDIA_MEDIA_SOURCE_NET_
#define _MULTIMEDIA_MEDIA_SOURCE_NET_

#include <HBSocket.h>
#include <HBThread.h>
#include <MediaSource.h>
#include <MediaSourceMem.h>
#include <RTP.h>

#include <string>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSN_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

struct TCPFragmentHeader{
    unsigned int    FragmentSize;
};

#define TCP_FRAGMENT_HEADER_SIZE            (sizeof(TCPFragmentHeader))

///////////////////////////////////////////////////////////////////////////////

class NetworkListener;

class MediaSourceNet :
    public MediaSourceMem
{
public:
    /// The constructor
    MediaSourceNet(Socket *pDataSocket);
    MediaSourceNet(unsigned int pPortNumber, enum TransportType pTransportType);
    MediaSourceNet(std::string pLocalName, Requirements *pTransportRequirements);

    /// The destructor
    virtual ~MediaSourceNet();

    /* grabbing control */
    virtual void StopGrabbing();

    /* device control */
    virtual std::string GetCurrentDevicePeerName();

    unsigned int GetListenerPort();

    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, int pChannels = 2);
    virtual bool CloseGrabDevice();

protected:
    /* network socket listener thread */
    friend class NetworkListener;

    void Init();

    NetworkListener     *mNetworkListener;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
