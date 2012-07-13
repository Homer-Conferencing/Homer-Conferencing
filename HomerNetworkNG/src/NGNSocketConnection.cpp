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
#include <NGNSocketName.h>
#include <NGNSocketConnection.h>
#include <Requirements.h>

#include <Berkeley/SocketName.h>  // TODO needed for the getNAme and getRemoteName functions... 
#include <Logger.h>
#include <string>
#include <NGNParseNetworkRequirment.h>



namespace Homer {  namespace Base {

using namespace std;

int NGNSocketConnection::static_stream_cnt = 1;

///////////////////////////////////////////////////////////////////////////////
//HINT: lossless transmission is not implemented by using TCP but by rely on a reaction by the network
NGNSocketConnection::NGNSocketConnection(std::string pTarget, Requirements *pRequirements)
{
    mIsClosed = true;

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
          (pRequirements->contains(RequirementTransmitBitErrors::type()))))
    {
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

        if (mIpv4only)
        {
          LOG(LOG_ERROR, "Can't use IPv6 address when IPv4 only\n");
        }
    }else
    {
        if (inet_pton(AF_INET, ((char*) pTarget.c_str()), &mSock_addr.s4.sin_addr))
        {
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
        } else{
          LOG(LOG_ERROR, "Invalid address\n");
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Step 4: Setup the socket stuff

    if ((mSocket = socket((mIpv4only ? AF_INET : AF_INET6), SOCK_STREAM, IPPROTO_SCTP)) < 0)
    {
        LOG(LOG_ERROR, "Socket Error");
    }
    const int on = 1;
    const int off = 0;
    if (!mIpv4only)
    {
        if (mIpv6only)
        {
            if (setsockopt(mSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const void*)&on, (socklen_t)sizeof(on)) < 0)
                LOG(LOG_ERROR, "Socket Error IPV6_ONLY");
        }else{
            if (setsockopt(mSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const void*)&off, (socklen_t)sizeof(off)) < 0)
                LOG(LOG_ERROR, "Socket Error IPV6_ONLY");
        }
    }

#ifdef SCTP_REMOTE_UDP_ENCAPS_PORT
        if(SPECIAL_PORT == mPort)
        {
            memset(&mEncaps, 0, sizeof(struct sctp_udpencaps));
            mEncaps.sue_address.ss_family = (mIpv4only ? AF_INET : AF_INET6);
            mEncaps.sue_port = htons(UDP_ENCAPSULATION);

            if (setsockopt(mSocket, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT, (const void*)&mEncaps, (socklen_t)sizeof(struct sctp_udpencaps)) < 0)
            {
                LOG(LOG_ERROR, "SetSockOpt error regarding UDP encapsulation");
            }
            else
                LOG(LOG_VERBOSE,"UDP encapsulation supported");
        }else
            LOG(LOG_VERBOSE,"%i is not the UDP encapsulation port %i", mPort, SPECIAL_PORT);
#endif

    /* now enable SCTP association */
    if (connect(mSocket, (struct sockaddr *)&mSock_addr, addr_len) < 0)
    {
        LOG(LOG_ERROR, "Socket error connect");
    }
    mStream = static_stream_cnt;  // This Socket only communicate over this stream id!
    static_stream_cnt++;
    LOG(LOG_VERBOSE, "Init of client finished and connected");

    /* start a parser tree for SCTP QoS requirements and additional transport requirements for SCTP */
   changeRequirements(pRequirements);

    mIsClosed = false;
}

// In the end this is the client side implementation
NGNSocketConnection::NGNSocketConnection(int fd)
{
    mIsClosed = false;
    LOG(LOG_VERBOSE, "Setup a connection for server listen on socket ID %i", fd);
    mClient = SERVER;
    mSocket = fd;
    mStream = static_stream_cnt;
    mRequirements = new Requirements();
    LOG(LOG_VERBOSE, "Setup a server configuration");
}

