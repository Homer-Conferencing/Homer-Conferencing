

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
 * Author:  Thomas Volkert
 * Since:   2010-12-19
 */

#include <Dialogs/HelpDialog.h>
#include <Configuration.h>
#include <Meeting.h>
#include <Logger.h>
#include <HBSystem.h>
#include <Snippets.h>

#include <QHttp>
#include <QUrl>
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
    mLbVersion->setText(RELEASE_VERSION_STRING);
}

HelpDialog::~HelpDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

void HelpDialog::initializeGUI()
{
    setupUi(this);
    mWvHelp->setVisible(false);

    mHttpGetHelpUrl = new QHttp(this);
    connect(mHttpGetHelpUrl, SIGNAL(done(bool)), this, SLOT(GotAnswerForHelpRequest(bool)));
    mHttpGetHelpUrl->setHost(RELEASE_SERVER);
    mHttpGetHelpUrl->get(PATH_HELP_TXT);
#ifdef LINUX
        QString tCurArch = (System::GetMachineType() == "x86") ? "linux32" : "linux64";
        mSystemData->setText(       "Operating System:  Linux\n"\
                                    "Kernel:  " + QString(System::GetLinuxKernelVersion().c_str()) + "\n"\
                                    "Library Qt:  " + QString(qVersion()) + "\n"\
                                    "\n"\
                                    "Number of cpu cores: " + QString("%1").arg(System::GetMachineCores()) + "\n"\
                                    "Current architecture: " + tCurArch + "\n"\
                                    "Target architecture: linux" + QString("%1").arg(ARCH_BITS) + "\n"\
									"\n"\
                                    "Linked AVCodec:  " + QString("%1").arg(LIBAVCODEC_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVCODEC_VERSION_MINOR) + "." + QString("%1").arg(LIBAVCODEC_VERSION_MICRO) + "\n"\
                                    "Linked AVDevice:  " + QString("%1").arg(LIBAVDEVICE_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVDEVICE_VERSION_MINOR) + "." + QString("%1").arg(LIBAVDEVICE_VERSION_MICRO) + "\n"\
                                    "Linked AVFormat:  " + QString("%1").arg(LIBAVFORMAT_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVFORMAT_VERSION_MINOR) + "." + QString("%1").arg(LIBAVFORMAT_VERSION_MICRO) + "\n"\
                                    "Linked AVUtil:  " + QString("%1").arg(LIBAVUTIL_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVUTIL_VERSION_MINOR) + "." + QString("%1").arg(LIBAVUTIL_VERSION_MICRO) + "\n"\
                                    "Linked SWScale:  " + QString("%1").arg(LIBSWSCALE_VERSION_MAJOR) + "." + QString("%1").arg(LIBSWSCALE_VERSION_MINOR) + "." + QString("%1").arg(LIBSWSCALE_VERSION_MICRO) + "\n"\
                                    "Linked sofia-sip: " + QString(MEETING.GetSofiaSipVersion().c_str()) + "\n"\
                                    "Linked glibc:  " + QString("%1").arg(__GLIBC__) + "." + QString("%1").arg(__GLIBC_MINOR__) + "\n"\
                                    "\n"\
                                    "QoS supported: " + (Socket::IsQoSSupported() ? "yes" : "no") + "\n"\
                                    "IPv6 supported: " + (Socket::IsIPv6Supported() ? "yes" : "no") + "\n"\
                                    "UDPlite supported: " + (Socket::IsTransportSupported(SOCKET_UDP_LITE) ? "yes" : "no") + "\n"\
                                    "DCCP supported: " + (Socket::IsTransportSupported(SOCKET_DCCP) ? "yes" : "no") + "\n"\
                                    "SCTP supported: " + (Socket::IsTransportSupported(SOCKET_SCTP) ? "yes" : "no") + "\n"\
                                    );

#endif
// TODO TV The output is not valid for Freebsd
#if (not defined BSD) && (not defined APPLE)
		QString tCurArch = (System::GetMachineType() == "x86") ? "bsd32" : "bsd64";
		mSystemData->setText(       "Operating System:  Linux\n"\
									"Kernel:  " + "??" + "\n"\
									"Library Qt:  " + QString(qVersion()) + "\n"\
									"\n"\
									"Number of cpu cores: " + QString("%1").arg(System::GetMachineCores()) + "\n"\
									"Current architecture: " + tCurArch + "\n"\
									"Target architecture: linux" + QString("%1").arg(ARCH_BITS) + "\n"\
									"\n"\
									"Linked AVCodec:  " + QString("%1").arg(LIBAVCODEC_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVCODEC_VERSION_MINOR) + "." + QString("%1").arg(LIBAVCODEC_VERSION_MICRO) + "\n"\
									"Linked AVDevice:  " + QString("%1").arg(LIBAVDEVICE_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVDEVICE_VERSION_MINOR) + "." + QString("%1").arg(LIBAVDEVICE_VERSION_MICRO) + "\n"\
									"Linked AVFormat:  " + QString("%1").arg(LIBAVFORMAT_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVFORMAT_VERSION_MINOR) + "." + QString("%1").arg(LIBAVFORMAT_VERSION_MICRO) + "\n"\
									"Linked AVUtil:  " + QString("%1").arg(LIBAVUTIL_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVUTIL_VERSION_MINOR) + "." + QString("%1").arg(LIBAVUTIL_VERSION_MICRO) + "\n"\
									"Linked SWScale:  " + QString("%1").arg(LIBSWSCALE_VERSION_MAJOR) + "." + QString("%1").arg(LIBSWSCALE_VERSION_MINOR) + "." + QString("%1").arg(LIBSWSCALE_VERSION_MICRO) + "\n"\
									"Linked sofia-sip: " + QString(MEETING.GetSofiaSipVersion().c_str()) + "\n"\
//									"Linked glibc:  " + QString("%1").arg(__GLIBC__) + "." + QString("%1").arg(__GLIBC_MINOR__) + "\n"\
									"\n"\
									"QoS supported: " + (Socket::IsQoSSupported() ? "yes" : "no") + "\n"\
									"IPv6 supported: " + (Socket::IsIPv6Supported() ? "yes" : "no") + "\n"\
									"UDPlite supported: " + (Socket::IsTransportSupported(SOCKET_UDP_LITE) ? "yes" : "no") + "\n"\
									"DCCP supported: " + (Socket::IsTransportSupported(SOCKET_DCCP) ? "yes" : "no") + "\n"\
									"SCTP supported: " + (Socket::IsTransportSupported(SOCKET_SCTP) ? "yes" : "no") + "\n"\
									);

#endif
#ifdef APPLE
        QString tOsxVer = "";
        int tOsxMajor = 0, tOsxMinor = 0;

        switch(QSysInfo::MacintoshVersion)
        {
            case QSysInfo::MV_CHEETAH:
                        tOsxVer = "OS X Cheetah";
                        tOsxMajor = 10;
                        tOsxMinor = 0;
                        break;
            case QSysInfo::MV_PUMA:
                        tOsxVer = "OS X Puma";
                        tOsxMajor = 10;
                        tOsxMinor = 1;
                        break;
            case QSysInfo::MV_JAGUAR:
                        tOsxVer = "OS X Jaguar";
                        tOsxMajor = 10;
                        tOsxMinor = 2;
                        break;
            case QSysInfo::MV_PANTHER:
                        tOsxVer = "OS X Panther";
                        tOsxMajor = 10;
                        tOsxMinor = 3;
                        break;
            case QSysInfo::MV_TIGER:
                        tOsxVer = "OS X Tiger";
                        tOsxMajor = 10;
                        tOsxMinor = 4;
                        break;
            case QSysInfo::MV_LEOPARD:
                        tOsxVer = "OS X Leopard";
                        tOsxMajor = 10;
                        tOsxMinor = 5;
                        break;
            case QSysInfo::MV_SNOWLEOPARD:
                        tOsxVer = "OS X Snow Leopard";
                        tOsxMajor = 10;
                        tOsxMinor = 6;
                        break;
            case QSysInfo::MV_LION:
                        tOsxVer = "OS X Lion";
                        tOsxMajor = 10;
                        tOsxMinor = 7;
                        break;
            default:
                        tOsxVer = "OS X";
                        break;
        }

        mSystemData->setText(       "Operating System: " + tOsxVer + "\n"\
                                    "Kernel:  " + QString("%1").arg(tOsxMajor) + "." + QString("%1").arg(tOsxMinor) + "\n"\
                                    "Library Qt: " + QString(qVersion()) + "\n"\
                                    "\n"\
                                    "Number of cpu cores: " + QString("%1").arg(System::GetMachineCores()) + "\n"\
                                    "Current architecture: apple64\n"\
                                    "Target architecture: apple" + QString("%1").arg(ARCH_BITS) + "\n"\
                                    "\n"\
                                    "Linked AVCodec:  " + QString("%1").arg(LIBAVCODEC_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVCODEC_VERSION_MINOR) + "." + QString("%1").arg(LIBAVCODEC_VERSION_MICRO) + "\n"\
                                    "Linked AVDevice:  " + QString("%1").arg(LIBAVDEVICE_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVDEVICE_VERSION_MINOR) + "." + QString("%1").arg(LIBAVDEVICE_VERSION_MICRO) + "\n"\
                                    "Linked AVFormat:  " + QString("%1").arg(LIBAVFORMAT_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVFORMAT_VERSION_MINOR) + "." + QString("%1").arg(LIBAVFORMAT_VERSION_MICRO) + "\n"\
                                    "Linked AVUtil:  " + QString("%1").arg(LIBAVUTIL_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVUTIL_VERSION_MINOR) + "." + QString("%1").arg(LIBAVUTIL_VERSION_MICRO) + "\n"\
                                    "Linked SWScale:  " + QString("%1").arg(LIBSWSCALE_VERSION_MAJOR) + "." + QString("%1").arg(LIBSWSCALE_VERSION_MINOR) + "." + QString("%1").arg(LIBSWSCALE_VERSION_MICRO) + "\n"\
                                    "Linked sofia-sip: " + QString(MEETING.GetSofiaSipVersion().c_str()) + "\n"\
                                    "\n"\
                                    "QoS supported: " + (Socket::IsQoSSupported() ? "yes" : "no") + "\n"\
                                    "IPv6 supported: " + (Socket::IsIPv6Supported() ? "yes" : "no") + "\n"\
                                    "UDPlite supported: " + (Socket::IsTransportSupported(SOCKET_UDP_LITE) ? "yes" : "no") + "\n"\
                                    "DCCP supported: " + (Socket::IsTransportSupported(SOCKET_DCCP) ? "yes" : "no") + "\n"\
                                    "SCTP supported: " + (Socket::IsTransportSupported(SOCKET_SCTP) ? "yes" : "no") + "\n"\
                                   );

    #endif
    #ifdef WIN32
        QString tWinVer = "";
        int tWinVerMajor, tWinVerMinor;

        System::GetWindowsKernelVersion(tWinVerMajor, tWinVerMinor);

        switch(QSysInfo::WindowsVersion)
        {
            case QSysInfo::WV_NT:
                        tWinVer = "Windows NT";
                        break;
            case QSysInfo::WV_2000:
                        tWinVer = "Windows 2000";
                        break;
            case QSysInfo::WV_XP:
                        tWinVer = "Windows XP";
                        break;
            case QSysInfo::WV_2003:
                        tWinVer = "Windows Server 2003/XP Pro x64";
                        break;
            case QSysInfo::WV_VISTA:
                        tWinVer = "Windows Vista/Server 2008";
                        break;
            case QSysInfo::WV_WINDOWS7:
                        tWinVer = "Windows 7/Server 2008 R2";
                        break;
            default:
                        tWinVer = "Windows";
                        break;
        }
        mSystemData->setText(       "Operating System: " + tWinVer + "\n"\
                                    "Kernel:  " + QString("%1").arg(tWinVerMajor) + "." + QString("%1").arg(tWinVerMinor) + "\n"\
                                    "Library Qt: " + QString(qVersion()) + "\n"\
                                    "\n"\
                                    "Number of cpu cores: " + QString("%1").arg(System::GetMachineCores()) + "\n"\
                                    "Current architecture: win32\n"\
                                    "Target architecture: win" + QString("%1").arg(ARCH_BITS) + "\n"\
									"\n"\
                                    "Linked AVCodec:  " + QString("%1").arg(LIBAVCODEC_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVCODEC_VERSION_MINOR) + "." + QString("%1").arg(LIBAVCODEC_VERSION_MICRO) + "\n"\
                                    "Linked AVDevice:  " + QString("%1").arg(LIBAVDEVICE_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVDEVICE_VERSION_MINOR) + "." + QString("%1").arg(LIBAVDEVICE_VERSION_MICRO) + "\n"\
                                    "Linked AVFormat:  " + QString("%1").arg(LIBAVFORMAT_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVFORMAT_VERSION_MINOR) + "." + QString("%1").arg(LIBAVFORMAT_VERSION_MICRO) + "\n"\
                                    "Linked AVUtil:  " + QString("%1").arg(LIBAVUTIL_VERSION_MAJOR) + "." + QString("%1").arg(LIBAVUTIL_VERSION_MINOR) + "." + QString("%1").arg(LIBAVUTIL_VERSION_MICRO) + "\n"\
                                    "Linked SWScale:  " + QString("%1").arg(LIBSWSCALE_VERSION_MAJOR) + "." + QString("%1").arg(LIBSWSCALE_VERSION_MINOR) + "." + QString("%1").arg(LIBSWSCALE_VERSION_MICRO) + "\n"\
                                    "Linked sofia-sip: " + QString(MEETING.GetSofiaSipVersion().c_str()) + "\n"\
                                    "\n"\
                                    "QoS supported: " + (Socket::IsQoSSupported() ? "yes" : "no") + "\n"\
                                    "IPv6 supported: " + (Socket::IsIPv6Supported() ? "yes" : "no") + "\n"\
                                    "UDPlite supported: " + (Socket::IsTransportSupported(SOCKET_UDP_LITE) ? "yes" : "no") + "\n"\
                                    "DCCP supported: " + (Socket::IsTransportSupported(SOCKET_DCCP) ? "yes" : "no") + "\n"\
                                    "SCTP supported: " + (Socket::IsTransportSupported(SOCKET_SCTP) ? "yes" : "no") + "\n"\
                                   );
    #endif
}

void HelpDialog::GotAnswerForHelpRequest(bool pError)
{
    if (pError)
    {
        mLbWaiting->setText("<font bgcolor='yellow' color='red'><b>fetch failed</b></font>");
        ShowError("Communication with server failed", "Could not determine help file which is provided by project server");
    }else
    {
        QString tHelpFile = QString(mHttpGetHelpUrl->readAll().constData());
        LOG(LOG_VERBOSE, "Loading help from http://"RELEASE_SERVER"%s", tHelpFile.toStdString().c_str());

        if (tHelpFile.contains("404 Not Found"))
        {
            mLbWaiting->setText("<font bgcolor='yellow' color='red'><b>fetch failed</b></font>");
            ShowError("Help data not found on server", "Could not determine help file which is provided by project server");
        }else
        {
            mLbWaiting->setVisible(false);
            mWvHelp->load(QUrl("http://"RELEASE_SERVER + tHelpFile));
            mWvHelp->show();
            mWvHelp->setVisible(true);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
