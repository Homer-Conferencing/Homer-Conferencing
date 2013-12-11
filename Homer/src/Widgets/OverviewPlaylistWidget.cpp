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
#include <QNetworkReply>
#include <QContextMenuEvent>
#include <QTemporaryFile>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define PLAYLIST_UPDATE_DELAY         			 	  250  //ms

#define ALLOWED_AV_TIME_DIFF							2  //s

// maximum recursive calls
#define MAX_PARSER_RECURSIONS						   32

///////////////////////////////////////////////////////////////////////////////

#define IS_SUPPORTED_WEB_LINK(x)						((x.toLower().startsWith("http://")) || (x.toLower().startsWith("mms://")) || (x.toLower().startsWith("mmst://")) || (x.toLower().startsWith("icyx://")))
#define IS_SUPPORTED_PLAYLIST(x)                        ((x.toLower().endsWith(".m3u")) || (x.toLower().endsWith(".wmx")) || (x.toLower().endsWith(".pls")))

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
    mCurrentFileAudioPlaying = false;
    mCurrentFileVideoPlaying = false;
    mCurrentRowInGui = -1;
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
    connect(mTbAddFile, SIGNAL(clicked()), this, SLOT(AddFileEntryDialog()));
    connect(mTbAddUrl, SIGNAL(clicked()), this, SLOT(AddPlaylistUrlsDialog()));
    connect(mTbDel, SIGNAL(clicked()), this, SLOT(DelPlaylistEntriesDialog()));
    connect(mTbSaveList, SIGNAL(clicked()), this, SLOT(SavePlaylistDialog()));
    connect(mLwFiles, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(Play()));
    mShortcutDel = new QShortcut(Qt::Key_Delete, mLwFiles);
    mShortcutIns = new QShortcut(Qt::Key_Insert, mLwFiles);
    mShortcutDel->setEnabled(true);
    mShortcutIns->setEnabled(true);
    connect(mShortcutDel, SIGNAL(activated()), this, SLOT(DelEntryDialogSc()));
    connect(mShortcutDel, SIGNAL(activatedAmbiguously()), this, SLOT(DelEntryDialogSc()));
    connect(mShortcutIns, SIGNAL(activated()), this, SLOT(AddEntryDialogSc()));
    connect(mShortcutIns, SIGNAL(activatedAmbiguously()), this, SLOT(AddEntryDialogSc()));
    mTimerId = startTimer(PLAYLIST_UPDATE_DELAY);
    SetVisible(CONF.GetVisibilityPlaylistWidgetMovie());
    mAssignedAction->setChecked(CONF.GetVisibilityPlaylistWidgetMovie());

    mNetworkAccessManager = new QNetworkAccessManager();
    connect(mNetworkAccessManager, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)), SLOT(ReceivedFileFromServerNeedAuthenication(QNetworkReply*, QAuthenticator*)));
    connect(mNetworkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(ReceivedFileFromServer(QNetworkReply*)));
}

OverviewPlaylistWidget::~OverviewPlaylistWidget()
{
	if (mTimerId != -1)
		killTimer(mTimerId);

	delete mNetworkAccessManager;

    CONF.SetVisibilityPlaylistWidgetMovie(isVisible());
}

///////////////////////////////////////////////////////////////////////////////
/// some static helpers
///////////////////////////////////////////////////////////////////////////////

static QString sAllLoadVideoFilter = (QString)QT_TRANSLATE_NOOP("Homer::Gui::OverviewPlaylistWidget", "All supported formats") + " (*.asf *.avi *.bmp *.divx *.dv *.flv *.jpg *.jpeg *.m4v *.mkv *.mov *.mpg *.mpeg *.mp4 *.mp4a *.m2ts *.m2t *.m3u *.ogg *.ogv *.pls *.png *.rm *.rmvb *.swf *.ts *.vob *.wmv *.wmx *.3gp)";
static QString sLoadVideoFilters = sAllLoadVideoFilter + ";;"\
                    "Advanced Systems Format (*.asf);;"\
                    "Audio Video Interleave Format (*.avi *.divx);;"\
                    "Digital Video Format (*.dv);;"\
					"Flash Video Format (*.flv);;"\
                    "Joint Photographic Experts Group (*.jpg *.jpeg);;"\
                    "Matroska Format (*.mkv);;"\
                    "MPEG-Program Stream Format (*.mpg *.mpeg);;"\
                    "MPEG-2 Transport Stream (*.m2ts *.m2t *.ts);;"\
                    "M3U Playlist File (*.m3u);;"\
                    "OGG Container format (*.ogg *.ogv);;"\
                    "PLS Playlist File (*.pls);;"\
                    "Portable Network Graphics (*.png);;"\
                    "Quicktime/MPEG4 Format (*.m4v *.mov *.mp4 *.mp4a *.3gp);;"\
                    "RealMedia Format (*.rm *.rmvb);;"\
                    "Small Web Format (*.swf);;"\
                    "Video Object Format (*.vob);;" \
                    "Windows Bitmap (*.bmp);;"\
                    "Windows Media Video Format (*.wmv);;"\
                    "Windows Media Redirector File (*.wmx)";

static QString sAllSaveVideoFilter = (QString)QT_TRANSLATE_NOOP("Homer::Gui::OverviewPlaylistWidget", "All supported formats") + " (*.avi *.m4v *.mov *.mp4 *.3gp)";
static QString sSaveVideoFilters = sAllSaveVideoFilter + ";;"\
                    "Audio Video Interleave Format (*.avi);;"\
                    "Quicktime/MPEG4 Format (*.m4v *.mov *.mp4 *.3gp)";

QString OverviewPlaylistWidget::LetUserSelectVideoSaveFile(QWidget *pParent, QString pDescription)
{
    QString tResult = QFileDialog::getSaveFileName(pParent,  pDescription,
                                                            CONF.GetDataDirectory() + Homer::Gui::OverviewPlaylistWidget::tr("/Homer-Video.avi"),
                                                            sSaveVideoFilters,
                                                            &sAllSaveVideoFilter,
                                                            CONF_NATIVE_DIALOGS);

    if (!tResult.isEmpty())
        CONF.SetDataDirectory(tResult.left(tResult.lastIndexOf('/')));

    return tResult;
}

