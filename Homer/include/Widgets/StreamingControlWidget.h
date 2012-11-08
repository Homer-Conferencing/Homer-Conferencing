/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: tool buttons for streaming control
 * Author:  Thomas Volkert
 * Since:   2010-11-17
 */

#ifndef _STREAMING_CONTROL_WIDGET
#define _STREAMING_CONTROL_WIDGET

#include <MediaSourceDesktop.h>
#include <Widgets/VideoWidget.h>
#include <Widgets/AudioWidget.h>
#include <Widgets/ParticipantWidget.h>

#include <ui_StreamingControlWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class StreamingControlWidget :
    public QWidget,
    public Ui_StreamingControlWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    StreamingControlWidget(ParticipantWidget* pBroadcastParticipantWidget, MediaSourceDesktop *pMediaSourceDesktop);

    /// The destructor.
    virtual ~StreamingControlWidget();

    void SetVideoInputSelectionVisible(bool pVisible = true);

private slots:
    void StartScreenSegmentStreaming();
    void StartVoiceStreaming();
    void StartCameraStreaming();
    void StartFileStreaming();
	void SelectedNewVideoInputStream(int pIndex);
	void SelectPushToTalkMode(bool pActive);

private:
    void timerEvent(QTimerEvent *pEvent);
    void initializeGUI();

    VideoWorkerThread       *mVideoWorker;
    AudioWorkerThread       *mAudioWorker;
    ParticipantWidget       *mBroadcastParticipantWidget;
    MediaSourceDesktop      *mMediaSourceDesktop;
    int 					mTimerId;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
