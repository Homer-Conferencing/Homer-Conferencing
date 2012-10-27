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
 * Purpose: Implementation of an abstract media sink
 * Author:  Thomas Volkert
 * Since:   2009-01-06
 */

#include <MediaSink.h>
#include <Logger.h>
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
    mMediaId = "";
    mMaxFps = 0;
    mCodec = "";
    mRunning = true;
    mMaxFpsTimestampLastFragment = 0;
    mMaxFpsFrameNumberLastFragment = 0;
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

void MediaSink::Start()
{
    mRunning = true;
}

void MediaSink::Stop()
{
    mRunning = false;
}

string MediaSink::GetId()
{
	return mMediaId;
}

void MediaSink::SetMaxFps(int pMaxFps)
{
	LOG(LOG_VERBOSE, "Setting max. %d FPS", pMaxFps);
	mMaxFps = pMaxFps;
}

int MediaSink::GetMaxFps()
{
	return mMaxFps;
}

bool MediaSink::BelowMaxFps(int pFrameNumber)
{
    int64_t tCurrentTime = Time::GetTimeStamp();
    int64_t tTimeDiff = tCurrentTime - mMaxFpsTimestampLastFragment;

    //LOG(LOG_VERBOSE, "Checking max. FPS for frame number %d", pFrameNumber);

    if (mMaxFps != 0)
    {
        //### skip capturing when we are too slow
        if (tTimeDiff < 1000*1000 / mMaxFps)
        {
        	return false;
        }
    }

    if(mMaxFpsFrameNumberLastFragment != pFrameNumber)
    {
    	mMaxFpsTimestampLastFragment = tCurrentTime;
    	mMaxFpsFrameNumberLastFragment = pFrameNumber;
    }

    return true;
}

}} //namespace
