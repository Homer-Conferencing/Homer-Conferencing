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
 * Purpose: Implementation of the media filter "system state"
 * Since:   2013-12-09
 */

#include <QImage>
#include <QPainter>
#include <QDate>
#include <QTime>

#include <MediaFilterSystemState.h>
#include <MediaSource.h>

#include <Logger.h>
#include <string>

namespace Homer { namespace Multimedia {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

MediaFilterSystemState::MediaFilterSystemState(MediaSource *pMediaSource):
    MediaFilter(pMediaSource)

{
    mMediaId = "Date";
}

MediaFilterSystemState::~MediaFilterSystemState()
{
}

///////////////////////////////////////////////////////////////////////////////

void MediaFilterSystemState::FilterChunk(char* pChunkBuffer, unsigned int pChunkBufferSize, int64_t pChunkbufferNumber, AVStream *pStream, bool pIsKeyFrame)
{
    int tResX;
    int tResY;
    mMediaSource->GetVideoGrabResolution(tResX, tResY);

    //LOG(LOG_VERBOSE, "Got %d bytes of %s video chunk %"PRId64" with resolution %dx%d", pChunkBufferSize, mMediaSource->GetSourceTypeStr().c_str(), pChunkbufferNumber, tResX, tResY);
    QImage tImage = QImage((unsigned char*)pChunkBuffer, tResX, tResY, QImage::Format_RGB32);
    QPainter *tPainter = new QPainter(&tImage);

    float tScalingFactor = ((float)tResY / 400);
    QFont tFont = QFont("Arial", 8 * tScalingFactor, QFont::Normal);
    tFont.setFixedPitch(true);
    tPainter->setRenderHint(QPainter::TextAntialiasing, false);
    tPainter->setFont(tFont);
    QDate tDate = QDate::currentDate();
    QTime tTime = QTime::currentTime();
    tPainter->setPen(QColor(Qt::black));
    tPainter->drawText(tResX - 60 * tScalingFactor, 20 * tScalingFactor, tTime.toString("hh:mm:ss"));
    tPainter->drawText(tResX - 70 * tScalingFactor, tResY - 10 * tScalingFactor, tDate.toString("dd.MM.yyyy"));
    tPainter->setPen(QColor(Qt::white));
    tPainter->drawText(tResX - 60 * tScalingFactor - 1, 20 * tScalingFactor - 1, tTime.toString("hh:mm:ss"));
    tPainter->drawText(tResX - 70 * tScalingFactor - 1, tResY - 10 * tScalingFactor - 1, tDate.toString("dd.MM.yyyy"));


    delete tPainter;
}

}} //namespace
