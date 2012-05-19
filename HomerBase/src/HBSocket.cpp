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
 * Purpose: Implementation of wrapper for os independent socket handling
 * Author:  Thomas Volkert
 * Since:   2010-09-22
 */

#include <Logger.h>
#include <HBSocket.h>
#include <HBSocketControlService.h>
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

#ifdef QOS_SETTINGS
    int sQoSSupported = true;
#else
    int sQoSSupported = false;
#endif
int sIPv6Supported = -1;
int sUDPliteSupported = -1;
int sDCCPSupported = -1;
int sSCTPSupported = -1;

///////////////////////////////////////////////////////////////////////////////

void Socket::SetDefaults(enum TransportType pTransportType)
{
    mIsConnected = false;
	mIsListening = false;
	mIsClientSocket = false;
	mSocketNetworkType = SOCKET_NETWORK_TYPE_INVALID;
	mSocketTransportType = SOCKET_TRANSPORT_TYPE_INVALID;
    mLocalPort = 0;
    mLocalHost = "";
    mSocketHandle = -1;
    mTcpClientSockeHandle = -1;
    mPeerHost = "";
    mPeerPort = 0;
    mUdpLiteChecksumCoverage = UDP_LITE_HEADER_SIZE;

    #if defined(WIN32) ||defined(WIN64) || defined(APPLE) || defined(BSD)
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
Socket* Socket::CreateServerSocket(enum NetworkType pIpVersion, enum TransportType pTransportType, unsigned int pListenerPort, bool pReusable, unsigned int pProbeStepping, unsigned int pHighestPossibleListenerPort)
{
    Socket* tResult = NULL;

    tResult = new Socket(pIpVersion, pTransportType, pListenerPort, pReusable, pProbeStepping, pHighestPossibleListenerPort);

    if (tResult == NULL)
        return NULL;

    if (tResult->mSocketHandle == -1)
    {
        delete tResult;
        return NULL;
    }

    LOGEX(Socket, LOG_VERBOSE, "Created %s-server socket at local address %s:%u with receive buffer size of %d bytes", TransportType2String(pTransportType).c_str(), tResult->mLocalHost.c_str(), tResult->mLocalPort, tResult->GetReceiveBufferSize());

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////
/// client socket
///////////////////////////////////////////////////////////////////////////////
Socket* Socket::CreateClientSocket(enum NetworkType pIpVersion, enum TransportType pTransportType, unsigned int pSenderPort, bool pReusable, unsigned int pProbeStepping, unsigned int pHighestPossibleSenderPort)
{
    Socket* tResult = NULL;

    tResult = new Socket(pIpVersion, pTransportType, pSenderPort, pReusable, pProbeStepping, pHighestPossibleSenderPort);

    if (tResult == NULL)
        return NULL;

    if (tResult->mSocketHandle == -1)
    {
        delete tResult;
        return NULL;
    }

    LOGEX(Socket, LOG_VERBOSE, "Created %s-client socket at local address %s:%u with receive buffer size of %d bytes", TransportType2String(pTransportType).c_str(), tResult->mLocalHost.c_str(), tResult->mLocalPort, tResult->GetReceiveBufferSize());

    tResult->mIsClientSocket = true;
    SVC_SOCKET_CONTROL.RegisterClientSocket(tResult);

    return tResult;
}

Socket::Socket(enum NetworkType pIpVersion, enum TransportType pTransportType, unsigned int pPort, bool pReusable, unsigned int pProbeStepping, unsigned int pHighestPossiblePort)
{
    if ((pIpVersion == SOCKET_IPv6) && (!IsIPv6Supported()))
    {
        LOG(LOG_ERROR, "IPv6 not supported by system, falling back to IPv4");
        pIpVersion = SOCKET_IPv4;
    }

    if ((pTransportType == SOCKET_UDP_LITE) && (!IsTransportSupported(SOCKET_UDP_LITE)))
    {
        LOG(LOG_ERROR, "UDPlite not supported by system, falling back to UDP");
        pTransportType = SOCKET_UDP;
    }

    if ((pTransportType == SOCKET_DCCP) && (!IsTransportSupported(SOCKET_DCCP)))
    {
        LOG(LOG_ERROR, "DCCP not supported by system, falling back to UDP");
        pTransportType = SOCKET_UDP;
    }

    if ((pTransportType == SOCKET_SCTP) && (!IsTransportSupported(SOCKET_SCTP)))
    {
        LOG(LOG_ERROR, "SCTP not supported by system, falling back to UDP");
        pTransportType = SOCKET_UDP;
    }

    SetDefaults(pTransportType);

    if (CreateSocket(pIpVersion))
    {
        if(pReusable)
            EnableReuse(pReusable);

        if (!BindSocket(pPort, pProbeStepping, pHighestPossiblePort))
            mSocketHandle = -1;
        if ((!pProbeStepping) && (mLocalPort != pPort) && (pPort != 0))
            LOG(LOG_ERROR, "Bound socket %d to another port than requested", mSocketHandle);
    }
}

Socket::~Socket()
{
    if (mIsClientSocket)
        SVC_SOCKET_CONTROL.UnregisterClientSocket(this);

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
        case SOCKET_DCCP:
            return "DCCP";
        case SOCKET_SCTP:
            return "SCTP";
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
    if ((pTypeStr == "DCCP") || (pTypeStr == "dccp"))
        return SOCKET_DCCP;
    if ((pTypeStr == "SCTP") || (pTypeStr == "sctp"))
        return SOCKET_SCTP;
    return SOCKET_TRANSPORT_TYPE_INVALID;
}

string Socket::NetworkType2String(enum NetworkType pSocketType)
{
    switch(pSocketType)
    {
        case SOCKET_IPv4:
            return "IPv4";
        case SOCKET_IPv6:
            return "IPv6";
        default:
            return "N/A";
    }
}

enum NetworkType Socket::String2NetworkType(std::string pTypeStr)
{
    if ((pTypeStr == "IPv4") || (pTypeStr == "ipv4"))
        return SOCKET_IPv4;
    if ((pTypeStr == "IPv6") || (pTypeStr == "ipv6"))
        return SOCKET_IPv6;
    return SOCKET_NETWORK_TYPE_INVALID;
}

std::string Socket::GetName()
{
    string tResult = "";

    tResult = mLocalHost + "<" + toString(mLocalPort) + ">(" + TransportType2String(mSocketTransportType) + ")";

    return tResult;
}

std::string Socket::GetPeerName()
{
    string tResult = "";

    mPeerDataMutex.lock();
    tResult = mPeerHost + "<" + toString(mPeerPort) + ">(" + TransportType2String(mSocketTransportType) + ")";
    mPeerDataMutex.unlock();

    return tResult;
}

bool Socket::EnableReuse(bool pActive)
{
    bool tResult = false;

	#if defined(APPLE) || defined(BSD)
		int tReuseOption = SO_REUSEPORT;
	#endif
	#if defined(LINUX) || defined(WIN32) || defined (WIN64)
		int tReuseOption = SO_REUSEADDR;
	#endif

	LOG(LOG_WARN, "Allow reusing of local port %u for future sockets", mLocalPort);

	// activate reuse of socket
	int tReuseOptionValue = pActive;
	if (setsockopt(mSocketHandle, SOL_SOCKET, tReuseOption, (char*)&tReuseOptionValue, sizeof(tReuseOptionValue)) < 0)
		LOG(LOG_ERROR, "Failed to set socket option SO_REUSEADDR/SO_REUSEPORT on socket %d", mSocketHandle);
	else
		tResult = true;

	return tResult;
}

enum NetworkType Socket::GetNetworkType()
{
    return mSocketNetworkType;
}

enum TransportType Socket::GetTransportType()
{
    return mSocketTransportType;
}

unsigned int Socket::GetLocalPort()
{
    return mLocalPort;
}

std::string Socket::GetLocalHost()
{
    return mLocalHost;
}

unsigned int Socket::GetPeerPort()
{
    return mPeerPort;
}

std::string Socket::GetPeerHost()
{
    string tResult;

    mPeerDataMutex.lock();
    tResult = mPeerHost;
    mPeerDataMutex.unlock();

    return tResult;
}

bool Socket::SetQoS(const QoSSettings &pQoSSettings)
{
    LOG(LOG_VERBOSE, "Desired QoS: %u KB/s min. data rate, %u ms max. delay, features: 0x%hX", pQoSSettings.DataRate, pQoSSettings.Delay, pQoSSettings.Features);

    mQoSSettings = pQoSSettings;

    if (IsQoSSupported())
    {
        setqos(mSocketHandle, pQoSSettings.DataRate, pQoSSettings.Delay, pQoSSettings.Features);
    }else
        LOG(LOG_WARN, "QoS support deactivated, settings will be ignored");

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

    LOGEX(Socket, LOG_VERBOSE, "Creating QoS profile %s with parameters: %u KB/s min. data rate, %hu ms max. delay", pQoSSettings.DataRate, pQoSSettings.Delay);

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

void Socket::UDPLiteSetCheckLength(int pBytes)
{
    /* Checksum coverage:
             Sender's side:
                 It defines how many bytes of the header are included in the checksum calculation
             Receiver's side:
                 It defines how many bytes should be at least included in the checksum calculation at sender's side.
                 If a packet's value is below the packet is silently discarded by the Linux kernel.
                 Moreover, packets with a checksum coverage of zero will be discarded in every case!
     */
    if (mSocketTransportType != SOCKET_UDP_LITE)
    {
        LOG(LOG_WARN, "Socket is not an UDPLite socket, will ignore the new value for the check length");
        return;
    }
    mUdpLiteChecksumCoverage = pBytes;
}

void Socket::TCPDisableNagle()
{
    if (mSocketTransportType != SOCKET_TCP)
    {
        LOG(LOG_WARN, "Socket is not a TCP socket, will ignore the request for disabling Nagle's algorithm");
        return;
    }

    LOG(LOG_VERBOSE, "Disabling NAgle's algorithm");
    int tFlag = 1;
    if (setsockopt(mSocketHandle, IPPROTO_TCP, TCP_NODELAY, (char*)&tFlag, sizeof(tFlag)) < 0)
        LOG(LOG_ERROR, "Failed to disable Nagle's algorithm for TCP on socket %d", mSocketHandle);
}


bool Socket::Send(string pTargetHost, unsigned int pTargetPort, void *pBuffer, ssize_t pBufferSize)
{
    SocketAddressDescriptor   tAddressDescriptor;
    unsigned int        tAddressDescriptorSize;
    bool                tTcpNeedsReconnect = false;
    int                 tSent = 0;
    bool                tResult = false;
    unsigned short int  tLocalPort = 0;
    bool                tTargetIsIPv6 = IS_IPV6_ADDRESS(pTargetHost);
    int                 tUdpLiteChecksumCoverage = mUdpLiteChecksumCoverage;

    if (mSocketHandle == -1)
        return false;

    //LOG(LOG_VERBOSE, "Try to send %d bytes via socket %d to %s<%u>", (int)pBufferSize, mSocketHandle, pTargetHost.c_str(), pTargetPort);

    if (!FillAddrDescriptor(pTargetHost, pTargetPort, &tAddressDescriptor, tAddressDescriptorSize))
    {
        LOG(LOG_ERROR ,"Could not process the target address of socket %d", mSocketHandle);
        return false;
    }

    switch(mSocketTransportType)
    {
		case SOCKET_UDP_LITE:
            #if defined(LINUX) || defined(APPLE) || defined(BSD)
		        LOG(LOG_VERBOSE, "Setting UDPlite checksum coverage to %d", tUdpLiteChecksumCoverage);
				if (setsockopt(mSocketHandle, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV, (char*)&tUdpLiteChecksumCoverage, sizeof(tUdpLiteChecksumCoverage)) < 0)
					LOG(LOG_ERROR, "Failed to set senders checksum coverage for UDPlite on socket %d", mSocketHandle);
			#endif
		case SOCKET_UDP:
		    mPeerDataMutex.lock();
		    mPeerHost = pTargetHost;
		    mPeerPort = pTargetPort;
		    mPeerDataMutex.unlock();
            #if defined(LINUX)
				tSent = sendto(mSocketHandle, pBuffer, (size_t)pBufferSize, MSG_NOSIGNAL, &tAddressDescriptor.sa, tAddressDescriptorSize);
			#endif
            #if defined(APPLE) || defined(BSD)
                tSent = sendto(mSocketHandle, pBuffer, (size_t)pBufferSize, 0, &tAddressDescriptor.sa, tAddressDescriptorSize);
            #endif
			#if defined(WIN32) ||defined(WIN64)
				tSent = sendto(mSocketHandle, (const char*)pBuffer, (int)pBufferSize, 0, &tAddressDescriptor.sa, (int)tAddressDescriptorSize);
			#endif
			break;
		case SOCKET_TCP:

			//#########################
			//### re-bind if required
			//#########################
		    mPeerDataMutex.lock();
			if ((mPeerHost != "") && (mPeerPort != 0) && ((mPeerHost != pTargetHost) || (mPeerPort != pTargetPort)))
			    tTcpNeedsReconnect = true;
            mPeerDataMutex.unlock();

            if (tTcpNeedsReconnect)
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
		            LOG(LOG_ERROR, "Failed to connect socket %d because \"%s\"(%d)", mSocketHandle, strerror(errno), errno);
		            return false;
		        }else
		        {
		            mIsConnected = true;
		            mPeerDataMutex.lock();
		        	mPeerHost = pTargetHost;
		        	mPeerPort = pTargetPort;
		        	mPeerDataMutex.unlock();
		        }
			}
			//#########################
			//### send
			//#########################
            #if defined(LINUX)
				tSent = send(mSocketHandle, pBuffer, (size_t)pBufferSize, MSG_NOSIGNAL);
			#endif
            #if defined(APPLE) || defined(BSD)
                tSent = send(mSocketHandle, pBuffer, (size_t)pBufferSize, 0);
            #endif
			#if defined(WIN32) ||defined(WIN64)
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
                LOG(LOG_VERBOSE, "Sent %d bytes via socket %d to %s<%u>", tSent, mSocketHandle, pTargetHost.c_str(), pTargetPort);
            #endif
            tResult = true;
        }
    }

    return tResult;
}

