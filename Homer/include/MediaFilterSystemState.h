/*****************************************************************************
 *
 * Copyright (C) 2013 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: media filter "system state"
 * Since:   2013-12-09
 */

#ifndef _MULTIMEDIA_MEDIA_FILTER_SYSTEM_STATE_
#define _MULTIMEDIA_MEDIA_FILTER_SYSTEM_STATE_

#include <MediaFilter.h>
#include <Header_Ffmpeg.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

class MediaFilterSystemState:
    public MediaFilter
{
public:
    MediaFilterSystemState(MediaSource *pMediaSource);

    virtual ~MediaFilterSystemState();

    // filter a chunk: either an RGB32 picture or a raw audio chunk
    virtual void FilterChunk(char* pChunkBuffer, unsigned int pChunkBufferSize, int64_t pChunkbufferNumber, AVStream *pStream, bool pIsKeyFrame);
protected:
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
