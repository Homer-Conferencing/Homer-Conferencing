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
 * Name:    MediaSink.h
 * Purpose: abstract media sink
 * Author:  Thomas Volkert
 * Since:   2009-01-06
 * Version: $Id$
 */

#ifndef _MULTIMEDIA_MEDIA_SINK_
#define _MULTIMEDIA_MEDIA_SINK_

#include <PacketStatistic.h>
#include <Header_Ffmpeg.h>

#include <string>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

enum MediaSinkType
{
    MEDIA_SINK_UNKNOWN = -1,
    MEDIA_SINK_VIDEO,
    MEDIA_SINK_AUDIO
};

class MediaSink:
    public Homer::Monitor::PacketStatistic
{

public:
    MediaSink(enum MediaSinkType pType= MEDIA_SINK_UNKNOWN);

    virtual ~MediaSink();

public:
    virtual void ProcessPacket(char* pPacketData, unsigned int pPacketSize, AVStream *pStream) = 0;

    std::string GetId() { return mMediaId; }

protected:
    unsigned long       mPacketNumber;
    std::string         mMediaId;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
