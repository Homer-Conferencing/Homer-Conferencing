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
 * Purpose: Implementation of a dialog for check for updates
 * Author:  Thomas Volkert
 * Since:   2010-12-19
 */

#include <Dialogs/UpdateCheckDialog.h>
#include <Configuration.h>
#include <Logger.h>
#include <Snippets.h>

#include <QProcess>
#include <QHttp>
#include <QDesktopServices>
#include <QProgressDialog>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QFile>
#include <QString>
#include <QFileDialog>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

UpdateCheckDialog::UpdateCheckDialog(QWidget* pParent) :
    QDialog(pParent)
{
	mDownloadProgressDialog = NULL;
    initializeGUI();
    mTbDownloadUpdate->hide();
    mLbVersion->setText(RELEASE_VERSION_STRING);
    mCbAutoUpdateCheck->setChecked(CONF.GetAutoUpdateCheck());
    mNetworkAccessManager = new QNetworkAccessManager(this);
}

UpdateCheckDialog::~UpdateCheckDialog()
{
	delete mNetworkAccessManager;
}

///////////////////////////////////////////////////////////////////////////////

void UpdateCheckDialog::initializeGUI()
{
    setupUi(this);

    mHttpGetVersionServer = new QHttp(this);
    TriggerVersionCheck(mHttpGetVersionServer, GotAnswerForVersionRequest);

    mHttpGetChangelogUrl = new QHttp(this);
    connect(mHttpGetChangelogUrl, SIGNAL(done(bool)), this, SLOT(GotAnswerForChangelogRequest(bool)));
    connect(mTbDownloadUpdate, SIGNAL(clicked()), this, SLOT(DownloadStart()));
    mHttpGetChangelogUrl->setHost(RELEASE_SERVER);
    mHttpGetChangelogUrl->get(PATH_CHANGELOG_TXT);
}

QString UpdateCheckDialog::GetNumericReleaseVersion(QString pServerVersion)
{
	QString tResult = "";
	int tPos = 0;
	while ((tPos < pServerVersion.size()) && (((pServerVersion[tPos] < '0') || (pServerVersion[tPos] > '9')) && (pServerVersion[tPos] != '.')))
		tPos++;

	tResult = pServerVersion.right(pServerVersion.size() - tPos);

	LOG(LOG_VERBOSE, "Determined numeric release version on server as %s", tResult.toStdString().c_str());

	return tResult;
}

void UpdateCheckDialog::DownloadStart()
{
	if(QFile::exists(PATH_INSTALL_EXE))
	{
		if(QProcess::startDetached(PATH_INSTALL_EXE))
			return;
	}

	if(mDownloadProgressDialog != NULL)
	{
		ShowInfo("Download of Homer archive running", "A download of a current Homer archive is already started!");
		return;
	}
	// find correct release name for this target system
	QString tReleaseFileName;
	QString tReleaseFileType;

	#ifdef WIN32
		tReleaseFileName = "Homer-Windows.zip";
		tReleaseFileType = "ZIP archive file (*.zip)";
	#endif
	#ifdef LINUX
		tReleaseFileName = "Homer-Linux" + QString(ARCH_BITS) + ".tar.bz2";
		tReleaseFileType = "bz2 archive file (*.bz2)";
	#endif
	#ifdef BSD
		tReleaseFileName = "Homer-BSD" + QString(ARCH_BITS) + ".tar.bz2";
		tReleaseFileType = "bz2 archive file (*.bz2)";
	#endif
	#ifdef APPLE
		tReleaseFileName = "Homer-OSX" + QString(ARCH_BITS) + ".tar.bz2";
		tReleaseFileType = "bz2 archive file (*.bz2)";
	#endif

	// ask for target location for new release archive
	QString tFileName;
	tFileName = QFileDialog::getSaveFileName(this,  "Save Homer archive to..",
																CONF.GetDataDirectory() + "/" + tReleaseFileName,
																tReleaseFileType,
																&tReleaseFileType,
																QFileDialog::DontUseNativeDialog);

	if (tFileName.isEmpty())
		return;

	CONF.SetDataDirectory(tFileName.left(tFileName.lastIndexOf('/')));

	// open the target file
	mDownloadHomerArchiveFile = new QFile(tFileName, this);
	if(!mDownloadHomerArchiveFile->open(QIODevice::WriteOnly))
	{
		ShowError("Could not store Homer archive", "Unable to store the downloaded Homer archive to \"" + tFileName + "\"");
		return;
	}

	// create progress dialogue
	mDownloadProgressDialog = new QProgressDialog(this);
	mDownloadProgressDialog->setWindowTitle("Download progress");
	mDownloadProgressDialog->setLabelText("<b>Downloading Homer archive</b>");
	connect(mDownloadProgressDialog, SIGNAL(canceled()), this, SLOT(DownloadStop()));

	LOG(LOG_VERBOSE, "Download progress dialog created");

	//start HTTP get for the Homer archive
	DownloadFireRequest(PATH_HOMER_RELEASES + GetNumericReleaseVersion(mServerVersion) + "/" + tReleaseFileName);
}

void UpdateCheckDialog::DownloadFireRequest(QString pTarget)
{
    LOG(LOG_VERBOSE, "Triggered download of file %s", pTarget.toStdString().c_str());
	mServerFile = pTarget;
	mDownloadReply = mNetworkAccessManager->get(QNetworkRequest(QUrl(pTarget)));
    connect(mDownloadReply, SIGNAL(finished()), this, SLOT(DownloadFinished()));
    connect(mDownloadReply, SIGNAL(readyRead()), this, SLOT(DownloadNewChunk()));
    connect(mDownloadReply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(DownloadProgress(qint64, qint64)));
    mDownloadAborted = false;
}

