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
 * Name:    Socket.cpp
 * Purpose: Implementation of wrapper for os independent socket handling
 * Author:  Thomas Volkert
 * Since:   2010-09-22
 * Version: $Id$
 */

#include <Logger.h>
#include <HBSocket.h>
#include <HBSystem.h>
#include <HBMutex.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <list>
#include <in_ext.h>
#include <inet_ext.h>
#include <socket_ext.h>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

int sIPv6Supported = -1;
int sUDPliteSupported = -1;
int sDCCPSupported = -1;

///////////////////////////////////////////////////////////////////////////////

void Socket::SetDefaults(enum TransportType pTransportType)
{
    mIsConnected = false;
	mIsListening = false;
	mSocketNetworkType = SOCKET_NETWORK_TYPE_INVALID;
	mSocketTransportType = SOCKET_TRANSPORT_TYPE_INVALID;
    mLocalPort = 0;
    mSocketHandle = -1;
    mTcpClientSockeHandle = -1;
    mConnectedHost = "";
    mConnectedPort = 0;
    mUdpLiteChecksumCoverage = UDP_LITE_HEADER_SIZE;

    #ifdef WIN32
		if (pTransportType == SOCKET_UDP_LITE)
		{
			LOG(LOG_ERROR, "UDPlite is not supported by Windows API, a common UDP socket will be used instead");
			pTransportType = SOCKET_UDP;
		}
        if (pTransportType == SOCKET_DCCP)
        {
            LOG(LOG_ERROR, "DCCP is not supported by Windows API, a common UDP socket will be used instead");
            pTransportType = SOCKET_UDP;
        }
	#endif
    mSocketTransportType = pTransportType;
}

///////////////////////////////////////////////////////////////////////////////
/// server socket
///////////////////////////////////////////////////////////////////////////////
Socket::Socket(unsigned int pListenerPort, enum TransportType pTransportType, unsigned int pProbeStepping, unsigned int pHighesPossibleListenerPort)
{
    LOG(LOG_VERBOSE, "Created server socket object with listener port %u, transport type %s, port probing stepping %d", pListenerPort, TransportType2String(pTransportType).c_str(), pProbeStepping);

    if ((pTransportType == SOCKET_UDP_LITE) && (!IsUDPliteSupported()))
    {
        LOG(LOG_ERROR, "UDPlite not supported by system, falling back to UDP");
        pTransportType = SOCKET_UDP;
    }

    if ((pTransportType == SOCKET_DCCP) && (!IsDCCPSupported()))
    {
        LOG(LOG_ERROR, "DCCP not supported by system, falling back to UDP");
        pTransportType = SOCKET_UDP;
    }

    SetDefaults(pTransportType);
    if (CreateSocket(SOCKET_IPv6))
    {
    	mLocalPort = BindSocket(pListenerPort, pProbeStepping, pHighesPossibleListenerPort);
    	if ((!pProbeStepping) && (mLocalPort != pListenerPort))
    		LOG(LOG_ERROR, "Bound socket %d to another port than requested", mSocketHandle);
    	if (mLocalPort == 0)
    		mSocketHandle = -1;
    }
    LOG(LOG_VERBOSE, "Created %s-listener for socket %d at local port %u", TransportType2String(mSocketTransportType).c_str(), mSocketHandle, mLocalPort);
}

///////////////////////////////////////////////////////////////////////////////
/// client socket
///////////////////////////////////////////////////////////////////////////////
Socket::Socket(enum NetworkType pIpVersion, enum TransportType pTransportType)
{
    LOG(LOG_VERBOSE, "Created client socket object with IP version %d, transport type %s", pIpVersion, TransportType2String(pTransportType).c_str());

    if ((pIpVersion == SOCKET_IPv6) && (!IsIPv6Supported()))
    {
    	LOG(LOG_ERROR, "IPv6 not supported by system, falling back to IPv4");
    	pIpVersion = SOCKET_IPv4;
    }

    if ((pTransportType == SOCKET_UDP_LITE) && (!IsUDPliteSupported()))
    {
        LOG(LOG_ERROR, "UDPlite not supported by system, falling back to UDP");
        pTransportType = SOCKET_UDP;
    }

    if ((pTransportType == SOCKET_DCCP) && (!IsDCCPSupported()))
    {
        LOG(LOG_ERROR, "DCCP not supported by system, falling back to UDP");
        pTransportType = SOCKET_UDP;
    }

    SetDefaults(pTransportType);
    if (CreateSocket(pIpVersion))
        mLocalPort = BindSocket();
    LOG(LOG_VERBOSE, "Created %s-sender for socket %d at local port %u", TransportType2String(mSocketTransportType).c_str(), mSocketHandle, mLocalPort);
}

