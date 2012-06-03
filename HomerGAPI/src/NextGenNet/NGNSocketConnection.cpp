/*****************************************************************************
 *
 * Copyright (C) 2012 Martin.Becke@uni-due.de 
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
 * Purpose: SocketConnection
 * Author:  Martin Becke
 * Since:   2012-05-30
 */

#include <GAPI.h>
#include <NextGenNet/NGNSocketName.h>
#include <NextGenNet/NGNSocketConnection.h>
#include <RequirementTransmitLossless.h>
#include <RequirementTransmitChunks.h>
#include <RequirementTransmitStream.h>
#include <RequirementTransmitBitErrors.h>
#include <RequirementTargetPort.h>
#include <RequirementLimitDelay.h>
#include <RequirementLimitDataRate.h>


#include <Berkeley/SocketName.h>  // TODO needed for the getNAme and getRemoteName functions... 
#include <Logger.h>
#include <string>

namespace Homer {  namespace Base {

using namespace std;



///////////////////////////////////////////////////////////////////////////////
//HINT: lossless transmission is not implemented by using TCP but by rely on a reaction by the network
NGNSocketConnection::NGNSocketConnection(std::string pTarget, Requirements *pRequirements)
{
  //////////////////////////////////////////////////////
  // Step 1: the basic setup 
  
  memset((void *) &mSock_addr, 0, sizeof(mSock_addr));
  mUnordered = false;
  mIpv4only  = true; // For UDP encapsulation only IPv4 is supported
  mIpv6only  = false;
  mClient    = CLIENT;
  socklen_t addr_len;
  
  //////////////////////////////////////////////////////
  // Step 2: parse the basic requirements
  // Parse of the needed requirements
  // List of requirements
  // - Port
  // - Type
  // The special feature are parsed in changeRequirements
  
  /* Parse for Port */
  RequirementTargetPort *tRequPort = (RequirementTargetPort*)pRequirements->get(RequirementTargetPort::type());
  if (tRequPort != NULL)
  {
      mPort = tRequPort->getPort();
  }else
  {
      LOG(LOG_WARN, "No target port given within requirement set, falling back to port 0");
      mPort = 0;
  }
  
  /* Parse type */
  if ((pRequirements->contains(RequirementTransmitChunks::type())) && (pRequirements->contains(RequirementTransmitStream::type())))
  {
    LOG(LOG_ERROR, "Detected requirement conflict between \"Req:Chunks\" and \"Req:Stream\"");
  }
  // INFO and TODO
  // - UDP is used for UDP encapsulation, 
  // - TCP is used for SCTP in one-to-one TCP style
  bool tTcp = (((!pRequirements->contains(RequirementTransmitChunks::type())) &&
	      (pRequirements->contains(RequirementTransmitStream::type()))));

  bool tUdp = ((pRequirements->contains(RequirementTransmitChunks::type())) &&
	      (!pRequirements->contains(RequirementTransmitStream::type())));

  if(((pRequirements->contains(RequirementTransmitChunks::type())) &&
		  (!pRequirements->contains(RequirementTransmitStream::type())) &&
		  (pRequirements->contains(RequirementTransmitBitErrors::type())))){
    LOG(LOG_ERROR, "The UDP light option is not valid in this context");  
  }

  
  ///////////////////////////////////////////////////
  // Step 3: Setup the address stuff
  
  if (inet_pton(AF_INET6, ((char*) pTarget.c_str()), &mSock_addr.s6.sin6_addr)) {
	  mSock_addr.s6.sin6_family = AF_INET6;
#ifdef HAVE_SIN_LEN
	  mSock_addr.s6.sin6_len = sizeof(struct sockaddr_in6);
#endif
	  mSock_addr.s6.sin6_port = htons(mPort);
	  addr_len = sizeof(struct sockaddr_in6);

	  if (mIpv4only) {
		  LOG(LOG_ERROR, "Can't use IPv6 address when IPv4 only\n");
	  }
    }else {
	  if (inet_pton(AF_INET, ((char*) pTarget.c_str()), &mSock_addr.s4.sin_addr))
	  {
		  mSock_addr.s4.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
		  mSock_addr.s4.sin_len = sizeof(struct sockaddr_in);
#endif
		  mSock_addr.s4.sin_port = htons(mPort);
		  addr_len = sizeof(struct sockaddr_in);

		  if (mIpv6only) {
			  LOG(LOG_ERROR, "Can't use IPv4 address when IPv6 only\n");
		      
		  }
	  } else {
		  LOG(LOG_ERROR, "Invalid address\n");
	  }
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  // Step 4: Setup the socket stuff
  
  if ((mSocket = socket((mIpv4only ? AF_INET : AF_INET6), SOCK_STREAM, IPPROTO_SCTP)) < 0){
    LOG(LOG_ERROR, "Socket Error");
  }
  const int on = 1;
  const int off = 0;
  if (!mIpv4only) {
	  if (mIpv6only) {
		  if (setsockopt(mSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const void*)&on, (socklen_t)sizeof(on)) < 0)
			  LOG(LOG_ERROR, "Socket Error IPV6_ONLY");
	  } else {
		  if (setsockopt(mSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const void*)&off, (socklen_t)sizeof(off)) < 0)
			  LOG(LOG_ERROR, "Socket Error IPV6_ONLY");
	  }
  }
  
