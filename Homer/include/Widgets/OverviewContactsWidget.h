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
 * Purpose: contact list dock widget
 * Author:  Thomas Volkert
 * Since:   2011-01-07
 */

#ifndef _OVERVIEW_CONTACTS_WIDGET_
#define _OVERVIEW_CONTACTS_WIDGET_

#include <Dialogs/ContactEditDialog.h>
#include <MeetingEvents.h>
#include <ContactsPool.h>

#include <QDockWidget>
#include <QModelIndex>
#include <QHostInfo>
#include <QStyledItemDelegate>

#include <ui_OverviewContactsWidget.h>

namespace Homer { namespace Gui {

// forward declarations
class ContactListModel;
struct ContactDescriptor;

///////////////////////////////////////////////////////////////////////////////
// use the SIP-Events-structure for signaling the deletion of a participant ///
///////////////////////////////////////////////////////////////////////////////
#define ADD_PARTICIPANT                                              200000

class AddParticipantEvent:
    public Homer::Conference::TEvent<AddParticipantEvent, ADD_PARTICIPANT>
{
public:
    AddParticipantEvent(QString pUser, QString pHost, QString pPort, QString pIp, int pInitState):User(pUser), Host(pHost), Port(pPort), Ip(pIp), InitState(pInitState)
        { }

    QString User, Host, Port;
    QString Ip;
    int InitState;
};

///////////////////////////////////////////////////////////////////////////////

class OverviewContactsWidget :
    public QDockWidget,
    public Ui_OverviewContactsWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    OverviewContactsWidget(QAction *pAssignedAction, QMainWindow *pMainWindow);

    /// The destructor.
    virtual ~OverviewContactsWidget();

    void InsertNew(QString pParticipant);

public slots:
    void SetVisible(bool pVisible);

private slots:
    void processCustomContextMenuRequest(const QPoint &pPos);
    void Edit(const QModelIndex &pIndex);
    void ContactParticipantDoubleClick(const QModelIndex &pIndex);
    void LookedUpContactHost(const QHostInfo &pHost);
    void SaveList();
    void LoadList();
    void InsertNew();
    void DeleteSelected();

private:
    void initializeGUI();
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pEvent);
    virtual void paintEvent(QPaintEvent *pEvent);
    virtual void timerEvent(QTimerEvent *pEvent);

    void ContactParticipantDelegateToMainWindow(ContactDescriptor *pContact, QString pIp, bool pCallAfterwards);
    void ContactParticipant(ContactDescriptor *pContact, bool pCallAfterwards = false);
    void InsertCopy(ContactDescriptor *pContact);
    void Edit(ContactDescriptor *pContact);
    void Delete(ContactDescriptor *pContact);
    void Dialog2Contact(ContactEditDialog *pCED, ContactDescriptor *pContact, bool pNewContact);
    void Contact2Dialog(ContactDescriptor *pContact, ContactEditDialog *pCED);

    QMainWindow             *mMainWindow;
    QPoint                  mWinPos;
    QAction                 *mAssignedAction;
    int                     mTimerId;
    ContactListModel        *mContactListModel;

    // we are little bit lazy here: we assume that no user will click faster than DNS system works!! (otherwise we would have to create a queue here)
    ContactDescriptor 		*mContact;
    bool					mCallAfterwards;
};

class ContactListModel:
    public QAbstractItemModel
{
    Q_OBJECT;
public:
    ContactListModel(QObject *pParent);

    virtual ~ContactListModel();

    virtual int columnCount(const QModelIndex &pParent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &pIndex, int pRole = Qt::DisplayRole) const;
    virtual QModelIndex index(int pRow, int pColumn, const QModelIndex &pParent = QModelIndex()) const;
    virtual QModelIndex parent(const QModelIndex &pIndex) const;
    virtual int rowCount(const QModelIndex &pParent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &pIndex) const;
    virtual QVariant headerData(int pSection, Qt::Orientation pOrientation, int pRole = Qt::DisplayRole) const;
    void UpdateView();

private:
    virtual void sort (int pColumn, Qt::SortOrder pOrder = Qt::AscendingOrder);
    QString GetContactName(const QModelIndex &pIndex) const;
    bool GetContactAvailability(const QModelIndex &pIndex) const;
    void *GetContactPointer(unsigned int pIndex) const;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

