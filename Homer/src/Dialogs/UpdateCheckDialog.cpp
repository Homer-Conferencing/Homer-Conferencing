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
 * Purpose: Implementation of a dialog for check for updates
 * Author:  Thomas Volkert
 * Since:   2010-12-19
 */

#include <Dialogs/UpdateCheckDialog.h>
#include <Configuration.h>
#include <Logger.h>
#include <Snippets.h>

#include <QHttp>
#include <QDesktopServices>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

UpdateCheckDialog::UpdateCheckDialog(QWidget* pParent) :
    QDialog(pParent)
{
    initializeGUI();
    mTbDownloadUpdate->hide();
    mLbVersion->setText(RELEASE_VERSION_STRING);
    mCbAutoUpdateCheck->setChecked(CONF.GetAutoUpdateCheck());
}

UpdateCheckDialog::~UpdateCheckDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

void UpdateCheckDialog::initializeGUI()
{
    setupUi(this);

    mHttpGetVersionServer = new QHttp(this);
    TriggerVersionCheck(mHttpGetVersionServer, GotAnswerForVersionRequest);

    mHttpGetChangelogUrl = new QHttp(this);
    connect(mHttpGetChangelogUrl, SIGNAL(done(bool)), this, SLOT(GotAnswerForChangelogRequest(bool)));
    connect(mTbDownloadUpdate, SIGNAL(clicked()), this, SLOT(DownloadUpdate()));
    mHttpGetChangelogUrl->setHost(RELEASE_SERVER);
    mHttpGetChangelogUrl->get(PATH_CHANGELOG_TXT);
}

void UpdateCheckDialog::DownloadUpdate()
{
    QString tUpdateLocation;
    QString tBits;
    if (QString(_OS_ARCH).contains("64"))
        tBits = "64";
    else
        tBits = "32";

    #ifdef WIN32
        tUpdateLocation = "http://sourceforge.net/projects/homer-conf/files/Homer-Windows.zip";
    #endif
    #ifdef LINUX
        tUpdateLocation = "http://sourceforge.net/projects/homer-conf/files/Homer-Linux" + tBits + ".tar.bz2";
    #endif
    #ifdef BSD
        tUpdateLocation = "http://sourceforge.net/projects/homer-conf/files/Homer-BSD" + tBits + ".tar.bz2";
    #endif
    #ifdef APPLE
        tUpdateLocation = "http://sourceforge.net/projects/homer-conf/files/Homer-OSX" + tBits + ".tar.bz2";
    #endif

    QDesktopServices::openUrl(tUpdateLocation);
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