void UpdateCheckDialog::DownloadStop()
{
	LOG(LOG_VERBOSE, "Download stopped");
	mDownloadAborted = true;
    mDownloadReply->abort();
	if(mDownloadHomerArchiveFile != NULL)
	{
		mDownloadHomerArchiveFile->close();
		delete mDownloadHomerArchiveFile;
		mDownloadHomerArchiveFile = NULL;
	}
	if(mDownloadProgressDialog != NULL)
	{
		delete mDownloadProgressDialog;
		mDownloadProgressDialog = NULL;
	}
	mDownloadReply->deleteLater();
}

void UpdateCheckDialog::DownloadProgress(qint64 pLoadedBytes, qint64 pTotalBytes)
{
	mDownloadProgressDialog->setMaximum((int)pTotalBytes);
	mDownloadProgressDialog->setValue((int)pLoadedBytes);
	mDownloadProgressDialog->setLabelText("<b>Downloading Homer archive</b><br>  from <font color=blue>" + mServerFile + "</font><br>  to <i>" + mDownloadHomerArchiveFile->fileName() + "</i><br>  <b>Loaded: " + Int2ByteExpression(pLoadedBytes) + "/" +  Int2ByteExpression(pTotalBytes) + " bytes</b>");
    mDownloadProgressDialog->show();
}

void UpdateCheckDialog::DownloadFinished()
{
	LOG(LOG_VERBOSE, "Download finished");
	if((!mDownloadAborted) && (mDownloadReply->error()))
	{
		ShowError("Failed to download Homer archive", "Unable to download Homer archive because " + mDownloadReply->errorString());
	}

	mDownloadHomerArchiveFile->flush();
	mDownloadHomerArchiveFile->close();

	// were we redirected to another target?
	QVariant tRedirectionTarget = mDownloadReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (!tRedirectionTarget.isNull())
    {
    	LOG(LOG_VERBOSE, "Have been redirected to new Homer archive location under %s", tRedirectionTarget.toString().toStdString().c_str());
		mDownloadReply->deleteLater();
		mDownloadHomerArchiveFile->open(QIODevice::WriteOnly);
		mDownloadHomerArchiveFile->resize(0);

		// start HTTP get for the Homer archive
		DownloadFireRequest(tRedirectionTarget.toString());
    }else
    {// everything was okay, we automatically open downloaded file and we delete objects
		if(!mDownloadAborted)
		    QDesktopServices::openUrl(mDownloadHomerArchiveFile->fileName());

		if(mDownloadHomerArchiveFile != NULL)
		{
			mDownloadHomerArchiveFile->close();
			delete mDownloadHomerArchiveFile;
			mDownloadHomerArchiveFile = NULL;
		}
		if(mDownloadProgressDialog != NULL)
		{
			delete mDownloadProgressDialog;
			mDownloadProgressDialog = NULL;
		}
		mDownloadReply->deleteLater();
    }
}

void UpdateCheckDialog::DownloadNewChunk()
{
    if (mDownloadHomerArchiveFile)
    	mDownloadHomerArchiveFile->write(mDownloadReply->readAll());
}

void UpdateCheckDialog::GotAnswerForVersionRequest(bool pError)
{
    if (pError)
    {
        mLbVersionServer->setText("<font bgcolor='yellow' color='red'><b>check failed</b></font>");
        ShowError("Communication with server failed", "Could not determine software version which is provided by project server");
    }else
    {
        mServerVersion = QString(mHttpGetVersionServer->readAll().constData());
        LOG(LOG_VERBOSE, "Got version on server: %s", mServerVersion.toStdString().c_str());
        if (mServerVersion.contains("404 Not Found"))
        {
            mLbVersionServer->setText("<font bgcolor='yellow' color='red'><b>check failed</b></font>");
            ShowError("Version data not found on server", "Could not determine software version which is provided by project server");
        }else
        {
            if (mServerVersion != RELEASE_VERSION_STRING)
            {
                mLbVersionServer->setText("<font color='red'><b>" + mServerVersion + "</b></font>");
                mTbDownloadUpdate->show();
            }else
                mLbVersionServer->setText("<font color='green'><b>" + mServerVersion + "</b></font>");
        }
    }
}

void UpdateCheckDialog::GotAnswerForChangelogRequest(bool pError)
{
    if (pError)
    {
        mLbWaiting->setText("<font bgcolor='yellow' color='red'><b>fetch failed</b></font>");
        ShowError("Communication with server failed", "Could not determine changelog file which is provided by project server");
    }else
    {
        QString tChangelog = QString(mHttpGetChangelogUrl->readAll().constData());
        LOG(LOG_VERBOSE, "Loading changelog from http://"RELEASE_SERVER"%s", tChangelog.toStdString().c_str());

        if (tChangelog.contains("404 Not Found"))
        {
            mLbWaiting->setText("<font bgcolor='yellow' color='red'><b>fetch failed</b></font>");
            ShowError("Changelog data not found on server", "Could not determine changelog file which is provided by project server");
        }else
        {
            mLbWaiting->setVisible(false);
            mWvChangelog->load(QUrl("http://"RELEASE_SERVER + tChangelog));
            mWvChangelog->show();
            mWvChangelog->setVisible(true);
        }
    }
}

int UpdateCheckDialog::exec()
{
    int tResult = QDialog::exec();

    CONF.SetAutoUpdateCheck(mCbAutoUpdateCheck->isChecked());

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
