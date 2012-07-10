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


#define NOT_ENOUGH_BUF              -1
#define STREAM_ID_NOT_VALID         -2
#define NOT_SUPPORTED_REQUIRMENT    -3
#define VALUE_NOT_SUPPORTED         -4



namespace Homer { namespace Base {
typedef struct _T_FROGGER_MSG{
    int     msg_type;
    int     stream;
    int     event;
    int     value;
} T_FROGGER_MSG;
   
typedef int FROGGER_MSG_TYPE;
typedef int FROGGER_MSG_STREAM;
typedef int FROGGER_REQUIRMENT;
typedef int FROGGER_REQUIRMENT_VALUE;
    
///////////////////////////////////////////////////////////////////////////////
/**
 * Just a helper class to generate msgs for frogger
 */
class NGNFroggerMSG
{
public:
    // Numbering TBD    
    static const int RQ_TYPE                    = 1;
    // Numbering TBD    
    static const int CV_RELIABLE                = 1;
    static const int CV_LIMIT_DELAY             = 2;
    static const int CV_LIMIT_DATA_RATE         = 3;
    static const int CV_TRANSMIT_LOSSLESS       = 4;
    static const int CV_TRANSMIT_CHUNKS         = 5;
    static const int CV_TRANSMIT_STREAM         = 6;
    static const int CV_TRANSMIT_BIT_ERRORS     = 7;
    
    static int setupRequirment(const char* buf, size_t len, FROGGER_MSG_STREAM aStream, FROGGER_REQUIRMENT aRequirment, FROGGER_REQUIRMENT_VALUE aValue=0){
        
//        if(len < (sizof(T_FROGGER_MSG)))
//            return NOT_ENOUGH_BUF;
        // Prepare Requirment MSG
        memset((void*)buf,0,sizeof(T_FROGGER_MSG));
        T_FROGGER_MSG* msg = (T_FROGGER_MSG*) buf;
        msg->msg_type = RQ_TYPE;
        
        // Proof and set Stream ID
        if(aStream > 16365)
            return STREAM_ID_NOT_VALID;
        msg->stream = aStream;
        
        // Proof and set requirment
        if(!(aRequirment==CV_RELIABLE ||
             aRequirment==CV_LIMIT_DELAY ||
             aRequirment==CV_LIMIT_DATA_RATE ||
             aRequirment==CV_TRANSMIT_LOSSLESS ||
             aRequirment==CV_TRANSMIT_CHUNKS ||
             aRequirment==CV_TRANSMIT_STREAM ||
             aRequirment==CV_TRANSMIT_BIT_ERRORS
             ))
            return NOT_SUPPORTED_REQUIRMENT;
        msg->event = aRequirment;
        
        // Proof and set value of requirment
        if(aRequirment==CV_TRANSMIT_LOSSLESS ||
           aRequirment==CV_TRANSMIT_CHUNKS ||
           aRequirment==CV_TRANSMIT_STREAM ||
           aRequirment==CV_TRANSMIT_BIT_ERRORS||
           aRequirment==CV_RELIABLE){
            if(aValue != 0)
                return VALUE_NOT_SUPPORTED;
            msg->value = 0;
        }
        else{
            if(aValue <= 0)
                return VALUE_NOT_SUPPORTED;
            msg->value = aValue;
        }
        return sizeof(T_FROGGER_MSG);
    }
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
