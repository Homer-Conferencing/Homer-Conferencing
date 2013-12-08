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
 * Purpose: playlist dock widget
 * Since:   2011-03-22
 */

#ifndef _OVERVIEW_PLAYLIST_WIDGET_
#define _OVERVIEW_PLAYLIST_WIDGET_

#include <Widgets/VideoWidget.h>
#include <Widgets/AudioWidget.h>
#include <PacketStatistic.h>

#include <QDockWidget>
#include <QTimerEvent>
#include <QShortcut>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QMutex>
#include <QIcon>
#include <QListWidget>

#include <ui_OverviewPlaylistWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

struct PlaylistEntry{
    QString     Name;
    QString     Location;
    QIcon       Icon;
};
typedef QList<PlaylistEntry>    Playlist;

///////////////////////////////////////////////////////////////////////////////

#define PLAYLISTWIDGET OverviewPlaylistWidget::GetInstance()

///////////////////////////////////////////////////////////////////////////////

class OverviewPlaylistWidget :
    public QDockWidget,
    public Ui_OverviewPlaylistWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    OverviewPlaylistWidget(QAction *pAssignedAction, QMainWindow* pMainWindow, VideoWorkerThread *pVideoWorker, AudioWorkerThread *pAudioWorker);

    /// The destructor.
    virtual ~OverviewPlaylistWidget();

    static OverviewPlaylistWidget& GetInstance();

    static QString LetUserSelectVideoSaveFile(QWidget *pParent, QString pDescription);
    static bool IsVideoFile(QString pFileName);
    static QStringList LetUserSelectAudioFile(QWidget *pParent, QString pDescription, bool pMultipleFiles = true);
    static QString LetUserSelectAudioSaveFile(QWidget *pParent, QString pDescription);
    static bool IsAudioFile(QString pFileName);
    static QStringList LetUserSelectMediaFile(QWidget *pParent, QString pDescription, bool pMultipleFiles = true, bool pOnlyDirectories = false);

    /* add playlist entries */
    void AddEntry(QString pLocation, bool pStartPlayback = false);

    void StartPlaylist(bool pAddDirectories = false);
    void StopPlaylist();
    void PlayNext();
    void PlayPrevious();

    /* parse playlist entries */
    static Playlist Parse(QString pLocation, QString pName = "", bool pAcceptVideo = true, bool pAcceptAudio = true);
    static void CheckAndRemoveFilePrefix(QString &pEntry);

public slots:
    void SetVisible(bool pVisible);

private slots:
    bool AddPlaylistFilesDirsDialog(bool pAddDirectories = false);
    bool AddPlaylistUrlsDialog();
    void AddEntryDialogSc();
    void DelPlaylistEntriesDialog();
    void DelEntryDialogSc();
    void SavePlaylistDialog();
    void Play(int pIndex = -1);
    void ActionPlay();
    void ActionPause();
    void ActionStop();

private:
    virtual void customEvent(QEvent* pEvent);
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);
    void timerEvent(QTimerEvent *pEvent);
    void dragLeaveEvent(QDragLeaveEvent *pEvent);
    void dragMoveEvent(QDragMoveEvent *pEvent);
    void dragEnterEvent(QDragEnterEvent *pEvent);
    void dropEvent(QDropEvent *pEvent);

    void initializeGUI();
    void FillRow(int pRow, const PlaylistEntry &pEntry);
    void UpdateView(int pDeletectPlaylistIndex = -1);

    int GetListSize();

    /* parse playlist entries - helpers */
    static Playlist ParseM3U(QString pFilePlaylist, bool pAcceptVideo, bool pAcceptAudio);
    static Playlist ParsePLS(QString pFilePlaylist, bool pAcceptVideo, bool pAcceptAudio);
    static Playlist ParseWMX(QString pFilePlaylist, bool pAcceptVideo, bool pAcceptAudio);
    static Playlist ParseDIR(QString pDirLocation, bool pAcceptVideo, bool pAcceptAudio);

    /* save playlist entries */
    void SaveM3U(QString pFileName);

    QString GetPlaylistEntry(int pIndex);
    QString GetPlaylistEntryName(int pIndex);
    void DeletePlaylistEntry(int pIndex);
    void RenamePlaylistEntry(int pIndex, QString pName);
    void ResetList();
    int GetPlaylistIndexFromGuiRow(int pRow);

    void RenameDialog();

    bool 				mEndlessLoop;
    bool				mIsPlayed;
    int 				mCurrentRowInGui;
    QPoint              mWinPos;
    QAction             *mAssignedAction;
    QShortcut           *mShortcutDel, *mShortcutIns;
    int 				mTimerId;
    VideoWorkerThread   *mVideoWorker;
    AudioWorkerThread   *mAudioWorker;
    QString 			mCurrentFile;
    bool                mCurrentFileAudioPlaying;
    bool                mCurrentFileVideoPlaying;
    Playlist            mPlaylist;
    QMutex              mPlaylistMutex;
    static int			sParseRecursionCount;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

