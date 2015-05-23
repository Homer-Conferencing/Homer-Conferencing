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
 * Purpose: Implementation of a help dialog
 * Since:   2010-12-19
 */

#include <Dialogs/HelpDialog.h>
#include <Configuration.h>
#include <Meeting.h>
#include <Logger.h>
#include <HBSystem.h>
#include <Snippets.h>
#include <Header_Ffmpeg.h>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDesktopWidget>
#include <QUrl>
#include <QSysInfo>
#ifdef LINUX
#include <linux/version.h>
#include <sys/utsname.h>
#endif

namespace Homer { namespace Gui {

using namespace Homer::Conference;

///////////////////////////////////////////////////////////////////////////////

HelpDialog::HelpDialog(QWidget* pParent) :
    QDialog(pParent)
{
    initializeGUI();

    QString tUrlHelpLocation = QString("http://" RELEASE_SERVER PATH_HELP_TXT);
    mHttpGetHelpUrl = new QNetworkAccessManager(this);
    HttpDownload(mHttpGetHelpUrl, tUrlHelpLocation, GotAnswerForHelpRequest);
}

HelpDialog::~HelpDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

QString HelpDialog::GetSystemInfo()
{
    QString tResult;

    QString tAdditionaLiblLines = "";
    QString tTargetArch = Homer::Gui::HelpDialog::tr("unknown");
    QString tCurArch = Homer::Gui::HelpDialog::tr("unknown");
    QString tOs = Homer::Gui::HelpDialog::tr("unknown");
    #ifdef LINUX
        tOs = "Linux";
        tCurArch = (System::GetMachineType() == "x86") ? "linux32" : "linux64";
        tTargetArch = "linux" + QString("%1").arg(ARCH_BITS);
        tAdditionaLiblLines = Homer::Gui::HelpDialog::tr("Linked glibc:") + "  " + QString("%1").arg(__GLIBC__) + "." + QString("%1").arg(__GLIBC_MINOR__) + "\n";
    #endif
    #if (defined BSD) && (not defined APPLE)
        tOs = "BSD";
        tCurArch = (System::GetMachineType() == "x86") ? "bsd32" : "bsd64";
        tTargetArch = "bsd" + QString("%1").arg(ARCH_BITS);
    #endif
    #ifdef APPLE
        tOs = Homer::Gui::HelpDialog::tr("OS X unknown version");
        tCurArch = (System::GetMachineType() == "x86") ? "apple32" : "apple64";
        tTargetArch = "apple" + QString("%1").arg(ARCH_BITS);

        switch(QSysInfo::MacintoshVersion)
        {
            case QSysInfo::MV_CHEETAH:
                        tOs = "OS X Cheetah";
                        break;
            case QSysInfo::MV_PUMA:
                        tOs = "OS X Puma";
                        break;
            case QSysInfo::MV_JAGUAR:
                        tOs = "OS X Jaguar";
                        break;
            case QSysInfo::MV_PANTHER:
                        tOs = "OS X Panther";
                        break;
            case QSysInfo::MV_TIGER:
                        tOs = "OS X Tiger";
                        break;
            case QSysInfo::MV_LEOPARD:
                        tOs = "OS X Leopard";
                        break;
            case QSysInfo::MV_SNOWLEOPARD:
                        tOs = "OS X Snow Leopard";
                        break;
            #if (QT_VERSION >= 0x040000)
                case QSysInfo::MV_LION:
                            tOs = "OS X Lion";
                            break;
            #endif
            #if (QT_VERSION >= 0x040803)
                case QSysInfo::MV_MOUNTAINLION:
                            tOs = "OS X Mountain Lion";
                            break;
            #endif
            #if (QT_VERSION >= 0x050101)
                case QSysInfo::MV_MAVERICKS:
                            tOs = "OS X Mavericks";
                            break;
            #endif
            default:
                        tOs = "OS X unknown version";
                        break;
        }
    #endif
    #ifdef WINDOWS
        tOs = Homer::Gui::HelpDialog::tr("Windows unknown version");
        tCurArch = (System::GetMachineType() == "x86") ? "win32" : "win64";
        tTargetArch = "win" + QString("%1").arg(ARCH_BITS);

        switch(QSysInfo::WindowsVersion)
        {
            case QSysInfo::WV_NT:
                        tOs = "Windows NT";
                        break;
            case QSysInfo::WV_2000:
                        tOs = "Windows 2000";
                        break;
            case QSysInfo::WV_XP:
                        tOs = "Windows XP";
                        break;
            case QSysInfo::WV_2003:
                        tOs = "Windows Server 2003/XP Pro x64";
                        break;
            case QSysInfo::WV_VISTA:
                        tOs = "Windows Vista/Server 2008";
                        break;
            case QSysInfo::WV_WINDOWS7:
                        tOs = "Windows 7/Server 2008 R2";
                        break;
            case QSysInfo::WV_WINDOWS8:
						tOs = "Windows 8/Server 2012";
						break;
            case QSysInfo::WV_WINDOWS8_1:
                        tOs = "Windows 8.1/Server 2012 R2";
                        break;
            default:
                        tOs = Homer::Gui::HelpDialog::tr("Windows unknown version");
                        break;
        }
    #endif

    tResult=    Homer::Gui::HelpDialog::tr("Operating System:") + "  " + tOs + "\n" +
                Homer::Gui::HelpDialog::tr("Kernel:") + "  " + QString(System::GetKernelVersion().c_str()) + "\n" +
                Homer::Gui::HelpDialog::tr("Library Qt:") + "  " + QString(qVersion()) + "\n" \
                "\n" +
                Homer::Gui::HelpDialog::tr("CPU cores:") + " " + QString("%1").arg(System::GetMachineCores()) + "\n" +
                Homer::Gui::HelpDialog::tr("Memory (hardware):") + " " + Int2ByteExpression(rint((float)System::GetMachineMemoryPhysical() / 1024 / 1024)) + " MB\n" +
                Homer::Gui::HelpDialog::tr("Memory (swap space):") + " " + Int2ByteExpression(rint((float)System::GetMachineMemorySwap() / 1024 / 1024)) + " MB\n" +
                Homer::Gui::HelpDialog::tr("Current architecture:") + " " + tCurArch + "\n" +
                Homer::Gui::HelpDialog::tr("Target architecture:") + " " + tTargetArch + "\n" \
                "\n" +
                Homer::Gui::HelpDialog::tr("Linked AVCodec:") + "  " + QString("%1").arg(LIBAVCODEC_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVCODEC_VERSION_MINOR) + "." + QString("%1").arg(LIBAVCODEC_VERSION_MICRO) + "\n" +
                Homer::Gui::HelpDialog::tr("Linked AVDevice:") + "  " + QString("%1").arg(LIBAVDEVICE_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVDEVICE_VERSION_MINOR) + "." + QString("%1").arg(LIBAVDEVICE_VERSION_MICRO) + "\n" +
                Homer::Gui::HelpDialog::tr("Linked AVFormat:") + "  " + QString("%1").arg(LIBAVFORMAT_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVFORMAT_VERSION_MINOR) + "." + QString("%1").arg(LIBAVFORMAT_VERSION_MICRO) + "\n" +
                Homer::Gui::HelpDialog::tr("Linked AVUtil:") + "  " + QString("%1").arg(LIBAVUTIL_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVUTIL_VERSION_MINOR) + "." + QString("%1").arg(LIBAVUTIL_VERSION_MICRO) + "\n" +
                Homer::Gui::HelpDialog::tr("Linked SWScale:") + "  " + QString("%1").arg(LIBSWSCALE_VERSION_MAJOR) + "." + QString("%1").arg(LIBSWSCALE_VERSION_MINOR) + "." + QString("%1").arg(LIBSWSCALE_VERSION_MICRO) + "\n" +
                Homer::Gui::HelpDialog::tr("Linked sofia-sip:") + " " + QString(MEETING.GetSofiaSipVersion().c_str()) + "\n" \
                "" + tAdditionaLiblLines + "\n" +
                Homer::Gui::HelpDialog::tr("QoS supported:") + " " + (Socket::IsQoSSupported() ? "yes" : "no") + "\n" +
                Homer::Gui::HelpDialog::tr("IPv6 supported:")+ " " + (Socket::IsIPv6Supported() ? "yes" : "no") + "\n" +
                Homer::Gui::HelpDialog::tr("UDP-Lite supported:") + " " + (Socket::IsTransportSupported(SOCKET_UDP_LITE) ? "yes" : "no") + "\n" +
                Homer::Gui::HelpDialog::tr("DCCP supported:") + " " + (Socket::IsTransportSupported(SOCKET_DCCP) ? "yes" : "no") + "\n" +
                Homer::Gui::HelpDialog::tr("SCTP supported:") + " " + (Socket::IsTransportSupported(SOCKET_SCTP) ? "yes" : "no");

    return tResult;
}

void HelpDialog::initializeGUI()
{
    setupUi(this);
    mWvHelp->setVisible(false);
    mSystemData->setText(GetSystemInfo());
    mLbVersion->setText(RELEASE_VERSION_STRING);

    QDesktopWidget *tDesktop = QApplication::desktop();
    int tScreenResX = tDesktop->screenGeometry(tDesktop->primaryScreen()).width();
    int tScreenResY = tDesktop->screenGeometry(tDesktop->primaryScreen()).height();
    if ((tScreenResX < width()) || (tScreenResY < height()))
        showMaximized();
}

void HelpDialog::GotAnswerForHelpRequest(QNetworkReply *pReply)
{
    int tErrorCode = pReply->error();
    if (tErrorCode != QNetworkReply::NoError)
    {
        // catch the 404
        if(tErrorCode == QNetworkReply::ContentNotFoundError)
        {
            mLbWaiting->setText("<font bgcolor='yellow' color='red'><b>fetch failed</b></font>");
            ShowError(Homer::Gui::HelpDialog::tr("Help data not found on server"), Homer::Gui::HelpDialog::tr("Can not download help data on project server"));
        }else{
            mLbWaiting->setText("<font bgcolor='yellow' color='red'><b>fetch failed</b></font>");
            ShowError(Homer::Gui::HelpDialog::tr("Communication with server failed"), Homer::Gui::HelpDialog::tr("Can not download help data on project server"));
        }
    }else
    {
        QString tHelpFile = QString(pReply->readAll().constData());
        LOG(LOG_VERBOSE, "Loading help from http://"RELEASE_SERVER"%s", tHelpFile.toStdString().c_str());

        mLbWaiting->setVisible(false);
        mWvHelp->load(QUrl("http://"RELEASE_SERVER + tHelpFile));
        mWvHelp->show();
        mWvHelp->setVisible(true);
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
