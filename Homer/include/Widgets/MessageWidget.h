/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Widget for handling instant message
 * Since:   2008-12-02
 */

#ifndef _MESSAGE_WIDGET_
#define _MESSAGE_WIDGET_

#include <HBSocket.h>

#include <QWidget>
#include <QEvent>

#include <Widgets/OverviewContactsWidget.h>
#include <ui_MessageWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class MessageWidget :
    public QWidget,
    public Ui_MessageWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    MessageWidget(QWidget* pParent = NULL);
    void Init(QMenu *pMenu, QString pParticipant, enum TransportType pParticipantTransport, bool pVisible = true);

    /// The destructor.
    virtual ~MessageWidget();
    void SetVisible(bool pVisible);
    void AddMessage(QString pSender, QString pMessage, bool pLocalMessage = false);
    void ShowNewState();
    void UpdateParticipantName(QString pParticipantName);
    void UpdateParticipantState(int pState);

    void InitializeMenuMessagesSettings(QMenu *pMenu);

public slots:
	void SendMessage();
    void SendFile(QList<QUrl> *tFileUrls = NULL);
    void ToggleVisibility();
    void SelectedMenuMessagesSettings(QAction *pAction);

private slots:
    void AddPArticipantToContacts();

private:
    QString ReplaceSmilesAndUrls(QString pMessage);
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);
    virtual void dragEnterEvent(QDragEnterEvent *pEvent);
    virtual void dropEvent(QDropEvent *pEvent);

    bool IsKnownContact();
    void initializeGUI();

    QPoint              mWinPos;
    QAction             *mAssignedAction;
    QString             mMessageHistory;
    QString             mParticipant;
    enum TransportType  mParticipantTransport;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
