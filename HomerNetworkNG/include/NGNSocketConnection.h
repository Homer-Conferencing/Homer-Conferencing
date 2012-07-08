/*****************************************************************************
 *
 * Copyright (C) 2012 Martin Becke
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

#ifndef _GAPI_NGNSOCKET_CONNECTION_
#define _GAPI_NGNSOCKET_CONNECTION_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#ifdef LINUX
#include <getopt.h>
#endif
#include <errno.h>

#include <Requirements.h>


const int SERVER =  0;
const int CLIENT = -1;
const int UDP_ENCAPSULATION = 9899;
const int SPECIAL_PORT 	    = 6000;
namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class NGNSocketConnection:
	public IConnection
{
public:
    NGNSocketConnection(std::string pTarget, Requirements *pRequirements);
    NGNSocketConnection(int fd);
    virtual ~NGNSocketConnection( );

    virtual bool isClosed();
    virtual int availableBytes();
    virtual void read(char* pBuffer, int &pBufferSize); //TODO: support non blocking mode
    virtual void write(char* pBuffer, int pBufferSize);
    virtual bool getBlocking();
    virtual void setBlocking(bool pState);
    virtual void cancel();
    virtual Name* getName();
    virtual Name* getRemoteName();
    virtual bool changeRequirements(Requirements *pRequirements);
    virtual Requirements getRequirements();
    virtual Events getEvents();

private:
    int mSocket;	// Socket Descriptor for initialisation
    int mClient;
    union sock_union{
		struct sockaddr sa;
		struct sockaddr_in s4;
		struct sockaddr_in6 s6;
	} mSock_addr;
     
    socklen_t mAddr_len;
    char 		*mpLocal_addr_ptr;
    unsigned int 	mNr_local_addr;
    uint16_t 	mPort;
    int 		mBufsize;
    struct sctp_assoc_value mAV;
    bool mUnordered;
    bool mIpv4only;
    bool mIpv6only;
  
#ifdef SCTP_REMOTE_UDP_ENCAPS_PORT
    struct sctp_udpencaps mEncaps;
#endif
    Requirements    mRequirements;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