bool OverviewPlaylistWidget::IsVideoFile(QString pFileName)
{
    if(pFileName == "")
        return false;

    pFileName = QString(pFileName.toLocal8Bit());

    int tPos = pFileName.lastIndexOf('.', -1);
    if (tPos == -1)
    {
        LOGEX(OverviewPlaylistWidget, LOG_ERROR, "Video file %s name lacks a correct format selecting end", pFileName.toStdString().c_str());
        return false;
    }

    QString tExt = pFileName.right(pFileName.size() - tPos).toLower();
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Checking for video content in file %s of type %s", pFileName.toStdString().c_str(), tExt.toStdString().c_str());

    if (sLoadVideoFilters.indexOf(tExt, 0) != -1)
    {
        LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "   ..found video content");
        return true;
    }else
    {
        LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "   ..no video content");
        return false;
    }
}

static QString sAllLoadAudioFilter =  (QString)QT_TRANSLATE_NOOP("Homer::Gui::OverviewPlaylistWidget", "All supported formats") + " (*.3gp *.asf *.avi *.divx *.flv *.m2ts *.m2t *.m3u *.m4v *.mka *.mkv *.mov *.mp3 *.mp4 *.mp4a *.mpg *.mpeg *.ogg *.ogv *.pls *.rm *.rmvb *.ts *.vob *.wav *.wmv *.wmx)";
static QString sLoadAudioFilters =  sAllLoadAudioFilter + ";;"\
                    "Advanced Systems Format (*.asf);;"\
                    "Audio Video Interleave Format (*.avi *.divx);;"\
					"Flash Video Format (*.flv);;"\
                    "MPEG-2 Transport Stream (*.m2ts *.m2t *.ts);;"\
                    "M3U Playlist File (*.m3u);;"\
                    "Matroska Format (*.mka *.mkv);;"\
                    "MPEG Audio Layer 2/3 Format (*.mp3);;"\
                    "MPEG-Program Stream Format (*.mpg *.mpeg);;"\
                    "OGG Container format (*.ogg *.ogv);;"\
                    "PLS Playlist File (*.pls);;"\
                    "Quicktime/MPEG4 Format (*.m4v *.mov *.mp4 *.mp4a *.3gp);;"\
                    "RealMedia Format (*.rm *.rmvb);;"\
                    "Video Object Format (*.vob);;" \
                    "Waveform Audio File Format (*.wav);;" \
                    "Windows Media Video Format (*.wmv);;"\
                    "Windows Media Redirector File (*.wmx)";

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
        if (!tResult.isEmpty())
        {
			Playlist tPlaylist = Parse(tResult.first(), "", false);
			if (tPlaylist.size() > 0)
				tResult = QStringList(tPlaylist.first().Location);
			else
			{
				tResult.clear();
			}
		}
	}
    if ((!tResult.isEmpty()) && (tResult.first() != ""))
    {
    	CONF.SetDataDirectory(tResult.first().left(tResult.first().lastIndexOf('/')));
    }
    return tResult;
}

static QString sAllSaveAudioFilter =  (QString)QT_TRANSLATE_NOOP("Homer::Gui::OverviewPlaylistWidget", "All supported formats") + " (*.mp3 *.wav)";
static QString sSaveAudioFilters =  sAllSaveAudioFilter + ";;"\
                    "MPEG Audio Layer 2/3 Format (*.mp3);;"\
                    "Wave Form Audio File Format (*.wav)";

QString OverviewPlaylistWidget::LetUserSelectAudioSaveFile(QWidget *pParent, QString pDescription)
{
    QString tResult = QFileDialog::getSaveFileName(pParent,  pDescription,
                                                            CONF.GetDataDirectory() + Homer::Gui::OverviewPlaylistWidget::tr("/Homer-Audio.mp3"),
                                                            sSaveAudioFilters,
                                                            &sAllSaveAudioFilter,
                                                            CONF_NATIVE_DIALOGS);

    if (!tResult.isEmpty())
        CONF.SetDataDirectory(tResult.left(tResult.lastIndexOf('/')));

    return tResult;
}

bool OverviewPlaylistWidget::IsAudioFile(QString pFileName)
{
    if(pFileName == "")
        return false;

    // explicitly allow audio streams
    if (IS_SUPPORTED_WEB_LINK(pFileName))
        return true;

    pFileName = QString(pFileName.toLocal8Bit());

    int tPos = pFileName.lastIndexOf('.', -1);
    if (tPos == -1)
    {
        LOGEX(OverviewPlaylistWidget, LOG_ERROR, "Audio file %s name lacks a correct format selecting end", pFileName.toStdString().c_str());
        return false;
    }

    QString tExt = pFileName.right(pFileName.size() - tPos).toLower();
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Checking for audio content in file %s of type %s", pFileName.toStdString().c_str(), tExt.toStdString().c_str());

    if (sLoadAudioFilters.indexOf(tExt, 0) != -1)
    {
        LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "   ..found audio content");
        return true;
    }else
    {
        LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "   ..no audio content");
        return false;
    }
}

