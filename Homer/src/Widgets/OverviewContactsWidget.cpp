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
 * Purpose: Implementation of OverviewContactsWidget.h
 * Since:   2011-01-07
 */

#include <Widgets/OverviewContactsWidget.h>
#include <MainWindow.h>
#include <Configuration.h>
#include <Logger.h>

#include <QStyledItemDelegate>
#include <QDockWidget>
#include <QModelIndex>
#include <QHostInfo>
#include <QPoint>
#include <QFileDialog>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

OverviewContactsWidget *sOverviewContactsWidget = NULL;

OverviewContactsWidget& OverviewContactsWidget::GetInstance()
{
	if (sOverviewContactsWidget == NULL)
		LOGEX(OverviewContactsWidget, LOG_WARN, "OverviewContactsWidget is still invalid");

    return *sOverviewContactsWidget;
}

OverviewContactsWidget::OverviewContactsWidget(QAction *pAssignedAction, QMainWindow* pMainWindow):
    QDockWidget(pMainWindow)
{
	sOverviewContactsWidget = this;
    mAssignedAction = pAssignedAction;
    mMainWindow = pMainWindow;

    mContactListModel = new ContactListModel(this);

    initializeGUI();

    setAllowedAreas(Qt::AllDockWidgetAreas);
    pMainWindow->addDockWidget(Qt::LeftDockWidgetArea, this);

    if (mAssignedAction != NULL)
    {
        connect(mAssignedAction, SIGNAL(triggered(bool)), this, SLOT(SetVisible(bool)));
        mAssignedAction->setChecked(true);
    }
    connect(toggleViewAction(), SIGNAL(toggled(bool)), mAssignedAction, SLOT(setChecked(bool)));
    connect(mTvContacts, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(processCustomContextMenuRequest(QPoint)));
    connect(mTvContacts, SIGNAL(doubleClicked(const QModelIndex)), this, SLOT(ContactParticipantDoubleClick(const QModelIndex)));
    connect(mTbSaveList, SIGNAL(clicked()), this, SLOT(SaveList()));
    connect(mTbLoadList, SIGNAL(clicked()), this, SLOT(LoadList()));
    connect(mTbAdd, SIGNAL(clicked()), this, SLOT(InsertNew()));
    connect(mTbDel, SIGNAL(clicked()), this, SLOT(DeleteSelected()));
    mTvContacts->setModel(mContactListModel);
    SetVisible(CONF.GetVisibilityContactsWidget());
    mAssignedAction->setChecked(CONF.GetVisibilityContactsWidget());
    CONTACTS.ProbeAvailabilityForAll();
}

OverviewContactsWidget::~OverviewContactsWidget()
{
    CONF.SetVisibilityContactsWidget(isVisible());
	delete mContactListModel;
}

///////////////////////////////////////////////////////////////////////////////

void OverviewContactsWidget::initializeGUI()
{
    setupUi(this);

    mTvContacts->sortByColumn(1);
}

void OverviewContactsWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewContactsWidget::SetVisible(bool pVisible)
{
	CONF.SetVisibilityContactsWidget(pVisible);
    if (pVisible)
    {
        move(mWinPos);
        show();
        // update GUI elements every x ms
        mTimerId = startTimer(CONF.GetContactPresenceCheckPeriod());
    }else
    {
        if (mTimerId != -1)
            killTimer(mTimerId);
        mWinPos = pos();
        hide();
    }
}

void OverviewContactsWidget::paintEvent(QPaintEvent *pEvent)
{
    QDockWidget::paintEvent(pEvent);
}

void OverviewContactsWidget::contextMenuEvent(QContextMenuEvent *pEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction(Homer::Gui::OverviewContactsWidget::tr("Add contact"));
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/22_22/Plus.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    QAction* tPopupRes = tMenu.exec(pEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare(Homer::Gui::OverviewContactsWidget::tr("Add contact")) == 0)
        {
            InsertNew();
            return;
        }
    }
}

void OverviewContactsWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif
    if (pEvent->timerId() == mTimerId)
    {
        CONTACTS.ProbeAvailabilityForAll();
        mContactListModel->UpdateView();
    }
}

void OverviewContactsWidget::keyPressEvent(QKeyEvent *pEvent)
{
    if (pEvent->key() == Qt::Key_Return)
    {
        ContactSelected();
        return;
    }
    if (pEvent->key() == Qt::Key_Insert)
    {
        InsertNew();
        return;
    }
    if (pEvent->key() == Qt::Key_F2)
    {
        EditSelected();
        return;
    }
    if (pEvent->key() == Qt::Key_Delete)
    {
        DeleteSelected();
        return;
    }
}

void OverviewContactsWidget::processCustomContextMenuRequest(const QPoint &pPos)
{
    QAction *tAction;

    QMenu tMenu(this);
    QPoint tRelPos = pPos;
    bool tValidEntryBelow = ((mTvContacts->indexAt(tRelPos).isValid()) && (mTvContacts->indexAt(tRelPos).internalPointer() != NULL));

    if (tValidEntryBelow)
    {
        //LOG(LOG_ERROR, "%s", tParticipant.toStdString().c_str());
        tAction = tMenu.addAction(Homer::Gui::OverviewContactsWidget::tr("Send message"));
        QIcon tIcon6;
        tIcon6.addPixmap(QPixmap(":/images/22_22/Message.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon6);
        tAction->setShortcut(Qt::Key_Enter);

        tAction = tMenu.addAction(Homer::Gui::OverviewContactsWidget::tr("Call"));
        QIcon tIcon5;
        tIcon5.addPixmap(QPixmap(":/images/22_22/Phone.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon5);

        tMenu.addSeparator();
    }

    tAction = tMenu.addAction(Homer::Gui::OverviewContactsWidget::tr("Add contact"));
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/22_22/Plus.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);
    tAction->setShortcut(Qt::Key_Insert);

    if (tValidEntryBelow)
    {
        tAction = tMenu.addAction(Homer::Gui::OverviewContactsWidget::tr("Edit contact"));
        QIcon tIcon3;
        tIcon3.addPixmap(QPixmap(":/images/22_22/Contact_Edit.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon3);
        tAction->setShortcut(Qt::Key_F2);

        tAction = tMenu.addAction(Homer::Gui::OverviewContactsWidget::tr("Duplicate contact"));
        QIcon tIcon2;
        tIcon2.addPixmap(QPixmap(":/images/22_22/Contact_Duplicate.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon2);

        tAction = tMenu.addAction(Homer::Gui::OverviewContactsWidget::tr("Delete contact"));
        QIcon tIcon4;
        tIcon4.addPixmap(QPixmap(":/images/22_22/Minus.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon4);
        tAction->setShortcut(Qt::Key_Delete);
    }

    tMenu.addSeparator();

    if (CONTACTS.ContactCount() > 0)
    {
        tAction = tMenu.addAction(Homer::Gui::OverviewContactsWidget::tr("Reset contact list"));
        QIcon tIcon3;
        tIcon3.addPixmap(QPixmap(":/images/22_22/Reload.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon3);

        tMenu.addSeparator();
    }

    tMenu.addSeparator();

    tAction = tMenu.addAction(Homer::Gui::OverviewContactsWidget::tr("Update availability"));
    tAction->setCheckable(true);
    tAction->setChecked(CONF.GetSipContactsProbing());

    tAction = tMenu.addAction(Homer::Gui::OverviewContactsWidget::tr("Detect unknown contacts"));
    tAction->setCheckable(true);
    tAction->setChecked(CONF.GetSipUnknownContactsProbing());

    QAction* tPopupRes = tMenu.exec(QCursor::pos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().contains(Homer::Gui::OverviewContactsWidget::tr("Send message")))
        {
            ContactSelected();
            return;
        }
        if (tPopupRes->text().contains(Homer::Gui::OverviewContactsWidget::tr("Call")))
        {
            ContactSelected(true);
            return;
        }
        if (tPopupRes->text().contains(Homer::Gui::OverviewContactsWidget::tr("Add contact")))
        {
            InsertNew();
            return;
        }
        if (tPopupRes->text().contains(Homer::Gui::OverviewContactsWidget::tr("Duplicate contact")))
        {
            InsertCopy((ContactDescriptor*)mTvContacts->indexAt(tRelPos).internalPointer());
            return;
        }
        if (tPopupRes->text().contains(Homer::Gui::OverviewContactsWidget::tr("Edit contact")))
        {
            EditSelected();
            return;
        }
        if (tPopupRes->text().contains(Homer::Gui::OverviewContactsWidget::tr("Delete contact")))
        {
            DeleteSelected();
            return;
        }

        if (tPopupRes->text().compare(Homer::Gui::OverviewContactsWidget::tr("Reset contact list")) == 0)
        {
            ResetList();
            return;
        }

        if (tPopupRes->text().contains(Homer::Gui::OverviewContactsWidget::tr("Update availability")))
        {
            bool tOldState = CONF.GetSipContactsProbing();
            CONF.SetSipContactsProbing(!CONF.GetSipContactsProbing());

            // trigger an explicit auto probing in case the user has activated this feature
            if (!tOldState)
                CONTACTS.ProbeAvailabilityForAll();

            return;
        }

        if (tPopupRes->text().contains(Homer::Gui::OverviewContactsWidget::tr("Detect unknown contacts")))
        {
            bool tOldState = CONF.GetSipUnknownContactsProbing();
            CONF.SetSipUnknownContactsProbing(!CONF.GetSipUnknownContactsProbing());

            // trigger an explicit auto probing in case the user has activated this feature
            if (!tOldState)
                CONTACTS.ProbeAvailabilityForAll();

            return;
        }

    }
}

void OverviewContactsWidget::Dialog2Contact(ContactEditDialog *pCED, ContactDescriptor *pContact, bool pNewContact)
{
    pContact->Name      = pCED->mLeName->text();
    if (pCED->mLeAddress->text().contains('@'))
    {
        pContact->User      = pCED->mLeAddress->text().section('@', 0, 0);
        pContact->Host      = pCED->mLeAddress->text().section('@', 1, 1);
    }else
    {
        pContact->User      = pContact->Name;
        pContact->Host      = pCED->mLeAddress->text();
    }
	pContact->Host = pContact->Host.toLower();
    pContact->Port      = (QString("%1").arg(pCED->mSbPort->value()));
    pContact->Transport = Socket::String2TransportType(pCED->mCbTransport->currentText().toStdString());
    pContact->Unknown = false;
    if (pNewContact)
    {
        pContact->Id    = CONTACTS.GetNextFreeId();
    }
}

void OverviewContactsWidget::Contact2Dialog(ContactDescriptor *pContact, ContactEditDialog *pCED)
{
    pCED->mLeName->setText(pContact->Name);
    pCED->mLeAddress->setText(pContact->User + "@" + pContact->Host.toLower());
    pCED->mSbPort->setValue((pContact->Port.toInt()));
    for (int i = 0; i < pCED->mCbTransport->count(); i++)
    {
        if (QString(Socket::TransportType2String(pContact->Transport).c_str()) == pCED->mCbTransport->itemText(i))
        {
        	pCED->mCbTransport->setCurrentIndex(i);
            break;
        }
    }
}

void OverviewContactsWidget::ContactParticipantDelegateToMainWindow(ContactDescriptor *pContact, QString pIp, bool pCallAfterwards)
{
    LOG(LOG_VERBOSE, "Delegating to contact participant to the main window (call afterwards: %d)..", pCallAfterwards);

    //CONTACTS.FavorizedContact(pContact->User, pContact->Host, pContact->Port);
    if (pCallAfterwards)
        QCoreApplication::postEvent(mMainWindow, (QEvent*) new QMeetingEvent(new AddParticipantEvent(pContact->User, pContact->Host, pContact->Port, pContact->Transport, pIp, CALLSTATE_RINGING)));
    else
        QCoreApplication::postEvent(mMainWindow, (QEvent*) new QMeetingEvent(new AddParticipantEvent(pContact->User, pContact->Host, pContact->Port, pContact->Transport, pIp, CALLSTATE_STANDBY)));

}

void OverviewContactsWidget::LookedUpContactHost(const QHostInfo &pHost)
{
    if (pHost.error() != QHostInfo::NoError)
    {
        LOG(LOG_ERROR, "Unable too lookup DNS entry for %s because of %s", pHost.hostName().toStdString().c_str(), pHost.errorString().toStdString().c_str());
        return;
    }

    foreach (QHostAddress tAddress, pHost.addresses())
    {
        LOG(LOG_VERBOSE, "Found DNS entry for %s with value %s", pHost.hostName().toStdString().c_str(), tAddress.toString().toStdString().c_str());
    }

    QString tIp = pHost.addresses().first().toString();
    ContactParticipantDelegateToMainWindow(mContact, tIp, mCallAfterwards);
}

void OverviewContactsWidget::ContactParticipant(ContactDescriptor *pContact, bool pCallAfterwards)
{
    if (pContact == NULL)
    {
        LOG(LOG_VERBOSE, "Cannot contact non existing contact");
        return;
    }

    mContact = pContact;
    mCallAfterwards = pCallAfterwards;

    // first sign is a letter? -> we have a DNS name here
    if (((pContact->Host[0] >= 'a') && (pContact->Host[0] <= 'z')) || ((pContact->Host[0] >= 'A') && (pContact->Host[0] <= 'Z')))
    {
        LOG(LOG_VERBOSE, "Try to lookup %s via DNS system", pContact->Host.toStdString().c_str());
        QHostInfo::lookupHost(pContact->Host, this, SLOT(LookedUpContactHost(QHostInfo)));
    }else
        ContactParticipantDelegateToMainWindow(mContact, pContact->Host, mCallAfterwards);
}

bool OverviewContactsWidget::InsertNew(QString pParticipant)
{
    bool tWasAdded = false;

    ContactEditDialog tCED;

    tCED.setWindowTitle(Homer::Gui::OverviewContactsWidget::tr("Add contact"));

    QString tUser;
    QString tPort;
    QString tHost;

    CONTACTS.SplitAddress(pParticipant, tUser, tHost, tPort);
    LOG(LOG_VERBOSE, "Going to add contact: %s (%s, %s, %s) to the contact list", pParticipant.toStdString().c_str(), tUser.toStdString().c_str(), tHost.toStdString().c_str(), tPort.toStdString().c_str());

    if(tPort == "")
    	tPort = "5060";
    tCED.mLeAddress->setText(tUser + "@" + tHost);
    int tPortScal;
    bool tOk = false;
    tPortScal = tPort.toInt(&tOk, 10);
    if(!tOk)
    {
        LOG(LOG_ERROR, "Unable to parse string to a valid scalar for the port value");
        return false;
    }
    tCED.mSbPort->setValue(tPortScal);
    tCED.mLeName->setText(tUser);
    tCED.mLeName->setFocus(Qt::TabFocusReason);


    if (tCED.exec() == QDialog::Accepted)
    {
        ContactDescriptor tContact;

        Dialog2Contact(&tCED, &tContact, true);
        CONTACTS.AddContact(tContact);
        mTvContacts->setVisible(true);
        mContactListModel->UpdateView();
        tWasAdded = true;
    }

    return tWasAdded;
}

void OverviewContactsWidget::InsertNew()
{
    ContactEditDialog tCED;

    tCED.setWindowTitle(Homer::Gui::OverviewContactsWidget::tr("Add contact"));
    tCED.mLeAddress->setText("user_account@" + QString(MEETING.GetHostAdr().c_str()));
    tCED.mLeName->setText("optional contact name");
    tCED.mLeName->setFocus(Qt::TabFocusReason);


    if (tCED.exec() == QDialog::Accepted)
    {
        ContactDescriptor tContact;

        Dialog2Contact(&tCED, &tContact, true);
        CONTACTS.AddContact(tContact);
        mTvContacts->setVisible(true);
        mContactListModel->UpdateView();
    }
}

void OverviewContactsWidget::EditSelected()
{
    QModelIndex tIndex = mTvContacts->currentIndex();
    ContactDescriptor* tContact = (ContactDescriptor*)tIndex.internalPointer();

    if (tContact == NULL)
    {
        LOG(LOG_VERBOSE, "Cannot edit non existing contact");
        return;
    }

    ContactEditDialog tCED;

    tCED.setWindowTitle(Homer::Gui::OverviewContactsWidget::tr("Edit contact"));
    Contact2Dialog(tContact, &tCED);
    tCED.mLeName->setFocus(Qt::TabFocusReason);

    if (tCED.exec() == QDialog::Accepted)
    {
        Dialog2Contact(&tCED, tContact, false);
        CONTACTS.SavePool();
        mContactListModel->UpdateView();
    }
}

void OverviewContactsWidget::DeleteSelected()
{
    ContactDescriptor* tContact;

    if (mTvContacts->selectionModel() == NULL)
        return;

    QModelIndexList tSelection = mTvContacts->selectionModel()->selectedRows();
    if (tSelection.size() == 1)
    {// selected one entry
        QModelIndex tIndex = mTvContacts->currentIndex();
        tContact = (ContactDescriptor*)tIndex.internalPointer();
        QString tContactDescription = (tContact->Name != "") ? tContact->Name : QString(MEETING.SipCreateId(tContact->User.toStdString(), tContact->Host.toStdString(), tContact->Port.toStdString()).c_str());

        QMessageBox tMB(QMessageBox::Question, Homer::Gui::OverviewContactsWidget::tr("Acknowledge deletion"), Homer::Gui::OverviewContactsWidget::tr("Do you want to delete \"") + tContactDescription + Homer::Gui::OverviewContactsWidget::tr("\" from the contact list?"), QMessageBox::Yes | QMessageBox::No);
        if (tMB.exec() != QMessageBox::Yes)
            return;

    }else if (tSelection.size() > 1)
    {// selected multiple entries
        QMessageBox tMB(QMessageBox::Question, Homer::Gui::OverviewContactsWidget::tr("Acknowledge deletion"), Homer::Gui::OverviewContactsWidget::tr("Do you want to delete ") + QString("%1").arg(tSelection.size()) + Homer::Gui::OverviewContactsWidget::tr(" entries from the contact list?"), QMessageBox::Yes | QMessageBox::No);
        if (tMB.exec() != QMessageBox::Yes)
            return;
    }

    for (int i = tSelection.size() -1; i >= 0; i--)
    {
        // get index
        QModelIndex tIndex = tSelection[i];

        // get direct access to contact entry
        ContactDescriptor* tContact = (ContactDescriptor*)tIndex.internalPointer();
        if (tContact == NULL)
        {
            LOG(LOG_VERBOSE, "Cannot delete non existing contact");
            return;
        }

        // delete this contact entry
        CONTACTS.RemoveContact(tContact->Id);
    }
}

void OverviewContactsWidget::ResetList()
{
    CONTACTS.ResetPool();
}

void OverviewContactsWidget::ContactSelected(bool pCall)
{
    QModelIndex tIndex = mTvContacts->currentIndex();
    ContactDescriptor* tContact = (ContactDescriptor*)tIndex.internalPointer();
    if (tContact == NULL)
    {
        LOG(LOG_VERBOSE, "Cannot contact non existing contact");
        return;
    }

    ContactParticipant(tContact, pCall);
}

void OverviewContactsWidget::InsertCopy(ContactDescriptor *pContact)
{
    if (pContact == NULL)
    {
        LOG(LOG_VERBOSE, "Cannot copy non existing contact");
        return;
    }

    ContactEditDialog tCED;

    tCED.setWindowTitle(Homer::Gui::OverviewContactsWidget::tr("Insert duplicated contact"));
    Contact2Dialog(pContact, &tCED);
    tCED.mLeName->setFocus(Qt::TabFocusReason);
    if (tCED.exec() == QDialog::Accepted)
    {
        ContactDescriptor tContact;

        Dialog2Contact(&tCED, &tContact, true);
        CONTACTS.AddContact(tContact);
    }
}

void OverviewContactsWidget::ContactParticipantDoubleClick(const QModelIndex &pIndex)
{
    if ((pIndex.isValid()) && (pIndex.internalPointer() != NULL))
        ContactParticipant((ContactDescriptor*)pIndex.internalPointer(), false);
}

void OverviewContactsWidget::SaveList()
{
    QString tContactsFile = QFileDialog::getSaveFileName(this,
                                                         Homer::Gui::OverviewContactsWidget::tr("Save contact list"),
                                                         CONF.GetContactFile(),
                                                         Homer::Gui::OverviewPlaylistWidget::tr("Contact list") + " (*.xml)",
                                                         NULL, CONF_NATIVE_DIALOGS);

    if (tContactsFile.isEmpty())
        return;

    if (!tContactsFile.endsWith(".xml"))
        tContactsFile += ".xml";

    CONTACTS.SavePool(tContactsFile.toStdString());
}

void OverviewContactsWidget::LoadList()
{
    QString tContactsFile = QFileDialog::getOpenFileName(this,
                                                         Homer::Gui::OverviewContactsWidget::tr("Load contact list"),
                                                         CONF.GetContactFile(),
                                                         Homer::Gui::OverviewPlaylistWidget::tr("Contact list") + " (*.xml)",
                                                         NULL, CONF_NATIVE_DIALOGS);

    if (tContactsFile.isEmpty())
        return;

    if (!tContactsFile.endsWith(".xml"))
        tContactsFile += ".xml";

    CONTACTS.LoadPool(tContactsFile.toStdString());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////  MODEL CLASS  /////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ContactListModel::ContactListModel(OverviewContactsWidget *pOverviewContactsWidget):
    QAbstractItemModel(pOverviewContactsWidget)
{
	mOverviewContactsWidget = pOverviewContactsWidget;
    CONTACTS.RegisterAtController(this);
}

ContactListModel::~ContactListModel()
{
}

///////////////////////////////////////////////////////////////////////////////
int ContactListModel::columnCount(const QModelIndex &pParent) const
{
    int tResult = 0;

    tResult = 3;

    //LOG(LOG_VERBOSE, "column count for %d:%d => %d", pParent.column(), pParent.row(), tResult);

    return tResult;
}

QVariant ContactListModel::data(const QModelIndex &pIndex, int pRole) const
{
    QVariant tResult;

    if (pIndex.isValid())
    {
        switch(pRole)
        {
            case Qt::DecorationRole:
                switch(pIndex.column())
                {
                    case 0:
                    	if (CONF.GetSipContactsProbing())
                    	{
                    		if (IsContactAvailable(pIndex))
                    		{
                    		    if(IsContactKnown(pIndex))
                    		    {
                    		        tResult = QPixmap(":/images/20_20/UserAvailable.png");//.scaled(20, 20, Qt::KeepAspectRatio, Qt::FastTransformation);
                    		    }else{
                                    tResult = QPixmap(":/images/22_22/Help.png");//.scaled(20, 20, Qt::KeepAspectRatio, Qt::FastTransformation);
                    		    }
                    		}else
                    		{
                                if(IsContactKnown(pIndex))
                                {
                                    tResult = QPixmap(":/images/20_20/UserUnavailable.png");//.scaled(20, 20, Qt::KeepAspectRatio, Qt::FastTransformation);
                                }
                    		}
                    	}else
                    	{
							tResult = QPixmap(":/images/22_22/User.png").scaled(20, 20, Qt::KeepAspectRatio, Qt::FastTransformation);
                    	}
                        break;
                    case 1:
                        //tResult = QPixmap(":/images/Users1.png").scaled(25, 25, Qt::KeepAspectRatio, Qt::FastTransformation);
                        break;
                    case 2:
                        break;
                    default:
                        break;
                }
                break;
            case Qt::DisplayRole:
                switch(pIndex.column())
                {
                    case 0:
                        break;
                    case 1:
                        tResult = GetContactName(pIndex);
                        break;
                    case 2:
                        tResult = GetContactSoftware(pIndex);
                        break;
                     default:
                        break;
                }
                break;
            case Qt::SizeHintRole:
                switch(pIndex.column())
                {
                    case 0:
                        tResult = QSize(25,25);
                        break;
                    case 1:
                        tResult = QSize(250,25);
                        break;
                    case 2:
                        tResult = QSize(150,25);
                        break;
                    default:
                        break;
                }
                break;
            case Qt::BackgroundColorRole:
                switch(pIndex.column())
                {
                    default:
                        break;
                }
                break;
            case Qt::TextColorRole:
                switch(pIndex.column())
                {
                    case 0:
                        break;
                    case 1:
                        tResult = QColor(0, 0, 0);
                        break;
                    case 2:
                        tResult = Qt::gray;
                        break;
                    default:
                        break;
                }
                break;
            case Qt::FontRole:
                switch(pIndex.column())
                {
                    case 0:
                        break;
                    case 1:
                        tResult = QFont("Arial", 9, QFont::Normal);
                        break;
                    case 2:
                        tResult = QFont("Arial", 8, QFont::Normal);
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

    //LOG(LOG_VERBOSE, "data for %d:%d => %s", pIndex.column(), pIndex.row(), tResult.toString().toStdString().c_str());

    return tResult;
}

QModelIndex ContactListModel::index(int pRow, int pColumn, const QModelIndex &pParent) const
{
    QModelIndex tResult;

    if (!pParent.isValid())
    {
        if (pRow < CONTACTS.ContactCount())
            tResult = createIndex(pRow, pColumn, GetContactPointer(pRow));
    }

    //LOG(LOG_VERBOSE, "index for %d:%d and parent %d:%d determined => %d:%d (%s)", pColumn, pRow, pParent.column(), pParent.row(), tResult.column(), tResult.row(), GetContactName(tResult).toStdString().c_str());

    return tResult;
}

QModelIndex ContactListModel::parent(const QModelIndex &pIndex) const
{
    QModelIndex tResult;

    //LOG(LOG_VERBOSE, "parent for %d:%d (%s) determined as %d:%d (%s)", pIndex.column(), pIndex.row(), pIndex.internalPointer() != NULL ? ((ContactDescriptor*)pIndex.internalPointer())->toString().c_str() : "null", tResult.column(), tResult.row(), tResult.internalPointer() != NULL ? ((ContactDescriptor*)tResult.internalPointer())->toString().c_str() : "null");

    return tResult;
}

int ContactListModel::rowCount(const QModelIndex &pParent) const
{
    int tResult = 0;

    if (!pParent.isValid())
        tResult =  CONTACTS.ContactCount();

    //LOG(LOG_VERBOSE, "row count for %d:%d => %d", pParent.column(), pParent.row(), tResult);

    return tResult;
}

Qt::ItemFlags ContactListModel::flags(const QModelIndex &pIndex) const
{
    if (!pIndex.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant ContactListModel::headerData(int pSection, Qt::Orientation pOrientation, int pRole) const
{
    QVariant tResult;

    if (pOrientation == Qt::Horizontal)
    {
        switch(pRole)
        {
            case Qt::DecorationRole:
                switch(pSection)
                {
                    case 0:
                        tResult = QPixmap(":/images/22_22/NetworkConnection.png");
                        break;
                    case 1:
                        tResult = QPixmap(":/images/22_22/User.png");
                        break;
                    case 2:
                        break;
                    default:
                        break;
                }
                break;
            case Qt::DisplayRole:
                switch(pSection)
                {
                    case 0:
                        return "";
                    case 1:
                        return Homer::Gui::OverviewContactsWidget::tr("Name");
                    case 2:
                        return Homer::Gui::OverviewContactsWidget::tr("Software");
                    default:
                        break;
                }
                break;
//            case Qt::SizeHintRole:
//                switch(pSection)
//                {
//                    case 0:
//                        tResult = QSize(25,25);
//                        break;
//                    case 1:
//                        tResult = QSize(250,25);
//                        break;
//                    case 2:
//                        tResult = QSize(150,25);
//                        break;
//                    default:
//                        break;
//                }
//                break;
            case Qt::FontRole:
                switch(pSection)
                {
                    case 0:
                        tResult = QFont("Sans", 8, QFont::Bold);
                        break;
                    case 1:
                        tResult = QFont("Sans", 8, QFont::Normal);
                        break;
                    case 2:
                        tResult = QFont("Sans", 8, QFont::Normal);
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

    return tResult;
}

void ContactListModel::UpdateView()
{
	QModelIndex tIndex = mOverviewContactsWidget->mTvContacts->currentIndex();

    reset();

    mOverviewContactsWidget->mTvContacts->setCurrentIndex(tIndex);
    mOverviewContactsWidget->mTvContacts->header()->setResizeMode(0, QHeaderView::Fixed);
    mOverviewContactsWidget->mTvContacts->header()->setResizeMode(1, QHeaderView::Stretch);
    mOverviewContactsWidget->mTvContacts->header()->setResizeMode(2, QHeaderView::Stretch);
}

void ContactListModel::sort (int pColumn, Qt::SortOrder pOrder)
{
    LOG(LOG_VERBOSE, "Sorting contact data base by column %d, order = %d", pColumn, pOrder);
    switch(pColumn)
    {
        case 0:
            CONTACTS.SortByState(pOrder);
            CONTACTS.SavePool();
            break;
        case 1:
            CONTACTS.SortByName(pOrder);
            CONTACTS.SavePool();
            break;
    }
}

QString OverviewContactsWidget::GetSoftwareStr(QString pSipSoftware)
{
    QString tResult = "";

    if(pSipSoftware.startsWith(USER_AGENT_SIGNATURE_PREFIX))
    {
        int tPos = pSipSoftware.indexOf("/");
        if(tPos != -1)
        {
            tResult = "Homer Conferencing " + pSipSoftware.right(pSipSoftware.size() - tPos -1);
        }else
        {
            tResult = "Homer Conferencing";
        }
    }else{
        tResult = pSipSoftware;
        tResult.replace("/", " ");
    }

    return tResult;
}

QString ContactListModel::GetContactSoftware(const QModelIndex &pIndex) const
{
    QString tResult = "";

    QString tSoftware = "";
    if (pIndex.internalPointer() != NULL)
        tSoftware = ((ContactDescriptor*)pIndex.internalPointer())->GetSoftwareName();

    tResult = CONTACTSWIDGET.GetSoftwareStr(tSoftware);

    return tResult;
}

QString ContactListModel::GetContactName(const QModelIndex &pIndex) const
{
    if (pIndex.internalPointer() != NULL)
        return ((ContactDescriptor*)pIndex.internalPointer())->GetContactName();
    else
        return "";
}

bool ContactListModel::IsContactAvailable(const QModelIndex &pIndex) const
{
    if (pIndex.internalPointer() != NULL)
        return ((ContactDescriptor*)pIndex.internalPointer())->IsOnline();
    else
        return false;
}

bool ContactListModel::IsContactKnown(const QModelIndex &pIndex) const
{
    if (pIndex.internalPointer() != NULL)
        return ((ContactDescriptor*)pIndex.internalPointer())->IsKnown();
    else
        return false;
}

void *ContactListModel::GetContactPointer(unsigned int pIndex) const
{
    return (void*)CONTACTS.GetContactByIndex(pIndex);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
