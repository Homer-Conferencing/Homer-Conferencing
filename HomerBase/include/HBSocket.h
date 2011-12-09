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
 * Purpose: wrapper for OS independent socket handling
 * Author:  Thomas Volkert
 * Since:   2010-09-22
 */

#ifndef _BASE_SOCKET_
#define _BASE_SOCKET_

#if defined(LINUX) || defined(APPLE) || defined(BSD)
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <io.h>
#include <BaseTsd.h>
#ifndef ssize_t
#define ssize_t SSIZE_T
#endif
#endif

#include <string>
#include <list>

#include <HBSocketQoSSettings.h>
#include <socket_ext.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define HBS_DEBUG_PACKETS

// maximum TCP connections a TCP based socket supports
#define MAX_INCOMING_CONNECTIONS        1 //TODO: support multiple clients

enum NetworkType{
    SOCKET_NETWORK_TYPE_INVALID = -1,
    SOCKET_IPv4 = 4,
    SOCKET_IPv6 = 6
};

enum TransportType{
    SOCKET_TRANSPORT_TYPE_INVALID = -1,
    SOCKET_UDP = 0, //RFC 768
    SOCKET_TCP, //RFC 793
    SOCKET_UDP_LITE, //RFC 3828, only for Linux, in Windows environments we fall back to UDP
    SOCKET_DCCP, // RFC 4340, only for Linux, in Windows environments we fall back to UDP
    SOCKET_SCTP // RFC 4960, only for Linux, in Windows environments we fall back to UDP
};

union SocketAddressDescriptor
{
    struct sockaddr sa;
    struct sockaddr_in sa_in;
    struct sockaddr_in6 sa_in6;
    struct sockaddr_storage sa_stor;
};

#define IP_OPTIONS_SIZE                         (sizeof(QoSIpOption))
#define IP4_HEADER_SIZE							20 // default size, additional bytes for IP options are possible
#define IP6_HEADER_SIZE							40 // fixed size
#define TCP_HEADER_SIZE							20 // default size, additional bytes for TCP options are possible
#define UDP_HEADER_SIZE							8 // fixed size
#define UDP_LITE_HEADER_SIZE					8 // fixed size

#define IS_IPV6_ADDRESS(x) (x.find(':') != string::npos)

///////////////////////////////////////////////////////////////////////////////

class Socket
{
public:
    /* client sockets */
	Socket(enum NetworkType pIpVersion, enum TransportType pTransportType = SOCKET_UDP);
    /* server socekts */
    Socket(unsigned int pListenerPort /* start number for auto probing */, enum TransportType pTransportType, unsigned int pProbeStepping = 0, unsigned int pHighesPossibleListenerPort = 0);

    virtual ~Socket( );

    /* getters */
    enum NetworkType GetNetworkType();
    enum TransportType GetTransportType();
    unsigned int GetLocalPort();
    std::string GetLocalHost();
    unsigned int GetPeerPort();
    std::string GetPeerHost();
    std::string GetName();
    std::string GetPeerName();

    /* QoS interface */
    static bool IsQoSSupported();
    static void DisableQoSSupport();
    bool SetQoS(const QoSSettings &pQoSSettings);
    bool GetQoS(QoSSettings &pQoSSettings);
    static bool CreateQoSProfile(const std::string &pProfileName, const QoSSettings &pQoSSettings);
    static QoSProfileList GetQoSProfiles();
    bool SetQoS(const std::string &pProfileName);

    /* transmission */
    bool Send(std::string pTargetHost, unsigned int pTargetPort, void *pBuffer, ssize_t pBufferSize);
    bool Receive(std::string &pSourceHost, unsigned int &pSourcePort, void *pBuffer, ssize_t &pBufferSize);

    /* transport layer support */
    static bool IsTransportSupported(enum TransportType pType);
    static void DisableTransportSupport(enum TransportType pType);
    void UDPLiteSetCheckLength(int pBytes = UDP_LITE_HEADER_SIZE);
    void TCPDisableNagle();
    static std::string TransportType2String(enum TransportType pSocketType);
    static enum TransportType String2TransportType(std::string pTypeStr);

    /* network layer support */
    static bool IsIPv6Supported();
    static void DisableIPv6Support();
    static std::string NetworkType2String(enum NetworkType pSocketType);
    static enum NetworkType String2NetworkType(std::string pTypeStr);

    /* handling of SocketAddressDescriptor */
    static bool GetAddrFromDescriptor(SocketAddressDescriptor *tAddressDescriptor, std::string &pHost, unsigned int &pPort);
    static bool FillAddrDescriptor(std::string pHost, unsigned int pPort, SocketAddressDescriptor *tAddressDescriptor, unsigned int &tAddressDescriptorSize);

private:
    void SetDefaults(enum TransportType pTransportType);

    bool CreateSocket(enum NetworkType pIpVersion = SOCKET_IPv6);
    bool BindSocket(unsigned int pPort = 0, unsigned int pProbeStepping = 1, unsigned int pHighesPossibleListenerPort = 0);
    static void DestroySocket(int pHandle);

    QoSSettings			mQoSSettings;
    int 			    mUdpLiteChecksumCoverage;
    enum TransportType	mSocketTransportType;
    enum NetworkType    mSocketNetworkType;
    bool			    mIsConnected;
    bool                mIsListening;
    std::string         mPeerHost;
    unsigned int        mPeerPort;
    int                 mSocketHandle, mTcpClientSockeHandle;
    unsigned int        mLocalPort;
    std::string         mLocalHost;
    bool                mIsClientSocket;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
