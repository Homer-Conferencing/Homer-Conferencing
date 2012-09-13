/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Dialog to open a video/audio preview
 * Author:  Thomas Volkert
 * Since:   2011-11-28
 */

#ifndef _OPEN_VIDEO_AUDIO_PREVIEW_DIALOG_
#define _OPEN_VIDEO_AUDIO_PREVIEW_DIALOG_

#include <ui_OpenVideoAudioPreviewDialog.h>
#include <MediaSource.h>

namespace Homer { namespace Gui {

using namespace Homer::Multimedia;

#define                             USE_GAPI

///////////////////////////////////////////////////////////////////////////////

class OpenVideoAudioPreviewDialog :
    public QDialog,
    public Ui_OpenVideoAudioPreviewDialog
{
    Q_OBJECT;
public:
    /// The default constructor
    OpenVideoAudioPreviewDialog(QWidget* pParent = NULL);

    /// The destructor.
    virtual ~OpenVideoAudioPreviewDialog();

    MediaSource* GetMediaSourceVideo();
    MediaSource* GetMediaSourceAudio();

    int exec();

    bool FileSourceSelected();

private slots:
    void ActionGetFile();

    void GAPIVideoSelectionChanged(QString pSelection);
    void GAPIAudioSelectionChanged(QString pSelection);

private:
    void initializeGUI();
    void SaveConfiguration();
    void LoadConfiguration();

    VideoDevices mVideoDevicesList;
    AudioDevices mAudioDevicesList;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
