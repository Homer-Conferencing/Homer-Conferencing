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
 * Purpose: Dialog for fine tuning of audio silence suppresion
 * Author:  Thomas Volkert
 * Since:   2013-01-06
 */

#ifndef _CONFIGURATION_AUDIO_SILENCE_DIALOG_
#define _CONFIGURATION_AUDIO_SILENCE_DIALOG_

#include <Widgets/AudioWidget.h>

#include <ui_ConfigurationAudioSilenceDialog.h>

namespace Homer { namespace Gui {
using namespace Homer::Multimedia;

///////////////////////////////////////////////////////////////////////////////

class ConfigurationAudioSilenceDialog :
    public QDialog,
    public Ui_ConfigurationAudioSilenceDialog
{
    Q_OBJECT;
public:
    /// The default constructor
    ConfigurationAudioSilenceDialog(QWidget* pParent, AudioWorkerThread *pWorker);

    /// The destructor.
    virtual ~ConfigurationAudioSilenceDialog();

    virtual int exec();

private slots:
	void ChangedThreshold(int pValue);
    void ClickedButton(QAbstractButton *pButton);

private:
    virtual void timerEvent(QTimerEvent *pEvent);
    void initializeGUI();
    void LoadConfiguration();
    void SaveConfiguration();

    AudioWorkerThread 	*mWorker;
    int					mLastAudioLevel;
    int64_t				mLastSkippedChunks;
    /* periodic tasks */
    int                 mTimerId;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