Socket::~Socket()
{
	DestroySocket(mSocketHandle);
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

string Socket::TransportType2String(enum TransportType pSocketType)
{
    switch(pSocketType)
    {
        case SOCKET_UDP:
            return "UDP";
        case SOCKET_TCP:
            return "TCP";
        case SOCKET_UDP_LITE:
            return "UDPlite";
        default:
            return "N/A";
    }
}

enum TransportType Socket::String2TransportType(string pTypeStr)
{
    if ((pTypeStr == "UDP") || (pTypeStr == "udp"))
        return SOCKET_UDP;
    if ((pTypeStr == "TCP") || (pTypeStr == "tcp"))
        return SOCKET_TCP;
    if ((pTypeStr == "UDPLITE") || (pTypeStr == "UDPlite") || (pTypeStr == "udplite"))
        return SOCKET_UDP_LITE;
    return SOCKET_UDP;
}

enum NetworkType Socket::GetNetworkType()
{
    return mSocketNetworkType;
}

enum TransportType Socket::GetTransportType()
{
    return mSocketTransportType;
}

unsigned int Socket::getLocalPort()
{
    return mLocalPort;
}

bool Socket::SetQoS(const QoSSettings &pQoSSettings)
{
    LOG(LOG_VERBOSE, "Desired QoS: %u KB/s min. data rate, %u ms max. delay, loss less: %d", pQoSSettings.MinDataRate, pQoSSettings.MaxDelay, pQoSSettings.Feature.Lossless);

    mQoSSettings = pQoSSettings;

	#ifdef WIN32
		LOG(LOG_WARN, "QoS interface unsupported for Windows environments, settings will be ignored");
		return false;
	#endif

    setqos(mSocketHandle, pQoSSettings.MinDataRate, pQoSSettings.MaxDelay, pQoSSettings.Features);

	return true;
}

bool Socket::GetQoS(QoSSettings &pQoSSettings)
{
    LOG(LOG_VERBOSE, "Getting current QoS settings");

    pQoSSettings = mQoSSettings;

	return true;
}

static list<QoSProfileDescriptor*> sQoSProfiles;
static Mutex sQoSProfileMutex;

bool Socket::CreateQoSProfile(const std::string &pProfileName, const QoSSettings &pQoSSettings)
{
    QoSProfileDescriptor *tQoSProfileDescriptor;
    QoSProfileList tResult;
    QoSProfileList::iterator tIt, tItEnd;

    LOGEX(Socket, LOG_VERBOSE, "Creating QoS profile %s with parameters: %u KB/s min. data rate, %u max. delay", pQoSSettings.MinDataRate, pQoSSettings.MaxDelay);

    // lock the static list of QoS profiles
    sQoSProfileMutex.lock();

    tItEnd = sQoSProfiles.end();
    for (tIt = sQoSProfiles.begin(); tIt != tItEnd; tIt++)
    {
        if ((*tIt)->Name == pProfileName)
        {
            // unlock the static list of QoS profiles
            sQoSProfileMutex.unlock();

            LOGEX(Socket, LOG_WARN, "QoS profile of name \"%s\" already registered", pProfileName.c_str());

            return false;
        }
    }

    tQoSProfileDescriptor = new QoSProfileDescriptor;
    tQoSProfileDescriptor->Name = pProfileName;
    tQoSProfileDescriptor->Settings = pQoSSettings;
    sQoSProfiles.push_back(tQoSProfileDescriptor);

    // unlock the static list of QoS profiles
    sQoSProfileMutex.unlock();

	return true;
}

QoSProfileList Socket::GetQoSProfiles()
{
    QoSProfileDescriptor *tQoSProfileDescriptor;
    QoSProfileList tResult;
    QoSProfileList::iterator tIt, tItEnd;

    LOGEX(Socket, LOG_VERBOSE, "Getting QoS settings for existing QoS profiles");

    // lock the static list of QoS profiles
    sQoSProfileMutex.lock();

    tItEnd = sQoSProfiles.end();
    for (tIt = sQoSProfiles.begin(); tIt != tItEnd; tIt++)
    {
        tQoSProfileDescriptor = new QoSProfileDescriptor;
        tQoSProfileDescriptor->Name = (*tIt)->Name;
        tQoSProfileDescriptor->Settings = (*tIt)->Settings;
        tResult.push_back(tQoSProfileDescriptor);
    }

    // unlock the static list of QoS profiles
    sQoSProfileMutex.unlock();

	return tResult;
}

bool Socket::SetQoS(const std::string &pProfileName)
{
    QoSProfileList::iterator tIt, tItEnd;

    LOG(LOG_VERBOSE, "Desired QoS profile: %s", pProfileName.c_str());

    // lock the static list of QoS profiles
    sQoSProfileMutex.lock();

    tItEnd = sQoSProfiles.end();
    for (tIt = sQoSProfiles.begin(); tIt != tItEnd; tIt++)
    {
        if ((*tIt)->Name == pProfileName)
        {
            QoSSettings tSettings = (*tIt)->Settings;

            // unlock the static list of QoS profiles
            sQoSProfileMutex.unlock();

            SetQoS(tSettings);

            return true;
        }
    }

    // unlock the static list of QoS profiles
    sQoSProfileMutex.unlock();

	return false;
}

void Socket::SetUdpLiteChecksumCoverage(int pBytes)
{
    /* Checksum coverage:
             Sender's side:
                 It defines how many bytes of the header are included in the checksum calculation
             Receiver's side:
                 It defines how many bytes should be at least included in the checksum calculation at sender's side.
                 If a packet's value is below the packet is silently discarded by the Linux kernel.
                 Moreover, packets with a checksum coverage of zero will be discarded in every case!
     */
	mUdpLiteChecksumCoverage = pBytes;
}

bool Socket::Send(string pTargetHost, unsigned int pTargetPort, void *pBuffer, ssize_t pBufferSize)
{
    SocketAddressDescriptor   tAddressDescriptor;
    unsigned int        tAddressDescriptorSize;
    int                 tSent = 0;
    bool                tResult = false;
    unsigned short int  tLocalPort = 0;
    bool                tTargetIsIPv6 = IS_IPV6_ADDRESS(pTargetHost);
    int                 tUdpLiteChecksumCoverage = mUdpLiteChecksumCoverage;

    if (mSocketHandle == -1)
        return false;

    #ifdef HBS_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Try to send %d bytes via socket %d to %s<%u>", pBufferSize, pTargetHost.c_str(), mSocketHandle, pTargetPort);
    #endif

    if (!FillAddrDescriptor(pTargetHost, pTargetPort, &tAddressDescriptor, tAddressDescriptorSize))
    {
        LOG(LOG_ERROR ,"Could not process the target address of socket %d", mSocketHandle);
        return false;
    }

    switch(mSocketTransportType)
    {
		case SOCKET_UDP_LITE:
			#ifdef LINUX
		        LOG(LOG_VERBOSE, "Setting UDPlite checksum coverage to %d", tUdpLiteChecksumCoverage);
				if (setsockopt(mSocketHandle, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV, (__const void*)&tUdpLiteChecksumCoverage, sizeof(int)) < 0)
					LOG(LOG_ERROR, "Failed to set senders checksum coverage for UDPlite on socket %d", mSocketHandle);
			#endif
		case SOCKET_UDP:
			#ifdef LINUX
				tSent = sendto(mSocketHandle, pBuffer, (size_t)pBufferSize, MSG_NOSIGNAL, &tAddressDescriptor.sa, tAddressDescriptorSize);
			#endif
			#ifdef WIN32
				tSent = sendto(mSocketHandle, (const char*)pBuffer, (int)pBufferSize, 0, &tAddressDescriptor.sa, (int)tAddressDescriptorSize);
			#endif
			break;
		case SOCKET_TCP:

			//#########################
			//### re-bind if required
			//#########################
			if ((mConnectedHost != "") &&          (mConnectedPort != 0) &&
			   ((mConnectedHost != pTargetHost) || (mConnectedPort != pTargetPort)))
			{
				tLocalPort = mLocalPort;
				DestroySocket(mSocketHandle);
				if (CreateSocket())
					mLocalPort = BindSocket(mLocalPort, 0);
				if (mLocalPort != tLocalPort)
					LOG(LOG_INFO, "Re-bind socket %d to another port than used before", mSocketHandle);
				mIsConnected = false;
			}
			//#########################
			//### connect
			//#########################
			if (!mIsConnected)
			{
		        if (connect(mSocketHandle, &tAddressDescriptor.sa, tAddressDescriptorSize) < 0)
		        {
		            LOG(LOG_ERROR, "Failed to connect socket %d because of \"%s\"(%d)", mSocketHandle, strerror(errno), errno);
		            return false;
		        }else
		        {
		            mIsConnected = true;
		        	mConnectedHost = pTargetHost;
		        	mConnectedPort = pTargetPort;
		        }
			}
			//#########################
			//### send
			//#########################
			#ifdef LINUX
				tSent = send(mSocketHandle, pBuffer, (size_t)pBufferSize, MSG_NOSIGNAL);
			#endif
			#ifdef WIN32
				tSent = send(mSocketHandle, (const char*)pBuffer, (int)pBufferSize, 0);
			#endif
			break;
    }
    if (tSent < 0 )
        LOG(LOG_ERROR, "Error when sending data via socket %d because of \"%s\"(%d)", mSocketHandle, strerror(errno), errno);
    else
    {
        if (tSent < (int)pBufferSize)
        {
            LOG(LOG_ERROR, "Insufficient data on socket %d was sent", mSocketHandle);
        }else
        {
            #ifdef HBS_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Having sent %d bytes via socket %d to %s<%u>", tSent, mSocketHandle, pTargetHost.c_str(), pTargetPort);
            #endif
            tResult = true;
        }
    }

    return tResult;
}

bool Socket::Receive(string &pSourceHost, unsigned int &pSourcePort, void *pBuffer, ssize_t &pBufferSize)
{
    int                     tClientHandle;
    ssize_t                 tReceivedBytes = 0;
    SocketAddressDescriptor tAddressDescriptor;
	#ifdef LINUX
		socklen_t           tAddressDescriptorSize = sizeof(tAddressDescriptor.sa_stor);
	#endif
	#ifdef WIN32
		int                 tAddressDescriptorSize = sizeof(tAddressDescriptor.sa_stor);
	#endif
    bool                    tResult = false;
    bool                    tSourceIsIPv6 = false;
    int                     tUdpLiteChecksumCoverage = mUdpLiteChecksumCoverage;

    if (mSocketHandle == -1)
        return false;

    switch(mSocketTransportType)
    {
		case SOCKET_UDP_LITE:
			#ifdef LINUX
				if (setsockopt(mSocketHandle, IPPROTO_UDPLITE, UDPLITE_RECV_CSCOV, (__const void*)&tUdpLiteChecksumCoverage, sizeof(int)) != 0)
					LOG(LOG_ERROR, "Failed to set receivers checksum coverage for UDPlite on socket %d", mSocketHandle);
			#endif
		case SOCKET_UDP:
            /*
             * receive data
             */
			#ifdef LINUX
				tReceivedBytes = recvfrom(mSocketHandle, pBuffer, (size_t)pBufferSize, MSG_NOSIGNAL, &tAddressDescriptor.sa, &tAddressDescriptorSize);
			#endif
			#ifdef WIN32
				tReceivedBytes = recvfrom(mSocketHandle, (char*)pBuffer, pBufferSize, 0, &tAddressDescriptor.sa, &tAddressDescriptorSize);
			#endif
		    if (tReceivedBytes >= 0)
		    {
		        if (!GetAddrFromDescriptor(&tAddressDescriptor, pSourceHost, pSourcePort))
		            LOG(LOG_ERROR ,"Could not determine the source address for socket %d", mSocketHandle);
		    }
            break;
		case SOCKET_TCP:
            /*
             * activate listener
             */
            if (!mIsListening)
            {
                if (listen(mSocketHandle, MAX_INCOMING_CONNECTIONS) < 0)
                {
                    LOG(LOG_ERROR, "Failed to execute listen on socket %d because of \"%s\"", strerror(errno), mSocketHandle);
                    return false;
                }else
                    LOG(LOG_VERBOSE, "Started IPv%d-TCP listening on socket %d at local port %d", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, mSocketHandle, mLocalPort);
                mIsListening = true;
            }

            /*
             * wait for new connection
             */
            if (!mIsConnected)
            {
                if ((mTcpClientSockeHandle = accept(mSocketHandle, &tAddressDescriptor.sa, &tAddressDescriptorSize)) < 0)
                    LOG(LOG_ERROR, "Failed to accept connections on socket %d because of \"%s\"", mSocketHandle, strerror(errno));
                else
                {
                    LOG(LOG_VERBOSE, "Having new IPv%d-TCP client socket %d connection on socket %d at local port %d", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, mTcpClientSockeHandle, mSocketHandle, mLocalPort);
                    mIsConnected = true;

                    if (!GetAddrFromDescriptor(&tAddressDescriptor, mConnectedHost, mConnectedPort))
                        LOG(LOG_ERROR ,"Could not determine the source address for socket %d", mSocketHandle);
                }
            }

            /*
             * receive data
             */
            #ifdef LINUX
                tReceivedBytes = recv(mTcpClientSockeHandle, pBuffer, (size_t)pBufferSize, MSG_NOSIGNAL);
                if (tReceivedBytes <= 0)
                {
                    close(mTcpClientSockeHandle);
                    LOG(LOG_VERBOSE, "Client socket %d was closed", mTcpClientSockeHandle);
                    mIsConnected = false;
                }

            #endif
            #ifdef WIN32
                tReceivedBytes = recv(mTcpClientSockeHandle, (char*)pBuffer, pBufferSize, 0);
                if (tReceivedBytes <= 0)
                {
                    closesocket(mTcpClientSockeHandle);
                    LOG(LOG_VERBOSE, "Client socket %d was closed", mTcpClientSockeHandle);
                    mIsConnected = false;
                }
            #endif
            pSourceHost = mConnectedHost;
            pSourcePort = mConnectedPort;
			break;
    }

    // reset source description in case of receive error
    if (tReceivedBytes >= 0)
    {
        #ifdef HBS_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Received %d bytes via socket %d at local port %d of %s socket", tReceivedBytes, mSocketHandle, mLocalPort, TransportType2String(mSocketType).c_str());
        #endif
        tResult = true;
    }else
    {
    	pSourceHost = "0.0.0.0";
    	pSourcePort = 0;
    	LOG(LOG_ERROR, "Error when receiving data via socket %d at port %u because of \"%s\" (code: %d)", mSocketHandle, mLocalPort, strerror(errno), tReceivedBytes);
    }

    pBufferSize = tReceivedBytes;

    return tResult;
}

bool Socket::IsIPv6Supported()
{
    if (sIPv6Supported == -1)
    {
        #ifdef WIN32
            int tMajor, tMinor;
            if ((!System::GetWindowsKernelVersion(tMajor, tMinor)) || (tMajor < 6))
            {
                // no IPv6 dual stack support in Windows XP/2k/2k3 -> disable IPv6 support at all
                LOGEX(Socket, LOG_ERROR, "Detected Windows version is too old (older than Vista) and lacks IPv6/IPv4 dual stack support, disabling IPv6 support");
                return false;
            }
            WORD tVersion = 0x0202; // requesting version 2.2
            WSADATA tWsa;

            WSAStartup(tVersion, &tWsa);
        #endif

        int tHandle = 0;

        if ((tHandle = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) > 0)
        {
            LOGEX(Socket, LOG_INFO, ">>> IPv6 sockets available <<<");
            #ifdef LINUX
                close(tHandle);
            #endif
            #ifdef WIN32
                closesocket(tHandle);
                // no WSACleanup() here, it would lead to a crash because of static context
            #endif
            sIPv6Supported = true;
        }else
        {
            LOGEX(Socket, LOG_INFO, ">>> IPv6 not supported, falling back to IPv4 <<<");
            sIPv6Supported = false;
        }
    }

    return (sIPv6Supported == true);
}

void Socket::DisableIPv6Support()
{
	sIPv6Supported = false;
}

bool Socket::IsUDPliteSupported()
{
    if (sUDPliteSupported == -1)
    {
        #ifdef WIN32
            sUDPliteSupported = false;
        #endif

        #ifdef LINUX
            int tHandle = 0;

            if ((tHandle = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDPLITE)) > 0)
            {
                LOGEX(Socket, LOG_INFO, ">>> UDPlite sockets available <<<");
                close(tHandle);
                sUDPliteSupported = true;
            }else
            {
                LOGEX(Socket, LOG_INFO, ">>> UDPlite not supported, falling back to UDP <<<");
                sUDPliteSupported = false;
            }
        #endif
    }

    return (sUDPliteSupported == true);
}

