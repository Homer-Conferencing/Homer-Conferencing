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
 * Purpose: NGNFroggerMSG
 * Author:  Martin Becke
 * Since:   2012-05-30
 */

#ifndef _GAPI_NGNFROGGER_MSG_
#define _GAPI_NGNFROGGER_MSG_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <Logger.h>
#include <assert.h>



#define NOT_ENOUGH_BUF              -1
#define STREAM_ID_NOT_VALID         -2
#define NOT_SUPPORTED_REQUIRMENT    -3
#define VALUE_NOT_SUPPORTED         -4

/*
 * Format
 *   -------------------------------------------------------------------------------
 *  |   Type    |   Stream  |   Event   | Values Count  |  Values 1 | ... | Value n |
 *   -------------------------------------------------------------------------------
 *  Note: All 32 Bit Integer see struct T_FROGGER_MSG
 *  TODO Need we typsafe datatypes?
 *  TODO Values Not supported yet
 */
#define FOG_COMON_HEADER 4   // 4 = 4 x Integer

/*
 * Example SCTP Packet:
 *  SCTP HEADER
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     Source Port Number        |     Destination Port Number   |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                      Verification Tag                         |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                           Checksum                            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  Data Chunk
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Type = 0    | Reserved|U|B|E|    Length                     |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                              TSN                              |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |      Stream Identifier S      |   Stream Sequence Number n    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                  Payload Protocol Identifier                  |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  \                                                               \
 *  /                 User Data (seq n of Stream S)                 /
 *  \                                                               \
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * To identifier the stream, it is possible to use the stream ID. Or because we use different ports for different Streams the port number.
 * So crtieria could be
 * - Dest. Port   2 Bytes   6000 or 5000
 * - Stream ID    2 Byte    if chunk type = 0; Special Stream ID 0 = Signaling for Requirments
 *
 * Example
 *
 * Payload of UDP => SCTP Header
 *        [ 1 ] [ 2 ] [     3   ] [    4    ] [5][6] [ 7 ]
 * 0000   ea d6 13 88 9e 20 c6 da 42 99 d6 a4 00  03 04 b0  ..... ..B.......
 *        [    8    ] [   9  ]
 * 0010   bd fd 09 7f 00 01 00 0f 00 00 00 00              ............
 *
 *
 *
 * SCTP Common Header
 * 1 = Source Port  2 Bytes
 * 2 = Dest. Port   2 Bytes here 5000 !!!!!!
 * 3 = VTag         4 Bytes
 * 4 = Checksum     4 Bytes
 * SCTP Chunk (Because we use different sockets for different streams we have no Bundling, so it is enough to trace the first chunk)
 * 5 = Chunk Type   1 Byte  here 0 !!!!!!!!!
 * 6 = Chunk Flag   1 Byte
 * 7 = Chunk length 2 Byte
 * 8 = TSN          4 Byte
 * 9 = Stream ID    2 Byte  here 1 !!!!!!!!!
 *
 */

namespace Homer { namespace Base {
typedef struct _T_FROGGER_MSG{
    int     msg_type;
    int     stream;
    int     event;
    int     value_cnt;
    int*    values;
} T_FROGGER_MSG;
   
typedef int FROGGER_MSG_TYPE;
typedef int FROGGER_MSG_STREAM;
typedef int FROGGER_REQUIRMENT;
typedef int FROGGER_REQUIRMENT_VALUES;
    
///////////////////////////////////////////////////////////////////////////////
/**
 * Just a helper class to generate msgs for frogger
 */
class NGNFroggerMSG
{
public:
    // Numbering TBD    
    static const int RQ_NEW                     = 1;
    static const int RQ_RENEW                   = 2;
    // Numbering TBD    
    static const int CV_RELIABLE                = 1;
    static const int CV_LIMIT_DELAY             = 2;
    static const int CV_LIMIT_DATA_RATE         = 3;
    static const int CV_TRANSMIT_LOSSLESS       = 4;
    static const int CV_TRANSMIT_CHUNKS         = 5;
    static const int CV_TRANSMIT_STREAM         = 6;
    static const int CV_TRANSMIT_BIT_ERRORS     = 7;

    static int setupRequirment(char* buf, size_t len, FROGGER_MSG_STREAM aStream, FROGGER_REQUIRMENT aRequirment, FROGGER_REQUIRMENT_VALUES* aValue, int value_count = 0, bool first= true){
        
        memset((void*)buf,0,sizeof(T_FROGGER_MSG));
        T_FROGGER_MSG* msg = (T_FROGGER_MSG*) buf;
        if(first)
            msg->msg_type = RQ_NEW;
        else
            msg->msg_type = RQ_RENEW;
        // Proof and set Stream ID
        if(aStream > 65365){
            assert(false);
        }
        msg->stream = aStream;
        
       switch(aRequirment){
            case CV_RELIABLE:
            case CV_LIMIT_DELAY:
            case CV_LIMIT_DATA_RATE:
            case CV_TRANSMIT_LOSSLESS:
            case CV_TRANSMIT_CHUNKS:
            case CV_TRANSMIT_STREAM:
            case CV_TRANSMIT_BIT_ERRORS:
                msg->event = aRequirment;
                break;
            default:
                msg->event = -1;
                assert(false);
                break;
       };
       switch(aRequirment){
            case CV_LIMIT_DELAY:
            case CV_LIMIT_DATA_RATE:
            case CV_TRANSMIT_BIT_ERRORS:{
                  msg->value_cnt  = 0;
// TODO VALUES
//                msg->value_cnt = value_count;
//                size_t n = sizeof(int) * value_count;
//                memcpy(msg->values,aValue,n);
            }
                break;
            default:
               msg->value_cnt = 0;
               if(aValue != 0){
                   msg->value_cnt = -1;
               }
               break;
       };
       return sizeof(T_FROGGER_MSG);
    }
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
