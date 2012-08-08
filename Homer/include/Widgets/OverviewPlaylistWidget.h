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
 * Author:  Thomas Volkert
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

    static QStringList LetUserSelectVideoFile(QWidget *pParent, QString pDescription, bool pMultipleFiles = true);
    static QString LetUserSelectVideoSaveFile(QWidget *pParent, QString pDescription);
    static bool IsVideoFile(QString pFileName);
    static QStringList LetUserSelectAudioFile(QWidget *pParent, QString pDescription, bool pMultipleFiles = true);
    static QString LetUserSelectAudioSaveFile(QWidget *pParent, QString pDescription);
    static bool IsAudioFile(QString pFileName);
    static QStringList LetUserSelectMovieFile(QWidget *pParent, QString pDescription, bool pMultipleFiles = true);
    static QStringList LetUserSelectMediaFile(QWidget *pParent, QString pDescription, bool pMultipleFiles = true);

public slots:
    void SetVisible(bool pVisible);
    void StartPlaylist();
    void StopPlaylist();

private slots:
    void AddEntryDialog();
    void AddEntryDialogSc();
    void DelEntryDialog();
    void DelEntryDialogSc();
    void SaveListDialog();
    void Play(int pIndex = -1);
    void PlayNext();
    void PlayLast();
    void ActionPlay();
    void ActionPause();
    void ActionStop();
    void ActionNext();
    void ActionLast();

private:
    virtual void customEvent(QEvent* pEvent);
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);
    void timerEvent(QTimerEvent *pEvent);
    void dragEnterEvent(QDragEnterEvent *pEvent);
    void dropEvent(QDropEvent *pEvent);

    void initializeGUI();
    void FillRow(int pRow, const PlaylistEntry &pEntry);
    void UpdateView();

    int GetListSize();
    void AddEntry(QString pLocation, QString pName = "");
    void AddM3UToList(QString pFilePlaylist);
    void AddPLSToList(QString pFilePlaylist);
    QString GetListEntry(int pIndex);
    QString GetListEntryName(int pIndex);
    void DeleteListEntry(int pIndex);
    void RenameListEntry(int pIndex, QString pName);
    void ResetList();

    void RenameDialog();

    bool 				mEndlessLoop;
    bool				mIsPlayed;
    int 				mCurrentFileId;
    QPoint              mWinPos;
    QAction             *mAssignedAction;
    QShortcut           *mShortcutDel, *mShortcutIns;
    int 				mTimerId;
    VideoWorkerThread   *mVideoWorker;
    AudioWorkerThread   *mAudioWorker;
    QString 			mCurrentFile;
    Playlist            mPlaylist;
    QMutex              mPlaylistMutex;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

