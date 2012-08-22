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

#include <QInputDialog>
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

///////////////////////////////////////////////////////////////////////////////

#define PLAYLIST_UPDATE_DELAY         			 	  250  //ms

#define ALLOWED_AV_TIME_DIFF							2  //s

// maximum recursive calls
#define MAX_PARSER_RECURSIONS						   32

///////////////////////////////////////////////////////////////////////////////

int OverviewPlaylistWidget::sParseRecursionCount = 0;
OverviewPlaylistWidget *sOverviewPlaylistWidget = NULL;

OverviewPlaylistWidget& OverviewPlaylistWidget::GetInstance()
{
	if (sOverviewPlaylistWidget == NULL)
		LOGEX(OverviewPlaylistWidget, LOG_WARN, "OverviewPlaylistWidget is still invalid");

    return *sOverviewPlaylistWidget;
}

OverviewPlaylistWidget::OverviewPlaylistWidget(QAction *pAssignedAction, QMainWindow* pMainWindow, VideoWorkerThread *pVideoWorker, AudioWorkerThread *pAudioWorker):
    QDockWidget(pMainWindow)
{
	sOverviewPlaylistWidget = this;
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
    connect(mLwFiles, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(Play()));
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
    SetVisible(CONF.GetVisibilityPlaylistWidgetMovie());
    mAssignedAction->setChecked(CONF.GetVisibilityPlaylistWidgetMovie());
}

OverviewPlaylistWidget::~OverviewPlaylistWidget()
{
	if (mTimerId != -1)
		killTimer(mTimerId);

    CONF.SetVisibilityPlaylistWidgetMovie(isVisible());
}

///////////////////////////////////////////////////////////////////////////////
/// some static helpers
///////////////////////////////////////////////////////////////////////////////

static QString sAllLoadVideoFilter = "All supported formats (*.asf *.avi *.bmp *.dv *.jpg *.jpeg *.m4v *.mkv *.mov *.mpg *.mpeg *.mp4 *.mp4a *.m2ts *.m3u *.pls *.png *.swf *.vob *.wmv *.3gp)";
static QString sLoadVideoFilters = sAllLoadVideoFilter + ";;"\
                    "Advanced Systems Format (*.asf);;"\
                    "Audio Video Interleave Format (*.avi);;"\
                    "Digital Video Format (*.dv);;"\
                    "Joint Photographic Experts Group (*.jpg *.jpeg);;"\
                    "Matroska Format (*.mkv);;"\
                    "MPEG-Program Stream Format (*.mpg *.mpeg);;"\
                    "MPEG-2 Transport Stream (*.m2ts);;"\
                    "M3U Playlist File (*.m3u);;"\
                    "PLS Playlist File (*.pls);;"\
                    "Portable Network Graphics (*.png);;"\
                    "Quicktime/MPEG4 Format (*.m4v *.mov *.mp4 *.mp4a *.3gp);;"\
                    "Small Web Format (*.swf);;"\
                    "Video Object Format (*.vob);;" \
                    "Windows Bitmap (*.bmp);;"\
                    "Windows Media Video Format (*.wmv)";

static QString sAllSaveVideoFilter = "All supported formats (*.avi *.m4v *.mov *.mp4 *.mp4a *.3gp)";
static QString sSaveVideoFilters = sAllSaveVideoFilter + ";;"\
                    "Audio Video Interleave Format (*.avi);;"\
                    "Quicktime/MPEG4 Format (*.m4v *.mov *.mp4 *.mp4a *.3gp)";

