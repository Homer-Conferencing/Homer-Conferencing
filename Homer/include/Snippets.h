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
 * Purpose: Small function snippets
 * Since:   2011-09-25
 */

#ifndef _SNIPPETS_
#define _SNIPPETS_

#include <QMessageBox>
#include <QObject>
#include <Logger.h>
#include <string.h>

#include <HBReflection.h>

namespace Homer { namespace Gui {

using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

#define HttpDownload(pNetworkManager, pUrl, pReplyHandler) {                                                                                                                            \
                                                                LOG_REMOTE(LOG_VERBOSE, std::string(GetObjectNameStr(this).c_str()), __LINE__, "HTTP download of: %s", pUrl.toStdString().c_str());  \
                                                                connect(pNetworkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(pReplyHandler(QNetworkReply*)));                  \
                                                                QNetworkRequest tHttpRequest = QNetworkRequest(QUrl(pUrl));                                                             \
                                                                tHttpRequest.setRawHeader("User-Agent", "Homer Conferencing/"HOMER_VERSION);                                            \
                                                                pNetworkManager->get(tHttpRequest);                                                                                     \
                                                            }

#define ShowMessage(pTitle, pMessage) DoShowMessage(GetObjectNameStr(this).c_str(), __LINE__, this, pTitle, pMessage)
inline void DoShowMessage(std::string pSource, int pLine, QWidget *pParent, QString pTitle, QString pMessage)
{
    LOG_REMOTE(LOG_INFO, pSource, pLine, pMessage.toStdString().c_str());
    QMessageBox *tMb = new QMessageBox(QMessageBox::NoIcon, pTitle, pMessage, QMessageBox::Close, pParent);
    tMb->setStyleSheet("");
    tMb->exec();
    delete tMb;
}

#define ShowInfo(pTitle, pMessage) DoShowInfo(GetObjectNameStr(this).c_str(), __LINE__, this, pTitle, pMessage)
inline void DoShowInfo(std::string pSource, int pLine, QWidget *pParent, QString pTitle, QString pMessage)
{
    LOG_REMOTE(LOG_INFO, pSource, pLine, pMessage.toStdString().c_str());
    QMessageBox *tMb = new QMessageBox(QMessageBox::Information, pTitle, pMessage, QMessageBox::Close, pParent);
    tMb->setStyleSheet("");
    tMb->exec();
    delete tMb;
}

#define ShowWarning(pTitle, pMessage) DoShowWarning(GetObjectNameStr(this).c_str(), __LINE__, this, pTitle, pMessage)
inline void DoShowWarning(std::string pSource, int pLine, QWidget *pParent, QString pTitle, QString pMessage)
{
    LOG_REMOTE(LOG_WARN, pSource, pLine, pMessage.toStdString().c_str());
    QMessageBox *tMb = new QMessageBox(QMessageBox::Warning, pTitle, pMessage, QMessageBox::Close, pParent);
    tMb->setStyleSheet("");
    tMb->exec();
    delete tMb;
}

#define ShowError(pTitle, pMessage) DoShowError(GetObjectNameStr(this).c_str(), __LINE__, this, pTitle, pMessage)
inline void DoShowError(std::string pSource, int pLine, QWidget *pParent, QString pTitle, QString pMessage)
{
    LOG_REMOTE(LOG_ERROR, pSource, pLine, pMessage.toStdString().c_str());
    QMessageBox *tMb = new QMessageBox(QMessageBox::Critical, pTitle, pMessage, QMessageBox::Close, pParent);
    tMb->setStyleSheet("");
    tMb->exec();
    delete tMb;
}

inline QString Int2ByteExpression(int64_t pSize)
{
    QString tResult = "";

    do{
        int64_t tRest = pSize % 1000;
        pSize /= 1000;
        if (pSize)
        {
            tResult = "." + QString("%1").arg(tRest, 3, 10, (QLatin1Char)'0') + tResult;
        }else
            tResult = QString("%1").arg(tRest) + tResult;
    }while(pSize);

    return tResult;
}
///////////////////////////////////////////////////////////////////////////////

}}

#endif
