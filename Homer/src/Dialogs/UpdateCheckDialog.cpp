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
#include <QDesktopWidget>
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

    mHttpGetVersionServer = new QHttp(this);
    TriggerVersionCheck(mHttpGetVersionServer, GotAnswerForVersionRequest);

    mHttpGetChangelogUrl = new QHttp(this);
    connect(mHttpGetChangelogUrl, SIGNAL(done(bool)), this, SLOT(GotAnswerForChangelogRequest(bool)));
    connect(mTbDownloadUpdate, SIGNAL(clicked()), this, SLOT(DownloadStart()));
    connect(mTbDownloadUpdateInstaller, SIGNAL(clicked()), this, SLOT(DownloadInstallerStart()));
    mHttpGetChangelogUrl->setHost(RELEASE_SERVER);
    mHttpGetChangelogUrl->get(PATH_CHANGELOG_TXT);

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

    mTbDownloadUpdate->hide();
    mTbDownloadUpdateInstaller->hide();
    mLbVersion->setText(RELEASE_VERSION_STRING);
    mCbAutoUpdateCheck->setChecked(CONF.GetAutoUpdateCheck());

    QDesktopWidget *tDesktop = QApplication::desktop();
    int tScreenResX = tDesktop->screenGeometry(tDesktop->primaryScreen()).width();
    int tScreenResY = tDesktop->screenGeometry(tDesktop->primaryScreen()).height();
    if ((tScreenResX < width()) || (tScreenResY < height()))
        showMaximized();
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
	if(mDownloadProgressDialog != NULL)
	{
		ShowInfo(Homer::Gui::UpdateCheckDialog::tr("Download of Homer update running"), Homer::Gui::UpdateCheckDialog::tr("A download of a Homer update is already started!"));
		return;
	}

	LOG(LOG_VERBOSE, "Starting download of update archive");

	// find correct release name for this target system
	QString tReleaseFileName;
	QString tReleaseFileType;

	#ifdef WIN32
		tReleaseFileName = "Homer-Windows.zip";
		tReleaseFileType = "ZIP archive file (*.zip)";
	#endif
	#ifdef WIN64
		tReleaseFileName = "Homer-Windows" + QString("%1").arg(ARCH_BITS) + ".zip";
		tReleaseFileType = "ZIP archive file (*.zip)";
	#endif
	#ifdef LINUX
		tReleaseFileName = "Homer-Linux" + QString("%1").arg(ARCH_BITS) + ".tar.bz2";
		tReleaseFileType = "bz2 archive file (*.bz2)";
	#endif
	#ifdef BSD
		tReleaseFileName = "Homer-BSD" + QString("%1").arg(ARCH_BITS) + ".tar.bz2";
		tReleaseFileType = "bz2 archive file (*.bz2)";
	#endif
	#ifdef APPLE
		tReleaseFileName = "Homer-OSX" + QString("%1").arg(ARCH_BITS) + ".tar.bz2";
		tReleaseFileType = "bz2 archive file (*.bz2)";
	#endif

	// ask for target location for new release archive
	QString tFileName;
	tFileName = QFileDialog::getSaveFileName(this,  Homer::Gui::UpdateCheckDialog::tr("Save Homer archive to.."),
																CONF.GetDataDirectory() + "/" + tReleaseFileName,
																tReleaseFileType,
																&tReleaseFileType,
																CONF_NATIVE_DIALOGS);

	if (tFileName.isEmpty())
		return;

	CONF.SetDataDirectory(tFileName.left(tFileName.lastIndexOf('/')));

	// open the target file
	mDownloadHomerUpdateFile = new QFile(tFileName, this);
	if(!mDownloadHomerUpdateFile->open(QIODevice::WriteOnly))
	{
		ShowError(Homer::Gui::UpdateCheckDialog::tr("Could not store Homer archive"), Homer::Gui::UpdateCheckDialog::tr("Unable to store the downloaded Homer archive to") + " \"" + tFileName + "\"");
		return;
	}

	// create progress dialog
	mDownloadProgressDialog = new QProgressDialog(this);
	mDownloadProgressDialog->setWindowTitle("Download progress");
	mDownloadProgressDialog->setLabelText("<b>Downloading Homer archive</b>");
	connect(mDownloadProgressDialog, SIGNAL(canceled()), this, SLOT(DownloadStop()));

	LOG(LOG_VERBOSE, "Download progress dialog created");

	//start HTTP get for the Homer archive
	DownloadFireRequest(PATH_HOMER_RELEASES + GetNumericReleaseVersion(mServerVersion) + "/" + tReleaseFileName);
}

