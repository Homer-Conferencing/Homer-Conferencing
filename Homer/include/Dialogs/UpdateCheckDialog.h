/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Dialog for check for updates
 * Since:   2010-12-19
 */

#ifndef _UPDATE_CHECK_DIALOG_
#define _UPDATE_CHECK_DIALOG_

#include <ui_UpdateCheckDialog.h>

#include <Snippets.h>

#include <QProgressDialog>
#include <QNetworkReply>
#include <QFile>
#include <QNetworkAccessManager>

#define TriggerVersionCheck(pHttpGetVersionServer, pAnswerHandleFunction)   {                                                                                         \
                                                                                QString tUrlServerVersion = QString("http://" RELEASE_SERVER PATH_VERSION_TXT);       \
                                                                                HttpDownload(pHttpGetVersionServer, tUrlServerVersion, GotAnswerForVersionRequest);   \
                                                                            }

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class UpdateCheckDialog :
    public QDialog,
    public Ui_UpdateCheckDialog
{
    Q_OBJECT;
public:
    /// The default constructor
    UpdateCheckDialog(QWidget* pParent = NULL);

    /// The destructor.
    virtual ~UpdateCheckDialog();

public slots:
    int exec();

private slots:
    void GotAnswerForVersionRequest(QNetworkReply *pReply);
    void GotAnswerForChangelogRequest(QNetworkReply *pReply);
    void DownloadStart();
    void DownloadInstallerStart();
    void DownloadStop();
    void DownloadFinished();
    void DownloadNewChunk();
    void DownloadProgress(qint64 pLoadedBytes, qint64 pTotalBytes);

private:
    void initializeGUI();
    void DownloadFireRequest(QString pTarget);
    QString GetNumericReleaseVersion(QString pServerVersion);

    QNetworkAccessManager   *mHttpGetVersionServer;
    QNetworkAccessManager   *mHttpGetChangelogUrl;
    QString                 mServerVersion;
    bool			        mDownloadStarted;
    QProgressDialog         *mDownloadProgressDialog;
    QNetworkAccessManager   *mHttpUpdateDownloader;
    QFile			        *mDownloadHomerUpdateFile;
    QNetworkReply           *mDownloadReply;
    QString 		        mServerFile;
    bool                    mDownloadAborted;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