void Socket::DisableUDPliteSupport()
{
    sUDPliteSupported = false;
}

bool Socket::IsDCCPSupported()
{
    if (sDCCPSupported == -1)
    {
        #ifdef WIN32
            sDCCPSupported = false;
        #endif

        #ifdef LINUX
            int tHandle = 0;

            if ((tHandle = socket(AF_INET6, SOCK_DGRAM, IPPROTO_DCCP)) > 0)
            {
                LOGEX(Socket, LOG_INFO, ">>> DCCP sockets available <<<");
                close(tHandle);
                sDCCPSupported = true;
            }else
            {
                LOGEX(Socket, LOG_INFO, ">>> DCCP not supported, falling back to UDP <<<");
                sDCCPSupported = false;
            }
        #endif
    }

    return (sDCCPSupported == true);
}

void Socket::DisableDCCPSupport()
{
    sDCCPSupported = false;
}

bool Socket::FillAddrDescriptor(string pHost, unsigned int pPort, SocketAddressDescriptor *tAddressDescriptor, unsigned int &tAddressDescriptorSize)
{
    // does pTargetHost contain a ':' char => we have an IPv6 based target
    bool tHostIsIPv6 = (pHost.find(':') != string::npos);

    //LOG(LOG_VERBOSE, "Sending %d bytes via socket %d towards host %s and port %d", pBufferSize, mSocketHandle, pTargetHost.c_str(), pTargetPort);
    if (tHostIsIPv6)
    {
        // Internet/IP type
        tAddressDescriptor->sa_in6.sin6_family = AF_INET6;

        // transform address
        if (inet_pton(AF_INET6, pHost.c_str(), &tAddressDescriptor->sa_in6.sin6_addr) < 0)
        {
            LOGEX(Socket, LOG_ERROR, "Error in inet-pton(IPv6) because of %s", strerror(errno));
            return false;
        }

        // port
        tAddressDescriptor->sa_in6.sin6_port = htons((unsigned short int)pPort);

        // flow related information = should be zero until its usage is specified
        tAddressDescriptor->sa_in6.sin6_flowinfo = 0;

        // scope id
        tAddressDescriptor->sa_in6.sin6_scope_id = 0;

        tAddressDescriptorSize = sizeof(tAddressDescriptor->sa_in6);
    }else
    {
        // zeros
        memset(&tAddressDescriptor->sa_in.sin_zero, 0, 8);

        // Internet/IP type
        tAddressDescriptor->sa_in.sin_family = AF_INET;

        // transform address
        //tAddressDescriptor->sa_in.sin_addr.s_addr = inet_addr(pHost.c_str());
        if (inet_pton(AF_INET, pHost.c_str(), &tAddressDescriptor->sa_in.sin_addr.s_addr) < 0)
        {
            LOGEX(Socket, LOG_ERROR, "Error in inet-pton(IPv4) because of %s", strerror(errno));
            return false;
        }

        // port
        tAddressDescriptor->sa_in.sin_port = htons((unsigned short int)pPort);

        tAddressDescriptorSize = sizeof(tAddressDescriptor->sa_in);
    }
    return true;
}

