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
 * Purpose: Implementation of a dialog for fine tuning of audio silence suppresion
 * Author:  Thomas Volkert
 * Since:   2013-01-06
 */

#include <Dialogs/ConfigurationAudioSilenceDialog.h>
#include <Configuration.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

ConfigurationAudioSilenceDialog::ConfigurationAudioSilenceDialog(QWidget* pParent, AudioWorkerThread *pWorker) :
    QDialog(pParent)
{
	mWorker = pWorker;
    initializeGUI();
    LoadConfiguration();

    mLastAudioLevel = 0;
    mLastSkippedChunks = 0;

    // trigger periodic timer event
    mTimerId = startTimer(50);
}

ConfigurationAudioSilenceDialog::~ConfigurationAudioSilenceDialog()
{
    if (mTimerId != -1)
        killTimer(mTimerId);
}

///////////////////////////////////////////////////////////////////////////////

void ConfigurationAudioSilenceDialog::initializeGUI()
{
    setupUi(this);
    connect(mSbThreshold, SIGNAL(valueChanged(int)), this, SLOT(ChangedThreshold(int)));
}

void ConfigurationAudioSilenceDialog::ChangedThreshold(int pValue)
{
	mPbSilenceRange->setValue(pValue);
	mWorker->SetSkipSilenceThreshold(pValue);
}

int ConfigurationAudioSilenceDialog::exec()
{
	int tResult;
	int tPreviousThreshold = mWorker->GetSkipSilenceThreshold();

	tResult = QDialog::exec();
	if (tResult == QDialog::Accepted)
	{// user acknowledged the settings
		SaveConfiguration();
	}else
	{// restore former settings
		mWorker->SetSkipSilenceThreshold(tPreviousThreshold);
	}

	return tResult;
}

void ConfigurationAudioSilenceDialog::LoadConfiguration()
{
	mSbThreshold->setValue(mWorker->GetSkipSilenceThreshold());
	mPbSilenceRange->setValue(mWorker->GetSkipSilenceThreshold());
	mLbSkippedChunks->setText(QString("%1").arg(mWorker->GetSkipSilenceSkippedChunks()));
	mPbLevel->setValue(100 - mWorker->GetLastAudioLevel());
}

void ConfigurationAudioSilenceDialog::SaveConfiguration()
{
	CONF.SetAudioSkipSilenceThreshold(mSbThreshold->value());
	mWorker->SetSkipSilenceThreshold(mSbThreshold->value());
}

void ConfigurationAudioSilenceDialog::timerEvent(QTimerEvent *pEvent)
{
	if (pEvent->timerId() != mTimerId)
    {
        LOG(LOG_WARN, "Qt event timer ID %d doesn't match the expected one %d", pEvent->timerId(), mTimerId);
        pEvent->ignore();
        return;
    }

	int64_t tSkippedChunks = mWorker->GetSkipSilenceSkippedChunks();
	if (mLastSkippedChunks != tSkippedChunks)
	{
		mLastSkippedChunks = tSkippedChunks;
		mLbSkippedChunks->setText(QString("%1").arg(mLastSkippedChunks));
	}

	int tAudioLevel = mWorker->GetLastAudioLevel();
	if (mLastAudioLevel != tAudioLevel)
	{
		mLastAudioLevel = tAudioLevel;
		mPbLevel->setValue(100 - tAudioLevel);
	}
}

void ConfigurationAudioSilenceDialog::ClickedButton(QAbstractButton *pButton)
{
    if (mBbButtons->standardButton(pButton) == QDialogButtonBox::Reset)
    {
    	mSbThreshold->setValue(128);
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