static QString sAllLoadMediaFilter = (QString)QT_TRANSLATE_NOOP("Homer::Gui::OverviewPlaylistWidget", "All supported formats") + " (*.asf *.avi *.bmp *.divx *.dv *.flv *.jpg *.jpeg *.m4v *.mka *.mkv *.mov *.mpg *.mpeg *.mp3 *.mp4 *.mp4a *.m2ts *.m2t *.m3u *.ogg *.ogv *.pls *.png *.rm *.rmvb *.swf *.ts *.vob *.wav *.wmv *.wmx *.3gp)";
static QString sLoadMediaFilters = sAllLoadMediaFilter + ";;"\
                    "Advanced Systems Format (*.asf);;"\
                    "Audio Video Interleave Format (*.avi *.divx);;"\
                    "Digital Video Format (*.dv);;"\
					"Flash Video Format (*.flv);;"\
                    "Joint Photographic Experts Group Format (*.jpg *.jpeg);;"\
                    "Matroska Format (*.mka *.mkv);;"\
                    "MPEG Audio Layer 2/3 Format (*.mp3);;"\
                    "MPEG-Program Stream Format (*.mpg *.mpeg);;"\
                    "MPEG-2 Transport Stream (*.m2ts *.m2t *.ts);;"\
                    "M3U Playlist File (*.m3u);;"\
                    "OGG Container format (*.ogg *.ogv);;"\
                    "Portable Network Graphics Format (*.png);;"\
                    "PLS Playlist File (*.pls);;"\
                    "Quicktime/MPEG4 Format (*.m4v *.mov *.mp4 *.mp4a *.3gp);;"\
                    "RealMedia Format (*.rm *.rmvb);;"\
                    "Small Web Format (*.swf);;"\
                    "Video Object Format (*.vob);;" \
                    "Waveform Audio File Format (*.wav);;" \
                    "Windows Bitmap (*.bmp);;"\
                    "Windows Media Video Format (*.wmv);;"\
                    "Windows Media Redirector File (*.wmx)";
QStringList OverviewPlaylistWidget::LetUserSelectMediaFile(QWidget *pParent, QString pDescription, bool pMultipleFiles, bool pOnlyDirectories)
{
    QStringList tResult;
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Current data directory is \"%s\"", CONF.GetDataDirectory().toStdString().c_str());

    if (pMultipleFiles)
    {
        if(pOnlyDirectories)
        {
            tResult = QStringList(QFileDialog::getExistingDirectory(pParent,  pDescription,
                                                                    CONF.GetDataDirectory(),
                                                                    CONF_NATIVE_DIALOGS));
        }else
        {
            tResult = QFileDialog::getOpenFileNames(pParent,  pDescription,
                                                                    CONF.GetDataDirectory(),
                                                                    sLoadMediaFilters,
                                                                    &sAllLoadMediaFilter,
                                                                    CONF_NATIVE_DIALOGS);
        }
    }else
    {
        if(pOnlyDirectories)
        {
            tResult = QStringList(QFileDialog::getExistingDirectory(pParent,  pDescription,
                                                                    CONF.GetDataDirectory(),
                                                                    CONF_NATIVE_DIALOGS));
        }else
        {
            tResult = QStringList(QFileDialog::getOpenFileName(pParent,  pDescription,
                                                                    CONF.GetDataDirectory(),
                                                                    sLoadMediaFilters,
                                                                    &sAllLoadMediaFilter,
                                                                    CONF_NATIVE_DIALOGS));
        }

        // use the file parser to avoid playlists and resolve them to one single entry
        if (!tResult.isEmpty())
        {
			Playlist tPlaylist = Parse(tResult.first(), "");
			if (tPlaylist.size() > 0)
				tResult = QStringList(tPlaylist.first().Location);
			else
				tResult.clear();
        }
	}
    if (!tResult.isEmpty())
        CONF.SetDataDirectory(tResult.first().left(tResult.first().lastIndexOf('/')));

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

void OverviewPlaylistWidget::initializeGUI()
{
    setupUi(this);
}

void OverviewPlaylistWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewPlaylistWidget::SetVisible(bool pVisible)
{
	LOG(LOG_VERBOSE, "Setting playlist widget visibility to %d", pVisible);

	CONF.SetVisibilityPlaylistWidgetMovie(pVisible);
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

void OverviewPlaylistWidget::StartPlaylist(bool pAddDirectories)
{
    if (GetListSize() == 0)
        LOG(LOG_VERBOSE, "Playlist start triggered but we don't have entries in the list");
    else
        LOG(LOG_VERBOSE, "Playlist start triggered and we already have entries in the list");

    int tFirstAddedPlaylistEntry = GetListSize();
    if (AddPlaylistFilesDirsDialog(pAddDirectories))
    {
        Play(tFirstAddedPlaylistEntry);

        if (!isVisible())
        	SetVisible(true);
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

    if (!mLwFiles->selectedItems().isEmpty())
    {
        tAction = tMenu.addAction(QPixmap(":/images/22_22/AV_Play.png"), Homer::Gui::OverviewPlaylistWidget::tr("Play selected"));
        tAction->setShortcut(Qt::Key_Enter);

        tMenu.addSeparator();
    }

    tAction = tMenu.addAction(QPixmap(":/images/22_22/Plus.png"), Homer::Gui::OverviewPlaylistWidget::tr("Add file(s)"));
    tAction->setShortcut(Qt::Key_Insert);

    tAction = tMenu.addAction(QPixmap(":/images/22_22/NetworkConnection.png"), Homer::Gui::OverviewPlaylistWidget::tr("Add url"));

    if (!mLwFiles->selectedItems().isEmpty())
    {
        tAction = tMenu.addAction(QPixmap(":/images/22_22/Contact_Edit.png"), Homer::Gui::OverviewPlaylistWidget::tr("Rename selected"));
        tAction->setShortcut(Qt::Key_F2);

        tAction = tMenu.addAction(QPixmap(":/images/22_22/Minus.png"), Homer::Gui::OverviewPlaylistWidget::tr("Delete selected"));
        tAction->setShortcut(Qt::Key_Delete);
    }

    tMenu.addSeparator();

    if (GetListSize() > 0)
    {
        tAction = tMenu.addAction(QPixmap(":/images/22_22/Reload.png"), Homer::Gui::OverviewPlaylistWidget::tr("Reset playlist"));

        tMenu.addSeparator();
    }

    tAction = tMenu.addAction(Homer::Gui::OverviewPlaylistWidget::tr("Endless loop"));
    tAction->setCheckable(true);
    tAction->setChecked(mEndlessLoop);

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare(Homer::Gui::OverviewPlaylistWidget::tr("Play selected")) == 0)
        {
            ActionPlay();
            return;
        }
        if (tPopupRes->text().compare(Homer::Gui::OverviewPlaylistWidget::tr("Add file(s)")) == 0)
        {
            AddPlaylistFilesDirsDialog();
            return;
        }
        if (tPopupRes->text().compare(Homer::Gui::OverviewPlaylistWidget::tr("Add url")) == 0)
        {
            AddPlaylistUrlsDialog();
            return;
        }
        if (tPopupRes->text().compare(Homer::Gui::OverviewPlaylistWidget::tr("Rename selected")) == 0)
        {
            RenameDialog();
            return;
        }
        if (tPopupRes->text().compare(Homer::Gui::OverviewPlaylistWidget::tr("Delete selected")) == 0)
        {
            DelPlaylistEntriesDialog();
            return;
        }
        if (tPopupRes->text().compare(Homer::Gui::OverviewPlaylistWidget::tr("Reset playlist")) == 0)
        {
            ResetList();
            return;
        }
        if (tPopupRes->text().compare(Homer::Gui::OverviewPlaylistWidget::tr("Endless loop")) == 0)
        {
            mEndlessLoop = !mEndlessLoop;
            LOG(LOG_VERBOSE, "Playlist has now endless loop activation %d", mEndlessLoop);
            return;
        }
    }
}