bool Socket::GetAddrFromDescriptor(SocketAddressDescriptor *tAddressDescriptor, string &pHost, unsigned int &pPort)
{
    char tSourceHostStr[INET6_ADDRSTRLEN];
    switch (tAddressDescriptor->sa_stor.ss_family)
    {
        default:
        case AF_INET:
            if (inet_ntop(AF_INET, &tAddressDescriptor->sa_in.sin_addr, tSourceHostStr, INET_ADDRSTRLEN /* is always smaller than INET6_ADDRSTRLEN */) == NULL)
            {
                LOGEX(Socket, LOG_ERROR, "Error in inet_ntop(IPv4) because of %s", strerror(errno));
                return false;
            }
            pPort = (unsigned int)ntohs(tAddressDescriptor->sa_in.sin_port);
            break;
        case AF_INET6:
            if (inet_ntop(AF_INET6, &tAddressDescriptor->sa_in6.sin6_addr, tSourceHostStr, INET6_ADDRSTRLEN) == NULL)
            {
                LOGEX(Socket, LOG_ERROR, "Error in inet_ntop(IPv6) because of %s", strerror(errno));
                return false;
            }
            pPort = (unsigned int)ntohs(tAddressDescriptor->sa_in6.sin6_port);
            break;
    }
    if (tSourceHostStr != NULL)
    {
        pHost = string(tSourceHostStr);
        // IPv4 in IPv6 address?
        if ((pHost.find(':') != string::npos) && (pHost.find('.') != string::npos))
             pHost.erase(0, pHost.rfind(':') + 1);
    }else
        pHost = "";

    return true;
}