void UpdateCheckDialog::DownloadInstallerStart()
{
	if(mDownloadProgressDialog != NULL)
	{
		ShowInfo(Homer::Gui::UpdateCheckDialog::tr("Download of Homer update running"), Homer::Gui::UpdateCheckDialog::tr("A download of a Homer update is already started!"));
		return;
	}

	LOG(LOG_VERBOSE, "Starting download of update installer");

	// find correct release name for this target system
	QString tReleaseFileName;
	QString tReleaseFileType;

	#ifdef WIN32
		tReleaseFileName = "Homer-Conferencing.exe";
		tReleaseFileType = Homer::Gui::UpdateCheckDialog::tr("Windows executable file (*.exe)");
	#endif
	#ifdef WIN64
		tReleaseFileName = "Homer-Conferencing64.exe";
		tReleaseFileType = Homer::Gui::UpdateCheckDialog::tr("Windows executable file (*.exe)");
	#endif
	#ifdef LINUX
		tReleaseFileName = "Homer-Conferencing.sh";
		tReleaseFileType = Homer::Gui::UpdateCheckDialog::tr("Linux shell script (*.sh)");
	#endif
	#if defined(BSD) && !defined(APPLE)
		return;
	#endif
	#ifdef APPLE
		tReleaseFileName = "Homer-Conferencing.dmg";
		tReleaseFileType = Homer::Gui::UpdateCheckDialog::tr("Apple disc image file (*.dmg)");
	#endif

	// ask for target location for new release installer
	QString tFileName;
	tFileName = QFileDialog::getSaveFileName(this,  Homer::Gui::UpdateCheckDialog::tr("Save Homer installer to.."),
																CONF.GetDataDirectory() + "/" + tReleaseFileName,
																tReleaseFileType,
																&tReleaseFileType,
																CONF_NATIVE_DIALOGS);

	if (tFileName.isEmpty())
		return;

	CONF.SetDataDirectory(tFileName.left(tFileName.lastIndexOf('/')));

	// open the target file
	mDownloadHomerUpdateFile = new QFile(tFileName, this);
	if(!mDownloadHomerUpdateFile->open(QIODevice::WriteOnly))
	{
		ShowError(Homer::Gui::UpdateCheckDialog::tr("Could not store Homer installer"), Homer::Gui::UpdateCheckDialog::tr("Unable to store the downloaded Homer installer to") + " \"" + tFileName + "\"");
		return;
	}

	// create progress dialog
	mDownloadProgressDialog = new QProgressDialog(this);
	mDownloadProgressDialog->setWindowTitle("Download progress");
	mDownloadProgressDialog->setLabelText("<b>Downloading Homer installer</b>");
	connect(mDownloadProgressDialog, SIGNAL(canceled()), this, SLOT(DownloadStop()));

	LOG(LOG_VERBOSE, "Download progress dialog created");

	//start HTTP get for the Homer installer
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
	if(mDownloadHomerUpdateFile != NULL)
	{
		mDownloadHomerUpdateFile->close();
		delete mDownloadHomerUpdateFile;
		mDownloadHomerUpdateFile = NULL;
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
	mDownloadProgressDialog->setLabelText("<b>" + Homer::Gui::UpdateCheckDialog::tr("Downloading Homer update") + "</b><br>  " + Homer::Gui::UpdateCheckDialog::tr("from") + " <font color=blue>" + mServerFile + "</font><br>  " + Homer::Gui::UpdateCheckDialog::tr("to") + " <i>" + mDownloadHomerUpdateFile->fileName() + "</i><br>  <b>"+ Homer::Gui::UpdateCheckDialog::tr("Loaded:") + " " + Int2ByteExpression(pLoadedBytes) + "/" +  Int2ByteExpression(pTotalBytes) + " " + Homer::Gui::UpdateCheckDialog::tr("bytes") + "</b>");
    mDownloadProgressDialog->show();
}

void UpdateCheckDialog::DownloadFinished()
{
	LOG(LOG_VERBOSE, "Download finished");
	if((!mDownloadAborted) && (mDownloadReply->error()))
	{
		ShowError(Homer::Gui::UpdateCheckDialog::tr("Failed to download Homer update"), Homer::Gui::UpdateCheckDialog::tr("Unable to download Homer update. The reason is:") + " \"" + mDownloadReply->errorString() + "\"");
		mDownloadAborted = true;
	}

	mDownloadHomerUpdateFile->flush();
	mDownloadHomerUpdateFile->close();

	// were we redirected to another target?
	QVariant tRedirectionTarget = mDownloadReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (!tRedirectionTarget.isNull())
    {
    	LOG(LOG_VERBOSE, "Have been redirected to new Homer update location under %s", tRedirectionTarget.toString().toStdString().c_str());
		mDownloadReply->deleteLater();
		mDownloadHomerUpdateFile->open(QIODevice::WriteOnly);
		mDownloadHomerUpdateFile->resize(0);

		// start HTTP get for the Homer update
		DownloadFireRequest(tRedirectionTarget.toString());
    }else
    {// everything was okay, we automatically open downloaded file and we delete objects
		if(!mDownloadAborted)
		    QDesktopServices::openUrl(mDownloadHomerUpdateFile->fileName());

		if(mDownloadHomerUpdateFile != NULL)
		{
			mDownloadHomerUpdateFile->close();
			delete mDownloadHomerUpdateFile;
			mDownloadHomerUpdateFile = NULL;
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
    if (mDownloadHomerUpdateFile)
    	mDownloadHomerUpdateFile->write(mDownloadReply->readAll());
}

void UpdateCheckDialog::GotAnswerForVersionRequest(bool pError)
{
    if (pError)
    {
        mLbVersionServer->setText("<font bgcolor='yellow' color='red'><b>" + Homer::Gui::UpdateCheckDialog::tr("check failed") + "</b></font>");
        ShowError(Homer::Gui::UpdateCheckDialog::tr("Communication with server failed"), Homer::Gui::UpdateCheckDialog::tr("Could not determine software version which is provided by project server"));
    }else
    {
        mServerVersion = QString(mHttpGetVersionServer->readAll().constData());
        LOG(LOG_VERBOSE, "Got version on server: %s", mServerVersion.toStdString().c_str());
        if (mServerVersion.contains("404 Not Found"))
        {
            mLbVersionServer->setText("<font bgcolor='yellow' color='red'><b>" + Homer::Gui::UpdateCheckDialog::tr("check failed") + "</b></font>");
            ShowError(Homer::Gui::UpdateCheckDialog::tr("Version data not found on server"), Homer::Gui::UpdateCheckDialog::tr("Could not determine software version which is provided by project server"));
        }else
        {
            if (mServerVersion != RELEASE_VERSION_STRING)
            {
                mLbVersionServer->setText("<font color='red'><b>" + mServerVersion + "</b></font>");
				#if defined(WINDOWS) || defined(LINUX)
                	mTbDownloadUpdate->show();
				#endif
				mTbDownloadUpdateInstaller->show();
            }else
                mLbVersionServer->setText("<font color='green'><b>" + mServerVersion + "</b></font>");
        }
    }
}

void UpdateCheckDialog::GotAnswerForChangelogRequest(bool pError)
{
    if (pError)
    {
        mLbWaiting->setText("<font bgcolor='yellow' color='red'><b>" + Homer::Gui::UpdateCheckDialog::tr("fetch failed") + "</b></font>");
        ShowError(Homer::Gui::UpdateCheckDialog::tr("Communication with server failed"), Homer::Gui::UpdateCheckDialog::tr("Could not determine changelog file which is provided by project server"));
    }else
    {
        QString tChangelog = QString(mHttpGetChangelogUrl->readAll().constData());
        LOG(LOG_VERBOSE, "Loading changelog from http://"RELEASE_SERVER"%s", tChangelog.toStdString().c_str());

        if (tChangelog.contains("404 Not Found"))
        {
            mLbWaiting->setText("<font bgcolor='yellow' color='red'><b>" + Homer::Gui::UpdateCheckDialog::tr("fetch failed") + "</b></font>");
            ShowError(Homer::Gui::UpdateCheckDialog::tr("Changelog data not found on server"), Homer::Gui::UpdateCheckDialog::tr("Could not determine changelog file which is provided by project server"));
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