QString OverviewPlaylistWidget::LetUserSelectVideoSaveFile(QWidget *pParent, QString pDescription)
{
    QString tResult = QFileDialog::getSaveFileName(pParent,  pDescription,
                                                            CONF.GetDataDirectory() + "/Homer-Video.avi",
                                                            sSaveVideoFilters,
                                                            &sAllSaveVideoFilter,
                                                            CONF_NATIVE_DIALOGS);

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

static QString sAllLoadAudioFilter =  "All supported formats (*.3gp *.asf *.avi *.m2ts *.m3u *.m4v *.mka *.mkv *.mov *.mp3 *.mp4 *.mp4a *.mpg *.mpeg *.pls *.vob *.wav *.wmv)";
static QString sLoadAudioFilters =  sAllLoadAudioFilter + ";;"\
                    "Advanced Systems Format (*.asf);;"\
                    "Audio Video Interleave Format (*.avi);;"\
                    "MPEG-2 Transport Stream (*.m2ts);;"\
                    "M3U Playlist File (*.m3u);;"\
                    "Matroska Format (*.mka *.mkv);;"\
                    "MPEG Audio Layer 2/3 Format (*.mp3);;"\
                    "MPEG-Program Stream Format (*.mpg *.mpeg);;"\
                    "PLS Playlist File (*.pls);;"\
                    "Quicktime/MPEG4 Format (*.m4v *.mov *.mp4 *.mp4a *.3gp);;"\
                    "Video Object Format (*.vob);;" \
                    "Waveform Audio File Format (*.wav);;" \
                    "Windows Media Video Format (*.wmv)";

QStringList OverviewPlaylistWidget::LetUserSelectAudioFile(QWidget *pParent, QString pDescription, bool pMultipleFiles)
{
    QStringList tResult;

    if (pMultipleFiles)
        tResult = QFileDialog::getOpenFileNames(pParent,  pDescription,
                                                                CONF.GetDataDirectory(),
                                                                sLoadAudioFilters,
                                                                &sAllLoadAudioFilter,
                                                                CONF_NATIVE_DIALOGS);
    else
    {
        tResult = QStringList(QFileDialog::getOpenFileName(pParent,  pDescription,
                                                                CONF.GetDataDirectory(),
                                                                sLoadAudioFilters,
                                                                &sAllLoadAudioFilter,
                                                                CONF_NATIVE_DIALOGS));

        // use the file parser to avoid playlists and resolve them to one single entry
        Playlist tPlaylist = Parse(tResult.first(), "", false);
        if (tPlaylist.size() > 0)
        	tResult = QStringList(tPlaylist.first().Location);
		else
			tResult.clear();
    }
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
                                                            CONF_NATIVE_DIALOGS);

    if (!tResult.isEmpty())
        CONF.SetDataDirectory(tResult.left(tResult.lastIndexOf('/')));

    return tResult;
}

bool OverviewPlaylistWidget::IsAudioFile(QString pFileName)
{
    // explicitly allow audio streams
    if (pFileName.startsWith("http://"))
        return true;

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

static QString sAllLoadMediaFilter = "All supported formats (*.asf *.avi *.bmp *.dv *.jpg *.jpeg *.m4v *.mka *.mkv *.mov *.mpg *.mpeg *.mp3 *.mp4 *.mp4a *.m2ts *.m3u *.pls *.png *.swf *.vob *.wav *.wmv *.3gp)";
static QString sLoadMediaFilters = sAllLoadMediaFilter + ";;"\
                    "Advanced Systems Format (*.asf);;"\
                    "Audio Video Interleave Format (*.avi);;"\
                    "Digital Video Format (*.dv);;"\
                    "Joint Photographic Experts Group (*.jpg *.jpeg);;"\
                    "Matroska Format (*.mka *.mkv);;"\
                    "MPEG Audio Layer 2/3 Format (*.mp3);;"\
                    "MPEG-Program Stream Format (*.mpg *.mpeg);;"\
                    "MPEG-2 Transport Stream (*.m2ts);;"\
                    "M3U Playlist File (*.m3u);;"\
                    "Portable Network Graphics (*.png);;"\
                    "PLS Playlist File (*.pls);;"\
                    "Quicktime/MPEG4 Format (*.m4v *.mov *.mp4 *.mp4a *.3gp);;"\
                    "Small Web Format (*.swf);;"\
                    "Video Object Format (*.vob);;" \
                    "Waveform Audio File Format (*.wav);;" \
                    "Windows Bitmap (*.bmp);;"\
                    "Windows Media Video Format (*.wmv)";
QStringList OverviewPlaylistWidget::LetUserSelectMediaFile(QWidget *pParent, QString pDescription, bool pMultipleFiles)
{
    QStringList tResult;
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Current data directory is \"%s\"", CONF.GetDataDirectory().toStdString().c_str());

    if (pMultipleFiles)
        tResult = QFileDialog::getOpenFileNames(pParent,  pDescription,
                                                                CONF.GetDataDirectory(),
                                                                sLoadMediaFilters,
                                                                &sAllLoadMediaFilter,
                                                                CONF_NATIVE_DIALOGS);
    else
        tResult = QStringList(QFileDialog::getOpenFileName(pParent,  pDescription,
                                                                CONF.GetDataDirectory(),
                                                                sLoadMediaFilters,
                                                                &sAllLoadMediaFilter,
                                                                CONF_NATIVE_DIALOGS));

    if (!tResult.isEmpty())
        CONF.SetDataDirectory(tResult.first().left(tResult.first().lastIndexOf('/')));

    return tResult;
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
	LOG(LOG_VERBOSE, "Setting playlist widget visibility to %d", pVisible);

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
    if (GetListSize() == 0)
        LOG(LOG_VERBOSE, "Playlist start triggered but we don't have entries in the list");
    else
        LOG(LOG_VERBOSE, "Playlist start triggered and we already have entries in the list");

    AddEntryDialog();
    Play(GetListSize() - 1);

    if (!isVisible())
    	SetVisible(true);
}