///////////////////////////////////////////////////////////////////////////////
/// helper functions
///////////////////////////////////////////////////////////////////////////////

bool Socket::CreateSocket(enum NetworkType pIpVersion)
{
    int tSelectedIPDomain = 0;
    bool tResult = false;

    #ifdef WIN32
        unsigned long int nonBlockingMode = 0; // blocking mode
        BOOL tNewBehaviour = false;
        DWORD tBytesReturned = 0;
        WORD tVersion = 0x0202; // requesting version 2.2
        WSADATA tWsa;

        WSAStartup(tVersion, &tWsa);
    #endif

    LOG(LOG_VERBOSE, "Creating socket for IPv%d, transport type %s", (pIpVersion == SOCKET_IPv6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str());

    switch(pIpVersion)
    {
        case SOCKET_IPv4:
            // create IPv4 only socket
            tSelectedIPDomain = PF_INET;
            break;

        case SOCKET_IPv6:
            if (IsIPv6Supported())
            {
                // create IPv4/IPv6 compatible socket
                tSelectedIPDomain = PF_INET6;
            }else
            {
                // falls back to IPv4
            	LOG(LOG_ERROR, "IPv6 not supported by system, will create IPv4 socket instead");
                tSelectedIPDomain = PF_INET;
            }
            break;
        default:
            break;
    }

    switch(mSocketTransportType)
    {
        case SOCKET_UDP_LITE:
            #ifdef LINUX
                if (IsUDPliteSupported())
                {
                    if ((mSocketHandle = socket(tSelectedIPDomain, SOCK_DGRAM, IPPROTO_UDPLITE)) < 0)
                        LOG(LOG_ERROR, "Could not create UDPlite socket");
                    else
                        tResult = true;
                    break;
                }
            #else
                LOG(LOG_ERROR, "UDPlite is not supported by Windows API, a common UDP socket will be used instead");
            #endif
        case SOCKET_UDP:
            if ((mSocketHandle = socket(tSelectedIPDomain, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                LOG(LOG_ERROR, "Could not create UDP socket");
            else
                tResult = true;
            #ifdef WIN32
                if (ioctlsocket(mSocketHandle, FIONBIO, &nonBlockingMode))
                {
                    LOG(LOG_ERROR, "Failed to set blocking-mode for socket %d", mSocketHandle);
                    tResult = false;
                }
//                #ifndef SIO_UDP_CONNRESET
//                #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
//                #endif
//                // Hint: http://support.microsoft.com/default.aspx?scid=kb;en-us;263823
//                //      set behavior of socket by disabling SIO_UDP_CONNRESET
//                //      without this the recvfrom() can fail, repeatedly, after a bad sendto() call
//                if (WSAIoctl(mSocketHandle, SIO_UDP_CONNRESET, &tNewBehaviour, sizeof(tNewBehaviour), NULL, 0, &tBytesReturned, NULL, NULL) < 0)
//                {
//                    LOG(LOG_ERROR, "Failed to set SIO_UDP_CONNRESET on UDP socket %d", mSocketHandle);
//                    tResult = false;
//                }
            #endif
            break;
        case SOCKET_TCP:
            if ((mSocketHandle = socket(tSelectedIPDomain, SOCK_STREAM, IPPROTO_TCP)) < 0)
                LOG(LOG_ERROR, "Could not create TCP socket");
            else
                tResult = true;
            #ifdef WIN32
                if (ioctlsocket(mSocketHandle, FIONBIO, &nonBlockingMode))
                {
                    LOG(LOG_ERROR, "Failed to set blocking-mode for socket %d", mSocketHandle);
                    tResult = false;
                }
            #endif
            break;
    }

    if (tResult)
    {
        LOG(LOG_VERBOSE, "Created IPv%d-%s socket with handle number %d", (tSelectedIPDomain == PF_INET6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str(), mSocketHandle);
        if (tSelectedIPDomain == PF_INET6)
        {
            // we force hybrid sockets, otherwise Windows will complain: http://msdn.microsoft.com/en-us/library/bb513665%28v=vs.85%29.aspx
            int tOnlyIpv6Sockets = false;
            bool tIpv6OnlyOkay = false;
			#ifdef LINUX
				tIpv6OnlyOkay = (setsockopt(mSocketHandle, IPPROTO_IPV6, IPV6_V6ONLY, (__const void*)&tOnlyIpv6Sockets, sizeof(tOnlyIpv6Sockets)) == 0);
			#endif
			#ifdef WIN32
                tIpv6OnlyOkay = (setsockopt(mSocketHandle, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&tOnlyIpv6Sockets, sizeof(tOnlyIpv6Sockets)) == 0);
			#endif
            if (tIpv6OnlyOkay)
                LOG(LOG_VERBOSE, "Set %s socket with handle number %d to IPv6only state %d", TransportType2String(mSocketTransportType).c_str(), mSocketHandle, tOnlyIpv6Sockets);
            else
                LOG(LOG_ERROR, "Failed to disable IPv6_only");
            mSocketNetworkType = SOCKET_IPv6;
        }else
            mSocketNetworkType = SOCKET_IPv4;
    }

    return tResult;
}

void Socket::DestroySocket(int pHandle)
{
    if (pHandle > 0)
    {
        #ifdef LINUX
            close(pHandle);
        #endif
        #ifdef WIN32
            closesocket(pHandle);
            WSACleanup();
        #endif
    }
}

unsigned short int Socket::BindSocket(unsigned int pPort, unsigned int pProbeStepping, unsigned int pHighesPossibleListenerPort)
{
    unsigned short int  tResult = 0;
    struct sockaddr_in  tAddressDescriptor4;
    struct sockaddr_in6 tAddressDescriptor6;
    struct sockaddr     *tAddressDescriptor;
    int                 tAddressDescriptorSize;

    if (mSocketHandle == -1)
        return 0;

    LOG(LOG_VERBOSE, "Trying to bind IPv%d-%s socket %d to local port %d", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str(), mSocketHandle, pPort);

    if (mSocketNetworkType == SOCKET_IPv6)
    {
        // Internet/IP type
        tAddressDescriptor6.sin6_family = AF_INET6;

        // listen on all interfaces and addresses
        tAddressDescriptor6.sin6_addr = in6addr_any;

        // port
        tAddressDescriptor6.sin6_port = htons((uint16_t)pPort);

        // flow related information = should be zero until its usage is specified
        tAddressDescriptor6.sin6_flowinfo = 0;

        tAddressDescriptor6.sin6_scope_id = 0;

        tAddressDescriptor = (sockaddr*)&tAddressDescriptor6;
        tAddressDescriptorSize = sizeof(tAddressDescriptor6);
    }else
    {
        // Internet/IP type
        tAddressDescriptor4.sin_family = AF_INET;

        // listen on all interfaces and addresses
        tAddressDescriptor4.sin_addr.s_addr = INADDR_ANY;

        // port
        tAddressDescriptor4.sin_port = htons((uint16_t)pPort);

        tAddressDescriptor = (sockaddr*)&tAddressDescriptor4;
        tAddressDescriptorSize = sizeof(tAddressDescriptor4);
    }

    // data port: search for the next free port and bind to it
    while (bind(mSocketHandle, tAddressDescriptor, tAddressDescriptorSize) < 0)
    {
        if (!pProbeStepping)
        {
            LOG(LOG_ERROR, "Failed to bind IPv%d-%s socket %d to port %d while auto probing is off, error occurred because of \"%s\"", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str(), mSocketHandle, pPort, strerror(errno));
            return 0;
        }

        LOG(LOG_INFO, "Failed to bind IPv%d-%s socket %d to local port %d because \"%s\", will try next alternative", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str(), mSocketHandle, pPort, strerror(errno));
        pPort += pProbeStepping;
        if ((pPort > 65535) || ((pPort > pHighesPossibleListenerPort) && (pHighesPossibleListenerPort != 0)))
        {
            LOG(LOG_ERROR, "Auto-probing for port binding failed, no further port numbers allowed");
            pPort = 0;
            break;
        }

        if (mSocketNetworkType == SOCKET_IPv6)
            tAddressDescriptor6.sin6_port = htons((uint16_t)pPort);
        else
            tAddressDescriptor4.sin_port = htons((uint16_t)pPort);
    }
    tResult = pPort;

    if (tResult)
        LOG(LOG_VERBOSE, "Bound IPv%d-%s socket %d to local port %d", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str(), mSocketHandle, tResult);

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
