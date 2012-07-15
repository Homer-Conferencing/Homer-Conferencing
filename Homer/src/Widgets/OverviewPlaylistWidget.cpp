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
 * Purpose: Implementation of OverviewPlaylistWidget
 * Author:  Thomas Volkert
 * Since:   2011-03-22
 */

#include <Widgets/OverviewPlaylistWidget.h>
#include <Configuration.h>
#include <Logger.h>
#include <Snippets.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QTimerEvent>
#include <QHeaderView>
#include <QFileDialog>
#include <QSizePolicy>
#include <QMenu>
#include <QUrl>
#include <QContextMenuEvent>

namespace Homer { namespace Gui {

#define PLAYLIST_UPDATE_DELAY         250 //ms
#define ALLOWED_AV_TIME_DIFF			2 //s

///////////////////////////////////////////////////////////////////////////////

OverviewPlaylistWidget::OverviewPlaylistWidget(QAction *pAssignedAction, QMainWindow* pMainWindow, int pPlaylistId, VideoWorkerThread *pVideoWorker, AudioWorkerThread *pAudioWorker):
    QDockWidget(pMainWindow)
{
	mPlaylistId = pPlaylistId;
    mAssignedAction = pAssignedAction;
    mVideoWorker = pVideoWorker;
    mAudioWorker = pAudioWorker;
    mCurrentFileId = -1;
    mTimerId = -1;
    mIsPlayed = false;
    mEndlessLoop = false;

    initializeGUI();

    setAllowedAreas(Qt::AllDockWidgetAreas);
    pMainWindow->addDockWidget(Qt::LeftDockWidgetArea, this);

    if (mAssignedAction != NULL)
    {
        connect(mAssignedAction, SIGNAL(triggered(bool)), this, SLOT(SetVisible(bool)));
        mAssignedAction->setChecked(false);
    }
    connect(toggleViewAction(), SIGNAL(toggled(bool)), mAssignedAction, SLOT(setChecked(bool)));
    connect(mTbAdd, SIGNAL(clicked()), this, SLOT(AddEntryDialog()));
    connect(mTbDel, SIGNAL(clicked()), this, SLOT(DelEntryDialog()));
    connect(mTbSaveList, SIGNAL(clicked()), this, SLOT(SaveListDialog()));
    connect(mLwFiles, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(PlayItem(QListWidgetItem *)));
    mShortcutDel = new QShortcut(Qt::Key_Delete, mLwFiles);
    mShortcutIns = new QShortcut(Qt::Key_Insert, mLwFiles);
    mShortcutDel->setEnabled(true);
    mShortcutIns->setEnabled(true);
    connect(mShortcutDel, SIGNAL(activated()), this, SLOT(DelEntryDialogSc()));
    connect(mShortcutDel, SIGNAL(activatedAmbiguously()), this, SLOT(DelEntryDialogSc()));
    connect(mShortcutIns, SIGNAL(activated()), this, SLOT(AddEntryDialogSc()));
    connect(mShortcutIns, SIGNAL(activatedAmbiguously()), this, SLOT(AddEntryDialogSc()));
    connect(mTbPlay, SIGNAL(clicked()), this, SLOT(ActionPlay()));
    connect(mTbPause, SIGNAL(clicked()), this, SLOT(ActionPause()));
    connect(mTbStop, SIGNAL(clicked()), this, SLOT(ActionStop()));
    connect(mTbNext, SIGNAL(clicked()), this, SLOT(ActionNext()));
    connect(mTbLast, SIGNAL(clicked()), this, SLOT(ActionLast()));
    mTimerId = startTimer(PLAYLIST_UPDATE_DELAY);
    switch(mPlaylistId)
    {
		case PLAYLIST_VIDEO:
			setWindowTitle("Video playlist");
			SetVisible(CONF.GetVisibilityPlaylistWidgetVideo());
		    mAssignedAction->setChecked(CONF.GetVisibilityPlaylistWidgetVideo());
			break;
		case PLAYLIST_AUDIO:
			setWindowTitle("Audio playlist");
			SetVisible(CONF.GetVisibilityPlaylistWidgetAudio());
		    mAssignedAction->setChecked(CONF.GetVisibilityPlaylistWidgetAudio());
			break;
		case PLAYLIST_MOVIE:
			setWindowTitle("Movie playlist");
			SetVisible(CONF.GetVisibilityPlaylistWidgetMovie());
		    mAssignedAction->setChecked(CONF.GetVisibilityPlaylistWidgetMovie());
			break;
		default:
			break;
    }
}

OverviewPlaylistWidget::~OverviewPlaylistWidget()
{
	if (mTimerId != -1)
		killTimer(mTimerId);

    switch(mPlaylistId)
    {
		case PLAYLIST_VIDEO:
		    CONF.SetVisibilityPlaylistWidgetVideo(isVisible());
			break;
		case PLAYLIST_AUDIO:
		    CONF.SetVisibilityPlaylistWidgetAudio(isVisible());
			break;
		case PLAYLIST_MOVIE:
		    CONF.SetVisibilityPlaylistWidgetMovie(isVisible());
			break;
		default:
			break;
    }
}

///////////////////////////////////////////////////////////////////////////////

void OverviewPlaylistWidget::initializeGUI()
{
    setupUi(this);

    // hide id column
//    mTwFiles->setColumnHidden(5, true);
//    mTwFiles->sortItems(5);
//    mTwFiles->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
//    for (int i = 0; i < 2; i++)
//        mTwFiles->horizontalHeader()->resizeSection(i, mTwFiles->horizontalHeader()->sectionSize(i) * 2);
}

void OverviewPlaylistWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewPlaylistWidget::SetVisible(bool pVisible)
{
    if (pVisible)
    {
        move(mWinPos);
        show();
    }else
    {
        mWinPos = pos();
        hide();
    }
}

void OverviewPlaylistWidget::StartPlaylist()
{
	if (!mIsPlayed)
	{
		if (mLwFiles->count() == 0)
			AddEntryDialog();

		PlayItem(mLwFiles->item(mCurrentFileId));
	}
}

void OverviewPlaylistWidget::StopPlaylist()
{
	mIsPlayed = false;
}

void OverviewPlaylistWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction("Add an entry");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/22_22/Plus.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    if (!mLwFiles->selectedItems().isEmpty())
    {
        tAction = tMenu.addAction("Delete selected");
        QIcon tIcon2;
        tIcon2.addPixmap(QPixmap(":/images/22_22/Minus.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon2);
    }

    tMenu.addSeparator();

    tAction = tMenu.addAction("Endless loop");
    tAction->setCheckable(true);
    tAction->setChecked(mEndlessLoop);

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Add an entry") == 0)
        {
            AddEntryDialog();
            return;
        }
        if (tPopupRes->text().compare("Delete selected") == 0)
        {
            DelEntryDialog();
            return;
        }
        if (tPopupRes->text().compare("Endless loop") == 0)
        {
            mEndlessLoop = !mEndlessLoop;
            LOG(LOG_VERBOSE, "Playlist %d has now endless loop activation %d", mPlaylistId, mEndlessLoop);
            return;
        }
    }
}

void OverviewPlaylistWidget::DelEntryDialog()
{
    QList<QListWidgetItem*> tItems = mLwFiles->selectedItems();

    if (tItems.isEmpty())
        return;

    QListWidgetItem* tItem;
    foreach(tItem, tItems)
    {
        mLwFiles->removeItemWidget(tItem);
        delete tItem;
    }
}

static QString sAllLoadVideoFilter = "All supported formats (*.asf *.avi *.dv *.mkv *.mov *.mpg *.mpeg *.mp4 *.mp4a *.m3u *.swf *.vob *.wmv *.3gp)";
static QString sLoadVideoFilters = sAllLoadVideoFilter + ";;"\
                    "Advanced Systems Format (*.asf);;"\
                    "Audio Video Interleave Format (*.avi);;"\
                    "Digital Video Format (*.dv);;"\
                    "Matroska Format (*.mkv);;"\
                    "MPEG-Program Stream Format (*.mpg *.mpeg);;"\
                    "Playlist file (*.m3u);;"\
                    "Quicktime/MPEG4 Format (*.mov *.mp4 *.mp4a *.3gp);;"\
                    "Small Web Format (*.swf);;"\
                    "Video Object Format (*.vob);;" \
                    "Windows Media Video Format (*.wmv)";

QStringList OverviewPlaylistWidget::LetUserSelectVideoFile(QWidget *pParent, QString pDescription, bool pMultipleFiles)
{
    QStringList tResult;

    if (pMultipleFiles)
        tResult = QFileDialog::getOpenFileNames(pParent, pDescription,
                                                                CONF.GetDataDirectory() + "/Homer-Video.avi",
                                                                sLoadVideoFilters,
                                                                &sAllLoadVideoFilter,
                                                                QFileDialog::DontUseNativeDialog);
    else
        tResult = QStringList(QFileDialog::getOpenFileName(pParent,  pDescription,
                                                                CONF.GetDataDirectory() + "/Homer-Video.avi",
                                                                sLoadVideoFilters,
                                                                &sAllLoadVideoFilter,
                                                                QFileDialog::DontUseNativeDialog));

    if (!tResult.isEmpty())
        CONF.SetDataDirectory(tResult.first().left(tResult.first().lastIndexOf('/')));

    return tResult;
}

static QString sAllSaveVideoFilter = "All supported formats (*.avi *.mov *.mp4 *.mp4a *.3gp)";
static QString sSaveVideoFilters = sAllSaveVideoFilter + ";;"\
                    "Audio Video Interleave Format (*.avi);;"\
                    "Quicktime/MPEG4 Format (*.mov *.mp4 *.mp4a *.3gp)";

QString OverviewPlaylistWidget::LetUserSelectVideoSaveFile(QWidget *pParent, QString pDescription)
{
    QString tResult = QFileDialog::getSaveFileName(pParent,  pDescription,
                                                            CONF.GetDataDirectory() + "/Homer-Video.avi",
                                                            sSaveVideoFilters,
                                                            &sAllSaveVideoFilter,
                                                            QFileDialog::DontUseNativeDialog);

    if (!tResult.isEmpty())
        CONF.SetDataDirectory(tResult.left(tResult.lastIndexOf('/')));

    return tResult;
}

bool OverviewPlaylistWidget::IsVideoFile(QString pFileName)
{
    pFileName = QString(pFileName.toLocal8Bit());

    int tPos = pFileName.lastIndexOf('.', -1);
    if (tPos == -1)
    {
        LOGEX(OverviewPlaylistWidget, LOG_ERROR, "Video file name lacks a correct format selecting end");
        return false;
    }

    QString tExt = pFileName.right(pFileName.size() - tPos).toLower();
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Checking for video content in file %s of type %s", pFileName.toStdString().c_str(), tExt.toStdString().c_str());

    if (sLoadVideoFilters.indexOf(tExt, 0) != -1)
        return true;
    else
        return false;
}

static QString sAllLoadAudioFilter =  "All supported formats (*.mp3 *.avi *.mka *.mkv *.m3u *.wav)";
static QString sLoadAudioFilters =  sAllLoadAudioFilter + ";;"\
                    "Audio Video Interleave Format (*.avi);;"\
                    "Matroska Format (*.mka);;"\
                    "MPEG Audio Layer 2/3 Format (*.mp3);;"\
                    "Playlist file (*.m3u);;"\
                    "Waveform Audio File Format (*.wav)";

QStringList OverviewPlaylistWidget::LetUserSelectAudioFile(QWidget *pParent, QString pDescription, bool pMultipleFiles)
{
    QStringList tResult;

    if (pMultipleFiles)
        tResult = QFileDialog::getOpenFileNames(pParent,  pDescription,
                                                                CONF.GetDataDirectory() + "/Homer-Audio.mp3",
                                                                sLoadAudioFilters,
                                                                &sAllLoadAudioFilter,
                                                                QFileDialog::DontUseNativeDialog);
    else
        tResult = QStringList(QFileDialog::getOpenFileName(pParent,  pDescription,
                                                                CONF.GetDataDirectory() + "/Homer-Audio.mp3",
                                                                sLoadAudioFilters,
                                                                &sAllLoadAudioFilter,
                                                                QFileDialog::DontUseNativeDialog));

    if (!tResult.isEmpty())
        CONF.SetDataDirectory(tResult.first().left(tResult.first().lastIndexOf('/')));

    return tResult;
}

static QString sAllSaveAudioFilter =  "All supported formats (*.mp3 *.wav)";
static QString sSaveAudioFilters =  sAllSaveAudioFilter + ";;"\
                    "MPEG Audio Layer 2/3 Format (*.mp3);;"\
                    "Waveform Audio File Format (*.wav)";

QString OverviewPlaylistWidget::LetUserSelectAudioSaveFile(QWidget *pParent, QString pDescription)
{
    QString tResult = QFileDialog::getSaveFileName(pParent,  pDescription,
                                                            CONF.GetDataDirectory() + "/Homer-Audio.mp3",
                                                            sSaveAudioFilters,
                                                            &sAllSaveAudioFilter,
                                                            QFileDialog::DontUseNativeDialog);

    if (!tResult.isEmpty())
        CONF.SetDataDirectory(tResult.left(tResult.lastIndexOf('/')));

    return tResult;
}

bool OverviewPlaylistWidget::IsAudioFile(QString pFileName)
{
    pFileName = QString(pFileName.toLocal8Bit());

    int tPos = pFileName.lastIndexOf('.', -1);
    if (tPos == -1)
    {
        LOGEX(OverviewPlaylistWidget, LOG_ERROR, "Audio file name lacks a correct format selecting end");
        return false;
    }

    QString tExt = pFileName.right(pFileName.size() - tPos).toLower();
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Checking for audio content in file %s of type %s", pFileName.toStdString().c_str(), tExt.toStdString().c_str());

    if (sLoadAudioFilters.indexOf(tExt, 0) != -1)
        return true;
    else
        return false;
}

QString sAllLoadMovieFilter = "All supported formats (*.avi *.mkv *.mov *.mpeg *.mp4 *.mp4a *.m3u *.swf *.vob *.wmv *.3gp)";
QString sLoadMovieFilters =  sAllLoadMovieFilter + ";;"\
                    "Audio Video Interleave Format (*.avi);;"\
                    "Matroska Format (*.mkv);;"\
                    "MPEG-Program Stream Format (*.mpeg);;"\
                    "Playlist file (*.m3u);;"\
                    "Quicktime/MPEG4 Format (*.mov *.mp4 *.mp4a *.3gp);;"\
                    "Small Web Format (*.swf);;"\
                    "Video Object Format (*.vob);;" \
                    "Windows Media Video Format (*.wmv)";

QStringList OverviewPlaylistWidget::LetUserSelectMovieFile(QWidget *pParent, QString pDescription, bool pMultipleFiles)
{
    QStringList tResult;

    if (pMultipleFiles)
        tResult = QFileDialog::getOpenFileNames(pParent,  pDescription,
                                                                CONF.GetDataDirectory() + "/Homer-Movie.avi",
                                                                sLoadMovieFilters,
                                                                &sAllLoadMovieFilter,
                                                                QFileDialog::DontUseNativeDialog);
    else
        tResult = QStringList(QFileDialog::getOpenFileName(pParent,  pDescription,
                                                                CONF.GetDataDirectory() + "/Homer-Movie.avi",
                                                                sLoadMovieFilters,
                                                                &sAllLoadMovieFilter,
                                                                QFileDialog::DontUseNativeDialog));

    if (!tResult.isEmpty())
        CONF.SetDataDirectory(tResult.first().left(tResult.first().lastIndexOf('/')));

    return tResult;
}

void OverviewPlaylistWidget::AddEntryDialog()
{
    bool tListWasEmpty = (mLwFiles->count() == 0);

	QStringList tFileNames;

	switch(mPlaylistId)
	{
		case PLAYLIST_VIDEO:
		    tFileNames = LetUserSelectVideoFile(this, "Add video files to playlist");
		    break;
		case PLAYLIST_AUDIO:
			tFileNames = LetUserSelectAudioFile(this, "Add audio files to playlist");
			break;
		case PLAYLIST_MOVIE:
			tFileNames = LetUserSelectMovieFile(this,  "Add movie files to playlist");
			break;
		default:
			break;
	}

    if (tFileNames.isEmpty())
        return;

    QString tFile;
    foreach(tFile, tFileNames)
    {
        AddFileToList(tFile);
    }

    if((tListWasEmpty) && (mLwFiles->count() > 0))
    {
        mCurrentFileId = 0;
        LOG(LOG_VERBOSE, "Setting to file %d in playlist %d", mCurrentFileId, mPlaylistId);
        mLwFiles->setCurrentRow(mCurrentFileId, QItemSelectionModel::Clear | QItemSelectionModel::Select);
        if (!isVisible())
        	SetVisible(true);
    }
}

void OverviewPlaylistWidget::AddEntryDialogSc()
{
    if (mLwFiles->hasFocus())
        AddEntryDialog();
}

void OverviewPlaylistWidget::DelEntryDialogSc()
{
    if (mLwFiles->hasFocus())
        DelEntryDialog();
}

void OverviewPlaylistWidget::SaveListDialog()
{
    QString tFileName;
    tFileName = QFileDialog::getSaveFileName(this,  "Save playlist to..",
                                                                CONF.GetDataDirectory() + "/Homer.m3u",
                                                                "Playlist file (*.m3u)",
                                                                &*(new QString("Playlist file (*.m3u)")),
                                                                QFileDialog::DontUseNativeDialog);

    if (tFileName.isEmpty())
        return;

    QString tPlaylistData;
    QFile tPlaylistFile(tFileName);
    for (int i = 0; i < mLwFiles->count(); i++)
    {
        QListWidgetItem* tItem = mLwFiles->item(i);
        QString tEntry = tItem->data(Qt::DisplayRole).toString();
        LOG(LOG_VERBOSE, "Writing to m3u: %s the entry %s", tFileName.toStdString().c_str(), tEntry.toStdString().c_str());
        tPlaylistData += tEntry + '\n';
    }
    if (!tPlaylistFile.open(QIODevice::WriteOnly))
    {
    	ShowError("Could not store playlist file", "Couldn't write playlist in " + tFileName);
        return;
    }

    tPlaylistFile.write(tPlaylistData.toUtf8());
    tPlaylistFile.close();
}

void OverviewPlaylistWidget::PlayItem(QListWidgetItem *pItem)
{
	if (pItem == NULL)
		return;

	mIsPlayed = true;
	mCurrentFile = pItem->data(Qt::DisplayRole).toString();
    switch(mPlaylistId)
    {
        case PLAYLIST_VIDEO:
            mVideoWorker->PlayFile(mCurrentFile);
            break;
        case PLAYLIST_AUDIO:
            mAudioWorker->PlayFile(mCurrentFile);
            break;
        case PLAYLIST_MOVIE:
            mVideoWorker->PlayFile(mCurrentFile);
            mAudioWorker->PlayFile(mCurrentFile);
            break;
        default:
            break;
    }
    mCurrentFileId = mLwFiles->row(pItem);
    LOG(LOG_VERBOSE, "Setting current row to %d in playlist %d", mCurrentFileId, mPlaylistId);
    mLwFiles->setCurrentRow(mCurrentFileId, QItemSelectionModel::Clear | QItemSelectionModel::Select);
}

void OverviewPlaylistWidget::PlayNext()
{
    int tNewFileId = -1;

    // derive file id of next file which should be played
	if (mCurrentFileId < mLwFiles->count() -1)
    {
		tNewFileId = mCurrentFileId + 1;
    }else
    {
    	if (mEndlessLoop)
    	{
    		tNewFileId = 0;
    	}else
    	{
    		//LOG(LOG_VERBOSE, "End of playlist %d reached", mPlaylistId);
    		return;
    	}
    }

	LOG(LOG_VERBOSE, "Playing file entry %d", tNewFileId);

	// finally play the next file
	switch(mPlaylistId)
	{
		case PLAYLIST_VIDEO:
			PlayItem(mLwFiles->item(tNewFileId));
			break;
		case PLAYLIST_AUDIO:
			PlayItem(mLwFiles->item(tNewFileId));
			break;
		case PLAYLIST_MOVIE:
			PlayItem(mLwFiles->item(tNewFileId));
			break;
		default:
			break;
	}
}

void OverviewPlaylistWidget::PlayLast()
{
	switch(mPlaylistId)
    {
        case PLAYLIST_VIDEO:
            if (mCurrentFileId > 0)
                PlayItem(mLwFiles->item(mCurrentFileId - 1));
            break;
        case PLAYLIST_AUDIO:
            if (mCurrentFileId > 0)
                PlayItem(mLwFiles->item(mCurrentFileId - 1));
            break;
        case PLAYLIST_MOVIE:
            if (mCurrentFileId > 0)
                PlayItem(mLwFiles->item(mCurrentFileId - 1));
            break;
        default:
            break;
    }
}

void OverviewPlaylistWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif
    if (pEvent->timerId() == mTimerId)
    {
//    	if (mPlaylistId == PLAYLIST_VIDEO)
//    		LOG(LOG_ERROR, "mCurrentFileId %d mLwFiles->count() %d", mCurrentFileId, mLwFiles->count());

		#ifdef SYNCHRONIZE_AUDIO_VIDEO
			// synch. video and audio by seeking in audio stream based on the position of the video stream, the other way round it would be more time consuming!
			int64_t tTimeDiff = mVideoWorker->GetSeekPos() - mAudioWorker->GetSeekPos();
			if ((mPlaylistId == PLAYLIST_MOVIE) && ((tTimeDiff < -ALLOWED_AV_TIME_DIFF) || (tTimeDiff > ALLOWED_AV_TIME_DIFF)))
			{
				if ((!mVideoWorker->EofReached()) && (!mAudioWorker->EofReached()) && (mVideoWorker->GetCurrentDevice() == mAudioWorker->GetCurrentDevice()))
				{
					LOG(LOG_ERROR, "AV stream asynchronous (difference: %lld seconds, max is %d seconds), synchronizing now..", tTimeDiff, ALLOWED_AV_TIME_DIFF);
					mAudioWorker->Seek(mVideoWorker->GetSeekPos());
				}
			}
		#endif

    	// play next if EOF is reached
    	switch(mPlaylistId)
        {
            case PLAYLIST_VIDEO:
            	// stop if current file wasn't yet switched to the desired one;
            	if (mVideoWorker->CurrentFile() != mCurrentFile)
            		return;

            	// do we already play the desired file and are we at EOF?
                if ((mLwFiles->item(mCurrentFileId) != NULL) && (mVideoWorker->GetCurrentDevice().contains(mLwFiles->item(mCurrentFileId)->data(Qt::DisplayRole).toString())) && (mVideoWorker->EofReached()))
                {
                	PlayNext();
                }
                break;
            case PLAYLIST_AUDIO:
            	// stop if current file wasn't yet switched to the desired one;
            	if (mAudioWorker->CurrentFile() != mCurrentFile)
            		return;

            	// do we already play the desired file and are we at EOF?
                if ((mLwFiles->item(mCurrentFileId) != NULL) && (mAudioWorker->GetCurrentDevice().contains(mLwFiles->item(mCurrentFileId)->data(Qt::DisplayRole).toString())) && (mAudioWorker->EofReached()))
                {
                	PlayNext();
                }
                break;
            case PLAYLIST_MOVIE:
            	// stop if current file wasn't yet switched to the desired one;
            	if ((mVideoWorker->CurrentFile() != mCurrentFile) || (mAudioWorker->CurrentFile() != mCurrentFile))
            		return;

            	// do we already play the desired file and are we at EOF?
                if ((mLwFiles->item(mCurrentFileId) != NULL) &&
                	(mVideoWorker->GetCurrentDevice().contains(mLwFiles->item(mCurrentFileId)->data(Qt::DisplayRole).toString())) && (mVideoWorker->EofReached()) &&
                	(mAudioWorker->GetCurrentDevice().contains(mLwFiles->item(mCurrentFileId)->data(Qt::DisplayRole).toString())) && (mAudioWorker->EofReached()))
                {
                	PlayNext();
                }
                break;
            default:
                break;
        }
    }
}

void OverviewPlaylistWidget::dragEnterEvent(QDragEnterEvent *pEvent)
{
    if (pEvent->mimeData()->hasUrls())
    {
        pEvent->acceptProposedAction();
        QList<QUrl> tList = pEvent->mimeData()->urls();
        QUrl tUrl;
        int i = 0;

        foreach(tUrl, tList)
            LOG(LOG_VERBOSE, "New drag+drop url (%d) \"%s\"", ++i, tUrl.toString().toStdString().c_str());
        return;
    }
}

void OverviewPlaylistWidget::dropEvent(QDropEvent *pEvent)
{
    bool tListWasEmpty = (mLwFiles->count() == 0);

    if (pEvent->mimeData()->hasUrls())
    {
        LOG(LOG_VERBOSE, "Got some dropped urls");
        QList<QUrl> tUrlList = pEvent->mimeData()->urls();
        QUrl tUrl;
        foreach(tUrl, tUrlList)
        {
            AddFileToList(tUrl.toLocalFile());
        }
        pEvent->acceptProposedAction();
        return;
    }

    if ((tListWasEmpty) && (mLwFiles->count() > 0) && (mIsPlayed))
        PlayItem(mLwFiles->item(mCurrentFileId));
}

void OverviewPlaylistWidget::AddM3UToList(QString pFilePlaylist)
{
    QString tDir = pFilePlaylist.left(pFilePlaylist.lastIndexOf('/'));
    LOG(LOG_VERBOSE, "Opening playlist file %s", pFilePlaylist.toStdString().c_str());
    LOG(LOG_VERBOSE, "..in directory: %s", tDir.toStdString().c_str());

    QFile tPlaylistFile(pFilePlaylist);
    if (!tPlaylistFile.open(QIODevice::ReadOnly))
    {
        LOG(LOG_ERROR, "Couldn't read playlist from %s", pFilePlaylist.toStdString().c_str());
    }else
    {
        QByteArray tLine;
        tLine = tPlaylistFile.readLine();
        while (!tLine.isEmpty())
        {
            QString tLineString = QString(tLine);
            while((tLineString.endsWith(QChar(0x0A))) || (tLineString.endsWith(QChar(0x0D))))
                tLineString = tLineString.left(tLineString.length() - 1); //remove any "new line" char from the end

            if (!tLineString.startsWith("#EXT"))
            {
                LOG(LOG_VERBOSE, "Found playlist entry: %s", tLineString.toStdString().c_str());
                if (tLineString.startsWith("http://"))
                    AddFileToList(tLineString);
                else
                    AddFileToList(tDir + "/" + tLineString);
            }else
                LOG(LOG_VERBOSE, "Found playlist extended entry: %s", tLineString.toStdString().c_str());

            tLine = tPlaylistFile.readLine();
        }
    }
}

void OverviewPlaylistWidget::AddFileToList(QString pFile)
{
    if (pFile.endsWith(".m3u"))
    {
        AddM3UToList(pFile);
    }else
    {
        LOG(LOG_VERBOSE, "Adding to playlist: %s", pFile.toStdString().c_str());
        switch(mPlaylistId)
        {
            case PLAYLIST_VIDEO:
                mLwFiles->addItem(new QListWidgetItem(QIcon(":/images/VideoReel.png"), pFile));
                break;
            case PLAYLIST_AUDIO:
                mLwFiles->addItem(new QListWidgetItem(QIcon(":/images/Speaker.png"), pFile));
                break;
            case PLAYLIST_MOVIE:
                mLwFiles->addItem(new QListWidgetItem(QIcon(":/images/22_22/ArrowRight.png"), pFile));
                break;
            default:
                break;
        }
    }
}

void OverviewPlaylistWidget::ActionPlay()
{
	LOG(LOG_VERBOSE, "Triggered play");
	PlayItem(mLwFiles->item(mCurrentFileId));
}

void OverviewPlaylistWidget::ActionPause()
{
	if (!mIsPlayed)
		return;

	LOG(LOG_VERBOSE, "Triggered pause");
    switch(mPlaylistId)
    {
        case PLAYLIST_VIDEO:
        	mVideoWorker->PauseFile();
            break;
        case PLAYLIST_AUDIO:
        	mAudioWorker->PauseFile();
            break;
        case PLAYLIST_MOVIE:
        	mVideoWorker->PauseFile();
        	mAudioWorker->PauseFile();
            break;
        default:
            break;
    }
}

void OverviewPlaylistWidget::ActionStop()
{
	if (!mIsPlayed)
		return;

	LOG(LOG_VERBOSE, "Triggered stop");
    switch(mPlaylistId)
    {
        case PLAYLIST_VIDEO:
        	mVideoWorker->StopFile();
            break;
        case PLAYLIST_AUDIO:
        	mAudioWorker->StopFile();
            break;
        case PLAYLIST_MOVIE:
        	mVideoWorker->StopFile();
        	mAudioWorker->StopFile();
            break;
        default:
            break;
    }
}

void OverviewPlaylistWidget::ActionLast()
{
	if (!mIsPlayed)
		return;

	LOG(LOG_VERBOSE, "Triggered last");
	PlayLast();
}

void OverviewPlaylistWidget::ActionNext()
{
	if (!mIsPlayed)
		return;

	LOG(LOG_VERBOSE, "Triggered next");
	PlayNext();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