bool Socket::Receive(string &pSourceHost, unsigned int &pSourcePort, void *pBuffer, ssize_t &pBufferSize)
{
    ssize_t                 tReceivedBytes = 0;
    SocketAddressDescriptor tAddressDescriptor;
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		socklen_t           tAddressDescriptorSize = sizeof(tAddressDescriptor.sa_stor);
	#endif
	#if defined(WIN32) || defined(WIN64)
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
            #if defined(LINUX) || defined(APPLE) || defined(BSD)
				if (setsockopt(mSocketHandle, IPPROTO_UDPLITE, UDPLITE_RECV_CSCOV, (char*)&tUdpLiteChecksumCoverage, sizeof(tUdpLiteChecksumCoverage)) != 0)
					LOG(LOG_ERROR, "Failed to set receivers checksum coverage for UDPlite on socket %d", mSocketHandle);
			#endif
            // continue as it was UDP receiving
		case SOCKET_UDP:
            /*
             * receive data
             */
            #if defined(LINUX)
				tReceivedBytes = recvfrom(mSocketHandle, pBuffer, (size_t)pBufferSize, MSG_NOSIGNAL, &tAddressDescriptor.sa, &tAddressDescriptorSize);
			#endif
            #if defined(APPLE) || defined(BSD)
                tReceivedBytes = recvfrom(mSocketHandle, pBuffer, (size_t)pBufferSize, 0, &tAddressDescriptor.sa, &tAddressDescriptorSize);
            #endif
			#if defined(WIN32) ||defined(WIN64)
				tReceivedBytes = recvfrom(mSocketHandle, (char*)pBuffer, (int)pBufferSize, 0, &tAddressDescriptor.sa, &tAddressDescriptorSize);
			#endif
		    if (tReceivedBytes >= 0)
		    {
		        mPeerDataMutex.lock();
		    	mPeerHost = GetAddrFromDescriptor(&tAddressDescriptor, &mPeerPort);
		    	if (mPeerHost == "")
		            LOG(LOG_ERROR ,"Could not determine the UDP/UDPLite source address for socket %d", mSocketHandle);
		    	mPeerDataMutex.unlock();
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
                if ((mTcpClientSockeHandle = (int)accept(mSocketHandle, &tAddressDescriptor.sa, &tAddressDescriptorSize)) < 0)
                    LOG(LOG_ERROR, "Failed to accept connections on socket %d because of \"%s\"", mSocketHandle, strerror(errno));
                else
                {
                    LOG(LOG_VERBOSE, "Having new IPv%d-TCP client socket %d connection on socket %d at local port %d", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, mTcpClientSockeHandle, mSocketHandle, mLocalPort);
                    mIsConnected = true;

                    mPeerDataMutex.lock();
    		    	mPeerHost = GetAddrFromDescriptor(&tAddressDescriptor, &mPeerPort);
    		    	if (mPeerHost == "")
                        LOG(LOG_ERROR ,"Could not determine the TCP source address for socket %d", mSocketHandle);
    		    	mPeerDataMutex.unlock();
                }
            }

            /*
             * receive data
             */
            #if defined(LINUX)
                tReceivedBytes = recv(mTcpClientSockeHandle, pBuffer, (size_t)pBufferSize, MSG_NOSIGNAL);
            #endif
            #if defined(APPLE) || defined(BSD)
                tReceivedBytes = recv(mTcpClientSockeHandle, pBuffer, (size_t)pBufferSize, 0);
            #endif
            #if defined(WIN32) ||defined(WIN64)
                tReceivedBytes = recv(mTcpClientSockeHandle, (char*)pBuffer, (int)pBufferSize, 0);
            #endif
            if (tReceivedBytes < 0)
            {
                DestroySocket(mTcpClientSockeHandle);
                LOG(LOG_VERBOSE, "Client TCP socket %d was closed", mTcpClientSockeHandle);
                mIsConnected = false;
            }
			break;
    }

    // reset source description in case of receive error
    if (tReceivedBytes >= 0)
    {
        mPeerDataMutex.lock();
        pSourceHost = mPeerHost;
        pSourcePort = mPeerPort;
        mPeerDataMutex.unlock();
        #ifdef HBS_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Received %d bytes via socket %d at local port %d of %s socket", tReceivedBytes, mSocketHandle, mLocalPort, TransportType2String(mSocketTransportType).c_str());
        #endif
        tResult = true;

        if (tReceivedBytes == pBufferSize)
        {
            LOG(LOG_WARN, "Full buffer size was used, maybe given application buffer is too small?");
        }
    }else
    {
    	pSourceHost = "0.0.0.0";
    	pSourcePort = 0;
    	LOG(LOG_ERROR, "Error when receiving data via socket %d at port %u because of \"%s\" (code: %d)", mSocketHandle, mLocalPort, strerror(errno), tReceivedBytes);
    }

    pBufferSize = tReceivedBytes;

    return tResult;
}

