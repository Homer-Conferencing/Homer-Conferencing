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

#include <Header_Ffmpeg.h>
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

// maximum number of acceptable continuous receive errors
#define MAX_RECEIVE_ERRORS                                  3
#define SOCKET_RECEIVE_BUFFER_SIZE                      2 * 1024 * 1024

///////////////////////////////////////////////////////////////////////////////
struct TCPFragmentHeader{
    unsigned int    FragmentSize;
};

#define TCP_FRAGMENT_HEADER_SIZE            (sizeof(TCPFragmentHeader))

///////////////////////////////////////////////////////////////////////////////

class MediaSourceNet :
    public MediaSourceMem, public Thread
{
public:
    /// The constructor
    MediaSourceNet(Socket *pDataSocket, bool pRtpActivated = true);
    MediaSourceNet(unsigned int pPortNumber, enum TransportType pTransportType, bool pRtpActivated = true);

    /// The destructor
    virtual ~MediaSourceNet();

    unsigned int getListenerPort();

    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);

    virtual void StopGrabbing();

    virtual void* Run(void* pArgs = NULL);

private:
    void Init(Socket *pDataSocket, bool pRtpActivated = true);

protected:

    char                *mPacketBuffer;
    unsigned int        mListenerPort;
    Socket              *mDataSocket;
    bool				mListenerRunning;
    bool                mListenerSocketOutside;
    int                 mReceiveErrors;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