void OverviewPlaylistWidget::DelPlaylistEntriesDialog()
{
    if (mLwFiles->count() < 1)
        return;

    QModelIndexList tSelection = mLwFiles->selectionModel()->selectedRows();

    for (int i = tSelection.size() -1; i >= 0; i--)
        DeletePlaylistEntry(GetPlaylistIndexFromGuiRow(tSelection[i].row()));
}

bool OverviewPlaylistWidget::AddPlaylistFilesDirsDialog(bool pAddDirectories)
{
    LOG(LOG_VERBOSE, "User wants to add a file entry to playlist");

    bool tListWasEmpty = (mLwFiles->count() == 0);

	QStringList tFileNames;

    if(pAddDirectories)
        tFileNames = LetUserSelectMediaFile(this, Homer::Gui::OverviewPlaylistWidget::tr("Add directory to playlist"), true, true);
    else
        tFileNames = LetUserSelectMediaFile(this, Homer::Gui::OverviewPlaylistWidget::tr("Add file(s) to playlist"), true, false);

    if (tFileNames.isEmpty())
        return false;

    QString tFile;
    foreach(tFile, tFileNames)
    {
        LOG(LOG_VERBOSE, "Adding selected playlist entry: %s", tFile.toStdString().c_str());
        AddEntry(tFile);
    }

    if((tListWasEmpty) && (mLwFiles->count() > 0))
    {
        mCurrentRowInGui = 0;
        LOG(LOG_VERBOSE, "Setting to file %d in playlist", mCurrentRowInGui);
        mLwFiles->setCurrentRow(mCurrentRowInGui, QItemSelectionModel::Clear | QItemSelectionModel::Select);
        if (!isVisible())
        	SetVisible(true);
    }

    return true;
}

bool OverviewPlaylistWidget::AddPlaylistUrlsDialog()
{
    LOG(LOG_VERBOSE, "User wants to add an url entry to playlist");

    bool tListWasEmpty = (mLwFiles->count() == 0);

    bool tAck = false;
    QString tUrl = QInputDialog::getText(this, Homer::Gui::OverviewPlaylistWidget::tr("Adding an url to the playlist"), Homer::Gui::VideoWidget::tr("Adding url:") + "                                                                            ", QLineEdit::Normal, "", &tAck);

    if (!tAck)
        return false;

    if (tUrl.isEmpty())
        return false;

    AddEntry(tUrl);

    if((tListWasEmpty) && (mLwFiles->count() > 0))
    {
        mCurrentRowInGui = 0;
        LOG(LOG_VERBOSE, "Setting to file %d in playlist", mCurrentRowInGui);
        mLwFiles->setCurrentRow(mCurrentRowInGui, QItemSelectionModel::Clear | QItemSelectionModel::Select);
        if (!isVisible())
            SetVisible(true);
    }

    return true;
}

void OverviewPlaylistWidget::AddEntryDialogSc()
{
    if (mLwFiles->hasFocus())
        AddPlaylistFilesDirsDialog();
}

void OverviewPlaylistWidget::DelEntryDialogSc()
{
    if (mLwFiles->hasFocus())
        DelPlaylistEntriesDialog();
}

void OverviewPlaylistWidget::SavePlaylistDialog()
{
    if (GetListSize() < 1)
        return;

    QString tFileName;
    tFileName = QFileDialog::getSaveFileName(   this,
                                                Homer::Gui::OverviewPlaylistWidget::tr("Save playlist"),
                                                CONF.GetDataDirectory() + "/Homer.m3u",
                                                Homer::Gui::OverviewPlaylistWidget::tr("Playlist File") + " (*.m3u)",
                                                &*(new QString(Homer::Gui::OverviewPlaylistWidget::tr("Playlist File") + " (*.m3u)")),
                                                CONF_NATIVE_DIALOGS);

    if (tFileName.isEmpty())
        return;

    SaveM3U(tFileName);
}

void OverviewPlaylistWidget::SaveM3U(QString pFileName)
{
    QString tPlaylistData;
    PlaylistEntry tEntry;

    // make sure the playlist is correctly ordered
    UpdateView();

    mPlaylistMutex.lock();
    tPlaylistData += "#EXTM3U\n";
    foreach(tEntry, mPlaylist)
    {
        QString tPlaylistEntry = tEntry.Location;
        QString tPlaylistEntryName = tEntry.Name;
        LOG(LOG_VERBOSE, "Writing to m3u %s the entry %s(name: %s)", pFileName.toStdString().c_str(), tPlaylistEntry.toStdString().c_str(), tPlaylistEntryName.toStdString().c_str());
        tPlaylistData += "#EXTINF:-1," + tPlaylistEntryName + "\n";
        tPlaylistData += tPlaylistEntry + '\n';
    }
    mPlaylistMutex.unlock();

    QFile tPlaylistFile(pFileName);
    if (!tPlaylistFile.open(QIODevice::WriteOnly))
    {
    	ShowError(Homer::Gui::OverviewPlaylistWidget::tr("Could not store playlist file"), Homer::Gui::OverviewPlaylistWidget::tr("Couldn't write playlist in") + " " + pFileName);
        return;
    }

    tPlaylistFile.write(tPlaylistData.toUtf8());
    tPlaylistFile.close();
}