  if(SPECIAL_PORT == mPort){
    memset(&mEncaps, 0, sizeof(struct sctp_udpencaps));
    mEncaps.sue_address.ss_family = (mIpv4only ? AF_INET : AF_INET6);
    mEncaps.sue_port = htons(UDP_ENCAPSULATION);
    
    if (setsockopt(mSocket, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT, (const void*)&mEncaps, (socklen_t)sizeof(struct sctp_udpencaps)) < 0) {
	      LOG(LOG_ERROR, "Socketopt Error UDP encapsulation");
    }
    else
      LOG(LOG_VERBOSE,"UDP encapsulation supported");
  } 
  else
    LOG(LOG_VERBOSE,"%i is not the UDP encapsulation Port %i", mPort, SPECIAL_PORT);



  /* start a parser tree for SCTP QoS requirements and additional transport requirements for SCTP */
  changeRequirements(pRequirements);

  /* now enable SCTP association */
  if (connect(mSocket, (struct sockaddr *)&mSock_addr, addr_len) < 0) {
	  LOG(LOG_ERROR, "Socket Error Connect");
  }
  LOG(LOG_VERBOSE, "Init Client finished and connected");
}

// In the end this is the client side implementation
NGNSocketConnection::NGNSocketConnection(int fd){
  LOG(LOG_VERBOSE, "Setup a Connection for server Listen on Socket ID %i", fd);
  mClient = SERVER;
  mSocket = fd;
  LOG(LOG_VERBOSE, "Setup a server config");  
  
}

NGNSocketConnection::~NGNSocketConnection()
{
  cancel();
  close(mSocket);
  mSocket = -1;
}

///////////////////////////////////////////////////////////////////////////////

bool NGNSocketConnection::isClosed()
{
  if(mSocket < 0)
    return true;
  return false; 
}

int NGNSocketConnection::availableBytes()
{
  return 0; //TODO
}

void NGNSocketConnection::read(char* pBuffer, int &pBufferSize)
{
  int fd = mSocket;
  if(mSocket > 0)
  {    
    
    if(mClient == SERVER){
	LOG(LOG_VERBOSE, "Start Accept");
	if ((mClient = accept(mSocket, (struct sockaddr *)&mSock_addr, &mAddr_len)) < 0) {
	LOG(LOG_ERROR, "Error accept");
	}
	LOG(LOG_VERBOSE, "Finished Accept");
      
    }
    if(mClient > SERVER)
      fd = mClient;
    
    socklen_t len = (socklen_t)0;;
    int flags = 0;
    pBufferSize = sctp_recvmsg(fd, (void*)pBuffer, pBufferSize, NULL, &len, NULL, &flags);

  }else
      LOG(LOG_ERROR, "Invalid socket");
}

void NGNSocketConnection::write(char* pBuffer, int pBufferSize)
{
  int fd = mSocket;
  size_t n;
  if(mSocket > 0)
  {
      
    if(mClient == SERVER){
	LOG(LOG_VERBOSE, "Start Accept");
	if ((mClient = accept(mSocket, (struct sockaddr *)&mSock_addr, &mAddr_len)) < 0) {
	LOG(LOG_ERROR, "Error accept");
	}
	LOG(LOG_VERBOSE, "Finished Accept");
    }
    if(mClient > SERVER)
      fd = mClient;
      
    if (sctp_sendmsg(fd, (void*)pBuffer, pBufferSize, NULL, 0, 0, 0, 0, 0, 0) < 0) {
	    LOG(LOG_ERROR, "sctp_sendmsg %i", errno);
    }
  }else
      LOG(LOG_ERROR, "Invalid socket");
}

bool NGNSocketConnection::getBlocking()
{
	return false; //TODO
}

void NGNSocketConnection::setBlocking(bool pState)
{
	// TODO
}

void NGNSocketConnection::cancel()
{
    if(mSocket > 0)
    {
        LOG(LOG_VERBOSE, "Connection to %s will be canceled now", getName()->toString().c_str());
//        if (mBlockingMode) //TODO: do this only if a call to read() is still blocked
//        {
	// TODO do some signaling for end of transmission
	
//            LOG(LOG_VERBOSE, "Try to do loopback signaling to local IPv%d listener at port %u, transport %d", mSocket->GetNetworkType(), 0xFFFF & mSocket->GetLocalPort(), mSocket->GetTransportType());
//            Socket  *tSocket = Socket::CreateClientSocket(mSocket->GetNetworkType(), mSocket->GetTransportType());
//            char    tData[8];
//            switch(tSocket->GetNetworkType())
//            {
//                case SOCKET_IPv4:
//                    LOG(LOG_VERBOSE, "Doing loopback signaling to IPv4 listener to port %u", mSocket->GetLocalPort());
//                    if (!tSocket->Send("127.0.0.1", mSocket->GetLocalPort(), tData, 0))
//                        LOG(LOG_ERROR, "Error when sending data through loopback IPv4-UDP socket");
//                    break;
//                case SOCKET_IPv6:
//                    LOG(LOG_VERBOSE, "Doing loopback signaling to IPv6 listener to port %u", mSocket->GetLocalPort());
//                    if (!tSocket->Send("::1", mSocket->GetLocalPort(), tData, 0))
//                        LOG(LOG_ERROR, "Error when sending data through loopback IPv6-UDP socket");
//                    break;
//                default:
//                    LOG(LOG_ERROR, "Unknown network type");
//                    break;
//            }
//            delete tSocket;
//        }
	
	if(mClient != SERVER){
	   close(mClient);
	   mClient = CLIENT;
	}
	else
	   close(mSocket);
    }
}

Name* NGNSocketConnection::getName()
{
  LOG(LOG_VERBOSE, "Get Name");
    if(mSocket > 0)
    {
        return  new SocketName("Remote Next Generation Network",0);
    }else
    {
        return NULL;
    }
}

Name* NGNSocketConnection::getRemoteName()
{
//    LOG(LOG_VERBOSE, "Get Remote Name");
    if(mSocket  > 0)
    {
        return new SocketName("Remote Next Generation Network",0);
    }else
    {
        return NULL;
    }
}

bool NGNSocketConnection::changeRequirements(Requirements *pRequirements)
{

    // TODO SCTP specific requirments
    // possible:
    // - out of order
    // - notification
    // - partial reliable 
    // - mobility
    // - multihoming
    // - etc...
    
    mRequirements = *pRequirements; //TODO: maybe some requirements were dropped?

    return true;
}

Requirements NGNSocketConnection::getRequirements()
{
	return mRequirements;
}

Events NGNSocketConnection::getEvents()
{
	Events tResult;

	//TODO:

	return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
