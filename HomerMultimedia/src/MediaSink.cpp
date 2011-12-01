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
 * Purpose: Implementation of an abstract media sink
 * Author:  Thomas Volkert
 * Since:   2009-01-06
 */

#include <MediaSink.h>

#include <string>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

MediaSink::MediaSink(enum MediaSinkType pType):
    PacketStatistic()
{
    SetOutgoingStream();
    mPacketNumber = 0;
    mMaxFps = 0;
    switch(pType)
    {
        case MEDIA_SINK_VIDEO:
            ClassifyStream(DATA_TYPE_VIDEO);
            break;
        case MEDIA_SINK_AUDIO:
            ClassifyStream(DATA_TYPE_AUDIO);
            break;
        default:
            break;
    }
}

MediaSink::~MediaSink()
{
}

///////////////////////////////////////////////////////////////////////////////

void MediaSink::SetMaxFps(int pMaxFps)
{
	LOG(LOG_VERBOSE, "Setting max. %D FPS", pMaxFps);
	mMaxFps = pMaxFps;
}

int MediaSink::GetMaxFps()
{
	return mMaxFps;
}

}} //namespace