void OverviewPlaylistWidget::StopPlaylist()
{
	mIsPlayed = false;
}

void OverviewPlaylistWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

    if (!mLwFiles->selectedItems().isEmpty())
    {
        tAction = tMenu.addAction("Play selected");
        QIcon tIcon0;
        tIcon0.addPixmap(QPixmap(":/images/22_22/Audio_Play.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon0);

        tMenu.addSeparator();
    }

    tAction = tMenu.addAction("Add an entry");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/22_22/Plus.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    if (!mLwFiles->selectedItems().isEmpty())
    {
        tAction = tMenu.addAction("Rename selected");
        QIcon tIcon15;
        tIcon15.addPixmap(QPixmap(":/images/22_22/Contact_Edit.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon15);

        tAction = tMenu.addAction("Delete selected");
        QIcon tIcon2;
        tIcon2.addPixmap(QPixmap(":/images/22_22/Minus.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon2);
    }

    tMenu.addSeparator();

    if (GetListSize() > 0)
    {
        tAction = tMenu.addAction("Reset playlist");
        QIcon tIcon3;
        tIcon3.addPixmap(QPixmap(":/images/22_22/Reload.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon3);

        tMenu.addSeparator();
    }

    tAction = tMenu.addAction("Endless loop");
    tAction->setCheckable(true);
    tAction->setChecked(mEndlessLoop);

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Play selected") == 0)
        {
            ActionPlay();
            return;
        }
        if (tPopupRes->text().compare("Add an entry") == 0)
        {
            AddEntryDialog();
            return;
        }
        if (tPopupRes->text().compare("Rename selected") == 0)
        {
            RenameDialog();
            return;
        }
        if (tPopupRes->text().compare("Delete selected") == 0)
        {
            DelEntryDialog();
            return;
        }
        if (tPopupRes->text().compare("Reset playlist") == 0)
        {
            ResetList();
            return;
        }
        if (tPopupRes->text().compare("Endless loop") == 0)
        {
            mEndlessLoop = !mEndlessLoop;
            LOG(LOG_VERBOSE, "Playlist has now endless loop activation %d", mEndlessLoop);
            return;
        }
    }
}

void OverviewPlaylistWidget::DelEntryDialog()
{
    int tSelectectRow = -1;

    if (mLwFiles->selectionModel()->currentIndex().isValid())
    {
        int tSelectedRow = mLwFiles->selectionModel()->currentIndex().row();
        DeleteListEntry(tSelectedRow);
    }
}

void OverviewPlaylistWidget::AddEntryDialog()
{
    LOG(LOG_VERBOSE, "User wants to add a new entry to playlist");

    bool tListWasEmpty = (mLwFiles->count() == 0);

	QStringList tFileNames;

    tFileNames = LetUserSelectMediaFile(this, "Add media files to playlist");

    if (tFileNames.isEmpty())
        return;

    QString tFile;
    foreach(tFile, tFileNames)
    {
        AddEntry(tFile);
    }

    if((tListWasEmpty) && (mLwFiles->count() > 0))
    {
        mCurrentFileId = 0;
        LOG(LOG_VERBOSE, "Setting to file %d in playlist", mCurrentFileId);
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
    if (GetListSize() < 1)
        return;

    QString tFileName;
    tFileName = QFileDialog::getSaveFileName(this,  "Save playlist to..",
                                                                CONF.GetDataDirectory() + "/Homer.m3u",
                                                                "Playlist File (*.m3u)",
                                                                &*(new QString("Playlist File (*.m3u)")),
                                                                CONF_NATIVE_DIALOGS);

    if (tFileName.isEmpty())
        return;

    QString tPlaylistData;
    PlaylistEntry tEntry;
    mPlaylistMutex.lock();
    foreach(tEntry, mPlaylist)
    {
        QString tPlaylistEntry = tEntry.Location;
        LOG(LOG_VERBOSE, "Writing to m3u %s the entry %s", tFileName.toStdString().c_str(), tPlaylistEntry.toStdString().c_str());
        tPlaylistData += tPlaylistEntry + '\n';

    }
    mPlaylistMutex.unlock();

    QFile tPlaylistFile(tFileName);
    if (!tPlaylistFile.open(QIODevice::WriteOnly))
    {
    	ShowError("Could not store playlist file", "Couldn't write playlist in " + tFileName);
        return;
    }

    tPlaylistFile.write(tPlaylistData.toUtf8());
    tPlaylistFile.close();
}

void OverviewPlaylistWidget::Play(int pIndex)
{
    LOG(LOG_VERBOSE, "Got trigger to play entry %d", pIndex);

    if (pIndex == -1)
	{
	    if (mLwFiles->selectionModel()->currentIndex().isValid())
	        pIndex = mLwFiles->selectionModel()->currentIndex().row();
	}

    if ((pIndex == -1) && (GetListSize() > 0))
    {
        pIndex = 0;
    }

    if (pIndex == -1)
	{
	    LOG(LOG_VERBOSE, "Index is invalid, playback start skipped");
	    return;
	}

	mIsPlayed = true;
	mCurrentFile = GetListEntry(pIndex);

	// VIDEO: we don't support video streaming yet, otherwise we play the file
	if (!mCurrentFile.startsWith("http://"))
        mVideoWorker->PlayFile(mCurrentFile);
	// AUDIO: play the file
	mAudioWorker->PlayFile(mCurrentFile);

    mCurrentFileId = pIndex;
    LOG(LOG_VERBOSE, "Setting current row to %d in playlist", mCurrentFileId);
    mLwFiles->setCurrentRow(mCurrentFileId);
}

void OverviewPlaylistWidget::PlayNext()
{
    int tNewFileId = -1;

    // derive file id of next file which should be played
	if (mCurrentFileId < GetListSize() -1)
    {
		tNewFileId = mCurrentFileId + 1;
    }else
    {
    	if (mEndlessLoop)
    	{
    		tNewFileId = 0;
    	}else
    	{
    		//LOG(LOG_VERBOSE, "End of playlist reached");
    		return;
    	}
    }

	LOG(LOG_VERBOSE, "Playing file entry %d", tNewFileId);

	// finally play the next file
    Play(tNewFileId);
}

void OverviewPlaylistWidget::PlayLast()
{
    if (mCurrentFileId > 0)
        Play(mCurrentFileId - 1);
}

void OverviewPlaylistWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif
    if (pEvent->timerId() == mTimerId)
    {
    	// play next if EOF is reached
        // stop if current file wasn't yet switched to the desired one;
        if ((mVideoWorker->CurrentFile() != mCurrentFile) || (mAudioWorker->CurrentFile() != mCurrentFile))
            return;

        // do we already play the desired file and are we at EOF?
        if ((mCurrentFileId != -1) &&
            (mVideoWorker->GetCurrentDevice().contains(GetListEntry(mCurrentFileId))) && (mVideoWorker->EofReached()) &&
            (mAudioWorker->GetCurrentDevice().contains(GetListEntry(mCurrentFileId))) && (mAudioWorker->EofReached()))
        {
            PlayNext();
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
            LOG(LOG_VERBOSE, "New entering drag+drop url (%d) \"%s\"", ++i, tUrl.toString().toStdString().c_str());
        return;
    }
}

void OverviewPlaylistWidget::dropEvent(QDropEvent *pEvent)
{
    bool tListWasEmpty = (GetListSize() == 0);

    if (pEvent->mimeData()->hasUrls())
    {
        LOG(LOG_VERBOSE, "Got some dropped urls");
        QList<QUrl> tUrlList = pEvent->mimeData()->urls();
        QUrl tUrl;
        foreach(tUrl, tUrlList)
        {
            AddEntry(tUrl.toLocalFile());
        }
        pEvent->acceptProposedAction();
        return;
    }

    if ((tListWasEmpty) && (GetListSize() > 0) && (mIsPlayed))
        Play(mCurrentFileId);
}

int OverviewPlaylistWidget::GetListSize()
{
    int tResult = 0;

    mPlaylistMutex.lock();
    tResult = mPlaylist.size();
    mPlaylistMutex.unlock();

    return tResult;
}

void OverviewPlaylistWidget::AddEntry(QString pLocation, bool pStartPlayback)
{
    // remove "file:///" and "file://" from the beginning if existing
    #ifdef WIN32
        if (pLocation.startsWith("file:///"))
        	pLocation = pLocation.right(pLocation.size() - 8);

        if (pLocation.startsWith("file://"))
        	pLocation = pLocation.right(pLocation.size() - 7);
    #else
        if (pLocation.startsWith("file:///"))
        	pLocation = pLocation.right(pLocation.size() - 7);

        if (pLocation.startsWith("file://"))
        	pLocation = pLocation.right(pLocation.size() - 6);
    #endif

	Playlist tPlaylist = Parse(pLocation);
	LOG(LOG_VERBOSE, "Parsed %d new playlist entries", tPlaylist.size());

	mPlaylistMutex.lock();

	int tInsertionPosition = mPlaylist.size();

	PlaylistEntry tPlaylistEntry;
	foreach(tPlaylistEntry, tPlaylist)
	{
		LOG(LOG_VERBOSE, "Adding to playist: %s(%s)", tPlaylistEntry.Name.toStdString().c_str(), tPlaylistEntry.Location.toStdString().c_str());
		mPlaylist.push_back(tPlaylistEntry);
	}

	mPlaylistMutex.unlock();

    // trigger GUI update
    QApplication::postEvent(this, new QEvent(QEvent::User));

    if (pStartPlayback)
    	Play(tInsertionPosition);
}

Playlist OverviewPlaylistWidget::Parse(QString pLocation, QString pName, bool pAcceptVideo, bool pAcceptAudio)
{
	Playlist tResult;
	PlaylistEntry tPlaylistEntry;

	sParseRecursionCount ++;
	if (sParseRecursionCount < MAX_PARSER_RECURSIONS)
	{
		bool tIsWebUrl = pLocation.startsWith("http://");

		LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Parsing %s", pLocation.toStdString().c_str());

		if (pLocation.endsWith(".m3u"))
		{// an M3U playlist file
			tResult += ParseM3U(pLocation, pAcceptVideo, pAcceptAudio);
		}else if (pLocation.endsWith(".pls"))
		{// a PLS playlist file
			tResult += ParsePLS(pLocation, pAcceptVideo, pAcceptAudio);
		}else if ((!tIsWebUrl) && (QDir(pLocation).exists()))
		{// a directory
			tResult += ParseDIR(pLocation, pAcceptVideo, pAcceptAudio);
		}else
		{// an url or a local file
			// set the location
			tPlaylistEntry.Location = pLocation;

			if (pName == "")
			{
				// set the name
				if (!tIsWebUrl)
				{// derive a descriptive name from the location
					int tPos = tPlaylistEntry.Location.lastIndexOf('\\');
					if (tPos == -1)
						tPos = tPlaylistEntry.Location.lastIndexOf('/');
					if (tPos != -1)
					{
						tPos += 1;
						tPlaylistEntry.Name = tPlaylistEntry.Location.mid(tPos, pLocation.size() - tPos);
					}else
					{
						tPlaylistEntry.Name = tPlaylistEntry.Location;
					}
				}else
				{// we have a web url
					tPlaylistEntry.Name = tPlaylistEntry.Location;
				}
			}else
				tPlaylistEntry.Name = pName;

			bool tIsAudioFile = IsAudioFile(tPlaylistEntry.Location);
			bool tIsVideoFile = IsVideoFile(tPlaylistEntry.Location);

			// check file for A/V content
			if ((pAcceptVideo && tIsVideoFile) || (pAcceptAudio && (tIsAudioFile || tIsWebUrl)))
			{
				LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Adding to parsed playlist: %s at location %s", tPlaylistEntry.Name.toStdString().c_str(), tPlaylistEntry.Location.toStdString().c_str());

				// create playlist entry
				if (tIsWebUrl)
					tPlaylistEntry.Icon = QIcon(":/images/22_22/NetworkConnection.png");
				else
				{
					if (tIsVideoFile && !tIsAudioFile) // video file
						tPlaylistEntry.Icon = QIcon(":/images/46_46/VideoReel.png");
					else if (!tIsVideoFile && tIsAudioFile) // audio file
						tPlaylistEntry.Icon = QIcon(":/images/46_46/Speaker.png");
					else // audio/video file
						tPlaylistEntry.Icon = QIcon(":/images/22_22/Audio_Play.png");
				}

				// save playlist entry
				tResult.push_back(tPlaylistEntry);
			}else
				LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Ignoring entry: %s at location %s", tPlaylistEntry.Name.toStdString().c_str(), tPlaylistEntry.Location.toStdString().c_str());
		}
	}else
	{
		LOGEX(OverviewPlaylistWidget, LOG_ERROR, "Maximum of %d parser recursions reached, terminating here", MAX_PARSER_RECURSIONS);
	}

    sParseRecursionCount--;

    return tResult;
}

Playlist OverviewPlaylistWidget::ParseM3U(QString pFilePlaylist, bool pAcceptVideo, bool pAcceptAudio)
{
	Playlist tResult;
	PlaylistEntry tPlaylistEntry;
	tPlaylistEntry.Location = "";
	tPlaylistEntry.Name = "";

    QString tDir = pFilePlaylist.left(pFilePlaylist.lastIndexOf('/'));
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Parsing M3U playlist %s", pFilePlaylist.toStdString().c_str());
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "..in directory: %s", tDir.toStdString().c_str());

    QFile tPlaylistFile(pFilePlaylist);
    if (!tPlaylistFile.open(QIODevice::ReadOnly))
    {
    	LOGEX(OverviewPlaylistWidget, LOG_ERROR, "Couldn't read M3U playlist from %s", pFilePlaylist.toStdString().c_str());
    }else
    {
        QByteArray tLine;
        tLine = tPlaylistFile.readLine();
        while (!tLine.isEmpty())
        {
            QString tLineString = QString(tLine);

            //remove any "new line" char from the end
            while((tLineString.endsWith(QChar(0x0A))) || (tLineString.endsWith(QChar(0x0D))))
                tLineString = tLineString.left(tLineString.length() - 1);

            // parse the playlist line
        	if (tLineString != "")
        	{
				if (!tLineString.startsWith("#EXT"))
				{// we have a location and the entry is complete
						LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found playlist entry location: %s", tLineString.toStdString().c_str());
						if (tLineString.startsWith("http://"))
							tPlaylistEntry.Location = tLineString;
						else
							tPlaylistEntry.Location = tDir + "/" + tLineString;
						if (tPlaylistEntry.Name == "")
							tPlaylistEntry.Name = tPlaylistEntry.Location;
						tResult += Parse(tPlaylistEntry.Location, tPlaylistEntry.Name, pAcceptVideo, pAcceptAudio);
						tPlaylistEntry.Location = "";
						tPlaylistEntry.Name = "";
				}else
				{
					if (!tLineString.startsWith("#EXTINF"))
					{// we have extended information including a name
						if (tLineString.indexOf(',') != -1)
						{
							tLineString = tLineString.right(tLineString.size() - tLineString.indexOf(',') - 1);
							LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found playlist entry name: %s", tLineString.toStdString().c_str());
							tPlaylistEntry.Name = tLineString;
						}else
							LOGEX(OverviewPlaylistWidget, LOG_WARN, "Invalid format of line: %s", tLineString.toStdString().c_str());
					}else
						LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found playlist extended entry: %s", tLineString.toStdString().c_str());
				}
        	}

            tLine = tPlaylistFile.readLine();
        }
    }

    return tResult;
}

Playlist OverviewPlaylistWidget::ParsePLS(QString pFilePlaylist, bool pAcceptVideo, bool pAcceptAudio)
{
	Playlist tResult;

    QString tDir = pFilePlaylist.left(pFilePlaylist.lastIndexOf('/'));
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Parsing PLS playlist file %s", pFilePlaylist.toStdString().c_str());
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "..in directory: %s", tDir.toStdString().c_str());

    QFile tPlaylistFile(pFilePlaylist);
    if (!tPlaylistFile.open(QIODevice::ReadOnly))
    {
    	LOGEX(OverviewPlaylistWidget, LOG_ERROR, "Couldn't read PLS playlist from %s", pFilePlaylist.toStdString().c_str());
    }else
    {
        QByteArray tLine;
        tLine = tPlaylistFile.readLine();
        int tPlaylistEntries = 0;
        int tFoundPlaylisEntries = -1;
        bool tPlaylistEntryParsed = false;
        PlaylistEntry tPlaylistEntry;
        tPlaylistEntry.Location = "";
        tPlaylistEntry.Name = "";

        while ((tFoundPlaylisEntries < tPlaylistEntries) && (!tLine.isEmpty()))
        {
            QString tLineString = QString(tLine);

            // remove line delimiters
            while((tLineString.endsWith(QChar(0x0A))) || (tLineString.endsWith(QChar(0x0D))))
                tLineString = tLineString.left(tLineString.length() - 1); //remove any "new line" char from the end

            QStringList tLineSplit = tLineString.split(("="));
            if (tLineSplit.size() == 2)
            {
                QString tKey = tLineSplit[0];
                QString tValue = tLineSplit[1];
                LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found key \"%s\" with value \"%s\"", tKey.toStdString().c_str(), tValue.toStdString().c_str());
                // parse the playlist line
                if (tKey.startsWith("NumberOfEntries"))
                {// "NumberOfEntries"
                    bool tConversionWasOkay = false;
                    tPlaylistEntries = tValue.toInt(&tConversionWasOkay);
                    if (!tConversionWasOkay)
                    {
                    	LOGEX(OverviewPlaylistWidget, LOG_ERROR, "Unable to convert \"%s\" into an integer value", tValue.toStdString().c_str());
                        return tResult;
                    }
                }else if (tKey.startsWith("File"))
                {// "File"
                    tPlaylistEntry.Location = tValue;
                }else if (tKey.startsWith("Title"))
                {// "Title"
                    tFoundPlaylisEntries++;
                    tPlaylistEntry.Name = tValue;
                    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found playlist entry: \"%s\" at location \"%s\"", tPlaylistEntry.Name.toStdString().c_str(), tPlaylistEntry.Location.toStdString().c_str());
					tResult += Parse(tPlaylistEntry.Location, tPlaylistEntry.Name, pAcceptVideo, pAcceptAudio);
                    tPlaylistEntry.Location = "";
                    tPlaylistEntry.Name = "";
                }
            }else
            {
                if (tLineString.startsWith("["))
                {// "[playlist]"
                    // nothing to do
                }else
                {
                    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Unexpected token in PLS playlist: \"%s\"", tLineString.toStdString().c_str());
                }
            }

            tLine = tPlaylistFile.readLine();
        }
    }

    return tResult;
}

Playlist OverviewPlaylistWidget::ParseDIR(QString pDirLocation, bool pAcceptVideo, bool pAcceptAudio)
{
	Playlist tResult;

	// ignore "." and ".."
	if ((pDirLocation.endsWith(".")) || (pDirLocation.endsWith("..")))
		return tResult;

    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Parsing directory %s", pDirLocation.toStdString().c_str());
    QDir tDirectory(pDirLocation);
    QStringList tDirEntries = tDirectory.entryList();
    QString tDirEntry;
    foreach(tDirEntry, tDirEntries)
    {
    	tResult += Parse(pDirLocation + '/' + tDirEntry, "", pAcceptVideo, pAcceptAudio);
    }

    return tResult;
}

QString OverviewPlaylistWidget::GetListEntry(int pIndex)
{
    QString tResult = "";

    mPlaylistMutex.lock();
    PlaylistEntry tEntry;
    int tIndex = 0;
    foreach(tEntry, mPlaylist)
    {
        if (tIndex == pIndex)
        {
            tResult = tEntry.Location;
            break;
        }
        tIndex++;
    }
    mPlaylistMutex.unlock();

    return tResult;
}

QString OverviewPlaylistWidget::GetListEntryName(int pIndex)
{
    QString tResult = "";

    mPlaylistMutex.lock();
    PlaylistEntry tEntry;
    int tIndex = 0;
    foreach(tEntry, mPlaylist)
    {
        if (tIndex == pIndex)
        {
            tResult = tEntry.Name;
            break;
        }
        tIndex++;
    }
    mPlaylistMutex.unlock();

    return tResult;
}

void OverviewPlaylistWidget::DeleteListEntry(int pIndex)
{
    int tIndex = 0;
    Playlist::iterator tIt;

    mPlaylistMutex.lock();

    if (mPlaylist.size() > 0)
    {
        for (tIt = mPlaylist.begin(); tIt != mPlaylist.end(); tIt++)
        {
            if (tIndex == pIndex)
            {
                mPlaylist.erase(tIt);
                break;
            }
            tIndex++;
        }
    }

    mPlaylistMutex.unlock();

    UpdateView();
}

void OverviewPlaylistWidget::RenameListEntry(int pIndex, QString pName)
{
    int tIndex = 0;
    Playlist::iterator tIt;

    LOG(LOG_VERBOSE, "Renaming index %d to %s", pIndex, pName.toStdString().c_str());

    mPlaylistMutex.lock();

    if (mPlaylist.size() > 0)
    {
        for (tIt = mPlaylist.begin(); tIt != mPlaylist.end(); tIt++)
        {
            if (tIndex == pIndex)
            {
                tIt->Name = pName;
                break;
            }
            tIndex++;
        }
    }

    mPlaylistMutex.unlock();

    UpdateView();
}

void OverviewPlaylistWidget::ResetList()
{
    Playlist::iterator tIt;

    mPlaylistMutex.lock();

    if (mPlaylist.size() > 0)
    {
        tIt = mPlaylist.begin();
        while (tIt != mPlaylist.end())
        {
            mPlaylist.erase(tIt);
            tIt = mPlaylist.begin();
        }
    }

    mPlaylistMutex.unlock();

    UpdateView();
}

void OverviewPlaylistWidget::RenameDialog()
{
    if (mLwFiles->selectionModel()->currentIndex().isValid())
    {
        int tSelectedRow = mLwFiles->selectionModel()->currentIndex().row();
        QString tCurrentName = GetListEntryName(tSelectedRow);
        QString tFillSpace = "";
        for (int i = 0; i < tCurrentName.length(); i++)
            tFillSpace += "  ";
        LOG(LOG_VERBOSE, "User wants to rename \"%s\" at index %d", tCurrentName.toStdString().c_str(), tSelectedRow);
        bool tOkay = false;
        QString tNewName = QInputDialog::getText(this, "Rename \"" + tCurrentName + "\"", "New name:           " + tFillSpace, QLineEdit::Normal, tCurrentName, &tOkay);
        if ((tOkay) && (!tNewName.isEmpty()))
        {
            RenameListEntry(tSelectedRow, tNewName);
        }
    }
}

void OverviewPlaylistWidget::ActionPlay()
{
	LOG(LOG_VERBOSE, "Triggered play");
	Play(mCurrentFileId);
}

void OverviewPlaylistWidget::ActionPause()
{
	if (!mIsPlayed)
		return;

	LOG(LOG_VERBOSE, "Triggered pause");
    mVideoWorker->PauseFile();
    mAudioWorker->PauseFile();
}

void OverviewPlaylistWidget::ActionStop()
{
	if (!mIsPlayed)
		return;

	LOG(LOG_VERBOSE, "Triggered stop");
    mVideoWorker->StopFile();
    mAudioWorker->StopFile();
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

void OverviewPlaylistWidget::FillRow(int pRow, const PlaylistEntry &pEntry)
{
    if (mLwFiles->item(pRow) != NULL)
        mLwFiles->item(pRow)->setText(pEntry.Name);
    else
    {
        QListWidgetItem *tItem = new QListWidgetItem(pEntry.Icon, pEntry.Name);
        tItem->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        mLwFiles->insertItem(pRow, tItem);
    }
}

void OverviewPlaylistWidget::UpdateView()
{
    Playlist::iterator tIt;
    int tRow = 0;
    int tSelectedRow = -1;

    //LOG(LOG_VERBOSE, "Updating view");

    LOG(LOG_VERBOSE, "Found row selection: %d", mCurrentFileId);

    if (GetListSize() != mLwFiles->count())
    {
        mLwFiles->clear();
    }

    mPlaylistMutex.lock();

    if (mPlaylist.size() > 0)
    {
        PlaylistEntry tEntry;
        foreach(tEntry, mPlaylist)
        {
            FillRow(tRow++, tEntry);
        }
    }

    mPlaylistMutex.unlock();

    if (mCurrentFileId != -1)
        mLwFiles->setCurrentRow (mCurrentFileId);
}

void OverviewPlaylistWidget::customEvent(QEvent* pEvent)
{
    if (pEvent->type() != QEvent::User)
    {
        LOG(LOG_ERROR, "Wrong Qt event type detected");
        return;
    }

    UpdateView();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