int Socket::GetReceiveBufferSize()
{
    int tResult = -1;
    socklen_t tResultSize = sizeof(tResult);

    if(mSocketHandle != -1)
    {
        int tCallRes = getsockopt(mSocketHandle, SOL_SOCKET, SO_RCVBUF, (char *)&tResult, &tResultSize);
        if (tCallRes < 0)
            LOG(LOG_ERROR, "Failed to get receive buffer size on socket %d", mSocketHandle);
        else
            LOG(LOG_VERBOSE, "Determined receive buffer size with %d bytes on socket %d", tResult, mSocketHandle);
    }

    return tResult;
}

bool Socket::SetReceiveBufferSize(int pSize)
{
    bool tResult = false;

    if(mSocketHandle != -1)
    {
        LOG(LOG_WARN, "Setting receive buffer size to %d bytes on socket %d", pSize, mSocketHandle);

        if (setsockopt(mSocketHandle, SOL_SOCKET, SO_RCVBUF, (char*)&pSize, sizeof(pSize)) < 0)
            LOG(LOG_ERROR, "Failed to get receive buffer size on socket %d", mSocketHandle);
        else
            tResult = true;
    }else
        LOG(LOG_ERROR, "Socket is invalid");

    return tResult;
}

bool Socket::IsQoSSupported()
{
    return (sQoSSupported == 1);
}