int OverviewPlaylistWidget::GetPlaylistIndexFromGuiRow(int pRow)
{
    return mLwFiles->item(pRow)->data(Qt::UserRole).toInt();
}

void OverviewPlaylistWidget::Play(int pIndex)
{
    LOG(LOG_VERBOSE, "Got trigger to play entry %d", pIndex);

    int tRow = pIndex;
    if (pIndex == -1)
	{
	    if (mLwFiles->selectionModel()->currentIndex().isValid())
	        tRow = mLwFiles->selectionModel()->currentIndex().row();
	    pIndex = GetPlaylistIndexFromGuiRow(tRow);
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
	mCurrentFile = GetPlaylistEntry(pIndex);

	mCurrentFileVideoPlaying = mVideoWorker->PlayFile(mCurrentFile);
	mCurrentFileAudioPlaying = mAudioWorker->PlayFile(mCurrentFile);

    mCurrentRowInGui = tRow;
    LOG(LOG_VERBOSE, "Setting current row to %d in playlist", mCurrentRowInGui);
    mLwFiles->selectionModel()->clearSelection();
    mLwFiles->setCurrentRow(mCurrentRowInGui);
}

void OverviewPlaylistWidget::PlayNext()
{
    int tNewFileId = -1;

	if (!mIsPlayed)
		return;

    if (GetListSize() < 1)
        return;

    // derive file id of next file which should be played
	if (mCurrentRowInGui < GetListSize() -1)
    {
		tNewFileId = mCurrentRowInGui + 1;
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

	LOG(LOG_WARN, "Playing playlist entry %d", tNewFileId);

	// finally play the next file
    Play(tNewFileId);
}

void OverviewPlaylistWidget::PlayPrevious()
{
	if (!mIsPlayed)
		return;

    if (mCurrentRowInGui > 0)
        Play(mCurrentRowInGui - 1);
}

void OverviewPlaylistWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
		static int tCounter = 0;
        LOG(LOG_VERBOSE, "New timer event %d", ++tCounter);
    #endif
    if (pEvent->timerId() == mTimerId)
    {
    	// play next if EOF is reached
        // stop if current file wasn't yet switched to the desired one;
        if (((mCurrentFileVideoPlaying) && (mVideoWorker->CurrentFile() != "") && (mVideoWorker->CurrentFile() != mCurrentFile)) ||
            ((mCurrentFileAudioPlaying) && (mAudioWorker->CurrentFile() != "") && (mAudioWorker->CurrentFile() != mCurrentFile)))
        {
        	LOG(LOG_VERBOSE, "Desired file %s wasn't started yet both in video and audio widget\naudio = %s\nvideo = %s", mCurrentFile.toStdString().c_str(), mAudioWorker->CurrentFile().toStdString().c_str(), mVideoWorker->CurrentFile().toStdString().c_str());
        	return;
        }

        //LOG(LOG_VERBOSE, "Video EOF: %d, audio EOF: %d", mVideoWorker->EofReached(), mAudioWorker->EofReached());

        // do we already play the desired file and are we at EOF?
        if (((!mCurrentFileVideoPlaying) || (mVideoWorker->EofReached())) &&
            ((!mCurrentFileAudioPlaying) || (mAudioWorker->EofReached())))
        {
            PlayNext();
        }else
        {
            //LOG(LOG_VERBOSE, "Continuing playback: audio = %s(EOF=%d), video = %s(EOF=%d)", mAudioWorker->CurrentFile().toStdString().c_str(), mAudioWorker->EofReached(), mVideoWorker->CurrentFile().toStdString().c_str(), mVideoWorker->EofReached());
        }
    }else
    	LOG(LOG_VERBOSE, "Got wrong timer ID: %d, waiting for %d", pEvent->timerId(), mTimerId);
}

void OverviewPlaylistWidget::dragLeaveEvent(QDragLeaveEvent *pEvent)
{
    LOG(LOG_VERBOSE, "DragLeave");
    QWidget::dragLeaveEvent(pEvent);
}

void OverviewPlaylistWidget::dragMoveEvent(QDragMoveEvent *pEvent)
{
    LOG(LOG_VERBOSE, "DragMove");
    if (pEvent->mimeData()->hasUrls())
    {
        pEvent->acceptProposedAction();
        QList<QUrl> tList = pEvent->mimeData()->urls();
        QUrl tUrl;
        int i = 0;

        foreach(tUrl, tList)
            LOG(LOG_VERBOSE, "New moving drag+drop url (%d) \"%s\"", ++i, tUrl.toString().toStdString().c_str());
        return;
    }
}

void OverviewPlaylistWidget::dragEnterEvent(QDragEnterEvent *pEvent)
{
    LOG(LOG_VERBOSE, "DragEnter");
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

    LOG(LOG_VERBOSE, "Drop");
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
        Play(mCurrentRowInGui);
}

int OverviewPlaylistWidget::GetListSize()
{
    int tResult = 0;

    mPlaylistMutex.lock();
    tResult = mPlaylist.size();
    mPlaylistMutex.unlock();

    return tResult;
}

void OverviewPlaylistWidget::CheckAndRemoveFilePrefix(QString &pEntry)
{
    // remove "file:///" and "file://" from the beginning if existing
    #ifdef WINDOWS
        if (pEntry.toLower().startsWith("file:///"))
            pEntry = pEntry.right(pEntry.size() - 8);

        if (pEntry.toLower().startsWith("file://"))
            pEntry = pEntry.right(pEntry.size() - 7);
    #else
        if (pEntry.toLower().startsWith("file:///"))
            pEntry = pEntry.right(pEntry.size() - 7);

        if (pEntry.toLower().startsWith("file://"))
            pEntry = pEntry.right(pEntry.size() - 6);
    #endif

}

void OverviewPlaylistWidget::AddPlaylist(Playlist pList, bool pStartPlayback)
{
	LOG(LOG_VERBOSE, "Adding playlist with %d entries", pList.size());

    if(pList.size() > 0)
    {
        mPlaylistMutex.lock();

        int tInsertionPosition = mPlaylist.size();

        PlaylistEntry tPlaylistEntry;
        foreach(tPlaylistEntry, pList)
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
}

void OverviewPlaylistWidget::AddEntry(QString pLocation, bool pStartPlayback)
{
    LOG(LOG_VERBOSE, "Adding playlist entry: %s", pLocation.toStdString().c_str());
    CheckAndRemoveFilePrefix(pLocation);

	Playlist tPlaylist = Parse(pLocation);
	LOG(LOG_VERBOSE, "Found %d new playlist entries", tPlaylist.size());

	AddPlaylist(tPlaylist, pStartPlayback);
}

void OverviewPlaylistWidget::ParseAndAppendDownloadedFile(QString pWebLocation)
{
    LOG(LOG_VERBOSE, "Requesting for web data from: %s", pWebLocation.toStdString().c_str());
    QNetworkRequest tRequest(QUrl::fromEncoded(pWebLocation.toLocal8Bit()));
    mNetworkAccessManager->get(tRequest);
}

void OverviewPlaylistWidget::ReceivedFileFromServer(QNetworkReply *pServerAnswer)
{
    QByteArray tFileData = pServerAnswer->readAll();
    LOG(LOG_VERBOSE, "Received a file from server:\n%s", tFileData.data());

    // write to a temporary file
    QTemporaryFile tTmpFile;
    if(tTmpFile.open())
    {
        tTmpFile.write(tFileData);
        tTmpFile.close();

        QString tReceivedFileName = pServerAnswer->request().url().toString();
        QString tTmpFileName = tTmpFile.fileName();

        LOG(LOG_VERBOSE, " ..belongs to web source: %s", tReceivedFileName.toStdString().c_str());

        // add the received content to the playlist
        Playlist tPlaylist;
        if (tReceivedFileName.endsWith(".m3u"))
        {// an M3U playlist file
            LOG(LOG_VERBOSE, " ..identified as M3U playlist");
            tPlaylist = ParseM3U(tTmpFileName, true, true);
        }else if (tReceivedFileName.endsWith(".pls"))
        {// a PLS playlist file
            LOG(LOG_VERBOSE, " ..identified as PLS playlist");
            tPlaylist = ParsePLS(tTmpFileName, true, true);
        }else if (tReceivedFileName.endsWith(".wmx"))
        {
            LOG(LOG_VERBOSE, " ..identified as WMX playlist");
            tPlaylist = ParseWMX(tTmpFileName, true, true);
        }else{
            LOG(LOG_ERROR, "Unsupported web content from: %s", tReceivedFileName.toStdString().c_str());
        }
        if(tPlaylist.size() > 0)
            AddPlaylist(tPlaylist, true);
    }else{
        LOG(LOG_ERROR, "Could not create a temporary file for storing the download web data");
    }
}

void OverviewPlaylistWidget::ReceivedFileFromServerNeedAuthenication(QNetworkReply *pReply, QAuthenticator *pAuthenticator)
{
    LOG(LOG_WARN, "Authentication required");
}

Playlist OverviewPlaylistWidget::Parse(QString pLocation, QString pName, bool pAcceptVideo, bool pAcceptAudio)
{
	Playlist tResult;
	PlaylistEntry tPlaylistEntry;

	if (pLocation == "")
		return tResult;

    CheckAndRemoveFilePrefix(pLocation);

    sParseRecursionCount ++;
	if (sParseRecursionCount < MAX_PARSER_RECURSIONS)
	{
		bool tIsWebUrl = IS_SUPPORTED_WEB_LINK(pLocation);
		bool tIsSupportedPlaylist = IS_SUPPORTED_PLAYLIST(pLocation);
		bool tIsNormalFile = true;

		LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Parsing %s", pLocation.toStdString().c_str());

        if (!tIsWebUrl)
        {// something locally stored
            LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "  ..found local data: %s", pLocation.toStdString().c_str());
            if (QDir(pLocation).exists())
            {// a directory
                tResult += ParseDIR(pLocation, pAcceptVideo, pAcceptAudio);
            }else if (pLocation.endsWith(".m3u"))
            {// an M3U playlist file
                tResult += ParseM3U(pLocation, pAcceptVideo, pAcceptAudio);
            }else if (pLocation.endsWith(".pls"))
            {// a PLS playlist file
                tResult += ParsePLS(pLocation, pAcceptVideo, pAcceptAudio);
            }else if (pLocation.endsWith(".wmx"))
            {// a WMX shortcut file
                tResult += ParseWMX(pLocation, pAcceptVideo, pAcceptAudio);
            }
		}

        if(tResult.size() == 0)
		{// an url or a local file?
            LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "  ..found web data: %s", pLocation.toStdString().c_str());
		    if((tIsWebUrl) && (tIsSupportedPlaylist))
		    {// download the file and access its contents
	            LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "  ..found web playlist: %s", pLocation.toStdString().c_str());
		        PLAYLISTWIDGET.ParseAndAppendDownloadedFile(pLocation);
		    }else
		    {// direct file/URL access
                LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "  ..found web stream: %s", pLocation.toStdString().c_str());

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
                            tPlaylistEntry.Icon = QIcon(":/images/22_22/AV_Play.png");
                    }

                    // save playlist entry
                    tResult.push_back(tPlaylistEntry);
                }else
                {
                    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Ignoring entry: %s at location %s", tPlaylistEntry.Name.toStdString().c_str(), tPlaylistEntry.Location.toStdString().c_str());
                }
		    }
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

	if (pFilePlaylist.toLower().startsWith("http://"))
	{
	    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "M3U file is located on a web server");
        return tResult;
	}

	#ifdef WINDOWS
    	QString tDir = pFilePlaylist.left(pFilePlaylist.lastIndexOf('\\'));
	#else
    	QString tDir = pFilePlaylist.left(pFilePlaylist.lastIndexOf('/'));
	#endif
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
				if (!tLineString.toLower().startsWith("#ext"))
				{// we have a location and the entry is complete
                    bool tLocationIsValid = true;

                    if (IS_SUPPORTED_WEB_LINK(tLineString))
                    {// web link
                        tPlaylistEntry.Location = tLineString;
                        LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found playlist entry web location: %s", tPlaylistEntry.Location.toStdString().c_str());
                    }else
                    {// local file
                        if (QFile::exists(tLineString))
                        {
                            tPlaylistEntry.Location = tLineString;
                            LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found playlist entry local relative location: %s", tPlaylistEntry.Location.toStdString().c_str());
                        }else if(QFile::exists(tDir + "/" +  tLineString))
                        {
                            tPlaylistEntry.Location = tDir + "/" + tLineString;
                            LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found playlist entry local absolute location: %s", tPlaylistEntry.Location.toStdString().c_str());
                        }else
                        {
                            LOGEX(OverviewPlaylistWidget, LOG_WARN, "Found playlist entry with invalid Invalid location: %s, playlist is located in %s", tLineString.toStdString().c_str(), tDir.toStdString().c_str());
                            tLocationIsValid = false;
                        }
                    }

                    if(tLocationIsValid)
                    {
                        if (tPlaylistEntry.Name == "")
                            tPlaylistEntry.Name = tPlaylistEntry.Location;
                        tResult += Parse(tPlaylistEntry.Location, tPlaylistEntry.Name, pAcceptVideo, pAcceptAudio);
                    }
                    tPlaylistEntry.Location = "";
                    tPlaylistEntry.Name = "";
				}else
				{
					if (tLineString.toLower().startsWith("#extinf"))
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

Playlist OverviewPlaylistWidget::ParseWMX(QString pFilePlaylist, bool pAcceptVideo, bool pAcceptAudio)
{
	Playlist tResult;
	PlaylistEntry tPlaylistEntry;
	tPlaylistEntry.Location = "";
	tPlaylistEntry.Name = "";

    if (pFilePlaylist.toLower().startsWith("http://"))
    {
        LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "WMX file is located on a web server");
        return tResult;
    }

	#ifdef WINDOWS
		QString tDir = pFilePlaylist.left(pFilePlaylist.lastIndexOf('\\'));
	#else
		QString tDir = pFilePlaylist.left(pFilePlaylist.lastIndexOf('/'));
	#endif
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Parsing WMX short cut file %s", pFilePlaylist.toStdString().c_str());
    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "..in directory: %s", tDir.toStdString().c_str());

    QFile tPlaylistFile(pFilePlaylist);
    if (!tPlaylistFile.open(QIODevice::ReadOnly))
    {
    	LOGEX(OverviewPlaylistWidget, LOG_ERROR, "Couldn't read WMX playlist from %s", pFilePlaylist.toStdString().c_str());
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
				LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found playlist entry location: %s", tLineString.toStdString().c_str());
				tResult += Parse(tLineString, tLineString, pAcceptVideo, pAcceptAudio);
        	}

            tLine = tPlaylistFile.readLine();
        }
    }

    return tResult;
}

