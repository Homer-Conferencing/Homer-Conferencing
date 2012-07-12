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
 * Purpose: NGNParseRequirment
 * Author:  Martin Becke
 * Since:   2012-05-30
 */

#ifndef _GAPI_NGNPARSE_NETWORK_REQUIRMENT_
#define _GAPI_NGNPARSE_NETWORK_REQUIRMENT_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <NGNSocketBinding.h>
#include <NGNSocketConnection.h>

#include <HBSocketQoSSettings.h>
#include <Requirements.h>
#include <RequirementTransmitLossless.h>
#include <RequirementTransmitChunks.h>
#include <RequirementTransmitStream.h>
#include <RequirementTransmitBitErrors.h>
#include <RequirementTargetPort.h>
#include <RequirementLimitDelay.h>
#include <RequirementLimitDataRate.h>


#include <NGNFroggerMSG.h>

#define FIRST_CALL_REQUIRMENT    true
#define RECALL_REQUIRMENT        false

#define NOT_A_VALID_CLASS       -1
#define NOT_A_VALID_TARGET      -2

namespace Homer { namespace Base {    
///////////////////////////////////////////////////////////////////////////////
/**
 * Just a helper class to generate msgs for frogger
 */
class NGNParseNetworkRequirment
{
public:
   
    static int parse(NGNSocketBinding* tmp, NGNSocketConnection* target, int kind=FIRST_CALL_REQUIRMENT){
        Requirements *r = (tmp->getRequirements());
        return parseAndSignal(r, target, kind);
    }
    static int parse(NGNSocketConnection* tmp, NGNSocketConnection* target, int kind=FIRST_CALL_REQUIRMENT){
        Requirements *r = (tmp->getRequirements());
        return parseAndSignal(r, target, kind);
    }                        
                              
    static int parseAndSignal(Requirements* r = NULL, NGNSocketConnection* target = NULL, bool kind=FIRST_CALL_REQUIRMENT){
       if(r==NULL)
           return NOT_A_VALID_CLASS;
       if(target==NULL)
           return NOT_A_VALID_TARGET;
        
        // Parse it like Thomas by the List from Thomas
       /*
            + RequirementLimitDelay         => DONE
            + RequirementLimitDataRate      => DONE
            + RequirementTransmitLossless   => DONE
            + RequirementTransmitChunks     => DONE ->  requires choice of UDP/UDPLite
            + RequirementTransmitStream     => DONE ->  requires choice of TCP
            + RequirementTransmitBitErrors  => DONE ->  requires UDPLite

            Video IP:
            + RequirementTransmitChunks  <- falls UDP/UDPLite gewählt
            + RequirementTransmitStream  <- falls TCP gewählt ==> kannst du für diese Demo gnorieren
            + RequirementTransmitBitErrors  <- falls UDPLite gewählt ==> kannst du für diese Demo gnorieren, da das ein Widerspruch zu "lossless" ist



            Folgende Requirements übermittelt dann der Filetransfer:
            File NGN:
            + RequirementTransmitChunks
            + RequirementTransmitLossless
        */
       //######################
       // RequirementLimitDelay
       if(r->contains(RequirementLimitDelay::type()))
       {

            const int v_cnt = 1;
            const int tMaxDelay = 0;
            const int len = FOG_COMON_HEADER + v_cnt;
            char buf[len];
            int values[v_cnt];
            RequirementLimitDelay* tReqDelay = (RequirementLimitDelay*)r->get(RequirementLimitDelay::type());
            values[tMaxDelay] = tReqDelay->getMaxDelay();
            NGNFroggerMSG::setupRequirment(buf,len,target->mStream,NGNFroggerMSG::CV_LIMIT_DELAY,values,v_cnt,kind);
       }

       //######################
       // RequirementLimitDataRate
       if(r->contains(RequirementLimitDataRate::type()))
       {
            const int v_cnt = 2;
            const int len = FOG_COMON_HEADER + v_cnt;
            char buf[len];
            const int tMinDataRate_cnt = 0;
            const int tMaxDataRate_cnt = 1;
            int values[v_cnt];
            RequirementLimitDataRate* tReqDataRate = (RequirementLimitDataRate*)r->get(RequirementLimitDataRate::type());
            values[tMinDataRate_cnt] = tReqDataRate->getMinDataRate();
            values[tMaxDataRate_cnt] = tReqDataRate->getMaxDataRate();
            NGNFroggerMSG::setupRequirment(buf,len,target->mStream,NGNFroggerMSG::CV_LIMIT_DATA_RATE,values,v_cnt,kind);
       }
       //######################
       // RequirementTransmitLossless
       if(r->contains(RequirementTransmitLossless::type())){
           const int v_cnt = 0;
           const int len = FOG_COMON_HEADER + v_cnt;
           char buf[len];
           int values[v_cnt];
           NGNFroggerMSG::setupRequirment(buf,len,target->mStream,NGNFroggerMSG::CV_TRANSMIT_LOSSLESS,values,v_cnt,kind);

       }
       //######################
       // RequirementTransmitChunks
       if(r->contains(RequirementTransmitChunks::type())){
           const int v_cnt = 0;
           const int len = FOG_COMON_HEADER + v_cnt;
           int buf[len];
       }
       // RequirementTransmitStream
       //######################
       if(r->contains(RequirementTransmitStream::type())){
           const int v_cnt = 0;
           const int len = FOG_COMON_HEADER + v_cnt;
           char buf[len];
           int values[v_cnt];
           NGNFroggerMSG::setupRequirment(buf,len,target->mStream,NGNFroggerMSG::CV_TRANSMIT_STREAM,values,v_cnt,kind);

       }
       //######################
       // RequirementTransmitBitErrors
       if(r->contains(RequirementTransmitBitErrors::type())){
           const int v_cnt = 1;
           const int len = FOG_COMON_HEADER + v_cnt;
           char buf[len];
           const int tSecuredFrontDataSize_cnt = 0;
           int values[v_cnt];
           RequirementTransmitBitErrors* tReqBitErr = (RequirementTransmitBitErrors*)r->get(RequirementTransmitBitErrors::type());
           values[tSecuredFrontDataSize_cnt] = tReqBitErr->getSecuredFrontDataSize();
           NGNFroggerMSG::setupRequirment(buf,len,target->mStream,NGNFroggerMSG::CV_TRANSMIT_BIT_ERRORS,values,v_cnt,kind);
       }
       return 0;

    };
};
///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
