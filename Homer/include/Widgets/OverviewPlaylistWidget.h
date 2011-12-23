/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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

#include <QDockWidget>
#include <QTimerEvent>
#include <QShortcut>
#include <QMutex>
#include <Widgets/VideoWidget.h>
#include <Widgets/AudioWidget.h>
#include <PacketStatistic.h>

#include <ui_OverviewPlaylistWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

//#define SYNCHRONIZE_AUDIO_VIDEO

#define	PLAYLIST_VIDEO						1
#define	PLAYLIST_AUDIO						2
#define	PLAYLIST_MOVIE						3

///////////////////////////////////////////////////////////////////////////////

class OverviewPlaylistWidget :
    public QDockWidget,
    public Ui_OverviewPlaylistWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    OverviewPlaylistWidget(QAction *pAssignedAction, QMainWindow* pMainWindow, int pPlaylistId, VideoWorkerThread *pVideoWorker, AudioWorkerThread *pAudioWorker);

    /// The destructor.
    virtual ~OverviewPlaylistWidget();

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
    void PlayItem(QListWidgetItem *pItem);
    void PlayNext();
    void PlayLast();
    void ActionPlay();
    void ActionPause();
    void ActionStop();
    void ActionNext();
    void ActionLast();

public:
    static QStringList LetUserSelectVideoFile(QWidget *pParent, QString pDescription, bool pMultipleFiles = true);
    static QString LetUserSelectVideoSaveFile(QWidget *pParent, QString pDescription);
    static bool IsVideoFile(QString pFileName);
    static QStringList LetUserSelectAudioFile(QWidget *pParent, QString pDescription, bool pMultipleFiles = true);
    static QString LetUserSelectAudioSaveFile(QWidget *pParent, QString pDescription);
    static bool IsAudioFile(QString pFileName);
    static QStringList LetUserSelectMovieFile(QWidget *pParent, QString pDescription, bool pMultipleFiles = true);

private:
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);
    void timerEvent(QTimerEvent *pEvent);
    void dragEnterEvent(QDragEnterEvent *pEvent);
    void dropEvent(QDropEvent *pEvent);

    void initializeGUI();
    void AddFileToList(QString pFile);

    bool 				mEndlessLoop;
    bool				mIsPlayed;
    int 				mCurrentFileId;
    QPoint              mWinPos;
    QAction             *mAssignedAction;
    QShortcut           *mShortcutDel, *mShortcutIns;
    int					mPlaylistId;
    int 				mTimerId;
    VideoWorkerThread   *mVideoWorker;
    AudioWorkerThread   *mAudioWorker;
    QString 			mCurrentFile;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