Playlist OverviewPlaylistWidget::ParsePLS(QString pFilePlaylist, bool pAcceptVideo, bool pAcceptAudio)
{
    PlaylistEntry tPlaylistEntry;
    int tPlaylistEntries = -1;
    int tLoadedPlaylistEntries = 0;
	Playlist tResult;

    if (pFilePlaylist.toLower().startsWith("http://"))
    {
        LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "PLS file is located on a web server");
        return tResult;
    }

	#ifdef WINDOWS
		QString tDir = pFilePlaylist.left(pFilePlaylist.lastIndexOf('\\'));
	#else
		QString tDir = pFilePlaylist.left(pFilePlaylist.lastIndexOf('/'));
	#endif
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
        tPlaylistEntry.Location = "";
        tPlaylistEntry.Name = "";

        while (((tLoadedPlaylistEntries < tPlaylistEntries) || (tPlaylistEntries == -1)) && (!tLine.isEmpty()))
        {
            QString tLineString = QString(tLine);

            // remove line delimiters
            while((tLineString.endsWith(QChar(0x0A))) || (tLineString.endsWith(QChar(0x0D))))
                tLineString = tLineString.left(tLineString.length() - 1); //remove any "new line" char from the end

            QStringList tLineSplit = tLineString.split(("="));
            if (tLineSplit.size() == 2)
            {
                QString tKey = tLineSplit[0].toLower();
                QString tValue = tLineSplit[1];
                LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found key \"%s\" with value \"%s\"", tKey.toStdString().c_str(), tValue.toStdString().c_str());

                /*
                 * Add an entry to the result
                 */
                if((tKey.startsWith("file")) && (tPlaylistEntry.Location != ""))
                {
                    LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found playlist entry: \"%s\" at location \"%s\"", tPlaylistEntry.Name.toStdString().c_str(), tPlaylistEntry.Location.toStdString().c_str());
                     tResult += Parse(tPlaylistEntry.Location, tPlaylistEntry.Name, pAcceptVideo, pAcceptAudio);
                     tPlaylistEntry.Location = "";
                     tPlaylistEntry.Name = "";
                     tLoadedPlaylistEntries++;
                }

                // parse the playlist line
                if (tKey.startsWith("numberofentries"))
                {// "NumberOfEntries"
                    bool tConversionWasOkay = false;
                    tPlaylistEntries = tValue.toInt(&tConversionWasOkay);
                    if (!tConversionWasOkay)
                    {
                    	LOGEX(OverviewPlaylistWidget, LOG_ERROR, "Unable to convert \"%s\" into an integer value", tValue.toStdString().c_str());
                        return tResult;
                    }
                }else if (tKey.startsWith("file"))
                {// "File"
					if (IS_SUPPORTED_WEB_LINK(tValue))
					{// web link
						tPlaylistEntry.Location = tValue;
					}else
					{// local file
						if ((!tLineString.startsWith("/")) && (!tValue.startsWith("\\")) && (!tValue.indexOf(":\\") == 1))
							tPlaylistEntry.Location = tDir + tValue;
						else
							tPlaylistEntry.Location = tValue;
					}
                }else if (tKey.startsWith("title"))
                {// "Title"
                    tPlaylistEntry.Name = tValue;
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
        }// while()
    }

    /*
     * Add the last entry to the result
     */
    if (tLoadedPlaylistEntries < tPlaylistEntries)
    {
    	LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Loaded %d of %d playlist entries, assuming a pending playlist entry", tLoadedPlaylistEntries, tPlaylistEntries);
        LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "Found playlist entry: \"%s\" at location \"%s\"", tPlaylistEntry.Name.toStdString().c_str(), tPlaylistEntry.Location.toStdString().c_str());
		tResult += Parse(tPlaylistEntry.Location, tPlaylistEntry.Name, pAcceptVideo, pAcceptAudio);
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
        Playlist tAddPlaylist = Parse(pDirLocation + '/' + tDirEntry, "", pAcceptVideo, pAcceptAudio);
        PlaylistEntry tEntry;
        foreach(tEntry , tAddPlaylist)
        {
            LOGEX(OverviewPlaylistWidget, LOG_VERBOSE, "  ..adding directory entry: %s", tEntry.Location.toStdString().c_str());
        }
        tResult += tAddPlaylist;
    }

    return tResult;
}

