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
 * Purpose: SocketBinding
 * Author:  Martin Becke
 * Since:   2012-05-30
 */

#include <GAPI.h>
#include <NGNSocketName.h>
#include <NGNSocketConnection.h>
#include <NGNSocketBinding.h>
#include <RequirementTransmitLossless.h>
#include <RequirementTransmitChunks.h>
#include <RequirementTransmitStream.h>
#include <RequirementTransmitBitErrors.h>
#include <RequirementTargetPort.h>
#include <RequirementLimitDelay.h>
#include <RequirementLimitDataRate.h>

#include <HBSocket.h>
#include <HBSocketQoSSettings.h>

#include <Logger.h>

#include <string>

#include <NGNParseNetworkRequirment.h>

namespace Homer { namespace Base {

using namespace std;

// In the end this is the server side implementation

///////////////////////////////////////////////////////////////////////////////
//HINT: lossless transmission is not implemented by using TCP but by rely on a reaction by the network
NGNSocketBinding::NGNSocketBinding(std::string pLocalName, Requirements *pRequirements)
{
    mIsClosed = true;
    LOG(LOG_VERBOSE, "Start SCTP Server for %s", pLocalName.c_str());
    // resolution of connection by target and requirements.
    LOG(LOG_VERBOSE, "memset mSock_addr with sizof %i", sizeof(mSock_addr));
    memset((void *) &mSock_addr, 0, sizeof(mSock_addr));
    struct sctp_event_subscribe event;
    bzero(&event, sizeof(event));
    mUnordered = false;
    mIpv4only  = true; // For UDP encapsulation only IPv4 is supported
    mIpv6only  = false;
    // helper
    socklen_t addr_len;
    int on = 1;

    
    // parse of the needed requirements
    // List of requirements
    // - Port
    // - Type
    
    ///////////////////////////////////////////////
    // Parse for Port
    RequirementTargetPort *tRequPort = (RequirementTargetPort*)pRequirements->get(RequirementTargetPort::type());
    if (tRequPort != NULL)
    {
        mPort = tRequPort->getPort();
    }else
    {
        LOG(LOG_WARN, "No target port given within requirement set, falling back to port 0");
        mPort = 0;
    }
   
    ///////////////////////////////////////////////
    // Parse for type
    // INFO and TODO
    // - UDP is used for UDP encapsulation, 
    // - TCP is used for SCTP in one-to-one TCP style
    if ((pRequirements->contains(RequirementTransmitChunks::type())) && (pRequirements->contains(RequirementTransmitStream::type())))
    {
        LOG(LOG_ERROR, "Detected requirement conflict between \"Req:Chunks\" and \"Req:Stream\"");
    }

    bool tTcp = (((!pRequirements->contains(RequirementTransmitChunks::type())) &&
                (pRequirements->contains(RequirementTransmitStream::type()))));

    bool tUdp = ((pRequirements->contains(RequirementTransmitChunks::type())) &&
                (!pRequirements->contains(RequirementTransmitStream::type())));

    if(((pRequirements->contains(RequirementTransmitChunks::type())) &&
                    (!pRequirements->contains(RequirementTransmitStream::type())) &&
                    (pRequirements->contains(RequirementTransmitBitErrors::type()))))
    {

        LOG(LOG_ERROR, "The UDP light option is not valid in this context");
    }

    LOG(LOG_VERBOSE, "Setup the address stuff for  %s port %i", pLocalName.c_str(), mPort);

    if(tTcp)
        LOG(LOG_VERBOSE, "Use default TCP style");
    else if (tUdp)
        LOG(LOG_VERBOSE, "Use default UDP encapsulation style");
    else
        LOG(LOG_VERBOSE, "Style not supported");
     
    ///////////////////////////////////////////////////
    // Setup the address stuff
    
    if (inet_pton(AF_INET6, ((char*) pLocalName.c_str()), &mSock_addr.s6.sin6_addr))
    {
        LOG(LOG_VERBOSE, "Setup IPv4 address for SCTP");
	    mSock_addr.s6.sin6_family = AF_INET6;
#ifdef HAVE_SIN_LEN
	    mSock_addr.s6.sin6_len = sizeof(struct sockaddr_in6);
#endif
	    mSock_addr.s6.sin6_port = htons(mPort);
	    addr_len = sizeof(struct sockaddr_in6);
	    if (mIpv4only)
	    {
		    LOG(LOG_ERROR, "Can't use IPv6 address when IPv4 only\n");
	    }
     }else
     {
	    if (inet_pton(AF_INET, ((char*) pLocalName.c_str()), &mSock_addr.s4.sin_addr))
	    {
		    LOG(LOG_VERBOSE, "Setup IPv4 address for SCTP");
		    mSock_addr.s4.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
		    mSock_addr.s4.sin_len = sizeof(struct sockaddr_in);
#endif
		    mSock_addr.s4.sin_port = htons(mPort);
		    addr_len = sizeof(struct sockaddr_in);

		    if (mIpv6only)
		    {
			    LOG(LOG_ERROR, "Can't use IPv4 address when IPv6 only\n");			
		    }
	    }else
	    {
		    LOG(LOG_ERROR, "Invalid address\n");
	    }
    }
    
    
    LOG(LOG_VERBOSE, "Setup Socket for %s : %i", pLocalName.c_str(), mPort);
    ////////////////////////////////////////////////////////////////////////////////
    // In every case we need a SCTP Socket
    if ((mSocket = socket((mIpv4only ? AF_INET : AF_INET6), SOCK_STREAM, IPPROTO_SCTP)) < 0)
    {
        LOG(LOG_ERROR, "Socket Error");
    }
//    const int on = 1;
//    const int off = 0;
//    if (!mIpv4only) {
//	    if (mIpv6only) {
//		    if (setsockopt(mSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const void*)&on, (socklen_t)sizeof(on)) < 0)
//			    LOG(LOG_ERROR, "Socket Error IPV6_ONLY");
//	    } else {
//		    if (setsockopt(mSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const void*)&off, (socklen_t)sizeof(off)) < 0)
//			    LOG(LOG_ERROR, "Socket Error IPV6_ONLY");
//	    }
//    }

#ifdef SCTP_REMOTE_UDP_ENCAPS_PORT
        if(SPECIAL_PORT_RT == mPort || SPECIAL_PORT_DT_0 == mPort || SPECIAL_PORT_DT_1 == mPort )
        {
            memset(&mEncaps, 0, sizeof(struct sctp_udpencaps));
            mEncaps.sue_address.ss_family = (mIpv4only ? AF_INET : AF_INET6);
            mEncaps.sue_port = htons(UDP_ENCAPSULATION);
            if (setsockopt(mSocket, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT, (const void*)&mEncaps, (socklen_t)sizeof(struct sctp_udpencaps)) < 0)
            {
                LOG(LOG_ERROR, "Socketopt Error UDP encapsulation");
            }else
                LOG(LOG_VERBOSE,"UDP encapsulation supported");
        }else
            LOG(LOG_VERBOSE,"%i is not the UDP encapsulation Port %i %i %i", mPort, SPECIAL_PORT_RT, SPECIAL_PORT_DT_0, SPECIAL_PORT_DT_1);
#endif

    LOG(LOG_VERBOSE,"MY");
    mIsClosed = false;
    ///////////////////////////////////////////////////////////////////////////
    // start a parser tree for SCTP QoS requirements and additional transport requirements for SCTP
//    changeRequirements(pRequirements);
#ifdef SCTP_RCVINFO
    setsockopt(mSocket, IPPROTO_SCTP, SCTP_RECVRCVINFO, &on, sizeof(on));
#else
    memset(&event, 0, sizeof(event));
    event.sctp_data_io_event = 1;
    if (setsockopt(mSocket, IPPROTO_SCTP, SCTP_EVENTS, &event, sizeof(event)) != 0) {
        perror("set event failed");
    }
#endif

    if (bind(mSocket, (struct sockaddr *)&mSock_addr, addr_len) != 0){
        LOG(LOG_ERROR, "bind %i", errno);
        mSocket = -1;
        mIsClosed = true;
    }
    if (listen(mSocket, 1) < 0){
        LOG(LOG_ERROR, "listen %i", errno);
        mSocket = -1;
        mIsClosed = true;
    }

}

NGNSocketBinding::~NGNSocketBinding()
{
    LOG(LOG_VERBOSE, "Destroying GAPI NGN bind object..");
    if (!isClosed())
    {
        cancel();
    }
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

bool NGNSocketBinding::isClosed()
{
    return mIsClosed;
}

IConnection* NGNSocketBinding::readConnection()
{
    if (mIsClosed)
        return NULL;

    return (IConnection*) (new NGNSocketConnection(mSocket));
}

Name* NGNSocketBinding::getName()
{
    if(mSocket > 0)
    {
        return NULL; //new NGNSocketName(mSocket->GetLocalHost(), mSocket->GetLocalPort());
    }else
    {
        return NULL;
    }
}

void NGNSocketBinding::cancel()
{
    if ((mSocket != 0) && (!isClosed()))
    {
        LOG(LOG_VERBOSE, "All connection will be canceled now");
	
        close (mSocket);
        mSocket = -1;
    }

    LOG(LOG_VERBOSE, "Canceled");
    mIsClosed = true;
}

bool NGNSocketBinding::changeRequirements(Requirements *pRequirements)
{
    mRequirements = pRequirements;
    NGNParseNetworkRequirment::parse(this,(NGNSocketConnection*)readConnection());
    return true;
}

Requirements* NGNSocketBinding::getRequirements()
{
	return mRequirements;
}

Events NGNSocketBinding::getEvents()
{
	Events tResult;

	//TODO:

	return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
