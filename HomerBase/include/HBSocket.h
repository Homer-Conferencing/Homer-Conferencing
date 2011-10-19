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
 * Name:    Socket.h
 * Purpose: wrapper for OS independent socket handling
 * Author:  Thomas Volkert
 * Since:   2010-09-22
 * Version: $Id$
 */

#ifndef _BASE_SOCKET_
#define _BASE_SOCKET_

#ifdef LINUX
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
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
    SOCKET_UDP_LITE //RFC 3828, only for Linux, in Windows environments we fall back to UDP
};

union SocketAddressDescriptor
{
    struct sockaddr sa;
    struct sockaddr_in sa_in;
    struct sockaddr_in6 sa_in6;
    struct sockaddr_storage sa_stor;
};

#define IP4_HEADER_SIZE							20 // default size, additional bytes for IP options are possible
#define IP6_HEADER_SIZE							40 // fixed size
#define TCP_HEADER_SIZE							20 // default size, additional bytes for TCP options are possible
#define UDP_HEADER_SIZE							8 // fixed size
#define UDP_LITE_HEADER_SIZE					8 // fixed size

#define IS_IPV6_ADDRESS(x) (x.find(':') != string::npos)

///////////////////////////////////////////////////////////////////////////////

// de/activate QoS interface
#define QOS_INTERFACE

#define QOS_FEATURE_NONE									0x00000000
#define QOS_FEATURE_LOSLESS									0x00000001

struct QoSSettings
{
    unsigned int MinDataRate; /* in KB/s */
    unsigned int MaxDelay; /* in ms */
    union{
        unsigned int Features;
        struct{
            bool Lossless; /* dropping allowed? */
            bool dummy[31];
        }__attribute__((__packed__))Feature;
    };
};

struct QoSProfileDescriptor
{
    std::string Name;
    QoSSettings Settings;
};

typedef std::list<QoSProfileDescriptor*> QoSProfileList;

///////////////////////////////////////////////////////////////////////////////

class Socket
{
public:
    /* client sockets */
	Socket(enum NetworkType pIpVersion, enum TransportType pTransportType = SOCKET_UDP);
    /* server socekts */
    Socket(unsigned int pListenerPort /* start number for auto probing */, enum TransportType pTransportType, unsigned int pProbeStepping = 0, unsigned int pHighesPossibleListenerPort = 0);

    virtual ~Socket( );

    /* get type information */
    enum NetworkType GetNetworkType();
    enum TransportType GetTransportType();

    unsigned int getLocalPort();

    /* QoS interface */
    bool SetQoS(const QoSSettings &pQoSSettings);
    bool GetQoS(QoSSettings &pQoSSettings);
    static bool CreateQoSProfile(const std::string &pProfileName, const QoSSettings &pQoSSettings);
    static QoSProfileList GetQoSProfiles();
    bool SetQoS(const std::string &pProfileName);

    /* checksum coverage */
    void SetUdpLiteChecksumCoverage(int pBytes = UDP_LITE_HEADER_SIZE);

    /* transmission */
    bool Send(std::string pTargetHost, unsigned int pTargetPort, void *pBuffer, ssize_t pBufferSize);
    bool Receive(std::string &pSourceHost, unsigned int &pSourcePort, void *pBuffer, ssize_t &pBufferSize);

    /* IPv6 support */
    static bool IsIPv6Supported();
    static void DisableIPv6Support();

    /* UDPlite support */
    static bool IsUDPliteSupported();
    static void DisableUDPliteSupport();

    /* transport type */
    static std::string TransportType2String(enum TransportType pSocketType);
    static enum TransportType String2TransportType(std::string pTypeStr);

    /* handling of SocketAddressDescriptor */
    static bool GetAddrFromDescriptor(SocketAddressDescriptor *tAddressDescriptor, std::string &pHost, unsigned int &pPort);
    static bool FillAddrDescriptor(std::string pHost, unsigned int pPort, SocketAddressDescriptor *tAddressDescriptor, unsigned int &tAddressDescriptorSize);

private:
    void SetDefaults(enum TransportType pTransportType);

    bool CreateSocket(enum NetworkType pIpVersion = SOCKET_IPv6);
    unsigned short int BindSocket(unsigned int pPort = 0, unsigned int pProbeStepping = 1, unsigned int pHighesPossibleListenerPort = 0);
    static void DestroySocket(int pHandle);

    QoSSettings			mQoSSettings;
    int 			    mUdpLiteChecksumCoverage;
    enum TransportType	mSocketTransportType;
    enum NetworkType    mSocketNetworkType;
    bool			    mIsConnected;
    bool                mIsListening;
    std::string         mConnectedHost;
    unsigned int        mConnectedPort;
    int                 mSocketHandle, mTcpClientSockeHandle;
    unsigned int        mLocalPort;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