void Socket::DisableQoSSupport()
{
    printf("QoS support disabled\n");
    sQoSSupported = false;
}

bool Socket::IsIPv6Supported()
{
    if (sIPv6Supported == -1)
    {
        #if defined(WIN32) ||defined(WIN64)
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

        if ((tHandle = (int)socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) > 0)
        {
            LOGEX(Socket, LOG_INFO, ">>> IPv6 sockets available <<<");
            #if defined(LINUX) || defined(APPLE) || defined(BSD)
                close(tHandle);
            #endif
            #if defined(WIN32) ||defined(WIN64)
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

    return (sIPv6Supported == 1);
}

void Socket::DisableIPv6Support()
{
    printf("IPv6 support disabled\n");
	sIPv6Supported = false;
}

bool Socket::IsTransportSupported(enum TransportType pType)
{
    bool tResult = false;

    switch(pType)
    {
        case SOCKET_UDP:
            tResult = true;
            break;

        case SOCKET_TCP:
            tResult = true;
            break;

        case SOCKET_UDP_LITE:
            if (sUDPliteSupported == -1)
            {
                #if defined(WIN32) || defined(WIN64) || defined(APPLE) || defined(BSD)
                    sUDPliteSupported = false;
                #endif

                #if defined(LINUX)
                    int tHandle = 0;

                    if ((tHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDPLITE)) > 0)
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
            tResult = (sUDPliteSupported == 1);
            break;

        case SOCKET_DCCP:
            if (sDCCPSupported == -1)
            {
                #if defined(WIN32) || defined(WIN64) || defined(APPLE) || defined(BSD)
                    sDCCPSupported = false;
                #endif

                #if defined(LINUX)
                    int tHandle = 0;

                    if ((tHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_DCCP)) > 0)
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
            tResult = (sDCCPSupported == 1);
            break;

        case SOCKET_SCTP:
            if (sSCTPSupported == -1)
            {
                #if defined(WIN32) || defined(WIN64) || defined(APPLE) || defined(BSD)
                    sSCTPSupported = false;
                #endif

                #if defined(LINUX)
                    int tHandle = 0;

                    if ((tHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_SCTP)) > 0)
                    {
                        LOGEX(Socket, LOG_INFO, ">>> SCTP sockets available <<<");
                        close(tHandle);
                        sSCTPSupported = true;
                    }else
                    {
                        LOGEX(Socket, LOG_INFO, ">>> SCTP not supported, falling back to UDP <<<");
                        sSCTPSupported = false;
                    }
                #endif
            }
            tResult = (sSCTPSupported == 1);
            break;

        default:
        case SOCKET_TRANSPORT_TYPE_INVALID:
            LOGEX(Socket, LOG_ERROR, "Unsupported or invalid protocol type");
            break;
    }

    return tResult;
}

void Socket::DisableTransportSupport(enum TransportType pType)
{
    switch(pType)
    {
        case SOCKET_UDP:
            LOGEX(Socket, LOG_ERROR, "We don't do strange things");
            break;
        case SOCKET_TCP:
            LOGEX(Socket, LOG_ERROR, "We don't do strange things");
            break;
        case SOCKET_UDP_LITE:
            sUDPliteSupported = false;
            break;
        case SOCKET_DCCP:
            sDCCPSupported = false;
            break;
        case SOCKET_SCTP:
            sSCTPSupported = false;
            break;
        default:
        case SOCKET_TRANSPORT_TYPE_INVALID:
            LOGEX(Socket, LOG_ERROR, "Unsupported or invalid protocol type");
            break;

    }
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

        if ((pHost != ":") && (pHost != "::"))
        {
            // transform address
            if (inet_pton(AF_INET6, pHost.c_str(), &tAddressDescriptor->sa_in6.sin6_addr) < 0)
            {
                LOGEX(Socket, LOG_ERROR, "Error in inet-pton(IPv6) because of %s", strerror(errno));
                return false;
            }
        }else
        {
            tAddressDescriptor->sa_in6.sin6_addr = in6addr_any;
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

        if ((pHost != "") && (pHost != "*"))
        {
            // transform address
            //tAddressDescriptor->sa_in.sin_addr.s_addr = inet_addr(pHost.c_str());
            if (inet_pton(AF_INET, pHost.c_str(), &tAddressDescriptor->sa_in.sin_addr.s_addr) < 0)
            {
                LOGEX(Socket, LOG_ERROR, "Error in inet-pton(IPv4) because of %s", strerror(errno));
                return false;
            }
        }else
        {
            tAddressDescriptor->sa_in.sin_addr.s_addr = INADDR_ANY;
        }

        // port
        tAddressDescriptor->sa_in.sin_port = htons((unsigned short int)pPort);

        tAddressDescriptorSize = sizeof(tAddressDescriptor->sa_in);
    }
    return true;
}

string Socket::GetAddrFromDescriptor(SocketAddressDescriptor *tAddressDescriptor, unsigned int *pPort)
{
	string tResult = "";
    char tSourceHostStr[INET6_ADDRSTRLEN];
    switch (tAddressDescriptor->sa_stor.ss_family)
    {
        default:
        case AF_INET:
            if (inet_ntop(AF_INET, &tAddressDescriptor->sa_in.sin_addr, tSourceHostStr, INET_ADDRSTRLEN /* is always smaller than INET6_ADDRSTRLEN */) == NULL)
            {
                LOGEX(Socket, LOG_ERROR, "Error in inet_ntop(IPv4) because of %s", strerror(errno));
                return "";
            }
            if(pPort != NULL)
            	*pPort = (unsigned int)ntohs(tAddressDescriptor->sa_in.sin_port);
            break;
        case AF_INET6:
            if (inet_ntop(AF_INET6, &tAddressDescriptor->sa_in6.sin6_addr, tSourceHostStr, INET6_ADDRSTRLEN) == NULL)
            {
                LOGEX(Socket, LOG_ERROR, "Error in inet_ntop(IPv6) because of %s", strerror(errno));
                return "";
            }
            if(pPort != NULL)
            	*pPort = (unsigned int)ntohs(tAddressDescriptor->sa_in6.sin6_port);
            break;
    }
    if (tSourceHostStr != NULL)
    {
        tResult = string(tSourceHostStr);
        // IPv4 in IPv6 address?
        if ((tResult.find(':') != string::npos) && (tResult.find('.') != string::npos))
        	tResult.erase(0, tResult.rfind(':') + 1);
    }else
    	tResult = "";

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////
/// helper functions
///////////////////////////////////////////////////////////////////////////////

bool Socket::CreateSocket(enum NetworkType pIpVersion)
{
    int tSelectedIPDomain = 0;
    bool tResult = false;

    #if defined(WIN32) ||defined(WIN64)
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
            #if defined(LINUX) || defined(APPLE) || defined(BSD)
                if (IsTransportSupported(SOCKET_UDP_LITE))
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
            if ((mSocketHandle = (int)socket(tSelectedIPDomain, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                LOG(LOG_ERROR, "Could not create UDP socket");
            else
                tResult = true;
            #if defined(WIN32) ||defined(WIN64)
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
            if ((mSocketHandle = (int)socket(tSelectedIPDomain, SOCK_STREAM, IPPROTO_TCP)) < 0)
                LOG(LOG_ERROR, "Could not create TCP socket");
            else
                tResult = true;
            #if defined(WIN32) ||defined(WIN64)
                if (ioctlsocket(mSocketHandle, FIONBIO, &nonBlockingMode))
                {
                    LOG(LOG_ERROR, "Failed to set blocking-mode for socket %d", mSocketHandle);
                    tResult = false;
                }
            #endif
            break;
    }

    // do we have a valid socket?
    if (tResult)
    {
        LOG(LOG_VERBOSE, "Created IPv%d-%s socket with handle number %d", (tSelectedIPDomain == PF_INET6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str(), mSocketHandle);

        bool tSockOptResult = 0;

        // activate dual stack
        if (tSelectedIPDomain == PF_INET6)
        {
            // we force hybrid sockets, otherwise Windows will complain: http://msdn.microsoft.com/en-us/library/bb513665%28v=vs.85%29.aspx
            int tOnlyIpv6Sockets = false;
            tSockOptResult = (setsockopt(mSocketHandle, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&tOnlyIpv6Sockets, sizeof(tOnlyIpv6Sockets)) == 0);
            if (tSockOptResult)
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
        #if defined(LINUX) || defined(APPLE) || defined(BSD)
            close(pHandle);
        #endif
        #if defined(WIN32) ||defined(WIN64)
            closesocket(pHandle);
            WSACleanup();
        #endif
    }
}

bool Socket::BindSocket(unsigned int pPort, unsigned int pProbeStepping, unsigned int pHighesPossiblePort)
{
    bool tResult = true;
    SocketAddressDescriptor tAddressDescriptor;
	unsigned int   			tAddressDescriptorSize;

    if (mSocketHandle == -1)
    {
        LOG(LOG_ERROR, "Socket handle is invalid, cannot bind socket");
        return false;
    }

    LOG(LOG_VERBOSE, "Trying to bind IPv%d-%s socket %d to local port %d", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str(), mSocketHandle, pPort);

    if (!FillAddrDescriptor((mSocketNetworkType == SOCKET_IPv6) ? "::" : "*", pPort, &tAddressDescriptor, tAddressDescriptorSize))
    {
        LOG(LOG_ERROR ,"Could not process the bind address of socket %d", mSocketHandle);
        return false;
    }

    // data port: search for the next free port and bind to it
	#if defined(WIN32) ||defined(WIN64)
    	while (bind(mSocketHandle, &tAddressDescriptor.sa, (int)tAddressDescriptorSize) < 0)
	#else
		while (bind(mSocketHandle, &tAddressDescriptor.sa, tAddressDescriptorSize) < 0)
	#endif
    {
        if (!pProbeStepping)
        {
            LOG(LOG_ERROR, "Failed to bind IPv%d-%s socket %d to port %d while auto probing is off, error occurred because of \"%s\"", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str(), mSocketHandle, pPort, strerror(errno));
            tResult = false;
            return 0;
        }

        LOG(LOG_INFO, "Failed to bind IPv%d-%s socket %d to local port %d because \"%s\", will try next alternative", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str(), mSocketHandle, pPort, strerror(errno));
        pPort += pProbeStepping;
        if ((pPort > 65535) || ((pPort > pHighesPossiblePort) && (pHighesPossiblePort != 0)))
        {
            LOG(LOG_ERROR, "Auto-probing for port binding failed, no further port numbers allowed");
            tResult = false;
            pPort = 0;
            break;
        }

        if (mSocketNetworkType == SOCKET_IPv6)
            tAddressDescriptor.sa_in6.sin6_port = htons((uint16_t)pPort);
        else
            tAddressDescriptor.sa_in.sin_port = htons((uint16_t)pPort);
    }

    // find local port if bind was successful
    if(tResult)
    {
        if (getsockname(mSocketHandle, &tAddressDescriptor.sa, (socklen_t *)&tAddressDescriptorSize) <  0)
        {
            LOG(LOG_ERROR, "Failed to determine the local socket name");
        }else
        {
        	mLocalHost = GetAddrFromDescriptor(&tAddressDescriptor, &mLocalPort);
        	if (mLocalHost == "")
                LOG(LOG_ERROR ,"Could not determine the local BIND address for socket %d", mSocketHandle);
            else
                LOG(LOG_VERBOSE, "Bound IPv%d-%s socket %d to %s:%d", (mSocketNetworkType == SOCKET_IPv6) ? 6 : 4, TransportType2String(mSocketTransportType).c_str(), mSocketHandle, mLocalHost.c_str(), mLocalPort);
        }
    }

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
