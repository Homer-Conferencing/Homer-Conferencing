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
 * Since:   2008-12-02
 */

#include <Widgets/MessageWidget.h>
#include <Widgets/MessageHistory.h>
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
using namespace Homer::Base;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

MessageWidget::MessageWidget(QWidget* pParent):
    QWidget(pParent)
{
    mAssignedAction = NULL;

    hide();
}

void MessageWidget::Init(QMenu *pMenu, QString pParticipant, enum TransportType pParticipantTransport, bool pVisible)
{
    mParticipant = pParticipant;
    mParticipantTransport = pParticipantTransport;

    initializeGUI();

    mPbCall->SetPartner(mParticipant, mParticipantTransport);

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
    if (mAssignedAction != NULL)
        connect(mAssignedAction, SIGNAL(triggered()), this, SLOT(ToggleVisibility()));

    //####################################################################
    //### update GUI
    //####################################################################

    //TODO: remove the following if the feature is complete
    #ifdef RELEASE_VERSION
        mPbFile->setEnabled(false);
    #endif

    setWindowTitle(mParticipant);
    connect(mTeMessage, SIGNAL(SendTrigger()), this, SLOT(SendMessage()));
    connect(mPbFile, SIGNAL(released()), this, SLOT(SendFile()));
    connect(mPbAdd, SIGNAL(released()), this, SLOT(AddPArticipantToContacts()));
    if(IsKnownContact())
    {
        mPbAdd->hide();
    }else
    {
        mPbAdd->show();
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
        mPbAdd->hide();
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

void MessageWidget::InitializeMenuMessagesSettings(QMenu *pMenu)
{
    QAction *tAction;

    pMenu->clear();

    tAction = pMenu->addAction(QPixmap(":/images/22_22/Save.png"), Homer::Gui::MessageWidget::tr("Save history"));
    if (isVisible())
        tAction = pMenu->addAction(QPixmap(":/images/22_22/Close.png"), Homer::Gui::MessageWidget::tr("Close window"));
    else
        tAction = pMenu->addAction(QPixmap(":/images/22_22/Screencasting.png"), Homer::Gui::MessageWidget::tr("Show window"));
    pMenu->addSeparator();

    if ((!IsKnownContact()) && (mParticipant != BROACAST_IDENTIFIER))
        tAction = pMenu->addAction(QPixmap(":/images/22_22/Plus.png"), Homer::Gui::MessageWidget::tr("Add contact"));
}

void MessageWidget::SelectedMenuMessagesSettings(QAction *pAction)
{
    if (pAction != NULL)
    {
        if (pAction->text().compare(Homer::Gui::MessageWidget::tr("Show window")) == 0)
        {
            ToggleVisibility();
            return;
        }
        if (pAction->text().compare(Homer::Gui::MessageWidget::tr("Close window")) == 0)
        {
            ToggleVisibility();
            return;
        }
        if (pAction->text().compare(Homer::Gui::MessageWidget::tr("Save history")) == 0)
        {
            mTbMessageHistory->Save();
            return;
        }
        if (pAction->text().compare(Homer::Gui::MessageWidget::tr("Add to contacts")) == 0)
        {
            LOG(LOG_VERBOSE, "Adding this participant to contacts");
            AddPArticipantToContacts();
            return;
        }
    }
}

void MessageWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QMenu tMenu(this);

    InitializeMenuMessagesSettings(&tMenu);

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
        SelectedMenuMessagesSettings(tPopupRes);
}

void MessageWidget::AddPArticipantToContacts()
{
	LOG(LOG_VERBOSE, "User wants to add user %s to his contact list", mParticipant.toStdString().c_str());
    if (mParticipant != BROACAST_IDENTIFIER)
        if (CONTACTSWIDGET.InsertNew(mParticipant))
            mPbAdd->hide();
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

QString MessageWidget::ReplaceSmilesAndUrls(QString pMessage)
{
    QString tResult = "";

    // filter and replace URLs
    QString tOutputMessage = "";
    int tStartPos = 0;
    int tEndPos = -1;

    while (tStartPos < pMessage.size())
    {
        tEndPos = pMessage.indexOf(' ', tStartPos);
        if (tEndPos == -1)
        {
            if (tEndPos < pMessage.size() -1)
                tEndPos = pMessage.size();
            else
                break;
        }

        QString tWord = pMessage.mid(tStartPos, tEndPos - tStartPos);
        LOG(LOG_VERBOSE, "Message token: \"%s\"", tWord.toStdString().c_str());

        //#########################
        //### Filter SMILES
//        if ((tWord == ":)") || (tWord == ":-)"))
//        {// laughing smile
//            LOG(LOG_VERBOSE, "Found smile");
//            tOutputMessage.append("<img src=\"" URL_SMILE "\" width=\"22\" height=\"22\" title=\":-)\">");
//        }else
		//#########################
		//### Filter HTTP URLS
        if ((tWord.startsWith("http://")) && (tWord.size() > 7))
        {
            LOG(LOG_VERBOSE, "Found http reference: %s", tWord.toStdString().c_str());
            tOutputMessage.append("<a href=\"" + tWord + "\" title=\"go to the web site " + tWord + " \">" + tWord + "</a>");
        }else
        //#########################
        //### Filter HTTPS URLS
        if ((tWord.startsWith("https://")) && (tWord.size() > 8))
        {
            LOG(LOG_VERBOSE, "Found https reference: %s", tWord.toStdString().c_str());
            tOutputMessage.append("<a href=\"" + tWord + "\" title=\"go to the web site " + tWord + " \">" + tWord + "</a>");
        }else
		//#########################
		//### Filter FTP URLS
		if ((tWord.startsWith("ftp://")) && (tWord.size() > 6))
		{
            LOG(LOG_VERBOSE, "Found ftp reference: %s", tWord.toStdString().c_str());
            tOutputMessage.append("<a href=\"" + tWord + "\" title=\"go to the ftp server " + tWord + " \">" + tWord + "</a>");
		}else
		//#########################
		//### Filter MAILTO URLS
		if ((tWord.startsWith("mailto://")) && (tWord.size() > 9))
		{
            LOG(LOG_VERBOSE, "Found mailto reference: %s", tWord.toStdString().c_str());
            tOutputMessage.append("<a href=\"" + tWord + "\" title=\"write a mail to " + tWord.right(tWord.length() - 9) + " \">" + tWord + "</a>");
		}else
            tOutputMessage.append(tWord);

        if (tEndPos < pMessage.size() -1)
            tOutputMessage.append(' ');
        tStartPos = tEndPos + 1;
    }

    // set the new history
    if (tOutputMessage.size() > 0)
        tResult = tOutputMessage;

    return tResult;
}

void MessageWidget::AddMessage(QString pSender, QString pMessage, bool pLocalMessage)
{
    // replace ENTER with corresponding html tag
    // hint: necessary because this QTextEdit is in html-mode and caused by this it ignores "\n"
    pMessage.replace(QString("\n"), QString("<br>"));

    pMessage = ReplaceSmilesAndUrls(pMessage);

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

void MessageWidget::SendMessage()
{
	// is message size > 0 ?
    if (mTeMessage->toPlainText().size() == 0)
        return;

    if (MEETING.SendMessage(QString(mParticipant.toLocal8Bit()).toStdString(), mParticipantTransport, mTeMessage->toPlainText().toStdString()))
    {
        AddMessage(QString(MEETING.GetLocalUserName().c_str()), mTeMessage->toPlainText(), true);
        mTeMessage->Clear();
    }else
        ShowError(Homer::Gui::MessageWidget::tr("Error occurred"), Homer::Gui::MessageWidget::tr("Message could not be sent!"));
}

void MessageWidget::SendFile(QList<QUrl> *tFileUrls)
{
    QStringList tSelectedFiles;

    if ((tFileUrls == NULL) || (tFileUrls->size() == 0))
    {
        tSelectedFiles = QFileDialog::getOpenFileNames(this, Homer::Gui::MessageWidget::tr("Select files for transfer to") + " " + mParticipant,
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

void MessageWidget::ShowNewState()
{
    mPbCall->ShowNewState();
}

void MessageWidget::UpdateParticipantName(QString pParticipantName)
{
    LOG(LOG_VERBOSE, "New participant name is %s", pParticipantName.toStdString().c_str());
    mLbParticipant->setText("   " + pParticipantName + "   ");
    ShowNewState();
}

void MessageWidget::UpdateParticipantState(int pState)
{
    switch(pState)
    {
        case CONTACT_UNDEFINED_STATE:
            mLbPartitipantState->setEnabled(false);
            mLbPartitipantState->setToolTip(Homer::Gui::MessageWidget::tr("undefined state"));
            break;
        case CONTACT_UNAVAILABLE:
            mLbPartitipantState->setEnabled(true);
            mLbPartitipantState->setPixmap(QPixmap(":/images/22_22/Error.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::FastTransformation));
            mLbPartitipantState->setToolTip(Homer::Gui::MessageWidget::tr("contact unavailable"));
            break;
        case CONTACT_AVAILABLE:
            mLbPartitipantState->setEnabled(true);
            mLbPartitipantState->setPixmap(QPixmap(":/images/32_32/UserAvailable.png"));
            mLbPartitipantState->setToolTip(Homer::Gui::MessageWidget::tr("contact available"));
            break;
        default:
            LOG(LOG_ERROR, "Unknown state given");
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
