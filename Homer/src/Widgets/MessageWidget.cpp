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
 * Purpose: Implementation of a widget for a message dialoge
 * Author:  Thomas Volkert
 * Since:   2008-12-02
 */

#include <Widgets/MessageWidget.h>
#include <Widgets/OverviewContactsWidget.h>
#include <Dialogs/FileTransferAckDialog.h>
#include <Configuration.h>
#include <Meeting.h>
#include <Logger.h>
#include <ContactsManager.h>
#include <Snippets.h>

#include <QWidget>
#include <QMenu>
#include <QTime>
#include <QFileDialog>
#include <QInputDialog>
#include <QScrollBar>
#include <QMessageBox>
#include <QContextMenuEvent>

using namespace Homer::Conference;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

MessageWidget::MessageWidget(QWidget* pParent):
    QWidget(pParent)
{
    mAssignedAction = NULL;

    hide();
}

void MessageWidget::Init(QMenu *pMenu, QString pParticipant, bool pVisible)
{
    mParticipant = pParticipant;

    initializeGUI();

    mPbCall->SetPartner(mParticipant);

    //####################################################################
    //### create the remaining necessary menu item and short cuts
    //####################################################################

    if (pMenu != NULL)
    {
        mAssignedAction = pMenu->addAction(pParticipant);
        mAssignedAction->setCheckable(true);
        mAssignedAction->setChecked(pVisible);
        QIcon tIcon;
        tIcon.addPixmap(QPixmap(":/images/22_22/Checked.png"), QIcon::Normal, QIcon::On);
        tIcon.addPixmap(QPixmap(":/images/22_22/Unchecked.png"), QIcon::Normal, QIcon::Off);
        mAssignedAction->setIcon(tIcon);
    }

    //####################################################################
    //### update GUI
    //####################################################################

    //TODO: remove the following if the feature is complete
    #ifdef RELEASE_VERSION
        mPbFile->setEnabled(false);
    #endif

    setWindowTitle(mParticipant);
    if (mAssignedAction != NULL)
        connect(mAssignedAction, SIGNAL(triggered()), this, SLOT(ToggleVisibility()));
    connect(mTeMessage, SIGNAL(SendTrigger()), this, SLOT(SendText()));
    connect(mPbFile, SIGNAL(released()), this, SLOT(SendFile()));
    connect(mTbAdd, SIGNAL(released()), this, SLOT(AddPArticipantToContacts()));
    if(IsKnownContact())
    {
        mTbAdd->hide();
    }else
    {
        mTbAdd->show();
    }
    if (pVisible)
    {
        show();
    }else
    {
        hide();
    }
    UpdateParticipantState(CONTACT_UNDEFINED_STATE);

    //### set focus setFocus
    mTeMessage->setFocus(Qt::TabFocusReason);

    // are we part of broadcast window?
    if (mParticipant == BROACAST_IDENTIFIER)
    {
        mTbAdd->hide();
        mPbCall->setEnabled(false);
    }
}

MessageWidget::~MessageWidget()
{
    // are we part of broadcast window?
    if (mParticipant == BROACAST_IDENTIFIER)
    {
        CONF.SetVisibilityBroadcastMessageWidget(isVisible());
    }

    if (mAssignedAction != NULL)
        delete mAssignedAction;
}

///////////////////////////////////////////////////////////////////////////////

bool MessageWidget::IsKnownContact()
{
    QString tUser, tHost, tPort;
    CONTACTS.SplitAddress(mParticipant, tUser, tHost, tPort);
    return CONTACTS.IsKnownContact(tUser, tHost, tPort);
}

void MessageWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction("Save history");
    QIcon tIcon2;
    tIcon2.addPixmap(QPixmap(":/images/22_22/Save.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon2);

    tAction = tMenu.addAction("Close messages");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/22_22/Close.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    tMenu.addSeparator();

    if ((!IsKnownContact()) && (mParticipant != BROACAST_IDENTIFIER))
    {
        tAction = tMenu.addAction("Add to contacts");
        QIcon tIcon3;
        tIcon3.addPixmap(QPixmap(":/images/22_22/Plus.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon3);
    }

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Close messages") == 0)
        {
            ToggleVisibility();
            return;
        }
        if (tPopupRes->text().compare("Save history") == 0)
        {
            mTbMessageHistory->Save();
            return;
        }
        if (tPopupRes->text().compare("Add to contacts") == 0)
        {
        	LOG(LOG_VERBOSE, "Adding this participant to contacts");
        	AddPArticipantToContacts();
            return;
        }
    }
}

void MessageWidget::AddPArticipantToContacts()
{
	LOG(LOG_VERBOSE, "User wants to add user %s to his contact list", mParticipant.toStdString().c_str());
    if (mParticipant != BROACAST_IDENTIFIER)
        if (CONTACTSWIDGET.InsertNew(mParticipant))
            mTbAdd->hide();
}

void MessageWidget::closeEvent(QCloseEvent *pEvent)
{
    ToggleVisibility();
}

void MessageWidget::dragEnterEvent(QDragEnterEvent *pEvent)
{
    if ((pEvent->mimeData()->hasText()) && (!pEvent->mimeData()->hasUrls()))
    {
        pEvent->acceptProposedAction();
        LOG(LOG_VERBOSE, "New drag+drop text %s", pEvent->mimeData()->text().toStdString().c_str());
        return;
    }
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

void MessageWidget::dropEvent(QDropEvent *pEvent)
{
    if ((pEvent->mimeData()->hasText()) && (!pEvent->mimeData()->hasUrls()))
    {
        LOG(LOG_VERBOSE, "New dropped text %s", pEvent->mimeData()->text().toStdString().c_str());
        mTeMessage->setPlainText(mTeMessage->toPlainText() + pEvent->mimeData()->text());
        pEvent->acceptProposedAction();
        return;
    }

    if (pEvent->mimeData()->hasUrls())
    {
        LOG(LOG_VERBOSE, "Got some dropped urls");
        QList<QUrl> tList = pEvent->mimeData()->urls();
        SendFile(&tList);
        pEvent->acceptProposedAction();
        return;
    }
}

void MessageWidget::ToggleVisibility()
{
    if (isVisible())
        SetVisible(false);
    else
        SetVisible(true);
}

void MessageWidget::SetVisible(bool pVisible)
{
    if (pVisible)
    {
        move(mWinPos);
        show();
        if (mAssignedAction != NULL)
            mAssignedAction->setChecked(true);

    }else
    {
        mWinPos = pos();
        hide();
        if (mAssignedAction != NULL)
            mAssignedAction->setChecked(false);
    }
}

void MessageWidget::initializeGUI()
{
    setupUi(this);

    mTbMessageHistory->setTextColor(QColor(100, 100, 100));
    QFont tFont = mTbMessageHistory->font();
    tFont.setBold(false);
    tFont.setItalic(false);
    //tFont.setPointSize(tFont.pointSize() - 1);
    mTbMessageHistory->setFont(tFont);
}

void MessageWidget::AddMessage(QString pSender, QString pMessage, bool pLocalMessage)
{
    // replace ENTER with corresponding html tag
    // hint: necessary because this QTextEdit is in html-mode and caused by this it ignores "\n"
    pMessage.replace(QString("\n"), QString("<br>"));

    if (pSender != "")
    {
        if (pLocalMessage)
            mMessageHistory += "<font color=blue><b>" + QTime::currentTime().toString("hh:mm:ss") + "</b></font> <font color=black><b>" + pSender + "</b>:</font> " + pMessage + "<br>";
        else
            mMessageHistory += "<font color=blue><b>" + QTime::currentTime().toString("hh:mm:ss") + "</b></font> <font color=red><b>" + pSender + "</b>:</font> " + pMessage + "<br>";
    }else
    {
        if (pLocalMessage)
            mMessageHistory += "<font color=blue><b>" + QTime::currentTime().toString("hh:mm:ss") + "</b></font> <font color=gray>" + pMessage + "</font><br>";
        else
            mMessageHistory += "<font color=blue><b>" + QTime::currentTime().toString("hh:mm:ss") + "</b></font> <font color=red><b>" + mParticipant + "</b>:</font> " + pMessage + "<br>";
    }

    // set the new history
    mTbMessageHistory->Update(mMessageHistory);
}

void MessageWidget::SendText()
{
	// is message size > 0 ?
    if (mTeMessage->toPlainText().size() == 0)
        return;

    if (MEETING.SendMessage(QString(mParticipant.toLocal8Bit()).toStdString(), mTeMessage->toPlainText().toStdString()))
    {
        AddMessage(QString(MEETING.GetLocalUserName().c_str()), mTeMessage->toPlainText(), true);
        mTeMessage->setPlainText("");
        mTeMessage->setFocus(Qt::TabFocusReason);
    }else
        ShowError("Error occurred", "Message could not be sent!");
}

void MessageWidget::SendFile(QList<QUrl> *tFileUrls)
{
    QStringList tSelectedFiles;

    if ((tFileUrls == NULL) || (tFileUrls->size() == 0))
    {
        tSelectedFiles = QFileDialog::getOpenFileNames(this,       "Select files for transfer to " + mParticipant,
                                                                   CONF.GetDataDirectory(),
                                                                   "All files (*)",
                                                                   NULL,
                                                                   CONF_NATIVE_DIALOGS);

        if (tSelectedFiles.isEmpty())
            return;

        QString tFirstFileName = *tSelectedFiles.constBegin();
        CONF.SetDataDirectory(tFirstFileName.left(tFirstFileName.lastIndexOf('/')));
    }else
    {
        QUrl tUrl;
        foreach(tUrl, *tFileUrls)
            tSelectedFiles.push_back(QString(tUrl.toLocalFile().toLocal8Bit()));

        FileTransferAckDialog tDialog(this, mParticipant, tSelectedFiles);
        if (tDialog.exec() != QDialog::Accepted)
            return;
    }

    QString tFile;
    foreach (tFile, tSelectedFiles)
        printf("TODO: send file %s\n", tFile.toStdString().c_str());
}

//TODO: use better widget which automatically interprets links
void MessageWidget::SendLink()
{
    bool tAck = false;
    QString tLink = QInputDialog::getText(this, "Send web link to " + mParticipant, "Web address:                                                                             ", QLineEdit::Normal, "http://", &tAck);

    if ((!tAck) || (tLink == ""))
        return;

    tLink = "<a href=" + tLink + ">" + tLink + "</a>";

    if (MEETING.SendMessage(QString(mParticipant.toLocal8Bit()).toStdString(), tLink.toStdString()))
        AddMessage(QString(MEETING.GetLocalUserName().c_str()), tLink, true);
    else
        ShowError("Error occurred", "Message could not be sent!");
}

void MessageWidget::ShowNewState()
{
    mPbCall->ShowNewState();
}

void MessageWidget::UpdateParticipantName(QString pParticipantName)
{
    LOG(LOG_VERBOSE, "New participant name is %s", pParticipantName.toStdString().c_str());
    mLbParticipant->setText("  \"" + pParticipantName + "\"  ");
    ShowNewState();
}

void MessageWidget::UpdateParticipantState(int pState)
{
    switch(pState)
    {
        case CONTACT_UNDEFINED_STATE:
            mLbPartitipantState->setEnabled(false);
            mLbPartitipantState->setToolTip("undefined contact state");
            break;
        case CONTACT_UNAVAILABLE:
            mLbPartitipantState->setEnabled(true);
            mLbPartitipantState->setPixmap(QPixmap(":/images/22_22/Error.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::FastTransformation));
            mLbPartitipantState->setToolTip("contact is unavailable");
            break;
        case CONTACT_AVAILABLE:
            mLbPartitipantState->setEnabled(true);
            mLbPartitipantState->setPixmap(QPixmap(":/images/32_32/UserAvailable.png"));
            mLbPartitipantState->setToolTip("contact is available");
            break;
        default:
            LOG(LOG_ERROR, "Unknown state given");
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