NGNSocketConnection::~NGNSocketConnection()
{
    cancel(); // Just to be sure
    close(mSocket);
    mSocket = -1;
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

bool NGNSocketConnection::isClosed()
{
    if ((mSocket < 0) || (mIsClosed)){
        mSocket = -1;
        return true;
    }
    return false;
}

int NGNSocketConnection::availableBytes()
{
    return 0; //TODO
}

void NGNSocketConnection::read(char* pBuffer, int &pBufferSize)
{
    int fd = mSocket;
    bool bSignal = true;
    int on = 1;

    if(mSocket > 0)
    {
        if(mClient == SERVER)
        {
            LOG(LOG_VERBOSE, "Start Accept");
            if ((mClient = accept(mSocket, (struct sockaddr *)&mSock_addr, &mAddr_len)) < 0)
            {
                //TODO: handling of mIsClosed = true;
                LOG(LOG_ERROR, "Error accept");
            }
            LOG(LOG_VERBOSE, "Finished Accept");
        }
        if(mClient > SERVER)
            fd = mClient;
    
        socklen_t len = (socklen_t)0;;
        int flags = 0;

        while(bSignal){
            unsigned int    i;
            union {
                 struct sockaddr sa;
                 struct sockaddr_in sin;
                 struct sockaddr_in6 sin6;
            } addr;
            socklen_t       fromlen, infolen;
            struct sctp_rcvinfo info;
            unsigned int    infotype;
            struct iovec    iov;
            struct sctp_event event;
            uint16_t        event_types[] = { SCTP_ASSOC_CHANGE,
               SCTP_PEER_ADDR_CHANGE,
               SCTP_SHUTDOWN_EVENT,
               SCTP_ADAPTATION_INDICATION
            };
            memset(&event, 0, sizeof(event));
            event.se_assoc_id = SCTP_FUTURE_ASSOC;
            event.se_on = 1;
            for (i = 0; i < sizeof(event_types) / sizeof(uint16_t); i++) {
               event.se_type = event_types[i];
               if (setsockopt(fd, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(event)) < 0) {
                   LOG(LOG_ERROR,"setsockopt");
               }
            }

//          TODO Future Feature autocloser
            /* Configure auto-close timer. */
//            int timeout = 5;    //sec
//            if (setsockopt(fd, IPPROTO_SCTP, SCTP_AUTOCLOSE, &timeout, sizeof(timeout)) < 0) {
//                LOG(LOG_ERROR,"setsockopt SCTP_AUTOCLOSE");
//               exit(1);
//            }

            /* Enable delivery of SCTP_RCVINFO. */
            on = 1;
            if (setsockopt(fd, IPPROTO_SCTP, SCTP_RECVRCVINFO, &on, sizeof(on)) < 0) {
                LOG(LOG_ERROR,"setsockopt SCTP_RECVRCVINFO");
               exit(-1);
            }

            flags = 0;
            memset(&addr, 0, sizeof(addr));
            fromlen = (socklen_t) sizeof(addr);
            memset(&info, 0, sizeof(info));
            infolen = (socklen_t) sizeof(info);
            infotype = 0;
            iov.iov_base = &pBuffer[0];
            iov.iov_len = pBufferSize;
            LOG(LOG_VERBOSE,"Use buf of size %i", pBufferSize);
            int n = sctp_recvv(fd, &iov, 1, &addr.sa, &fromlen, &info, &infolen, &infotype, &flags);

            bSignal = false;
            pBufferSize = n;
            if (flags & MSG_NOTIFICATION) {
                       print_notification(iov.iov_base);
            } else {
                char            addrbuf[INET6_ADDRSTRLEN];
                const char     *ap;
                in_port_t       port;

                if (addr.sa.sa_family == AF_INET) {
                   ap = inet_ntop(AF_INET, &addr.sin.sin_addr,
                                  addrbuf, INET6_ADDRSTRLEN);
                   port = ntohs(addr.sin.sin_port);
                } else {
                   ap = inet_ntop(AF_INET6, &addr.sin6.sin6_addr,
                                  addrbuf, INET6_ADDRSTRLEN);
                   port = ntohs(addr.sin6.sin6_port);
                }
                LOG(LOG_VERBOSE,"Message received from %s:%u: len=%d", ap, port, (int) n);

                // Work for Frogger Signalling switch (infotype) {
                switch (infotype) {
                case SCTP_RECVV_RCVINFO:
                    LOG(LOG_VERBOSE,", sid=%u", info.rcv_sid);
                    {
                    LOG(LOG_VERBOSE, "I Got data %i  from Stream %i",n, info.rcv_sid);
                    switch(info.rcv_sid){
                       case SCTP_SIGNALING_STREAM:
                           bSignal = true;
                           printFroggerMSG(pBuffer,pBufferSize);
                           LOG(LOG_VERBOSE, "Ignore Signal...wait for Data");
                           break;
                       default:
                           bSignal = false;
                           break;
                    }
                    }
                    if (info.rcv_flags & SCTP_UNORDERED) {
                        LOG(LOG_VERBOSE,", unordered");
                    } else {
                        LOG(LOG_VERBOSE,", ssn=%u", info.rcv_ssn);
                    }
                    LOG(LOG_VERBOSE,", tsn=%u", info.rcv_tsn);
                    LOG(LOG_VERBOSE,", ppid=%u.\n", ntohl(info.rcv_ppid));
                    break;
                case SCTP_RECVV_NOINFO:
                case SCTP_RECVV_NXTINFO:
                case SCTP_RECVV_RN:

                    break;
                default:
                    LOG(LOG_VERBOSE," unknown infotype.\n");
                    break;
                }
            }
        }
    }else
        LOG(LOG_ERROR, "Invalid socket");
}

void NGNSocketConnection::printFroggerMSG(const char* buf, int len){
   if(len > 3){
       T_FROGGER_MSG* msg = (T_FROGGER_MSG*) buf;
       switch(msg->msg_type){
       case NGNFroggerMSG::RQ_NEW:
           LOG(LOG_VERBOSE,"New Requirement: Signal %i",msg->msg_type);
           LOG(LOG_VERBOSE,"Set Req. for Stream:      %i",msg->stream);
           break;
       case NGNFroggerMSG::RQ_RENEW:
           LOG(LOG_VERBOSE,"Reset Requirement: Signal %i",msg->msg_type);
           LOG(LOG_VERBOSE,"Set Req. for Stream:      %i",msg->stream);
           break;
       default:
           LOG(LOG_ERROR, "Type Not Supported");
           return;
       }
       LOG(LOG_VERBOSE,"Get Event:                   %i",msg->event);
       switch(msg->event){
       case NGNFroggerMSG::CV_RELIABLE  :
           LOG(LOG_VERBOSE,"CV_RELIABLE");
           break;
       case NGNFroggerMSG::CV_LIMIT_DELAY :
           LOG(LOG_VERBOSE,"CV_LIMIT_DELAY");
           break;
       case NGNFroggerMSG::CV_LIMIT_DATA_RATE :
           LOG(LOG_VERBOSE,"SCV_LIMIT_DELAY");
           break;
       case NGNFroggerMSG::CV_TRANSMIT_LOSSLESS :
           LOG(LOG_VERBOSE,"CV_LIMIT_DELAY");
           break;
       case NGNFroggerMSG::CV_TRANSMIT_CHUNKS :
           LOG(LOG_VERBOSE,"CV_LIMIT_DELAY");
           break;
       case NGNFroggerMSG::CV_TRANSMIT_STREAM:
           LOG(LOG_VERBOSE,"CV_LIMIT_DELAY");
           break;
       case NGNFroggerMSG::CV_TRANSMIT_BIT_ERRORS :
           LOG(LOG_VERBOSE,"CV_TRANSMIT_BIT_ERRORS");
           break;
       default:
           LOG(LOG_ERROR, "Event Not Supported");
           break;
       }
       LOG(LOG_VERBOSE,"The message have attributes  %i",msg->value_cnt);
       for(int j =0; j< msg->value_cnt;){
           int value = msg->values[j];
           LOG(LOG_VERBOSE,"Value %i = %i",++j,value);
       }
   }
   else
       LOG(LOG_ERROR, "Not a valid signal message");
}


void NGNSocketConnection::print_notification(void *buf)
{
    struct sctp_assoc_change *sac;
    struct sctp_paddr_change *spc;
    struct sctp_adaptation_event *sad;
    union sctp_notification *snp;
    char            addrbuf[INET6_ADDRSTRLEN];
    const char     *ap;
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;

    snp = (sctp_notification *)buf;

    switch (snp->sn_header.sn_type) {
    case SCTP_ASSOC_CHANGE:
        sac = &snp->sn_assoc_change;
        LOG(LOG_VERBOSE,"^^^ Association change: ");
        switch (sac->sac_state) {
        case SCTP_COMM_UP:
            LOG(LOG_VERBOSE,"Communication up (streams (in/out)=(%u/%u)).\n",
                   sac->sac_inbound_streams, sac->sac_outbound_streams);
            break;
        case SCTP_COMM_LOST:
            LOG(LOG_VERBOSE,"Communication lost (error=%d).\n", sac->sac_error);
            break;
        case SCTP_RESTART:
            LOG(LOG_VERBOSE,"Communication restarted (streams (in/out)=(%u/%u).\n",
                   sac->sac_inbound_streams, sac->sac_outbound_streams);
            break;
        case SCTP_SHUTDOWN_COMP:
            LOG(LOG_VERBOSE,"Communication completed.\n");
            break;
        case SCTP_CANT_STR_ASSOC:
            LOG(LOG_VERBOSE,"Communication couldn't be started.\n");
            break;
        default:
            LOG(LOG_VERBOSE,"Unknown state: %d.\n", sac->sac_state);
            break;
        }
        break;
    case SCTP_PEER_ADDR_CHANGE:
        spc = &snp->sn_paddr_change;
        if (spc->spc_aaddr.ss_family == AF_INET) {
            sin = (struct sockaddr_in *) &spc->spc_aaddr;
            ap = inet_ntop(AF_INET, &sin->sin_addr, addrbuf, INET6_ADDRSTRLEN);
        } else {
            sin6 = (struct sockaddr_in6 *) &spc->spc_aaddr;
            ap = inet_ntop(AF_INET6, &sin6->sin6_addr, addrbuf, INET6_ADDRSTRLEN);
        }
        LOG(LOG_VERBOSE,"^^^ Peer Address change: %s ", ap);
        switch (spc->spc_state) {
        case SCTP_ADDR_AVAILABLE:
            LOG(LOG_VERBOSE,"is available.\n");
            break;
        case SCTP_ADDR_UNREACHABLE:
            LOG(LOG_VERBOSE,"is not available (error=%d).\n", spc->spc_error);
            break;
        case SCTP_ADDR_REMOVED:
            LOG(LOG_VERBOSE,"was removed.\n");
            break;
        case SCTP_ADDR_ADDED:
            LOG(LOG_VERBOSE,"was added.\n");
            break;
        case SCTP_ADDR_MADE_PRIM:
            LOG(LOG_VERBOSE,"is primary.\n");
            break;
        default:
            LOG(LOG_VERBOSE,"unknown state (%d).\n", spc->spc_state);
            break;
        }
        break;
    case SCTP_SHUTDOWN_EVENT:
        LOG(LOG_VERBOSE,"^^^ Shutdown received.\n");
        break;
    case SCTP_ADAPTATION_INDICATION:
        sad = &snp->sn_adaptation_event;
        LOG(LOG_VERBOSE,"^^^ Adaptation indication 0x%08x received.\n",
               sad->sai_adaptation_ind);
        break;
    default:
        LOG(LOG_VERBOSE,"^^^ Unknown event of type: %u.\n", snp->sn_header.sn_type);
        break;
    };
}

void NGNSocketConnection::write(char* pBuffer, int pBufferSize)
{
    int fd = mSocket;
    size_t n;
    if(mSocket > 0)
    {
        if(mClient == SERVER)
        {
            LOG(LOG_VERBOSE, "Start Accept");
            if ((mClient = accept(mSocket, (struct sockaddr *)&mSock_addr, &mAddr_len)) < 0)
            {
                //TODO: handling of mIsClosed = true;
                LOG(LOG_ERROR, "Error accept");
            }
            LOG(LOG_VERBOSE, "Finished Accept");
        }
        else{
            writeToStream(pBuffer,pBufferSize,mStream);
        }

    }else
        LOG(LOG_ERROR, "Invalid socket");
}

void NGNSocketConnection::writeToStream(char* pBuffer, int pBufferSize,int iStream){

    int fd = mSocket;
    struct iovec    iov;
//    TODO Setup for future features
//    struct sctp_status status;
//    struct sctp_initmsg init;
    struct sctp_sndinfo info;

    int len;
    // Prepare stream transfer
    // TODO Currently the signalling will be sind over 0
    bzero(&info,sizeof(info));
    if(SCTP_SIGNALING_STREAM == iStream){
        LOG(LOG_VERBOSE, "We send signal information stream %i len %i", iStream, pBufferSize);
        printFroggerMSG(pBuffer, pBufferSize);
    }
    if(mClient > SERVER)
              int fd = mClient;
    else{
        LOG(LOG_VERBOSE, "Use mSocket (Im Server?) %i", mSocket);
    }

    memset(&info, 0, sizeof(info));
    info.snd_ppid = htonl(0);
    info.snd_flags = 0; //SCTP_UNORDERED;
    info.snd_sid = iStream;
    iov.iov_base = &pBuffer[0];
    iov.iov_len = pBufferSize;

    len = sctp_sendv(fd,(const struct iovec *) &iov, 1, NULL, 0, &info, sizeof(info), SCTP_SENDV_SNDINFO, 0);


    LOG(LOG_VERBOSE, "Send %i Data over Stream %i",len, info.snd_sid);
    if (len < 0)
    {
       //TODO: handling of mIsClosed = true;
       LOG(LOG_ERROR, "sctp_sendmsg %i", errno);
    }
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
//        if (mBlockingMode)
//        {
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
	// TODO Works only in 1:1 relation (One Server <-> One Client
    if(mClient != SERVER)
    {
        close(mClient);
        mClient = CLIENT;
    }
    else
        close(mSocket);
    }
    LOG(LOG_VERBOSE, "Canceled");
    mIsClosed = true;
}

Name* NGNSocketConnection::getName()
{
    LOG(LOG_VERBOSE, "Get Name");
    if(mSocket > 0)
    {
        // TODO Perhaps more information are possible
        return  new SocketName("Local Next Generation Network",0);
    }else
    {
        return NULL;
    }
}

Name* NGNSocketConnection::getRemoteName()
{
    if(mSocket  > 0)
    {
        // TODO Perhaps more information are possible
        return new SocketName("Remote Next Generation Network",0);
    }else
    {
        return NULL;
    }
}

bool NGNSocketConnection::changeRequirements(Requirements *pRequirements)
{
    mRequirements = pRequirements;
    //TODO: MBE Future Work, SCTP should react on Requirements
    NGNParseNetworkRequirment::parse(this,this);

    return true;
}

Requirements* NGNSocketConnection::getRequirements()
{
	return mRequirements;
}

Events NGNSocketConnection::getEvents()
{
	Events tResult;
	return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