QString OverviewPlaylistWidget::GetPlaylistEntry(int pIndex)
{
    QString tResult = "";

    mPlaylistMutex.lock();

    if (pIndex < mPlaylist.size())
        tResult = mPlaylist[pIndex].Location;

    mPlaylistMutex.unlock();

    return tResult;
}

QString OverviewPlaylistWidget::GetPlaylistEntryName(int pIndex)
{
    QString tResult = "";

    mPlaylistMutex.lock();

    if (pIndex < mPlaylist.size())
        tResult = mPlaylist[pIndex].Name;

    mPlaylistMutex.unlock();

    return tResult;
}

void OverviewPlaylistWidget::DeletePlaylistEntry(int pIndex)
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

void OverviewPlaylistWidget::RenamePlaylistEntry(int pIndex, QString pName)
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
        int tPlaylistIndex = GetPlaylistIndexFromGuiRow(tSelectedRow);
        QString tCurrentEntry = GetPlaylistEntry(tPlaylistIndex);
        QString tCurrentName = GetPlaylistEntryName(tPlaylistIndex);
        QString tFillSpace = "";
        for (int i = 0; i < tCurrentEntry.length(); i++)
            tFillSpace += "  ";
        LOG(LOG_VERBOSE, "User wants to rename \"%s\" at index %d", tCurrentEntry.toStdString().c_str(), tSelectedRow);
        bool tOkay = false;
        QString tNewName = QInputDialog::getText(this, "Enter name for \"" + tCurrentEntry + "\"", "New name:           " + tFillSpace, QLineEdit::Normal, tCurrentName, &tOkay);
        if ((tOkay) && (!tNewName.isEmpty()))
        {
            RenamePlaylistEntry(tPlaylistIndex, tNewName);
        }
    }
}

void OverviewPlaylistWidget::ActionPlay()
{
	LOG(LOG_VERBOSE, "Triggered play");
	Play(mCurrentRowInGui);
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

void OverviewPlaylistWidget::FillRow(int pRow, const PlaylistEntry &pEntry)
{
    if (mLwFiles->item(pRow) != NULL)
    {
        mLwFiles->item(pRow)->setText(pEntry.Name);
        mLwFiles->item(pRow)->setData(Qt::UserRole, pRow);
    }else
    {
        QListWidgetItem *tItem = new QListWidgetItem(pEntry.Icon, pEntry.Name);
        tItem->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        tItem->setData(Qt::UserRole, pRow);
        mLwFiles->insertItem(pRow, tItem);
    }
}

void OverviewPlaylistWidget::UpdateView(int pDeletectPlaylistIndex)
{
    Playlist::iterator tIt;
    int tRow = 0;
    int tSelectedRow = -1;

    //LOG(LOG_VERBOSE, "Updating view");

    LOG(LOG_VERBOSE, "Found row selection: %d, deleted PL index: %d", mCurrentRowInGui, pDeletectPlaylistIndex);

    // re-order the playlist according to the ordering in the GUI
    mPlaylistMutex.lock();
    if (mLwFiles->count() > 0)
    {
        Playlist tNewPlaylist;
        int i;
        for (i = 0; i < mLwFiles->count(); i++)
        {
            int tPlayistIndex = GetPlaylistIndexFromGuiRow(i);
            if (i != pDeletectPlaylistIndex)
            {
                if ((tPlayistIndex >= 0) && (tPlayistIndex < mPlaylist.size()))
                    tNewPlaylist.push_back(mPlaylist[tPlayistIndex]);
            }
        }

        // are there additional new playlist entries?
        if (pDeletectPlaylistIndex == -1)
        {
            for (; i < mPlaylist.size(); i++)
            {
                if (i != pDeletectPlaylistIndex)
                {
                    if ((i >= 0) && (i < mPlaylist.size()))
                        tNewPlaylist.push_back(mPlaylist[i]);
                }
            }
        }
        mPlaylist = tNewPlaylist;
    }

    // overwrite the playlist with the new ordered one
    mPlaylistMutex.unlock();

    // reset the GUI
    if (GetListSize() != mLwFiles->count())
    {
        mLwFiles->clear();
    }

    // update the GUI
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

    if (mCurrentRowInGui != -1)
        mLwFiles->setCurrentRow (mCurrentRowInGui);
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
